#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare a PRIVATE, self-contained Apple Silicon app from owned, extracted assets.

No downloads or game extraction occur here. The public release builder deliberately
does not call this tool. Originals are read-only inputs; an existing output is never
replaced. Requires macOS/Xcode command line tools and FFmpeg for --music=ogg.
"""
import argparse
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile

from build_release import ROOT, VERSION, copy_licenses
from compact_assets import asset_files, digest, ogg_track


def run(*args, **kwargs):
    return subprocess.run([str(x) for x in args], check=True, **kwargs)


def install_icon(source, resources):
    """Encode supplied square artwork as the standard macOS icon family.

    This is format conversion/resampling only: do not redraw, crop, remove the
    background or reinterpret supplied artwork. sips/iconutil ship with macOS.
    """
    dimensions = subprocess.check_output(
        ['sips', '-g', 'pixelWidth', '-g', 'pixelHeight', str(source)], text=True)
    width = re.search(r'^\s+pixelWidth: (\d+)$', dimensions, re.M)
    height = re.search(r'^\s+pixelHeight: (\d+)$', dimensions, re.M)
    if not width or not height or int(width[1]) != int(height[1]) or int(width[1]) < 512:
        raise ValueError('App icon must be a square image at least 512 pixels wide')
    destination = resources / 'REminiscence2.icns'
    with tempfile.TemporaryDirectory(prefix='reminiscence2-icon-') as temp:
        iconset = Path(temp) / 'AppIcon.iconset'
        iconset.mkdir()
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                name = f'icon_{size}x{size}' + ('@2x' if scale == 2 else '') + '.png'
                run('sips', '-s', 'format', 'png', '-z', size * scale, size * scale,
                    source, '--out', iconset / name, stdout=subprocess.DEVNULL)
        run('iconutil', '-c', 'icns', iconset, '-o', destination)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True, help='Release arm64 engine')
    parser.add_argument('--amiga', type=Path, required=True, help='Unpacked WHDLoad root, with data/cine/music')
    parser.add_argument('--dos', type=Path, required=True, help='GOG StreamingAssets/data, not the installer root')
    parser.add_argument('--segacd', type=Path, required=True, help='Extracted US CD root, with DATA/ and music/')
    parser.add_argument('--music', choices=('ogg', 'lossless'), default='lossless',
                        help='ogg converts CD WAV music to high-quality Vorbis; other assets remain exact')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--icon', type=Path, help='Optional square PNG/JPEG artwork for Finder and Dock')
    parser.add_argument('--output', type=Path, default=ROOT / '.local/packages' /
                        f'REminiscence2-{VERSION}-macos-arm64-private')
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('This builder requires macOS')
    output = args.output.resolve()
    if output.exists():
        parser.error('Output already exists; choose a new private output directory')
    binary = args.binary.resolve()
    arch = subprocess.check_output(['lipo', '-archs', str(binary)], text=True).strip()
    if arch != 'arm64':
        parser.error(f'Expected a thin Apple Silicon binary, got: {arch}')
    dependencies = subprocess.check_output(['otool', '-L', str(binary)], text=True)
    for line in dependencies.splitlines()[1:]:
        dependency = line.strip().split(' (', 1)[0]
        if not dependency.startswith(('/usr/lib/', '/System/Library/')):
            parser.error(f'Non-system runtime dependency: {dependency}')
    version = subprocess.check_output([str(binary), '--version'], text=True)
    if f'REminiscence2 {VERSION}\n' not in version:
        parser.error('Rebuild the binary for the current source version first')
    sources = {name: asset_files(path.resolve(), name) for name, path in
               [('amiga', args.amiga), ('pc-gog', args.dos), ('sega-cd', args.segacd)]}
    ffmpeg = shutil.which(args.ffmpeg)
    if args.music == 'ogg' and not ffmpeg:
        parser.error('--music=ogg requires FFmpeg with libvorbis')
    output.parent.mkdir(parents=True, exist_ok=True)
    # Stage beside the final directory so promotion is atomic on this filesystem.
    with tempfile.TemporaryDirectory(prefix='.private-mac-', dir=output.parent) as temp:
        work = Path(temp)
        product = work / 'result'
        product.mkdir()
        app = work / 'uncompressed/REminiscence2.app'
        macos = app / 'Contents/MacOS'
        resources = app / 'Contents/Resources'
        macos.mkdir(parents=True)
        resources.mkdir()
        shutil.copy2(binary, macos / 'reminiscence2')
        run('strip', '-x', macos / 'reminiscence2')
        run('clang', '-arch', 'arm64', '-mmacosx-version-min=11.0',
            ROOT / 'packaging/desktop/mac_launcher.m', '-framework', 'Cocoa', '-o', macos / 'Play')
        numeric = VERSION.split('.')[0] + '.0.' + str(int(VERSION.split('.')[1]))
        info = {'CFBundleIdentifier': 'org.b0realis.reminiscence2', 'CFBundleName': 'REminiscence2',
                'CFBundleExecutable': 'Play', 'CFBundlePackageType': 'APPL',
                'CFBundleShortVersionString': numeric, 'CFBundleVersion': numeric,
                'LSMinimumSystemVersion': '11.0', 'NSHighResolutionCapable': True}
        icon = install_icon(args.icon.resolve(), resources) if args.icon else None
        if icon:
            info['CFBundleIconFile'] = icon.name
        (app / 'Contents/Info.plist').write_bytes(plistlib.dumps(info))
        for name in ('manual.md', 'CREDITS.md', 'fb.cfg'):
            shutil.copy2(ROOT / name, resources / name)
        copy_licenses(resources / 'licenses')
        (resources / 'licenses/README.txt').write_text(
            'Private personal-use bundle. Engine: GPLv3; new contributions: GPL-3.0-or-later.\n'
            'See the individual dependency notices. Apple system frameworks are supplied by macOS.\n'
            'Original game assets remain under their original owners\' terms, separate from the engine.\n'
            'Do not upload this asset-containing bundle as a public engine release.\n')
        report = {'version': VERSION, 'architecture': 'arm64', 'music': args.music, 'files': {}}
        if icon:
            report['icon'] = {'file': icon.name, 'sha256': digest(icon),
                              'source_sha256': digest(args.icon.resolve())}
        for edition, files in sources.items():
            print(f'Preparing {edition}: {len(files)} files', flush=True)
            for source, relative in files:
                destination = resources / 'assets' / edition / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                conversion = {}
                if edition == 'sega-cd' and source.suffix.lower() == '.wav' and args.music == 'ogg':
                    destination = destination.with_suffix('.ogg')
                    conversion = ogg_track(source, destination, ffmpeg)
                else:
                    shutil.copyfile(source, destination)
                    if digest(destination) != digest(source):
                        raise RuntimeError(f'Copied resource differs from original: {relative}')
                key = destination.relative_to(resources).as_posix()
                report['files'][key] = {'bytes': destination.stat().st_size,
                                        'sha256': digest(destination), **conversion}
        (resources / 'private-package.json').write_text(json.dumps(report, indent=2) + '\n')
        readme = (
            f'REminiscence2 {VERSION} — private Apple Silicon package\n\n'
            'Drag REminiscence2.app to Applications, then double-click it.\n'
            'Choose Amiga, DOS/GOG or Sega CD on the first launch.\n'
            'Esc opens QoL; Session switches editions, Audio switches music sources.\n'
            'Assets live inside the app: Contents/Resources/assets/{amiga,pc-gog,sega-cd}.\n'
            'Progress/settings live in ~/Library/Application Support/REminiscence2/SAVE.\n'
            'The app contains no installers, disc images, SDKs, tests or duplicate engine.\n'
            + ('CD music is Vorbis quality 6 (lossy), with original rate, channels and sample count.\n'
               if args.music == 'ogg' else 'CD music retains the original PCM WAV data.\n')
            + 'All movies, voices and retained game resources remain byte-identical.\n'
            'The app uses transparent lossless filesystem compression where macOS permits it.\n'
            'It is locally/ad-hoc signed, not Apple notarized. If macOS blocks it, use\n'
            'System Settings > Privacy & Security > Open Anyway after attempting launch.\n'
            'This bundle contains your private game assets; do not publish it.\n')
        (product / 'READ-ME.txt').write_text(readme)
        for target in (macos / 'reminiscence2', macos / 'Play', app):
            run('codesign', '--force', '--sign', '-', target)
        final_app = product / 'REminiscence2.app'
        run('ditto', '--hfsCompression', app, final_app)
        run('codesign', '--verify', '--deep', '--strict', final_app)
        listing = subprocess.check_output([str(final_app / 'Contents/MacOS/Play'),
                                          '--no-preferences', '--list-assets'], cwd='/', text=True)
        entries = [line.strip().split('\t', 1) for line in listing.splitlines() if '\t' in line]
        for edition, label in [('amiga:amiga', 'Amiga / amiga'), ('dos:pc-gog', 'DOS / GOG / pc-gog'),
                               ('sega:sega-cd', 'Sega CD (US) / sega-cd')]:
            if [edition, label] not in entries:
                raise RuntimeError(f'Bundled launcher cannot detect {edition}')
        (product / 'asset-discovery.txt').write_text(listing)
        image_root = work / 'disk'
        image_root.mkdir()
        run('ditto', '--preserveHFSCompression', final_app, image_root / final_app.name)
        shutil.copy2(product / 'READ-ME.txt', image_root / 'READ-ME.txt')
        (image_root / 'Applications').symlink_to('/Applications')
        dmg = product / f'REminiscence2-{VERSION}-AppleSilicon-private.dmg'
        run('hdiutil', 'create', '-quiet', '-srcfolder', image_root, '-fs', 'HFS+', '-volname',
            f'REminiscence2 {VERSION}', '-format', 'ULMO', dmg)
        run('hdiutil', 'verify', '-quiet', dmg)
        (product / (dmg.name + '.sha256')).write_text(f'{digest(dmg)}  {dmg.name}\n')
        product.rename(output)
    print(output / dmg.name)


if __name__ == '__main__':
    main()
