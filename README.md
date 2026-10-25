# REminiscence2

REminiscence2 is a preservation and modernization project for the Flashback
engine created by Gregory Montoir and extended by Alex Chermenin and other
contributors. It celebrates Delphine Software's 1992 game and the community
that has kept it playable.

Our aim is faithful gameplay with easier setup, dependable saves, accessible
controls, and support for current hardware and operating systems. Enhancements
that change the experience should be optional.

This is an early development tree. Its current version is defined in
[version.h](version.h) and printed by `reminiscence2 --version`. Patch versions
advance as `1.01`, `1.02`, and so on. It builds on
[chermenin/REminiscence](https://github.com/chermenin/REminiscence) at
`2200e18c6e5eb710b05c5095a751882591c21cf8`. REminiscence2 starts with one
fresh root commit; the upstream repository retains the earlier history.
Source notices, credits and pinned provenance are preserved here.
The original [README](docs/upstream/README.md) and [changelog](CHANGES.md)
remain available.

## Current status

The 0.5.6 enhancement pass keeps the selected fork as the base. It adds modular
MIDI/PRF playback, bounded Macintosh graphics and resource handling, common
Macintosh menus, adjacent-room rendering for more editions, configurable audio,
and format regression tests. It also repairs confirmed corruption in embedded
tables and fixes resource caching, effect replay and screenshot output.

Amiga, classic GOG DOS/Windows data, and Macintosh CD are explicit targets.
Macintosh rendering retains its native 512×448 resolution. The supplied Amiga
files pass a seven-level headless regression. The supplied GOG Windows installer's
DOS-format data passes startup, seven-level room rendering and pistol-effect
regressions. Synthetic Mac resources exercise menus and decoding; real Macintosh
CD validation and full playthroughs remain outstanding. The supplied Sega CD US
disc now supports independent gameplay assets, MCD movies with audio, spoken
dialogue and extracted CD music. Its tests cover all indexed movie frames and
seven level starts, not a full playthrough. Cartridge Mega Drive support remains
experimental; raw ROM/VGM playback is not implemented.

See [supported assets and required files](docs/ASSETS.md), the
[developer format reference](docs/formats/README.md), and
[Sega CD extraction instructions](docs/SEGA_CD.md).

The optional [offline movie upscaling tools](tools/MOVIE_UPSCALING.md)
produce local Real-CUGAN Pro MP4s with original timing and audio, plus tracked
repairs where needed. Results live in the ignored `upscaled` folder; the game
continues to use original MCD playback.

See the [0.5.6 integration ledger](docs/UPSTREAM_056.md),
[validation record](docs/VALIDATION.md), and [roadmap](docs/ROADMAP.md).
Linux, macOS and Windows CI runs native builds and tests. See the
[release recipes](docs/RELEASES.md) for desktop packages and the Quest flat-panel
APK. A native SDL3 backend remains future work.

The [1.09 campaign report](docs/CAMPAIGN_1.09.md) records the source review,
confirmed fixes, timing/menu changes and robustness tests. Version **1.10** adds
[unified recovery, named/protected saves, controller profiles, accessibility
assists, an objectives notebook and audio-loss recovery](docs/QOL_1.10.md).
Version **1.11** combines these improvements with fixes for save feedback,
controller defaults, disconnect pause frames and shortcut rewind. Download
packages from [GitHub Releases](https://github.com/b0realis/REminiscence2/releases)
and consult the validation record for testing limits.

The [release-readiness audit](docs/RELEASE_READINESS.md) ranks the next fixes
and covers Win64, Linux x86-64, Steam Deck, macOS Apple Silicon/Intel, and
a sideloaded Quest flat-panel Android app.

The [reference index](docs/reference/README.md) contains project-written format
research notes. Supplied third-party images and PDFs are kept in ignored local
storage and are excluded from source archives.

Read [manual.md](manual.md) for the original REminiscence2 player field guide:
setup, controls, movement practice, saves, display, music and command options.
It contains no reproduced publisher-manual text, comic or game artwork.

Joypad settings include persistent button remapping, a configurable dead zone,
optional save/load/rewind shortcuts, and a raw-joystick profile. Start always
opens settings. The optional Journal page adds short original passages for
reached story chapters, with save/rewind-aware reading progress.

By default, launches play the opening sequences and end at the title menu.
Sega CD combines its logos and introduction in one movie. Space or
A–Z skips the current scene by default; Backspace is always available. Esc, F10
or controller Start opens QoL settings immediately, including Levels and Save /
Load. Closing settings returns to the title; `--continue` explicitly resumes an
autosave without the openings. Enable **Session → Quicksave skips title sequence**
and save settings to resume the latest compatible quick/manual/checkpoint save
or autosave automatically. With no compatible save, the opening plays normally.

Keep editions in separate subfolders of `assets`. A picker remembers which to
play; **Session → Game edition** changes it later. **Audio → Soundtrack** selects
Amiga MOD, DOS PRF, extracted Sega CD music or a separate Ogg/WAV soundtrack
independently. For other supplied recordings, use `assets/music/<pack>/`; see
[the music guide](manual.md#music-sources). `--list-assets` prints source IDs.

## Screenshots

The in-game QoL menu at **1920×1080**, rendered over paused Amiga gameplay.
Press **Esc**, **F10**, or controller **Start** to open it. Its thirteen pages include
save/load, graphics, audio, controls, cheats and level selection.

![High-resolution QoL menu with Conrad's T-shirt choices and the turquoise EXIT GAME button](docs/screenshots/qol-highres.png)

| Amiga gameplay: level 1 | Amiga gameplay: level 6 |
| --- | --- |
| ![Conrad in the jungle](docs/screenshots/amiga-level1.png) | ![Conrad exploring the alien complex](docs/screenshots/amiga-level6.png) |

The [screenshot gallery](docs/screenshots/README.md) includes all seven levels,
the save browser, graphics settings and the compact **256×224** menu.
Controls includes an [interactive keyboard diagram](docs/screenshots/README.md#keyboard--action-guide):
colored action keys and arrow callouts follow your bindings as you change them.
Captures show the development build using player-supplied Amiga data.
Game artwork remains the property of its respective rights holders; the
engine's GPL does not relicense it. See the [image credits](docs/screenshots/README.md#image-credits).

## Build

Requirements: C and C++17 compilers, CMake 3.20 or later, SDL2 2.0.18+ development files,
zlib, and libmodplug. SDL's CMake package is preferred; pkg-config is the
fallback for finding SDL2.

On macOS with Homebrew:

```sh
brew install cmake pkgconf sdl2 libmodplug
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

If Homebrew is not on your PATH, use `/opt/homebrew/bin/cmake` on Apple Silicon
and add `-DCMAKE_PREFIX_PATH=/opt/homebrew` to the configure command. Current
Homebrew may provide the SDL2 API through `sdl2-compat` backed by SDL3; this
is distinct from a native SDL3 port of the engine.

On Debian or Ubuntu:

```sh
sudo apt-get install build-essential cmake pkg-config libsdl2-dev zlib1g-dev libmodplug-dev
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

To use the inherited internal MOD player, configure with
`-DREMINISCENCE2_USE_MODPLUG=OFF`; libmodplug is then unnecessary. The external
xBRZ scaler is retained in `scalers/xbr` but is not built by CMake in this pass.
The inherited Makefile remains available for existing development setups.

AdLib PRF music uses the bundled, pinned Nuked OPL3 dependency. Optional
FluidSynth and MT-32 backends can be enabled with
`-DREMINISCENCE2_FLUIDSYNTH=ON -DREMINISCENCE2_MT32EMU=ON`; install their development
libraries first (`brew install fluid-synth mt32emu` on macOS). On Linux,
install FluidSynth's development package, usually `libfluidsynth-dev`, and
[build Munt's libmt32emu](https://github.com/munt/munt/tree/master/mt32emu#building)
if your distribution does not provide it. For a custom installation prefix,
pass `-DCMAKE_PREFIX_PATH=/path/to/prefix` to the engine's CMake configure command.
The Linux CI workflow demonstrates this with a pinned Munt release.
These optional backends are off by default.
Their soundfonts and ROMs are supplied separately by the player.

## Run

For the local Apple Silicon build and prepared assets, run
`./play-local.sh` from the repository root. Use `--segacd`, `--gog` or `--amiga`
to select the corresponding prepared private data, or omit the selector to use
the edition picker when needed. It accepts extra engine options, for example `./play-local.sh --help`. The launcher resolves
its build, configuration and asset paths relative to itself, so it also works
when invoked by absolute path from another directory.

Supply your own original DOS, Amiga, Macintosh, Sega CD, or PC-98 game data. Game data
is separate from the engine and is not included in this repository.

From the repository root:

```sh
./build/release/reminiscence2 --help
./build/release/reminiscence2 -h
./build/release/reminiscence2 --version
./build/release/reminiscence2 --datapath="/path/to/Flashback/DATA" --window
```

Configuration defaults to `fb.cfg` in the current working directory. To launch
from anywhere, specify it explicitly:

```sh
/path/to/reminiscence2 --config="/path/to/fb.cfg" --datapath="/path/to/DATA" --window
```

Configuration keys must match exactly. Values accept `true`, `false`, `1`, and
`0`, ignoring case and surrounding whitespace. Unknown keys and invalid values
produce warnings; invalid values leave the current setting unchanged. Missing
explicit configuration files fail with an error; missing default `fb.cfg` uses
the built-in defaults.

Release packages include an empty `assets` folder beside the executable and a
one-click launcher. Copy prepared game files there, keeping each edition in its own subfolder. Default saves
use the platform's writable user-data folder; `--savepath` overrides it and
`--portable` keeps saves beside the executable. A sole existing folder argument
selects game data and tunes while retaining the normal save location.

Arrow keys move Conrad, Shift performs actions, Enter uses the selected item,
Backspace opens inventory, and Escape or F10 opens the new paused settings
overlay. Its Session, Graphics, Audio, Enhancements and Controls tabs provide
live display changes, separate volume levels, rewind/cutscene options and
keyboard rebinding. A clickable **Menu [F10]** shortcut is also shown during play.
The overlay now includes a save browser, all existing cheats, level/difficulty
selection, restart, clean screenshots and a turquoise **EXIT GAME** button.
A compact single-column layout fits the original 256×224 resolution without
enlarging the window. Ctrl+S and Ctrl+L save and load a manual slot.

Choose **Enhancements → Conrad’s T-shirt** for Original, Red/Pink or White fabric.
Only the T-shirt inside the brown jacket changes. It updates while paused and
can be saved with your preferences; the jacket, shoes and other artwork retain
their colours.

Choose **Save settings** to keep preferences in `preferences.cfg` under
`--savepath`. Explicit display/rewind command-line options override saved
preferences; `--no-preferences` ignores them for one launch. F10 stays available
as a recovery shortcut. See the [settings guide](docs/SETTINGS.md).

[Protection settings](docs/reference/copy-protection/README.md) explain how to
enable the original prompts using your own edition's documentation. Challenge
sheets and answers are not distributed with this project.

## Cutscene browser

Use the Python menu to list and play polygon cutscenes from your game files:

```sh
python3 tools/play_cutscenes.py /path/to/assets
python3 tools/play_cutscenes.py /path/to/assets --list
python3 tools/play_cutscenes.py /path/to/assets --play intro
```

Python 3.9+ needs no extra packages; the compiled engine provides rendering,
music and subtitles. Choose a menu number or name, then a part if prompted.
Backspace, Tab or Escape returns to the menu. The supplied Amiga package
contains 27 cutscene files and 68 selectable parts. See the
[cutscene player guide](docs/CUTSCENE_PLAYER.md) for preparation, controls,
edition status and all options.

Use `--outputrate=48000` to request a device rate, `--maximized` to start in a
maximized window, and `--debug=0x40` to enable a diagnostic category in either
Debug or Release builds. Audio settings do not change simulation timing.

PRF music is a fallback after existing MOD/OGG/CPC choices. It looks for PRF,
MIDI and INS files in the game folder, then the tune folder. Choose
`--mididriver=adlib`, `fluidsynth`, `mt32`, or `none`. FluidSynth requires
`--soundfont=PATH` and maps MT-32 programs to General MIDI; MT-32 uses
`--rompath=PATH` containing an MT32 or CM32L control/PCM ROM pair. The optional
`fix_fmopl_e0_reg` setting controls the original AdLib waveform-register quirk.
`use_prf_music=false` disables the PRF fallback.

The WHDLoad preparation tool, `tools/extract_amiga.py`, reads the LHA package
and decompresses the inner RNC files into an engine-ready directory. It uses
Python 3.9 or newer, [lhafile](https://github.com/FrodeSolheim/python-lhafile)
and the [Ancient](https://github.com/temisu/ancient) executable. On macOS:

```sh
python3 -m venv .local/extract-venv
.local/extract-venv/bin/python -m pip install -r tools/requirements-amiga.txt
brew install ancient
.local/extract-venv/bin/python tools/extract_amiga.py \
  /path/to/Flashback_v3.4_1163.lha .local/assets/amiga-ready
./build/release/reminiscence2 --datapath=.local/assets/amiga-ready --window
```

The output folder must be new. The tool preserves the game file hierarchy,
checks LHA and RNC checksums and sizes, and writes a JSON manifest with SHA-256
hashes. It skips the Amiga executable, WHDLoad slave, icons and package notes.
It rejects ambiguous resource names, unsafe paths and oversized inputs. A failed
conversion cleans up its temporary files. The source archive is read-only.
Use `--ancient=/path/to/ancient` for a custom executable or `--verbose` for
per-file progress; `--help` includes setup and usage instructions. No separate
`lha` command is needed.

The supplied archive prepares 128 game files (103 RNC-decompressed, 25 unchanged).
All match the existing reference assets byte for byte, and the prepared directory
passes the seven-level engine integration test. Private references and extracted
assets belong under the ignored `.local/` directory. The engine consumes these
prepared files and does not execute a WHDLoad slave or load LHA archives directly.

Persistent autosaves keep three generations per edition under `--savepath`,
separate from manual slots. They run every 60 active seconds by default, at
checkpoints/level starts and on clean exit when possible. Use
`--disk-autosave=15` through `600` to set the interval, or `--no-disk-autosave`
to disable writes. A living frame can still be hazardous; retain manual saves.

On the next launch, choose **Continue latest autosave**, or pass `--continue` to
restore directly. The loader checks assets before changing levels and tries an
older generation when a candidate is damaged or incompatible. **Save / Load**
previews and restores the retained generations.

`--autosave` enables separate, temporary rewind history. Open **Rewind** to
preview a moment by age, room and health before restoring it; Ctrl+R takes the
latest snapshot directly. Newer history is discarded after restoring an older
point. History is lost on exit or level changes.

**Session → Contextual control hints** enables short, optional reminders that
follow current keyboard/pad bindings. They contain no puzzle solutions and are
off by default. Each appears once per new game and pauses with gameplay.

## Development

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DREMINISCENCE2_SANITIZERS=ON
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
```

This requests AddressSanitizer and UndefinedBehaviorSanitizer. If the local
Apple sanitizer runtime hangs before `main()` on macOS, configure with
`-DREMINISCENCE2_SANITIZER_SET=undefined` to run UBSan separately; see the
validation record. Keep ASan checks enabled on a working toolchain.

Asset-free tests cover configuration and CLI validation, archive lookup,
static-table integrity, rewind streams, audio devices, MIDI/PRF timing and
synthesis, malformed graphics/resources, and Macintosh menu navigation using
synthetic data. Enable the private Amiga integration test with
`-DREMINISCENCE2_AMIGA_DATA=/absolute/path/to/unpacked/files`. An optional
`REMINISCENCE2_TEST_SOUNDFONT` path exercises successful FluidSynth playback.
These checks do not certify a full playthrough or all save compatibility.

The preparation tool has an additional asset-free test suite, using the Python
environment installed above:

```sh
.local/extract-venv/bin/python -m unittest discover -s tests -p test_extract_amiga.py -v
```

Check the public documentation boundary before preparing source archives:

```sh
python3 tools/check_release_docs.py
```

CI runs the same check. Private references stay in `.local/`; source archives
must be generated from the reviewed Git tree so the `export-ignore` rules in
`.gitattributes` apply.

## Credits and licensing

Thank you to Delphine Software, Gregory Montoir, Alex Chermenin, and everyone
who documented, ported, and maintained Flashback. See [credits](CREDITS.md).

The inherited [GPLv3 license](LICENSE) and third-party notices are preserved.
New REminiscence2 contributions are offered under `GPL-3.0-or-later`.
The later-version permission for the entire inherited tree needs verification;
see [licensing and provenance](docs/LICENSING.md) before preparing a release.
