<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Build REminiscence2 yourself

This guide covers a fresh desktop checkout, the game player on its own, and the
visual editor on its own. Original Flashback data is **not needed to compile**
or run the public tests. Playing the game or playtesting with Conrad requires
your own prepared assets; see [asset preparation](ASSETS.md).

The editor is a separate application. Both executables compile shared engine
and content libraries, so an editor-only build still compiles simulation code.
It does not need to build the player executable unless you want external
native-campaign or whole-campaign playtests.

## 1. Install the build tools

Common requirements: a C and C++17 compiler, CMake 3.20+, SDL2 2.0.18+ development
files, and zlib development files. Python 3.10+ enables the full set of public
Python tests and packaging helpers. Git is needed to clone the repository.
The examples use the internal MOD music player, so libmodplug is optional.

### macOS: Apple Silicon or Intel

Install Xcode Command Line Tools, then the dependencies with Homebrew:

```sh
xcode-select --install
brew install cmake pkgconf sdl2 python
```

The macOS SDK supplies zlib. Use a native Terminal and Homebrew installation for
your architecture: arm64 on Apple Silicon, x86-64 on Intel. CMake normally
detects it. To select explicitly, add `-DCMAKE_OSX_ARCHITECTURES=arm64` or
`-DCMAKE_OSX_ARCHITECTURES=x86_64` at configuration time in a fresh build folder.
Build and run the tests natively on the corresponding Mac architecture.

### Debian/Ubuntu Linux

```sh
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libsdl2-dev zlib1g-dev git python3
```

These commands target the installed distribution. Portable Linux release
packages use a specific build baseline; see [release recipes](RELEASES.md).

### Windows x64

