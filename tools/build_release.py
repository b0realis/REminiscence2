#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/test a native release and assemble a relocatable package with no game data."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent.parent
PINS = json.loads((ROOT / 'packaging/dependencies.json').read_text())
VERSION = re.search(r'REMINISCENCE2_VERSION "([0-9.]+)"', (ROOT / 'version.h').read_text()).group(1)


def run(*args, **kwargs):
    subprocess.run([str(x) for x in args], check=True, cwd=ROOT, **kwargs)


def prepare_dependency(name):
    """Retrieve exact sources, checking content before extraction or execution."""
    offline = ROOT / 'third_party_src' / name
    if offline.is_dir(): return offline
    pin = PINS[name]
    archive = ROOT / 'deps' / f'{name}.tar.gz'
    archive.parent.mkdir(exist_ok=True)
    data = archive.read_bytes() if archive.exists() else urllib.request.urlopen(pin['url'], timeout=120).read()
    if hashlib.sha256(data).hexdigest() != pin['sha256']:
        raise RuntimeError(f'{name}: source SHA-256 mismatch')
    archive.write_bytes(data)
    output = ROOT / 'deps' / name
    if not output.exists():
        staging = output.with_suffix('.extracting')
        staging.mkdir(exist_ok=False)
        try:
            links = []
            with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as source:
                for entry in source.getmembers():
                    parts = Path(entry.name).parts[1:]
                    if not parts:
                        continue
                    if any(p in ('..', '') for p in parts) or entry.islnk() or not (entry.isfile() or entry.isdir() or entry.issym()):
                        raise RuntimeError('Unsafe dependency archive member')
                    destination = staging.joinpath(*parts)
                    if entry.issym():
                        target = (destination.parent / entry.linkname).resolve()
                        if not target.is_relative_to(staging.resolve()): raise RuntimeError('Unsafe dependency symlink')
                        links.append((target, destination))
                        continue
                    if entry.isdir():
                        destination.mkdir(parents=True, exist_ok=True)
                    else:
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        destination.write_bytes(source.extractfile(entry).read())
                        destination.chmod(entry.mode & 0o777)
            # Materialize safe internal aliases on Windows as well as POSIX.
            for target, destination in links:
                destination.parent.mkdir(parents=True, exist_ok=True)
                if target.is_dir(): shutil.copytree(target, destination)
                else: shutil.copy2(target, destination)
            staging.rename(output)
        finally:
            if staging.exists():
                shutil.rmtree(staging)
    return output


def copy_licenses(licenses):
    licenses.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / 'LICENSE', licenses / 'REminiscence2-GPL.txt')
    shutil.copy2(ROOT / 'scalers/xbr/NOTICE.txt', licenses / 'xBRZ-NOTICE.txt')
    for source in (ROOT / 'third_party').glob('**/*'):
        if source.is_file() and ('license' in source.name.lower() or 'copying' in source.name.lower()):
            relative = source.relative_to(ROOT / 'third_party')
            target = licenses / relative; target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(source, target)
    shutil.copy2(prepare_dependency('SDL2') / 'LICENSE.txt', licenses / 'SDL2.txt')
    shutil.copy2(prepare_dependency('zlib') / 'LICENSE', licenses / 'zlib.txt')
    shutil.copy2(ROOT / 'packaging/licenses/COPYING.RUNTIME', licenses / 'COPYING.RUNTIME')
    (licenses / 'README.txt').write_text('Combined engine: GPLv3; new REminiscence2 contributions: GPL-3.0-or-later.\nSee individual third-party notices, including xBRZ-NOTICE.txt.\nGCC-built desktop packages include compiler runtimes under GPLv3 with the\nGCC Runtime Library Exception 3.1 (COPYING.RUNTIME). SDK/system libraries\ncome from the operating system. Corresponding engine/SDL/zlib sources are\nprovided with the same GitHub release. Original game assets are not bundled.\n')


