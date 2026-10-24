<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Release packages and reproducible recipes

Every desktop download includes a precreated **assets** folder. Put one edition's
prepared original game files there; nested directories are indexed. Music
falls back to those same assets unless a separate tune path is selected. No original
game data, reference scans, ROMs or soundfonts are shipped. Manual and tool
instructions are included. Default saves/preferences live in writable user data;
`--portable` and explicit path switches remain available.

| Package | One-click play | Build recipe |
| --- | --- | --- |
| Windows x86-64 ZIP | `reminiscence2.exe` | `python tools/build_release.py --target win64` in MSYS2 UCRT64 |
| Linux x86-64 tar.gz | `play.sh` | `python3 tools/build_release.py --target linux64` |
| Steam Deck tar.gz | `play.sh`, add as a Non-Steam Game | `python3 tools/build_release.py --target steamdeck` |
| Apple Silicon ZIP | `REminiscence2.app` or `Play.command` | `python3 tools/build_release.py --target macos-arm64` |
| Intel Mac ZIP | `REminiscence2.app` or `Play.command` | `python3 tools/build_release.py --target macos-x86_64` |
| Quest arm64 APK | REminiscence2 in Unknown Sources | `python3 tools/build_release.py --target quest` |

On Mac keep the app beside the package's assets folder. The native Finder
launcher passes its absolute location to the engine; the standalone CLI also
finds assets beside itself. Mac packages target macOS 11 or later and contain
only system framework/dylib dependencies. They are ad-hoc signed, **not Apple
notarized**: Gatekeeper may require your first-launch approval in Privacy &
Security. No paid Apple signing identity is assumed.

Linux builds use Ubuntu 22.04 as the glibc baseline (2.35). SDL2/zlib are compiled
from pinned sources; libc and graphics/audio drivers come from the OS. Steam
Deck reuses this engine and includes launcher notes. Controller-only gameplay,
Gaming Mode, real GPU/audio and suspend/resume still require device checks;
this is not a Valve Verified claim. Linux archives preserve executable bits.

Windows uses UCRT64 GCC/Ninja. Runtime imports are recorded in the package;
necessary GCC threading/runtime DLLs are copied when imported. Desktop packages
use the internal MOD player and bundled AdLib synthesis, reducing dependencies.
Optional FluidSynth/MT-32 support remains available in source builds.

## Build requirements

Desktop: Python 3.9+, CMake 3.20+, a C/C++17 compiler, and native SDKs. Linux also
needs SDL's X11/Wayland/audio development headers (`libsdl2-dev` on Ubuntu is a
convenient way to install them); Windows uses MSYS2 UCRT64 GCC, CMake and Ninja.
Mac needs Command Line Tools. `packaging/dependencies.json` pins SDL 2.32.10 and
zlib 1.3.1 by immutable revision and SHA-256. Release CMake builds them itself.

`tools/build_release.py` builds, runs the asset-free CTest suites, checks library
dependencies, assembles launchers/docs/licenses, and smoke-tests the relocated
binary. Use `--build-dir` to reuse an existing build and `--output` to change the
archive destination. Build each Mac architecture natively for its test pass.
The GitHub **Release packages** workflow builds all targets and publishes build
artifacts; a tagged release uses those exact artifacts with SHA-256 checksums.

`python3 tools/package_source.py` archives tracked HEAD plus the exact SDL/zlib
sources. Ignored/private files are excluded. In that source archive, CMake can
build offline by setting `FETCHCONTENT_SOURCE_DIR_SDL2=third_party_src/SDL2` and
`FETCHCONTENT_SOURCE_DIR_ZLIB=third_party_src/zlib` to absolute paths. Standard
platform SDKs/toolchains and Gradle dependencies must already be installed.

## Quest: flat Android panel

This APK uses SDLActivity in an ordinary Android window. It does not require an
immersive VR scene. Start with a **Bluetooth gamepad or keyboard**; Touch-pointer
menu navigation is not a substitute for platformer controls. Physical Quest
validation remains pending until tested on an actual headset.

Build prerequisites: JDK 17, Gradle 8.9, Android SDK platform 35, CMake 3.22.1 and
NDK 28.2.13676358. The arm64-only build targets Android API 29+, with target SDK
35. Install SDK components with `sdkmanager`; accept SDK licenses separately.
The recipe copies only SDL's pinned Java host into the build, not game files.

A release build requires `REMINISCENCE2_KEYSTORE`, `REMINISCENCE2_STORE_PASSWORD`,
`REMINISCENCE2_KEY_ALIAS`, and `REMINISCENCE2_KEY_PASSWORD`. Store the key outside
Git and retain it for compatible APK updates. CI reads these through repository
secrets. For unsigned development credentials, `--quest-debug` creates a
separate `org.b0realis.reminiscence2.experimental` app so it cannot replace the
release installation.

The launcher creates **assets**, **SAVE** and **TUNES** under its app-owned
external files folder. You can import a prepared ZIP through Android's document
picker without broad storage permissions. ZIP imports validate paths, sizes and
a supported-data marker before replacing the old assets. WHDLoad LHA/RNC
preparation still runs on the computer with `tools/extract_amiga.py`.

### Push from Linux using adb

The Quest bundle ZIP includes the APK, precreated `assets` folder and
`push-to-quest.sh`. Fill that folder and run the launcher on Linux.

Enable headset Developer Mode, connect USB and authorize the computer. Install
Linux tools (`sudo apt install adb`), then run:

```sh
python3 tools/push_to_quest.py \
  --apk REminiscence2-VERSION-quest-arm64.apk \
  --assets /path/to/prepared/Flashback
```

Use `--serial DEVICE_ID` for multiple connected devices, `--experimental` for a
debug APK, or `--dry-run` to validate local inputs without connecting. The helper
installs/updates the APK, opens its launcher to create correctly owned folders,
checks free space, stages files through Download and copies them on the device.
Each transfer and installed file is SHA-256 checked. A complete new assets tree
is promoted only after verification; existing saves and tunes are preserved.
No root or all-files permission is required. A live headset deployment is a
separate hardware test; automated helper tests cannot establish USB/OS behavior.

## Tag and publish a verified preview

After committing the version bump, run the **Release packages** workflow on
that commit. All six targets and corresponding source must succeed, together
with the normal Build and test workflow. Prepare release notes, then run:

```sh
python3 tools/publish_release.py --run-id RUN_ID --notes /path/to/notes.md --publish
```

The publisher checks both workflows against exactly HEAD, downloads the eight
expected assets, validates archives/architectures/private-file boundaries and
source versions, writes SHA256SUMS, pushes the annotated `vVERSION` tag and
publishes a GitHub prerelease. Omit `--publish` for verification only. Physical
Deck/Quest and full GOG/Macintosh asset playthroughs remain release-test work;
a passing compiler does not establish those hardware/data combinations.
