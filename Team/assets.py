"""Private Content packs for the public source-only team repository (stdlib only)."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tempfile
import zipfile


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def save_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    os.replace(temporary, path)


def safe_name(name):
    p = PurePosixPath(name)
    if (not name or '\\' in name or ':' in name or p.is_absolute()
            or '..' in p.parts or '.' in name.split('/') or '' in name.split('/')
            or len(p.parts) < 2 or p.parts[0] != 'Content'):
        raise ValueError('Unsafe Content path: ' + name)
    return p


def target_path(root, name):
    safe_name(name)
    result = root.joinpath(*PurePosixPath(name).parts)
    if not result.resolve().is_relative_to((root / 'Content').resolve()):
        raise ValueError('Content path escapes workspace: ' + name)
    return result


def load_manifest(path):
    data = json.loads(Path(path).read_text(encoding='utf-8-sig'))
    if data.get('format') != 1 or not data.get('files'):
        raise ValueError('Unsupported or empty asset manifest')
    paths, packs = set(), {}
    for pack in data['packs']:
        name = pack['name']
        if Path(name).name != name or '\\' in name or '/' in name or ':' in name or name in packs:
            raise ValueError('Unsafe or duplicate pack name')
        packs[name] = pack
    for item in data['files']:
        safe_name(item['path'])
        key = item['path'].casefold()
        if key in paths or item['pack'] not in packs or len(item['sha256']) != 64:
            raise ValueError('Invalid asset record: ' + item['path'])
        paths.add(key)
    return data


def export_assets(root, manifest_path, output, version, tracked_only=False, initial=False):
    root, output = Path(root).resolve(), Path(output).resolve()
    if not version or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._' for c in version):
        raise ValueError('Use an ASCII version such as v0.17.1-base or v0.18.0-kitchen')
    if not (root / 'Content').is_dir() or output.is_relative_to(root / 'Content'):
        raise ValueError('Content is missing or output is inside Content')
    old = None if initial or not Path(manifest_path).exists() else load_manifest(manifest_path)
    prior = {x['path']: x for x in old['files']} if old else {}
    if tracked_only:
        names = subprocess.check_output(['git', 'ls-files', '-z', 'Content'], cwd=root).decode('utf-8').split('\0')
        names = [x for x in names if x]
    else:
        names = [p.relative_to(root).as_posix() for p in (root / 'Content').rglob('*')
                 if p.is_file() and not any(x in p.parts for x in ['__pycache__', 'Developers', 'Collections'])
                 and p.suffix.lower() not in ['.pyc', '.tmp', '.bak']]
    if not names:
        raise ValueError('Refusing to export an empty Content folder')
    records, groups = [], {}
    print('Hashing %d Content files...' % len(names), flush=True)
    for name in sorted(names):
        p = target_path(root, name)
        checksum = digest(p)
        previous = prior.get(name)
        if previous and checksum == previous['sha256']:
            records.append(previous)
            continue
        if initial:
            folder = PurePosixPath(name).parts[1]
            group = {'HouseFurniture': 'house', 'ModernHouse': 'modern', 'StarterContent': 'starter'}.get(folder, 'coursework')
        else:
            group = 'delta'
        pack = 'assets-' + version + '-' + group + '.zip'
        records.append({'path': name, 'size': p.stat().st_size, 'sha256': checksum, 'pack': pack})
        groups.setdefault(pack, []).append(name)
    if old and not groups and set(prior) == set(names):
        print('No asset changes. Manifest kept unchanged.', flush=True)
        return old
    old_pack_names = {x['name'] for x in old['packs']} if old else set()
    if set(groups) & old_pack_names:
        raise ValueError('Pack version already exists in the manifest; choose a new unique version')
    output.mkdir(parents=True, exist_ok=True)
    for name in groups:
        if (output / name).exists():
            raise FileExistsError('Pack already exists; choose a new version: ' + name)
    packs = {x['name']: x for x in old['packs']} if old else {}
    for name, members in sorted(groups.items()):
        archive = output / name
        print('Creating %s (%d files)...' % (name, len(members)), flush=True)
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=1, allowZip64=True) as z:
            for member in members:
                z.write(target_path(root, member), member)
        packs[name] = {'name': name, 'size': archive.stat().st_size, 'sha256': digest(archive)}
    used = {x['pack'] for x in records}
    manifest = {'format': 1, 'version': version, 'files': records,
                'packs': [packs[x] for x in sorted(used)]}
    save_json(output / 'asset-manifest.json', manifest)
    save_json(manifest_path, manifest)
    print('Exported %d changed files in %d new packs. Private upload directory: %s' %
          (sum(map(len, groups.values())), len(groups), output), flush=True)
    return manifest


def check_assets(root, manifest_path):
    root = Path(root).resolve()
    data = load_manifest(manifest_path)
    missing, changed = [], []
    for item in data['files']:
        p = target_path(root, item['path'])
        if not p.is_file():
            missing.append(item['path'])
        elif p.stat().st_size != item['size'] or digest(p) != item['sha256']:
            changed.append(item['path'])
    if missing or changed:
        raise ValueError('Assets differ from manifest. Missing=%d Changed=%d\n%s' %
                         (len(missing), len(changed), '\n'.join((missing + changed)[:12])))
    print('Verified %d assets, version %s.' % (len(data['files']), data['version']), flush=True)
    return data


def import_assets(root, manifest_path, package_directory):
    root, package_directory = Path(root).resolve(), Path(package_directory).resolve()
    data = load_manifest(manifest_path)
    state = root / '.team-assets' / 'installed-manifest.json'
    previous = {x['path']: x for x in load_manifest(state)['files']} if state.exists() else {}
    pending = {}
    # Preflight before writing any file. Never replace an unexported local edit.
    for item in data['files']:
        p = target_path(root, item['path'])
        if p.exists():
            checksum = digest(p)
            if checksum == item['sha256']:
                continue
            if item['path'] not in previous or previous[item['path']]['sha256'] != checksum:
                raise ValueError('Local asset edit protected. Export or back it up first: ' + item['path'])
        pending[item['path']] = item
    required = {x['pack'] for x in pending.values()}
    packs = {x['name']: x for x in data['packs']}
    for name in sorted(required):
        p = package_directory / name
        print('Verifying pack ' + name, flush=True)
        if not p.is_file() or p.stat().st_size != packs[name]['size'] or digest(p) != packs[name]['sha256']:
            raise ValueError('Missing or damaged private pack: ' + str(p))
        with zipfile.ZipFile(p) as z:
            entries = set()
            for entry in z.infolist():
                safe_name(entry.filename.rstrip('/'))
                key = entry.filename.casefold()
                if key in entries or stat.S_ISLNK(entry.external_attr >> 16):
                    raise ValueError('Duplicate path or symlink in pack')
                entries.add(key)
            for item in pending.values():
                if item['pack'] == name:
                    info = z.getinfo(item['path'])
                    if info.file_size != item['size']:
                        raise ValueError('Wrong unpacked size: ' + item['path'])
    for name in sorted(required):
        print('Importing ' + name, flush=True)
        with zipfile.ZipFile(package_directory / name) as z:
            for item in pending.values():
                if item['pack'] != name:
                    continue
                p = target_path(root, item['path'])
                p.parent.mkdir(parents=True, exist_ok=True)
                temporary = None
                try:
                    with tempfile.NamedTemporaryFile(dir=p.parent, prefix='.team-import-', delete=False) as tmp:
                        temporary = Path(tmp.name)
                        with z.open(item['path']) as source:
                            shutil.copyfileobj(source, tmp, 1024 * 1024)
                    if digest(temporary) != item['sha256']:
                        raise ValueError('Extracted checksum mismatch: ' + item['path'])
                    os.replace(temporary, p)
                finally:
                    if temporary and temporary.exists():
                        temporary.unlink()
    # Old managed files removed from a newer manifest remain on disk; no broad delete.
    check_assets(root, manifest_path)
    save_json(state, data)
    print('Import complete. Local edits were protected.', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['export', 'import', 'check'])
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--packages', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--version')
    parser.add_argument('--initial', action='store_true')
    parser.add_argument('--tracked-only', action='store_true')
    args = parser.parse_args()
    manifest = args.manifest or args.root / 'Team' / 'asset-manifest.json'
    try:
        if args.command == 'export':
            if not args.output or not args.version:
                parser.error('export needs --output and --version')
            export_assets(args.root, manifest, args.output, args.version, args.tracked_only, args.initial)
        elif args.command == 'import':
            if not args.packages:
                parser.error('import needs --packages')
            import_assets(args.root, manifest, args.packages)
        else:
            check_assets(args.root, manifest)
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