def common_files(package, build):
    (package / 'assets').mkdir()
    shutil.copy2(ROOT / 'packaging/desktop/ASSETS.txt', package / 'assets/README.txt')
    for name in ('README.md', 'manual.md', 'LICENSE', 'CREDITS.md', 'CHANGELOG.md', 'version.h', 'fb.cfg'):
        if (ROOT / name).exists(): shutil.copy2(ROOT / name, package / name)
    # Include tracked documentation only; ignored scans/reference art stay private.
    tracked_docs = json.loads((ROOT / 'packaging/public-docs.json').read_text())
    for name in tracked_docs:
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0] != 'docs':
            raise RuntimeError('Unsafe public documentation manifest path')
        target = package / name; target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, target)
    shutil.copy2(ROOT / 'docs/RELEASES.md', package / 'RELEASES.md')
    shutil.copytree(ROOT / 'tools', package / 'tools', ignore=shutil.ignore_patterns('__pycache__'))
    (package / 'packaging').mkdir()
    for manifest in ('dependencies.json', 'public-docs.json'):
        shutil.copy2(ROOT / 'packaging' / manifest, package / 'packaging' / manifest)
    copy_licenses(package / 'licenses')
    (package / 'dependency-manifest.json').write_text(json.dumps({'version': VERSION, 'dependencies': PINS, 'music': 'internal MOD, AdLib/OPL3; no ROMs or soundfonts'}, indent=2) + '\n')


def runtime_dll(name):
    # shutil.which applies Windows PATHEXT executable rules and can reject DLLs.
    # Search literal filenames instead; DLLs do not need an executable suffix.
    for directory in os.environ.get('PATH', '').split(os.pathsep):
        candidate = Path(directory) / name
        if candidate.is_file(): return candidate
    raise RuntimeError(f'Missing runtime DLL {name}')


