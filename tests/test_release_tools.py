# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_release
import push_to_quest

class ReleaseTools(unittest.TestCase):
    def test_version_and_pins(self):
        self.assertRegex(build_release.VERSION, r'^\d+\.\d{2,}$')
        for value in build_release.PINS.values():
            self.assertRegex(value['revision'], r'^[a-f0-9]{40}$')
            self.assertRegex(value['sha256'], r'^[a-f0-9]{64}$')
    def test_assets_paths_and_symlink_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); (root / 'a b').mkdir(); (root / 'a b/level1.lev').write_bytes(b'fixture')
            files = push_to_quest.asset_files(root)
            self.assertEqual(files[0][1], 'a b/level1.lev')
            try: (root / 'link').symlink_to(root / 'a b/level1.lev')
            except OSError: return
            with self.assertRaises(ValueError): push_to_quest.asset_files(root)
    def test_empty_assets_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError): push_to_quest.asset_files(Path(temp))
    def test_adb_dry_run_never_connects(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); (root / 'app.apk').write_bytes(b'fixture'); (root / 'assets').mkdir(); (root / 'assets/level1.lev').write_bytes(b'fixture')
            with patch.object(sys, 'argv', ['push_to_quest.py','--apk',str(root/'app.apk'),'--assets',str(root/'assets'),'--dry-run']), patch('subprocess.check_output', side_effect=AssertionError('adb executed')):
                push_to_quest.main()
    def test_adb_promotion_requires_verified_copy(self):
        import hashlib
        for damaged in (False, True):
            with self.subTest(damaged=damaged), tempfile.TemporaryDirectory() as temp:
                root = Path(temp); (root/'app.apk').write_bytes(b'fixture'); (root/'assets').mkdir(); (root/'assets/level1.lev').write_bytes(b'fixture')
                digest = hashlib.sha256(b'fixture').hexdigest(); commands = []
                def fake(command, **kwargs):
                    commands.append(command)
                    if command[-1] == 'devices': return 'List of devices attached\nquest-test\tdevice\n'
                    if 'shell' in command:
                        text = command[-1]
                        if text.startswith('test -d'): return 'ready'
                        if text.startswith('df -k'): return 'Filesystem 1K-blocks Used Available Use% Mounted on\n/dev/fuse 10000000 0 10000000 0% /sdcard'
                        if text.startswith('sha256sum'): return ('0'*64 if damaged else digest) + ' file'
                    return 'Success'
                with patch.object(sys, 'argv', ['push_to_quest.py','--apk',str(root/'app.apk'),'--assets',str(root/'assets')]), patch('shutil.which', return_value='/test/adb'), patch('subprocess.check_output', side_effect=fake):
                    if damaged:
                        with self.assertRaises(RuntimeError): push_to_quest.main()
                    else: push_to_quest.main()
                promotions = [c for c in commands if c[-1].startswith('mv ')]
                self.assertEqual(len(promotions), 0 if damaged else 1)
                self.assertFalse(any('/SAVE' in c[-1] for c in commands))
    def test_linux_launcher_argument_boundaries(self):
        if sys.platform == 'win32': self.skipTest('POSIX launcher')
        with tempfile.TemporaryDirectory(prefix='release with spaces ') as temp:
            root = Path(temp); (root / 'assets').mkdir()
            (root / 'play.sh').write_bytes((ROOT / 'packaging/desktop/play.sh').read_bytes())
            engine = root / 'reminiscence2'; engine.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n'); engine.chmod(0o755)
            result = subprocess.check_output(['sh',str(root/'play.sh'),'--window','literal $(echo nope)'],cwd='/',text=True).splitlines()
            self.assertEqual(result, ['--datapath='+str(root/'assets'),'--window','literal $(echo nope)'])
    def test_zip_preserves_launcher_mode_and_empty_assets_folder(self):
        import zipfile
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); directory = root/'package'; directory.mkdir(); (directory/'assets').mkdir()
            launcher = directory/'play.sh'; launcher.write_text('fixture'); launcher.chmod(0o755)
            output = root/'test.zip'; build_release.archive_directory(directory, output)
            with zipfile.ZipFile(output) as archive:
                self.assertIn('package/assets/', archive.namelist())
                self.assertTrue((archive.getinfo('package/play.sh').external_attr >> 16) & 0o111)

if __name__ == '__main__': unittest.main()
