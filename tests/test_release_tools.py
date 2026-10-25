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
import publish_release
import package_private_mac
import compact_assets
import package_macos
import json

class ReleaseTools(unittest.TestCase):
    def test_compact_assets_atomic_copy_and_overlap(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root / 'owned'; source.mkdir()
            (source / 'level1.map').write_bytes(b'fixture')
            (source / 'voice.vce').write_bytes(b'voice')
            (source / '.DS_Store').write_bytes(b'finder')
            report = compact_assets.compact({'pc-gog': source}, root / 'assets')
            self.assertEqual(set(report['files']), {'pc-gog/level1.map', 'pc-gog/voice.vce'})
            self.assertEqual((root / 'assets/pc-gog/voice.vce').read_bytes(), b'voice')
            with self.assertRaises(ValueError): compact_assets.compact({'pc-gog': source}, root / 'assets')
            with self.assertRaises(ValueError): compact_assets.compact({'pc-gog': source}, source / 'output')
            with patch.object(compact_assets.shutil, 'copyfile', side_effect=OSError('disk full')):
                with self.assertRaises(OSError): compact_assets.compact({'pc-gog': source}, root / 'failed')
            self.assertFalse((root / 'failed').exists())
            self.assertEqual((source / 'voice.vce').read_bytes(), b'voice')
    def test_compact_rejects_conversion_filename_collision(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root / 'owned'; (source / 'DATA').mkdir(parents=True); (source / 'music').mkdir()
            (source / 'DATA/PRESENT.LEV').write_bytes(b'fixture')
            (source / 'music/track02.wav').write_bytes(b'fixture')
            (source / 'music/track02.ogg').write_bytes(b'fixture')
            with patch('shutil.which', return_value='/test/ffmpeg'):
                with self.assertRaises(ValueError): compact_assets.compact({'sega-cd': source}, root / 'result', 'ogg')
            self.assertFalse((root / 'result').exists())
    def test_public_dmg_rejects_private_bundle_before_execution(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); app = root / 'Game.app'; assets = app / 'Contents/Resources/assets'; assets.mkdir(parents=True)
            (assets / 'level1.map').write_bytes(b'private fixture')
            with patch.object(package_macos, 'run', side_effect=AssertionError('Native command called')):
                with self.assertRaises(ValueError): package_macos.create_dmg(app, root / 'public.dmg')
    def test_private_assets_are_selected_without_changing_sources(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name in ('DATA/PRESENT.LEV', 'DATA/VOICE.VCE', 'SC1.MCD', 'SUB.BIN',
                         'music/track02.wav', 'MAIN.BIN', 'DATA/TEST.TST', '.DS_Store'):
                path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'fixture')
            selected = {rel.as_posix() for source, rel in package_private_mac.asset_files(root, 'sega-cd')}
            self.assertEqual(selected, {'DATA/PRESENT.LEV', 'DATA/VOICE.VCE', 'SC1.MCD', 'SUB.BIN', 'music/track02.wav'})
            self.assertTrue((root / 'MAIN.BIN').exists())
            with self.assertRaises(ValueError): package_private_mac.asset_files(root, 'amiga')
            try: (root / 'alias').symlink_to(root / 'SC1.MCD')
            except OSError: return
            with self.assertRaises(ValueError): package_private_mac.asset_files(root, 'sega-cd')
    def test_private_amiga_requires_prepared_resources(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); (root / 'data').mkdir()
            (root / 'data/level1.lev').write_bytes(b'RNC fixture')
            with self.assertRaises(ValueError): package_private_mac.asset_files(root, 'amiga')
            (root / 'data/level1.lev').write_bytes(b'unpacked fixture')
            (root / 'flashback').write_bytes(b'original executable')
            self.assertEqual([str(rel) for _, rel in package_private_mac.asset_files(root, 'amiga')], ['data/level1.lev'])
    @unittest.skipUnless(sys.platform == 'darwin', 'Native macOS launcher')
    def test_mac_launcher_bundled_and_adjacent_assets_after_relocation(self):
        import plistlib
        with tempfile.TemporaryDirectory(prefix='relocated mac app ') as temp:
            root = Path(temp); app = root / 'REminiscence2.app'
            macos = app / 'Contents/MacOS'; macos.mkdir(parents=True)
            resources = app / 'Contents/Resources'; resources.mkdir()
            (app / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'Play', 'CFBundlePackageType': 'APPL'}))
            engine = macos / 'reminiscence2'
            engine.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n'); engine.chmod(0o755)
            subprocess.run(['clang', str(ROOT / 'packaging/desktop/mac_launcher.m'), '-framework', 'Cocoa', '-o', str(macos / 'Play')], check=True)
            # Informational CLI use must never open a missing-assets modal.
            actual = subprocess.check_output([str(macos / 'Play'), '--version'], cwd='/', text=True).splitlines()
            self.assertTrue(actual[0].endswith('/Library/Application Support/REminiscence2/assets'))
            self.assertEqual(actual[1], '--version')
            (resources / 'assets').mkdir(); (resources / 'assets/empty').mkdir()
            (resources / 'assets/.DS_Store').write_bytes(b'ignored')
            for assets in (root / 'assets', resources / 'assets'):
                assets.mkdir(exist_ok=True); (assets / 'fixture').write_bytes(b'fixture')
                actual = subprocess.check_output([str(macos / 'Play'), '--window', 'literal $(echo nope)'], cwd='/', text=True).splitlines()
                self.assertEqual(actual, ['--datapath=' + str(assets), '--window', 'literal $(echo nope)'])
    def test_version_and_pins(self):
        self.assertRegex(build_release.VERSION, r'^\d+\.\d{2,}$')
        for value in build_release.PINS.values():
            self.assertRegex(value['revision'], r'^[a-f0-9]{40}$')
            self.assertRegex(value['sha256'], r'^[a-f0-9]{64}$')
    def test_public_docs_package_without_git_or_private_files(self):
        import json
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)/'source'; package = Path(temp)/'package'
            (source/'packaging/desktop').mkdir(parents=True); (source/'docs').mkdir(); (source/'tools').mkdir(); package.mkdir()
            (source/'packaging/desktop/ASSETS.txt').write_text('placeholder')
            (source/'packaging/public-docs.json').write_text(json.dumps(['docs/RELEASES.md']))
            (source/'packaging/dependencies.json').write_text('{}')
            (source/'docs/RELEASES.md').write_text('public fixture'); (source/'docs/private.pdf').write_bytes(b'private fixture')
            with patch.object(build_release,'ROOT',source), patch.object(build_release,'copy_licenses',side_effect=lambda path:path.mkdir()), patch('subprocess.check_output',side_effect=AssertionError('Git was executed')):
                build_release.common_files(package,Path(temp))
            self.assertTrue((package/'docs/RELEASES.md').is_file())
            self.assertFalse((package/'docs/private.pdf').exists())
    def test_offline_dependency_source_needs_no_download(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root/'third_party_src/SDL2'; source.mkdir(parents=True)
            with patch.object(build_release,'ROOT',root), patch('urllib.request.urlopen',side_effect=AssertionError('Network accessed')):
                self.assertEqual(build_release.prepare_dependency('SDL2'),source)
    def test_dll_lookup_does_not_apply_executable_suffixes(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'libwinpthread-1.dll'; path.write_bytes(b'fixture'); path.chmod(0o644)
            with patch.dict('os.environ', {'PATH': temp}):
                self.assertEqual(build_release.runtime_dll(path.name), path)
                with self.assertRaises(RuntimeError): build_release.runtime_dll('missing.dll')
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
    def test_publication_checks_full_artifact_set_and_private_files(self):
        import io, tarfile, zipfile
        version = build_release.VERSION; prefix = 'REminiscence2-' + version
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for target in ('win64', 'macos-arm64', 'macos-x86_64', 'quest-arm64-bundle'):
                with zipfile.ZipFile(root / (prefix+'-'+target+'.zip'), 'w') as z:
                    z.writestr(prefix+'-'+target+'/assets/README.txt','placeholder')
            with zipfile.ZipFile(root/(prefix+'-quest-arm64.apk'),'w') as z:
                z.writestr('lib/arm64-v8a/libmain.so',b'fixture'); z.writestr('lib/arm64-v8a/libSDL2.so',b'fixture')
            for target in ('linux64','steamdeck','source'):
                with tarfile.open(root/(prefix+'-'+target+'.tar.gz'),'w:gz') as archive:
                    data = {'assets/README.txt': b'placeholder'}
                    if target == 'source': data = {'version.h': ('#define REMINISCENCE2_VERSION "'+version+'"').encode(), 'third_party_src/SDL2/file': b'fixture', 'third_party_src/zlib/file': b'fixture'}
                    for name,content in data.items():
                        info = tarfile.TarInfo(prefix+'-'+target+'/'+name); info.size = len(content); archive.addfile(info,io.BytesIO(content))
            for target in ('macos-arm64', 'macos-x86_64'):
                dmg = root / (prefix + '-' + target + '.dmg'); dmg.write_bytes(b'DMG fixture')
                receipt = {'file': dmg.name, 'sha256': compact_assets.digest(dmg), 'asset_free': True,
                           'files': ['REminiscence2.app/Contents/Resources/assets/README.txt']}
                with zipfile.ZipFile(dmg.with_suffix('.zip'), 'a') as z:
                    z.writestr(prefix + '-' + target + '/dmg-verification.json', json.dumps(receipt))
            self.assertEqual(len(publish_release.verify_files(root)),10)
            dmg.write_bytes(b'altered DMG')
            with self.assertRaises(RuntimeError): publish_release.verify_files(root)
            dmg.write_bytes(b'DMG fixture')
            windows = root/(prefix+'-win64.zip')
            for suffix in ('R2S', 'R2M'):
                with zipfile.ZipFile(windows,'a') as z: z.writestr(prefix+'-win64/SAVE/private.'+suffix,b'private preview fixture')
                with self.assertRaises(RuntimeError): publish_release.verify_files(root)
                with zipfile.ZipFile(windows,'w') as z: z.writestr(prefix+'-win64/assets/README.txt',b'placeholder')
            with zipfile.ZipFile(root/(prefix+'-win64.zip'),'a') as z: z.writestr(prefix+'-win64/signing.p12',b'private fixture')
            with self.assertRaises(RuntimeError): publish_release.verify_files(root)
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
