import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import prepare_wine_patches as patches


def sha(value):
    return hashlib.sha256(value).hexdigest()


class PatchPreparationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.source = root / 'source'
        self.source.mkdir()
        (self.source / 'fixture.c').write_bytes(b'old\n')
        self.patches = root / 'patches'
        self.patches.mkdir()
        data = b'--- a/fixture.c\n+++ b/fixture.c\n@@ -1 +1 @@\n-old\n+new\n'
        (self.patches / 'fix.patch').write_bytes(data)
        self.manifest = {'patches': [{'file': 'fix.patch', 'sha256': sha(data), 'before': {'fixture.c': sha(b'old\n')}, 'after': {'fixture.c': sha(b'new\n')}}], 'final_files': {'fixture.c': sha(b'new\n')}}
        self.write_manifest()

    def write_manifest(self):
        (self.patches / 'series.json').write_text(json.dumps(self.manifest))

    def prepare(self, apply):
        with patch.object(patches, 'PATCHES', self.patches), contextlib.redirect_stdout(io.StringIO()):
            patches.prepare(self.source, apply)

    def test_check_does_not_modify_source_and_apply_is_idempotent(self):
        self.prepare(False)
        self.assertEqual((self.source / 'fixture.c').read_bytes(), b'old\n')
        self.prepare(True)
        self.prepare(True)
        self.assertEqual((self.source / 'fixture.c').read_bytes(), b'new\n')

    def test_changed_input_is_rejected_without_overwrite(self):
        (self.source / 'fixture.c').write_bytes(b'user edit\n')
        with self.assertRaisesRegex(SystemExit, 'Source checksum mismatch'):
            self.prepare(True)
        self.assertEqual((self.source / 'fixture.c').read_bytes(), b'user edit\n')

    def test_successful_patch_exit_is_insufficient_when_result_hash_differs(self):
        self.manifest['patches'][0]['after']['fixture.c'] = sha(b'not the patch result\n')
        self.write_manifest()
        with self.assertRaisesRegex(SystemExit, 'Source checksum mismatch'):
            self.prepare(True)
        self.assertEqual((self.source / 'fixture.c').read_bytes(), b'old\n')

    def test_source_symlink_cannot_modify_a_file_outside_the_copy(self):
        outside = self.source.parent / 'outside.c'
        outside.write_bytes(b'old\n')
        (self.source / 'fixture.c').unlink()
        (self.source / 'fixture.c').symlink_to(outside)
        with self.assertRaisesRegex(SystemExit, 'leaves the selected copy'):
            self.prepare(True)
        self.assertEqual(outside.read_bytes(), b'old\n')


if __name__ == '__main__':
    unittest.main()