def archive_directory(directory, output):
    if output.suffix == '.zip':
        with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(directory.rglob('*')):
                archive.write(path, path.relative_to(directory.parent))
    else:
        with tarfile.open(output, 'w:gz') as archive:
            archive.add(directory, arcname=directory.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True, choices=['win64', 'linux64', 'steamdeck', 'macos-arm64', 'macos-x86_64', 'quest'])
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/dist')
    parser.add_argument('--quest-debug', action='store_true', help='Build a separate experimental debug APK when release credentials are unavailable')
    args = parser.parse_args()
    args.output = args.output.resolve(); args.output.mkdir(parents=True, exist_ok=True)
    build = (args.build_dir or ROOT / 'build' / ('package-' + args.target)).resolve()
    if args.target == 'quest':
        prepare_dependency('SDL2')
        copy_licenses(ROOT / 'packaging/android/app/src/main/assets/licenses')
        if not args.quest_debug and not os.environ.get('REMINISCENCE2_KEYSTORE'):
            parser.error('Quest release requires REMINISCENCE2_KEYSTORE and signing passwords; use --quest-debug for a separate experimental app')
        run('gradle', '--no-daemon', '-p', ROOT / 'packaging/android', 'assembleDebug' if args.quest_debug else 'assembleRelease')
        variant = 'debug' if args.quest_debug else 'release'
        apk = ROOT / f'packaging/android/app/build/outputs/apk/{variant}/app-{variant}.apk'
        destination = args.output / f'REminiscence2-{VERSION}-quest-arm64{("-experimental" if args.quest_debug else "")}.apk'
        shutil.copy2(apk, destination)
        bundle = args.output / (destination.stem + '-bundle')
        if bundle.exists(): shutil.rmtree(bundle)
        bundle.mkdir()
        shutil.copy2(destination, bundle / destination.name)
        (bundle / 'assets').mkdir()
        shutil.copy2(ROOT / 'packaging/desktop/ASSETS.txt', bundle / 'assets/README.txt')
        shutil.copy2(ROOT / 'tools/push_to_quest.py', bundle / 'push_to_quest.py')
        shutil.copy2(ROOT / 'docs/RELEASES.md', bundle / 'RELEASES.md')
        shutil.copy2(ROOT / 'manual.md', bundle / 'manual.md')
        shutil.copy2(ROOT / 'LICENSE', bundle / 'LICENSE')
        copy_licenses(bundle / 'licenses')
        launcher = bundle / 'push-to-quest.sh'
        launcher.write_text('#!/bin/sh\nset -eu\nPACKAGE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec python3 "$PACKAGE_DIR/push_to_quest.py" --apk "$PACKAGE_DIR/' + destination.name + '" --assets "$PACKAGE_DIR/assets" "$@"\n')
        launcher.chmod(0o755)
        archive_directory(bundle, args.output / (bundle.name + '.zip'))
        shutil.rmtree(bundle)
        print(destination); return
    options = ['-DCMAKE_BUILD_TYPE=Release', '-DREMINISCENCE2_BUNDLED_DEPS=ON', '-DREMINISCENCE2_USE_MODPLUG=OFF', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5']
    for dependency in ('SDL2', 'zlib'):
        offline = ROOT / 'third_party_src' / dependency
        if offline.is_dir(): options += [f'-DFETCHCONTENT_SOURCE_DIR_{dependency.upper()}={offline}']
    if args.target.startswith('macos-'):
        options += ['-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0', '-DCMAKE_OSX_ARCHITECTURES=' + args.target.removeprefix('macos-')]
    if args.target == 'win64': options += ['-G', 'Ninja']
    run('cmake', '-S', ROOT, '-B', build, *options)
    run('cmake', '--build', build, '--parallel', os.environ.get('CMAKE_BUILD_PARALLEL_LEVEL', '3'))
    run('ctest', '--test-dir', build, '--output-on-failure')
    name = f'REminiscence2-{VERSION}-{args.target}'
    staging = build / 'distribution' / name
    if staging.exists(): shutil.rmtree(staging)
    staging.mkdir(parents=True)
    common_files(staging, build)
    binary = staging / ('reminiscence2.exe' if args.target == 'win64' else 'reminiscence2')
    shutil.copy2(build / binary.name, binary)
    if args.target.startswith('macos-'):
        dependencies = subprocess.check_output(['otool', '-L', str(binary)], text=True)
        if any(line.strip().startswith(('/opt/', '/usr/local/', '@rpath/')) for line in dependencies.splitlines()[1:]):
            raise RuntimeError('Non-system macOS dependency remains in package')
        from package_macos import create_app, create_dmg
        app = create_app(binary, staging / 'REminiscence2.app', args.target.removeprefix('macos-'))
        run('codesign', '--force', '--sign', '-', binary)
        dmg = args.output / (name + '.dmg')
        receipt = create_dmg(app, dmg)
        (staging / 'dmg-verification.json').write_text(json.dumps(receipt, indent=2) + '\n')
        shutil.copy2(ROOT / 'packaging/desktop/Play.command', staging / 'Play.command')
        (staging / 'native-dependencies.txt').write_text(dependencies)
    elif args.target == 'win64':
        (staging / 'play.cmd').write_text('@echo off\r\ncd /d "%~dp0"\r\n"%~dp0reminiscence2.exe" %*\r\nif errorlevel 1 pause\r\n')
        # Only toolchain runtime DLLs may accompany this statically built engine.
        imports = subprocess.check_output(['objdump', '-p', str(binary)], text=True)
        runtime_names = {'libwinpthread-1.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll'}
        for dll in re.findall(r'DLL Name: (\S+)', imports):
            if dll.lower() in runtime_names:
                source = runtime_dll(dll)
                shutil.copy2(source, staging / dll)
        compiler = Path(shutil.which('gcc')).resolve()
        notices = compiler.parent.parent / 'share/licenses'
        if not notices.is_dir(): raise RuntimeError('UCRT64 runtime license directory is missing')
        shutil.copytree(notices, staging / 'licenses/UCRT64')
        (staging / 'native-dependencies.txt').write_text(imports)
    else:
        shutil.copy2(ROOT / 'packaging/desktop/play.sh', staging / 'play.sh')
        dependencies = subprocess.check_output(['ldd', str(binary)], text=True)
        forbidden = ('libSDL', 'libz.so', 'libmodplug', 'libstdc++', 'libgcc_s')
        if any(value in dependencies for value in forbidden): raise RuntimeError('Non-system Linux dependency remains: ' + dependencies)
        (staging / 'native-dependencies.txt').write_text(dependencies)
        if args.target == 'steamdeck':
            (staging / 'STEAM-DECK.txt').write_text('Add play.sh as a Non-Steam Game in Desktop Mode, then launch from Gaming Mode.\nUse the Gamepad layout. Start opens QoL; D-pad/stick and A navigate.\nHardware sleep/resume and Touch-controller mappings need device testing.\n')
    # Smoke-test the relocated engine, independent of the source/build CWD.
    subprocess.run([str(binary), '--version'], cwd=staging / 'assets', check=True, stdout=subprocess.DEVNULL)
    archive = args.output / (name + ('.zip' if args.target == 'win64' or args.target.startswith('macos-') else '.tar.gz'))
    archive_directory(staging, archive)
    print(archive)

if __name__ == '__main__': main()
