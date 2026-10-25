<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Mac app packages and compact assets

macOS uses `.app` bundles and `.dmg` disk images; AppImage is a Linux format.
The Mac release recipe builds an asset-free ZIP **and DMG** for Apple Silicon
and Intel. The DMG contains the app, an Applications shortcut, this guide,
`compact_assets.py` and an empty `assets` folder with instructions.

Drag `REminiscence2.app` into Applications. On first launch, **Open assets folder**
opens `~/Library/Application Support/REminiscence2/assets`. Put each prepared
edition in its own subfolder there. Original game files are supplied by you.
Keep them outside the public app bundle so adding assets does not invalidate its
signature, and replacing the app leaves data and saves intact. An existing
adjacent `assets` folder also works when running the app from an extracted ZIP.
Private self-contained bundles continue to use their internal assets.

## Prepare compact assets (Python 3.9+)

Extract your WHDLoad, GOG and Sega CD files using the existing extraction tools
first (see `extract_amiga.py`, `extract_segacd.py` and the repository asset guide).
Then, from the repository:

```sh
python3 tools/compact_assets.py \
  --amiga /path/to/amiga-unpacked \
  --dos /path/to/gog-extracted \
  --segacd /path/to/segacd-extracted \
  --output ~/Downloads/REminiscence2-assets \
  --music ogg --ffmpeg /path/to/ffmpeg
```

Use any subset of the three edition arguments. `--dos` also accepts the inner
`Flashback_Data/StreamingAssets/data` directory. Output must be a **new** folder;
existing data is never replaced. Move the resulting `amiga`, `pc-gog` and
`sega-cd` folders into the app's assets folder. The same output works beside
Windows/Linux binaries or with the Quest transfer tool. For another compaction,
choose a new output directory; merge editions deliberately in Finder.

The default `--music lossless` copies music without conversion and needs no
FFmpeg. Optional `--music ogg` converts only Sega CD background WAV tracks to
Vorbis quality 6 (lossy), verifies a complete decode and the original sample
count/rate/channels, and retains all movies, voices and other runtime data
byte-for-byte. There is no AI upscaling or movie re-encoding here. Only known
non-runtime executables/disc extras are removed. Hidden file hashes in
`.reminiscence2-assets.json` document every retained output. Originals remain
untouched. A failed operation does not publish a partial assets directory.

## Build an asset-free disk image

Run on macOS with Xcode Command Line Tools and a current Release binary:

```sh
python3 tools/package_macos.py \
  --binary build/package-arm64/reminiscence2 --arch arm64 \
  --output build/mac-engine-only \
  --icon packaging/icons/flashback-app-icon.png
```

For Intel, use `--arch x86_64` and a corresponding binary. Omit `--icon` to omit
the separately supplied artwork; automated public packages do so by default.
The PNG has transparent corners; its original JPEG is retained for provenance.
The generated app is ad-hoc signed and verified, not Apple notarized. If macOS
blocks launch, use System Settings → Privacy & Security → Open Anyway after
attempting to open it. `package_macos.py` refuses bundles containing unrecognized
files or private game assets. It verifies the actual DMG-mounted signature and
records an inventory plus checksum for release validation.

The full recipe is `python3 tools/build_release.py --target macos-arm64`
(or `macos-x86_64`); it also builds/tests the engine and makes the CLI ZIP.
For a **private** app with all owned assets inside, use
[`package_private_mac.py`](PRIVATE_MAC_PACKAGE.md). Never publish that bundle.
