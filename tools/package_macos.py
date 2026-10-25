#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build an asset-free macOS app and drag-to-Applications DMG from a Release engine.

Run on macOS with Xcode command line tools. Original game files are installed
separately in ~/Library/Application Support/REminiscence2/assets.
"""
import argparse
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
from compact_assets import digest, run


def create_app(binary, app, arch, icon=None):
    from build_release import ROOT, VERSION, copy_licenses
    from package_private_mac import install_icon
    binary, app = Path(binary).resolve(), Path(app)
    if app.exists():
        raise ValueError('App output already exists')
    actual = subprocess.check_output(['lipo', '-archs', str(binary)], text=True).strip()
    if actual != arch:
        raise ValueError(f'Expected {arch}, got {actual}')
    dependencies = subprocess.check_output(['otool', '-L', str(binary)], text=True)
    for line in dependencies.splitlines()[1:]:
        if not line.strip().startswith(('/usr/lib/', '/System/Library/')):
            raise ValueError('Non-system dependency: ' + line.strip())
    version = subprocess.check_output([str(binary), '--version'], text=True)
    if f'REminiscence2 {VERSION}\n' not in version:
        raise ValueError('Rebuild the engine for the current source version first')
    macos, resources = app / 'Contents/MacOS', app / 'Contents/Resources'
    macos.mkdir(parents=True)
    resources.mkdir()
    shutil.copy2(binary, macos / 'reminiscence2')
    run('strip', '-x', macos / 'reminiscence2')
    run('clang', '-arch', arch, '-mmacosx-version-min=11.0', ROOT / 'packaging/desktop/mac_launcher.m', '-framework', 'Cocoa', '-o', macos / 'Play')
    numeric = VERSION.split('.')[0] + '.0.' + str(int(VERSION.split('.')[1]))
    info = {'CFBundleIdentifier': 'org.b0realis.reminiscence2', 'CFBundleName': 'REminiscence2',
            'CFBundleExecutable': 'Play', 'CFBundlePackageType': 'APPL',
            'CFBundleShortVersionString': numeric, 'CFBundleVersion': numeric,
            'LSMinimumSystemVersion': '11.0', 'NSHighResolutionCapable': True}
    if icon:
        info['CFBundleIconFile'] = install_icon(Path(icon).resolve(), resources).name
    (app / 'Contents/Info.plist').write_bytes(plistlib.dumps(info))
    for name in ('manual.md', 'CREDITS.md', 'fb.cfg'):
        shutil.copy2(ROOT / name, resources / name)
    copy_licenses(resources / 'licenses')
    (resources / 'assets').mkdir()
    shutil.copy2(ROOT / 'packaging/desktop/ASSETS.txt', resources / 'assets/README.txt')
    for target in (macos / 'reminiscence2', macos / 'Play', app):
        run('codesign', '--force', '--sign', '-', target)
    run('codesign', '--verify', '--deep', '--strict', app)
    return app


def create_dmg(app, output):
    from build_release import ROOT, VERSION
    app, output = Path(app).resolve(), Path(output).absolute()
    if output.exists():
        raise ValueError('DMG output already exists')
    # Accept only the known engine-only bundle layout. Never package a personal
    # bundle or infer that a populated assets directory is safe to distribute.
    allowed = {'Contents/Info.plist', 'Contents/MacOS/Play', 'Contents/MacOS/reminiscence2',
               'Contents/Resources/manual.md', 'Contents/Resources/CREDITS.md',
               'Contents/Resources/fb.cfg', 'Contents/Resources/REminiscence2.icns',
               'Contents/Resources/assets/README.txt'}
    for path in app.rglob('*'):
        relative = path.relative_to(app).as_posix()
        if path.is_symlink():
            raise ValueError('Unexpected symlink in public app')
        if path.is_file() and relative not in allowed and not relative.startswith(('Contents/Resources/licenses/', 'Contents/_CodeSignature/')):
            raise ValueError('Unexpected file in asset-free app: ' + relative)
    run('codesign', '--verify', '--deep', '--strict', app)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='reminiscence2-dmg-') as temp:
        stage = Path(temp) / 'disk'; stage.mkdir()
        shutil.copytree(app, stage / app.name)
        (stage / 'Applications').symlink_to('/Applications')
        (stage / 'assets').mkdir()
        shutil.copy2(ROOT / 'packaging/desktop/ASSETS.txt', stage / 'assets/README.txt')
        shutil.copy2(ROOT / 'tools/compact_assets.py', stage / 'compact_assets.py')
        shutil.copy2(ROOT / 'tools/MAC_PACKAGES.md', stage / 'MAC_PACKAGES.md')
        run('hdiutil', 'create', '-quiet', '-srcfolder', stage, '-fs', 'HFS+', '-volname', 'REminiscence2 ' + VERSION, '-format', 'ULMO', output)
    run('hdiutil', 'verify', '-quiet', output)
    # The signed app is verified again from the actual mounted DMG. CI can then
    # pass this content inventory and digest to the cross-platform publisher.
    with tempfile.TemporaryDirectory(prefix='reminiscence2-mount-') as temp:
        mount = Path(temp)
        run('hdiutil', 'attach', '-quiet', '-nobrowse', '-readonly', '-mountpoint', mount, output)
        try:
            run('codesign', '--verify', '--deep', '--strict', mount / app.name)
            names = sorted(p.relative_to(mount).as_posix() for p in mount.rglob('*') if p.is_file() and not p.is_symlink())
            if any('/assets/' in '/' + n and not n.endswith('/assets/README.txt') and n != 'assets/README.txt' for n in names):
                raise ValueError('Game data found in public DMG')
        finally:
            run('hdiutil', 'detach', '-quiet', mount)
    return {'file': output.name, 'sha256': digest(output), 'files': names, 'asset_free': True}


def main():
    from build_release import VERSION
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--arch', choices=('arm64', 'x86_64'), required=True)
    parser.add_argument('--output', type=Path, required=True, help='Fresh directory for app and DMG')
    parser.add_argument('--icon', type=Path, help='Optional square transparent PNG artwork')
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('Run this tool on macOS')
    if args.output.exists():
        parser.error('Choose a new output directory')
    args.output.mkdir(parents=True)
    app = create_app(args.binary, args.output / 'REminiscence2.app', args.arch, args.icon)
    dmg = args.output / f'REminiscence2-{VERSION}-macos-{args.arch}.dmg'
    report = create_dmg(app, dmg)
    (args.output / 'dmg-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(dmg)


if __name__ == '__main__':
    main()
