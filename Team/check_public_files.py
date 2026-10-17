"""Audit the committed/staged file list, never Content or private binary payloads."""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {'.cpp', '.h', '.cs', '.inl', '.py', '.ps1', '.json', '.md', '.ini', '.uproject', '.txt', '.yml', '.yaml'}


def main():
    paths = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode('utf-8').split('\0')
    errors = []
    for name in filter(None, paths):
        p = Path(name)
        if (p.parts[0].lower() in {'content', 'sourceassets', 'saved', 'binaries', 'intermediate', 'artifacts', '.team-assets'}
                or (p.suffix.lower() not in ALLOWED and p.name not in {'.gitignore', '.gitattributes'})
                or (ROOT / p).stat().st_size > 5 * 1024 * 1024):
            errors.append('Unexpected public file: ' + name)
            continue
        data = (ROOT / p).read_bytes()
        if b'\x00' in data:
            errors.append('Binary file: ' + name)
        text = data.decode('utf-8-sig', errors='replace')
        if re.search(r'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN [A-Z ]*PRIVATE KEY-----)', text):
            errors.append('Possible credential in: ' + name)
    if errors:
        print('\n'.join(errors))
        return 1
    print('Public-source audit passed: %d files.' % len([x for x in paths if x]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
