#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copy prepared Amiga, DOS/GOG and Sega CD resources into compact edition folders.

Original inputs are read-only. No game data is downloaded. Python 3.9+; FFmpeg
with libvorbis is needed only for optional lossy CD music conversion.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import wave


def run(*args, **kwargs):
    return subprocess.run([str(x) for x in args], check=True, **kwargs)


def asset_files(root, edition):
    """Retain complete resource directories, excluding known non-runtime extras."""
    root = Path(root)
    if root.is_symlink():
        raise ValueError(f'{edition}: symbolic input roots are not accepted')
    markers = {'amiga': 'data/level1.lev', 'pc-gog': 'level1.map',
               'sega-cd': 'DATA/PRESENT.LEV'}
    paths = sorted(root.rglob('*'))
    if any(p.is_symlink() for p in paths):
        raise ValueError(f'{edition}: symbolic links are not accepted')
    names = {p.relative_to(root).as_posix().lower() for p in paths if p.is_file()}
    if markers[edition].lower() not in names:
        raise ValueError(f'{edition}: expected a prepared directory containing {markers[edition]}')
    result = []
    for source in paths:
        rel = source.relative_to(root)
        if not source.is_file() or any(part.startswith('.') for part in rel.parts):
            continue
        name = rel.as_posix().lower()
        if edition == 'amiga' and name == 'flashback':
            continue  # Original Amiga executable; the engine does not execute/read it.
        if edition == 'sega-cd':
            if name in {'abs.txt', 'bib.txt', 'cpy.txt', 'main.bin', 'data/test.tst',
                        'reminiscence2-segacd.json'}:
                continue
            if not (name.startswith(('data/', 'music/')) or
                    rel.suffix.lower() == '.mcd' or name == 'sub.bin'):
                continue
        if edition == 'amiga':
            with source.open('rb') as stream:
                if stream.read(3) == b'RNC':
                    raise ValueError(f'Unpack WHDLoad resources first: {rel}')
        result.append((source, rel))
    return result


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def ogg_track(source, destination, ffmpeg):
    # Keep the source rate/channels and all samples, including gaps already
    # retained by the extractor. Only the CD background soundtrack is lossy.
    with wave.open(str(source), 'rb') as wav:
        frames, rate, channels = wav.getnframes(), wav.getframerate(), wav.getnchannels()
        if wav.getsampwidth() != 2 or channels not in (1, 2):
            raise ValueError(f'Expected PCM16 mono/stereo CD audio: {source}')
    run(ffmpeg, '-nostdin', '-v', 'error', '-n', '-i', source, '-map_metadata', '-1',
        '-c:a', 'libvorbis', '-q:a', '6', destination)
    # Decode the complete result: reject truncation or changed sample count.
    with tempfile.TemporaryFile() as pcm:
        run(ffmpeg, '-nostdin', '-v', 'error', '-i', destination, '-f', 's16le',
            '-acodec', 'pcm_s16le', '-ar', rate, '-ac', channels, '-', stdout=pcm)
        if pcm.tell() != frames * channels * 2:
            raise RuntimeError(f'Converted track has the wrong sample count: {destination.name}')
    return {'encoding': 'Vorbis quality 6 (lossy)', 'frames': frames,
            'sample_rate': rate, 'channels': channels, 'original_sha256': digest(source)}


def compact(sources, output, music='lossless', ffmpeg='ffmpeg'):
    output = Path(output).absolute()
    if output.is_symlink() or output.exists():
        raise ValueError('Output already exists; choose a fresh directory')
    if not sources:
        raise ValueError('Provide at least one prepared edition')
    plans = {}
    for edition, source in sources.items():
        source = Path(source)
        if source.is_symlink():
            raise ValueError('Input root must not be a symbolic link')
        source = source.resolve()
        if output.resolve().is_relative_to(source):
            raise ValueError('Output must not be inside an input directory')
        if edition == 'pc-gog' and (source / 'Flashback_Data/StreamingAssets/data').is_dir():
            source /= 'Flashback_Data/StreamingAssets/data'
        plans[edition] = asset_files(source, edition)
    if music not in ('lossless', 'ogg'):
        raise ValueError('Unsupported music encoding')
    if music == 'ogg' and not shutil.which(ffmpeg):
        raise ValueError('Ogg conversion requires FFmpeg with libvorbis')
    output.parent.mkdir(parents=True, exist_ok=True)
    report = {'schema': 1, 'music': music, 'files': {}}
    with tempfile.TemporaryDirectory(prefix='.compact-assets-', dir=output.parent) as temp:
        stage = Path(temp) / 'assets'
        stage.mkdir()
        for edition, files in plans.items():
            for source, relative in files:
                destination = stage / edition / relative
                convert = edition == 'sega-cd' and relative.parts[0].lower() == 'music' and source.suffix.lower() == '.wav' and music == 'ogg'
                if convert:
                    destination = destination.with_suffix('.ogg')
                if destination.exists():
                    raise ValueError(f'Duplicate output resource: {destination.relative_to(stage)}')
                destination.parent.mkdir(parents=True, exist_ok=True)
                conversion = ogg_track(source, destination, ffmpeg) if convert else {}
                if not convert:
                    shutil.copyfile(source, destination)
                    if digest(source) != digest(destination):
                        raise RuntimeError(f'Copy verification failed: {relative}')
                report['files'][destination.relative_to(stage).as_posix()] = {
                    'bytes': destination.stat().st_size, 'sha256': digest(destination), **conversion}
        # Hidden manifest is ignored by asset discovery; it accompanies personal data.
        (stage / '.reminiscence2-assets.json').write_text(json.dumps(report, indent=2) + '\n')
        if output.exists():
            raise ValueError('Output was created while preparing assets')
        stage.rename(output)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--amiga', type=Path, help='Unpacked WHDLoad root containing data/')
    parser.add_argument('--dos', type=Path, help='GOG extracted root or StreamingAssets/data')
    parser.add_argument('--segacd', type=Path, help='Extracted CD root containing DATA/, movies and music/')
    parser.add_argument('--output', type=Path, required=True, help='New assets directory; never overwrites existing files')
    parser.add_argument('--music', choices=('lossless', 'ogg'), default='lossless', help='ogg converts only CD music to Vorbis quality 6; movies and voices stay exact')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args()
    sources = {name: path for name, path in [('amiga', args.amiga), ('pc-gog', args.dos), ('sega-cd', args.segacd)] if path}
    try:
        report = compact(sources, args.output, args.music, args.ffmpeg)
    except (ValueError, RuntimeError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
    size = sum(item['bytes'] for item in report['files'].values())
    print(f"Prepared {len(report['files'])} files ({size / 1048576:.1f} MiB) in {args.output}")


if __name__ == '__main__':
    main()
