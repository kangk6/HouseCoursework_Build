import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('team_assets', Path(__file__).resolve().parents[1] / 'assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class AssetPacks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        (self.source / 'Content/Test').mkdir(parents=True)
        (self.source / 'Content/Test/room.umap').write_bytes(b'original map')
        (self.source / 'Content/Test/lamp.uasset').write_bytes(b'original lamp')
        self.manifest = self.root / 'manifest.json'
        self.base = self.root / 'base'
        with contextlib.redirect_stdout(io.StringIO()):
            assets.export_assets(self.source, self.manifest, self.base, 'test-base', initial=True)
        self.target = self.root / 'target'

    def install(self, packs=None):
        with contextlib.redirect_stdout(io.StringIO()):
            assets.import_assets(self.target, self.manifest, packs or self.base)

    def test_roundtrip_and_no_pack_needed_when_up_to_date(self):
        self.install()
        self.install(self.root / 'no-download-needed')
        self.assertEqual((self.target / 'Content/Test/room.umap').read_bytes(), b'original map')

    def test_incremental_update_needs_only_delta(self):
        self.install()
        (self.source / 'Content/Test/lamp.uasset').write_bytes(b'changed lamp')
        (self.source / 'Content/Test/new.uasset').write_bytes(b'new asset')
        delta = self.root / 'delta'
        with contextlib.redirect_stdout(io.StringIO()):
            assets.export_assets(self.source, self.manifest, delta, 'test-update')
        self.assertEqual(len(list(delta.glob('*.zip'))), 1)
        self.install(delta)
        self.assertEqual((self.target / 'Content/Test/lamp.uasset').read_bytes(), b'changed lamp')
        self.assertEqual((self.target / 'Content/Test/new.uasset').read_bytes(), b'new asset')

    def test_local_edits_are_not_overwritten(self):
        self.install()
        local = self.target / 'Content/Test/lamp.uasset'
        local.write_bytes(b'unshared local edit')
        with self.assertRaisesRegex(ValueError, 'Local asset edit protected'):
            self.install()
        self.assertEqual(local.read_bytes(), b'unshared local edit')

    def test_reusing_delta_version_in_another_folder_is_rejected(self):
        lamp = self.source / 'Content/Test/lamp.uasset'
        lamp.write_bytes(b'first delta')
        with contextlib.redirect_stdout(io.StringIO()):
            assets.export_assets(self.source, self.manifest, self.root / 'delta-one', 'unique-update')
            lamp.write_bytes(b'second delta')
            with self.assertRaisesRegex(ValueError, 'version already exists'):
                assets.export_assets(self.source, self.manifest, self.root / 'delta-two', 'unique-update')

    def test_damaged_pack_fails_before_content_writes(self):
        archive = next(self.base.glob('*.zip'))
        archive.write_bytes(archive.read_bytes() + b'bad bytes')
        with self.assertRaisesRegex(ValueError, 'Missing or damaged'):
            self.install()
        self.assertFalse((self.target / 'Content').exists())

    def test_archive_path_escape_rejected(self):
        archive = next(self.base.glob('*.zip'))
        with zipfile.ZipFile(archive, 'a') as z:
            z.writestr('../escape.txt', b'outside')
        data = assets.load_manifest(self.manifest)
        data['packs'][0]['sha256'] = assets.digest(archive)
        data['packs'][0]['size'] = archive.stat().st_size
        assets.save_json(self.manifest, data)
        with self.assertRaisesRegex(ValueError, 'Unsafe Content path'):
            self.install()
        self.assertFalse((self.root / 'escape.txt').exists())

    def test_known_baseline_can_be_adopted_without_download(self):
        shutil.copytree(self.source / 'Content', self.target / 'Content')
        self.install(self.root / 'absent')
        self.assertTrue((self.target / '.team-assets/installed-manifest.json').exists())


if __name__ == '__main__':
    unittest.main()