Install [MSYS2](https://www.msys2.org/), open its **UCRT64** terminal, update it
with `pacman -Syu` and follow any request to close/reopen the terminal. Then:

```sh
pacman -S --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-zlib mingw-w64-ucrt-x86_64-python
export CMAKE_GENERATOR=Ninja
```

Run the commands below in that same UCRT64 terminal. Do not mix its packages
with the plain MSYS or MINGW64 compiler. Built executables have the `.exe`
suffix. The tested Windows route is UCRT64 GCC/Ninja; this guide does not claim
MSVC support. Use `python` where an example calls `python3` if necessary.

## 2. Get the source

```sh
git clone https://github.com/b0realis/REminiscence2.git
cd REminiscence2
```

For a fixed release, check out its tag before configuring, for example:

```sh
git checkout v1.80
```

Alternatively, extract the release's `REminiscence2-VERSION-source.tar.gz` and
open a terminal in the extracted directory. It includes the exact SDL2/zlib
sources used by the packaged builds. The automatic GitHub “Source code” archives
do not include those additional dependency sources.

All following paths are relative to the source root. Use a separate build
folder for each compiler, architecture or sanitizer configuration.

## 3. Choose what to build

### Complete development build: player, editor and tests

```sh
cmake -S . -B build/dev -DCMAKE_BUILD_TYPE=Release -DREMINISCENCE2_USE_MODPLUG=OFF -DBUILD_TESTING=ON
cmake --build build/dev --parallel 3
ctest --test-dir build/dev --output-on-failure
```

The output includes `build/dev/reminiscence2`,
`build/dev/reminiscence2-editor`, their shared static libraries and the test
executables. On Windows the two applications end in `.exe`. Tests use synthetic
fixtures and the public editor examples; private asset tests are opt-in.

### Game engine/player only

```sh
cmake -S . -B build/player -DCMAKE_BUILD_TYPE=Release -DREMINISCENCE2_USE_MODPLUG=OFF -DBUILD_TESTING=OFF
cmake --build build/player --target reminiscence2 --parallel 3
./build/player/reminiscence2 --version
./build/player/reminiscence2 --help
```

This builds the player and its required libraries, including the in-game QoL
UI and custom-campaign runtime. It does not build the editor executable or test
programs. Start it with prepared data:

```sh
./build/player/reminiscence2 --datapath="/path/to/prepared/assets" --window
```

If the folder contains several editions, use `--list-assets` and select the
reported identifier with `--asset-set=ID`. `--datapath` is an asset directory,
not an installer, LHA archive, CUE file or disc image.

### Visual editor only

```sh
cmake -S . -B build/editor -DCMAKE_BUILD_TYPE=Release -DREMINISCENCE2_USE_MODPLUG=OFF -DBUILD_TESTING=OFF
cmake --build build/editor --target reminiscence2-editor --parallel 3
./build/editor/reminiscence2-editor --help
./build/editor/reminiscence2-editor examples/editor/workshop/project.r2project.json --validate
```

To create an editable project in a writable folder:

```sh
./build/editor/reminiscence2-editor .local/editor-projects/MyLevel/project.r2project.json
```

To study the supplied laboratory, copy its whole directory first so the example
and its relative media files stay intact:

```sh
mkdir -p .local/editor-projects
cp -R examples/editor/systems-lab .local/editor-projects/MyLab
./build/editor/reminiscence2-editor .local/editor-projects/MyLab/project.r2project.json --assets="/path/to/prepared/assets"
```

An editor-only build can author/validate documents and use embedded custom-room
playtests with suitable assets. **Native-campaign playtesting and the Campaign
workspace's Play action launch the separate player.** Build it in the same
directory when you need those features:

```sh
cmake --build build/editor --target reminiscence2 --parallel 3
```

Keep `reminiscence2` beside `reminiscence2-editor` (both `.exe` on Windows).
The editor locates it relative to its own executable. See the
[editor guide](../editor/README.md) for the four workspaces and live-edit limits.

## 4. Dependency and build choices

| CMake setting | Meaning |
| --- | --- |
| `BUILD_TESTING=ON` | Build the CTest programs and editor UI test hooks; build the default target before running CTest. |
| `BUILD_TESTING=OFF` | Omit tests; use named targets to build only the application you want. |
| `REMINISCENCE2_USE_MODPLUG=OFF` | Internal MOD player; no libmodplug dependency. |
| `REMINISCENCE2_USE_MODPLUG=ON` | Requires libmodplug development files (`libmodplug-dev` on Debian/Ubuntu, `libmodplug` with Homebrew). |
| `REMINISCENCE2_BUNDLED_DEPS=ON` | Compile pinned SDL2/zlib sources instead of system libraries. May download them on the first configuration. |
| `REMINISCENCE2_FLUIDSYNTH=ON` | Optional FluidSynth backend; requires its development library. Soundfonts are supplied separately. |
| `REMINISCENCE2_MT32EMU=ON` | Optional Munt/MT-32 backend; requires libmt32emu. ROMs are supplied separately. |
| `REMINISCENCE2_SANITIZERS=ON` | Enable ASan/UBSan with Clang or GCC; see the development section in the root README. |

For bundled SDL/zlib with CMake 4, also pass
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` for the pinned dependencies' older CMake
files. With the corresponding-source archive, CMake automatically finds
`third_party_src/SDL2` and `third_party_src/zlib`; after installing the platform
compiler/SDK and build prerequisites, these dependency sources need no download:

```sh
cmake -S . -B build/bundled -DCMAKE_BUILD_TYPE=Release -DREMINISCENCE2_BUNDLED_DEPS=ON -DREMINISCENCE2_USE_MODPLUG=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_TESTING=OFF
cmake --build build/bundled --target reminiscence2 reminiscence2-editor --parallel 3
```

Linux still needs the platform window/audio development headers; installing
`libsdl2-dev` provides the usual dependencies. Bundled dependencies do not remove
the need for native SDKs, OS graphics/audio drivers or a C/C++ toolchain.

## 5. Tools and extra validation

The Sega CD movie exporter is a separate optional target. From a configured
build directory:

```sh
cmake --build build/dev --target reminiscence2-export-mcd --parallel 3
```

The Python upscaler has its own optional environment/models; follow
[the movie tooling guide](../tools/MOVIE_UPSCALING.md). Python is not embedded
in the player or editor.

To enable local original-data integration tests, reconfigure the full build
with any available prepared editions, then build and run CTest again:

```sh
cmake -S . -B build/dev -DREMINISCENCE2_AMIGA_DATA="/path/to/unpacked-amiga" -DREMINISCENCE2_DOS_DATA="/path/to/dos-data" -DREMINISCENCE2_SEGACD_DATA="/path/to/extracted-segacd"
cmake --build build/dev --parallel 3
ctest --test-dir build/dev --output-on-failure
```

Omit unavailable edition arguments. Do not put original data into Git. See
[validation records](VALIDATION.md) for what automated checks cover, and the
root [development instructions](../README.md#development) for sanitizer setup,
including the Apple Clang runtime caveat.

## 6. Installable packages and Quest

A development executable may depend on libraries from your build machine.
For distribution, use `tools/build_release.py` and the
[platform release recipes](RELEASES.md), which build/test the selected target,
collect runtime dependencies, include guides/licenses and check the relocated
executables. Build each target on its native OS; the recipe is not a universal
cross-compiler. Desktop ZIP/tar packages contain both applications and the
public starter projects. Mac DMGs install the player; use the Mac ZIP for the editor.

Quest uses the Android player target `main` with SDLActivity and a separate
JDK/SDK/NDK/Gradle toolchain. The desktop editor is not built into the APK.
Use [the Quest recipe and Linux ADB deployment instructions](RELEASES.md#quest-flat-android-panel).

## Troubleshooting

- **SDL2 or zlib not found:** install development packages, not just runtime
  libraries. On a Mac with nonstandard discovery, add
  `-DCMAKE_PREFIX_PATH="$(brew --prefix)"` when configuring.
- **Missing libmodplug:** use `-DREMINISCENCE2_USE_MODPLUG=OFF`, or install its
  development package and reconfigure.
- **CTest says an executable is missing:** a named application target does not
  build every test. Use the complete build with `BUILD_TESTING=ON`, then build
  without `--target` before running the full suite.
- **Cannot launch native player from the editor:** build the player in the
  editor's build directory and keep both executables together.
- **Compiler/architecture changed:** configure into a fresh build directory.
  Reusing a CMake cache across compilers or architectures is unsupported.
- **Build runs out of memory:** reduce `--parallel 3` to `--parallel 1`.
- **Local convenience scripts cannot find a binary:** `play-local.sh` and
  `edit-local.sh` target this development workspace's `build/package-arm64`.
  For the folders in this guide, invoke their executables directly.

Only delete disposable build folders when cleaning up. Keep original assets,
authored projects and saves outside those folders.
