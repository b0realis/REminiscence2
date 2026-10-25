<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Compact personal Mac package

`package_private_mac.py` builds a self-contained Apple Silicon `.app` and a
compressed `.dmg` from an existing Release binary and your extracted game data.
This is a private transfer package: it must not be uploaded to a GitHub release.
Public engine packages contain no game assets; see [asset-free DMGs and compaction](MAC_PACKAGES.md).

Build the current Release engine first, then run on macOS with Python 3.9+ and
Xcode Command Line Tools:

```sh
python3 tools/package_private_mac.py \
  --binary build/package-arm64/reminiscence2 \
  --amiga .local/assets/amiga-unpacked \
  --dos .local/assets/gog-extracted/Flashback_Data/StreamingAssets/data \
  --segacd .local/assets/segacd \
  --music ogg --ffmpeg /path/to/ffmpeg
```

Add `--icon packaging/icons/flashback-app-icon.png` to use the supplied artwork
in Finder and the Dock. The builder converts a square PNG/JPEG (at least 512px)
into the complete 16–1024px `.icns` family using macOS tools, retaining the image
composition and transparency. The icon is installed before bundle signing. See
[the artwork provenance note](../packaging/icons/README.md) for its separate status.

The default output is a new versioned directory under ignored `.local/packages`.
An existing output is rejected; select a new `--output` path to build again.
`--music lossless` (the default) keeps PCM WAVs and does not need FFmpeg.
`--music ogg` converts only CD background music to Vorbis quality 6, a **lossy**
space-saving choice. Sample rate, channels and sample count are verified after a
complete decode. Movies, embedded movie audio, dialogue, effects and all other
retained resources remain byte-identical. Original inputs are never edited.

The builder excludes installers, Unity runtimes, disc images, the unused Amiga
executable and known Sega CD preservation-only files. It retains all languages
in the supplied GOG data, all CD movies and all eight CD music tracks. The app
contains one stripped arm64 engine, a tiny native launcher, settings, the manual
and dependency notices. It links only macOS libraries. A per-file SHA-256 manifest
records the actual bundle contents, including audio conversion details.

Assets live at:

```text
REminiscence2.app/Contents/Resources/assets/
  amiga/
  pc-gog/
  sega-cd/
```

The builder applies transparent lossless HFS+/APFS compression to the app files
where supported, then creates an LZMA-compressed disk image with an Applications
shortcut. Double-click the DMG and drag the app into Applications. First launch
offers the three editions. **Esc → Session** changes editions; **Audio** selects
a different soundtrack. Saves/settings live outside the app at
`~/Library/Application Support/REminiscence2/SAVE`, so replacing the app preserves
progress. To inspect the asset folders in Finder, choose **Show Package Contents**.
Some copy/archive tools expand filesystem compression; logical file sizes are
unchanged either way. Finder's reported logical size can exceed allocated space.

The bundle is ad-hoc signed and verified, but not Apple notarized. If Gatekeeper
blocks launch, use **System Settings → Privacy & Security → Open Anyway** after
the first attempt. AppImage is for Linux; the native `.app` is the Mac equivalent
for moving an application into Applications and opening it through Finder.

For a transfer, run `scp user@build-mac:/absolute/path/to/package.dmg ~/Downloads/`
on the receiving Mac. Use the generated `.sha256` file if you want to verify the
transfer with `shasum -a 256`.
