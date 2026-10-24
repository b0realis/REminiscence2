> Historical audit against the pre-release tree. Its filesystem, atomic-save,
> hotplug and packaging action items were implemented in versions 1.02–1.06.
> See [the changelog](../CHANGELOG.md), [current roadmap](ROADMAP.md) and
> [release recipes](RELEASES.md) for current status.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Release readiness and next fixes

Reviewed on 24 October 2026 against implementation commit `9f3e0d1`.
Patch **1.01** established versioning and recorded this review. **1.02** implements
the filesystem, atomic-save, archive, controller, footer and allocation changes
below. Release packaging and device validation are tracked in RELEASES.md.
The table retains the original findings to explain the changes.

The review covers startup, filesystem and archive access, save writing, SDL
rendering/input, the keyboard diagram and existing build/CI configuration.
It is a targeted source review, not a complete security or performance audit.
Performance opportunities below have not been benchmarked.

## Patch numbering

`version.h` is the single version source for the banner, `--version`, overlay
and CMake metadata. Delivered patches advance **1.01 → 1.02 → 1.03**.
The counter continues **1.09 → 1.10** and **1.99 → 1.100**; it is not a
floating-point number. Run:

```sh
python3 tools/bump_version.py --dry-run
python3 tools/bump_version.py
```

CMake represents display version `1.01` as numeric package version `1.0.1`.
It automatically reconfigures when the header changes. CLI tests use the
derived version, so the application and build metadata cannot silently drift.
Save-state and preference-schema versions remain independent. Historical
screenshots retain the version actually captured.

## First fixes to prioritize

| Priority | Finding in the current code | Smallest useful change | Validation |
| --- | --- | --- | --- |
| P1 | `fs.cpp:108–109` tests a Windows search handle with `if (h)`. A failed `FindFirstFile` returns the nonzero `INVALID_HANDLE_VALUE`, so the loop can consume uninitialized `findData`. | Check the documented failure value, then return a useful missing-directory diagnostic. | Native Win64 missing/empty directory tests and a Windows CI job. |
| P1 | `Game::saveGameState` opens the final save with `zwb` and reports success before close. `StdioFile::close` and `GzipFile::close` discard close errors. The existing atomic preference writer does not protect game saves. | Write a sibling temporary save; propagate compression/flush/close failures; replace the old slot only after success. | Interrupted/full/failed-write fixtures must retain the previous save; round-trip existing version-2/3 saves. |
| P1 | `ResourceAba::findEntry` uses `strcpy` into a fixed name field. ABA header checks rely on assertions, and PC-98 PAQ code reads embedded offsets such as `dst[18]` before proving the buffer is long enough. | Bound lookup names; validate entry tables, file ranges and allocation sizes; reject truncated PGE/ICN wrappers before indexing. | Synthetic malformed archives under Linux ASan/UBSan; retain the existing format tests. This is a source finding, not a claim of a demonstrated exploit. |
| P1 | Asset/save/tune defaults are `DATA`, `SAVE`, `TUNES` relative to the current working directory. The current Mac executable links Homebrew paths directly. | Define standard writable user storage, preserve explicit CLI paths, and add an explicit portable mode. Package runtime dependencies beside the executable. | Launch outside the checkout and from read-only locations; test non-ASCII paths and unwritable destinations. |
| P2, small | The desktop footer uses a fixed 160-pixel Resume button. Enlarged-text captures show its label clipping. | Size footer buttons from rendered text and available width; use a compact footer before labels overlap. | Check 75–175% text sizes, narrow windows and 256×224 without losing Resume or EXIT GAME. |
| P2 | Controllers are opened only at startup. There are no gameplay handlers for joystick/controller added/removed events, and device selection assumes index zero. | Reopen devices on hotplug, match events to instance IDs, and release held actions on disconnect. | Connect/disconnect/reconnect sequences, then physical Deck and Bluetooth-pad checks. |
| P2 | POSIX directory traversal follows `stat` into directories without cycle detection; `addPath` grows raw arrays with direct `realloc` assignments. Filename lookup scans the full list each time. | Replace traversal storage with owned containers, a visited-directory policy and a case-normalized index. Define duplicate-basename precedence explicitly. | Symlink loops, deep trees, missing directories, duplicate basenames and stable lookups. This improves startup reliability; speed gains depend on the data set. |

Source references: [filesystem](../fs.cpp), [save writer](../game.cpp#L2030),
[file backends](../file.cpp), [ABA](../resource_aba.cpp),
[PAQ](../resource_paq.cpp), [platform](../systemstub_sdl.cpp),
[footer](../settings_overlay.cpp#L696).

The Windows handle contract is documented by
[Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilea).

## Optimizations that preserve gameplay

1. **Reuse widescreen scratch buffers.** `copyWidescreenLeft/Right`, mirror and
   blur allocate full-frame RGB buffers, sometimes an additional blur buffer,
   on each call. Keep renderer-owned buffers and resize only when dimensions
   change. Measure room-transition time and allocation counts; preserve palette
   restoration and the existing 419-room Amiga regression.
2. **Avoid reprocessing an unchanged paused game frame.** The settings loop
   calls `drawGameFrame` about every 16 ms, which rescales/uploads the game
   texture even while only UI focus changes. Track changes to composed pixels,
   palette, zoom and scaler state, reuse the existing texture when unchanged,
   and continue drawing the interactive UI. This is a battery/CPU opportunity,
   especially for Deck and Quest, not a reason to change simulation speed.
3. **Reuse the Amiga T-shirt segmentation workspace.** `markAmigaTshirt`
   allocates `seen` and component vectors for each decoded Conrad frame.
   Reuse bounded workspace first; consider a resource-scoped frame cache only
   after measurement. Keep the exact segmentation rule and every jacket,
   shoe, equipment and orientation regression intact.
4. **Cache static keyboard geometry.** The Controls page reconstructs the
   keyboard and SDL key-name strings on every UI frame. Cache the static caps
   and derive highlights/callouts from current bindings. This is a minor UI
   allocation improvement, not an expected gameplay FPS gain.

Relevant implementation: [SDL rendering](../systemstub_sdl.cpp#L404),
[Conrad drawing](../game.cpp#L1063), [appearance](../appearance.cpp#L25),
[keyboard model](../keyboard_diagram.cpp#L75).

Do not change collision, script execution, animation timing or the nominal
30 Hz simulation as an incidental optimization. Collect before/after timings
with the same assets, scene and settings before quoting performance gains.

## Already completed: do not redo

Current code already has save-load preflight checks, bounded memory streams,
the PC-98 existence-recursion fix, focus-loss input clearing, negotiated audio
with a 512-frame request, renderer fallback, preference persistence, save
browsing and the live keyboard diagram. Several items in the initial historical
[source audit](SOURCE_AUDIT.md) predate these changes. Atomic game saves and
controller hotplug are still separate outstanding work.

## Target release packages

Desktop packages retain a command-line engine. Game assets remain separately
supplied by the player. A native Quest package needs an Android activity to
launch the same engine.

| Download target | Architecture / format | Work before calling it supported |
| --- | --- | --- |
| Win64 | Native x86-64 executable and DLLs in a ZIP | Add native CI, fix the failed-handle bug, resolve POSIX header/CLI portability and SDL entry points for tests, collect the DLL dependency closure, test asset/save paths. UCRT64 is a practical first toolchain to investigate. |
| Linux64 | x86-64 portable tarball; AppImage can follow | Build against a documented runtime baseline, bundle permitted non-system libraries, provide a launcher and check the package on a clean system. Ubuntu CI success alone is not a portability guarantee. |
| Steam Deck | The Linux x86-64 engine plus a Deck launcher/profile | Validate Gaming Mode, controller-only startup/gameplay/QoL, 1280×800 readability, reconnect, sleep/resume and writable saves. It does not need a separate gameplay engine. |
| macOS Apple Silicon | Native arm64 executable plus bundled dependencies | Local arm64 code already runs. Replace Homebrew-only dynamic-library paths, collect transitive libraries, choose/test the minimum macOS version and provide a relocatable CLI package. |
| macOS Intel | Native x86-64 build and matching dependencies | Build/test the Intel slice and every bundled library independently. A successful arm64 build does not establish Intel compatibility. |
| Quest flat panel | Signed Android arm64 APK, sideloaded | Add SDLActivity/Gradle/NDK hosting, an Android shared engine target, app-owned asset/save paths, data import/deployment, lifecycle/texture recovery and tested input. This is a port, rather than another desktop archive. |

**Mac distribution:** start with separate native arm64 and x86-64 packages.
A Universal 2 executable is possible, including for a CLI app, once every
bundled compiled dependency has both slices. Verify actual Mach-O architectures
and dynamic-library paths. A Finder app wrapper is optional; it is not needed
to preserve command-line operation. See
[Apple's universal-binary documentation](https://developer.apple.com/documentation/apple-silicon/building-a-universal-macos-binary).

**Linux/Deck distribution:** use a controlled build/runtime baseline and a
complete dependency manifest. Valve documents matching the selected runtime
to the SDK used to compile the executable. Deck usability also requires
controller coverage and readable UI, not just producing a Linux binary. See
[SteamOS/Linux development](https://partner.steamgames.com/doc/store/application/platforms/linux)
and [hardware compatibility](https://partner.steamgames.com/doc/steamhardware/compat).

**Windows toolchain:** the existing GNU/POSIX assumptions make
[MSYS2 UCRT64](https://www.msys2.org/docs/environments/) worth trying first.
MSVC compatibility and a clean Windows install are separate validation steps;
no native Windows build result was produced in this review.

## Quest: the flat-panel route

Yes: retain the C++/SDL engine and host it as an ordinary Android 2D app.
Horizon OS supplies the spatial panel; an immersive scene or an OpenXR renderer
is not required for this scope. Meta documents 2D Android apps in resizable
windows and controller/hand interactions mapped to Android gestures:
[Android apps on Horizon OS](https://developers.meta.com/vr/documentation/android-apps/horizon-os-apps/).

The adjacent ShandalarGodot project's `docs/handhelds.md`, section **Meta Quest
3**, records precisely this approach and a Quest 3 test. Its APK deployment
helpers stage data in Download before copying into app-created storage to work
around headset ownership issues. This is relevant local evidence, not evidence
that REminiscence2 already runs on the headset. Its Godot export templates do
not compile this SDL engine.

For REminiscence2:

- Use SDL's Android activity and build the engine/shared libraries for
  `arm64-v8a`. Start with the internal MOD/AdLib paths to reduce optional native
  dependencies. SDL documents the host in its
  [Android port guide](https://wiki.libsdl.org/SDL2/README-android).
- Let the app create writable directories. Import player-owned data into its
  storage, or provide an explicit ADB deployment helper. Keep game files out of
  the APK and keep Android permissions limited to actual needs.
- Test suspend/resume and texture recreation. Current platform code has no
  explicit Android app-lifecycle or renderer-reset recovery path.
- Use a Bluetooth gamepad for the first gameplay proof, or add on-screen
  directional/action controls. A headset laser/pointer can navigate the menu;
  it is not by itself a complete platformer input scheme. Native Touch-controller
  gameplay mapping must be verified rather than assumed.
- Keep an update-signing key outside Git so later sideloaded APKs can update
  the same installation. Choose package ID, minimum OS and supported headset
  models as part of the actual Android port.

## Recommended order

1. **Next patch:** small release blockers—Windows scanner failure handling,
   portable build smoke coverage and text-scaled footer sizing.
2. Atomic game saves with durable error reporting and interrupted-write tests.
3. Safe indexed asset discovery and targeted ABA/PAQ bounds checks.
4. Produce testable Win64/Linux64/arm64-Mac/Intel-Mac packages with version,
   checksums, licenses, corresponding source and a dependency manifest.
5. Add Deck packaging and controller/suspend validation; measure allocation and
   paused-render optimizations before changing them.
6. Build the Quest Android host, asset path/import flow and flat-panel gameplay
   proof, then validate on the headset before declaring support.

Keep SDL2 for the first reproducible releases. A native SDL3 migration can be a
later milestone rather than a prerequisite for these packages.

## Validation boundary

The **1.01** versioning patch builds locally in Release and UBSan Debug. All
9 Release suites and all 11 Debug suites pass, including the private Amiga and
cutscene checks with UBSan set to halt on error. The executable reports `1.01`,
CMake derives `1.0.1`, and the bump helper previews `1.02`. Documentation-boundary
and whitespace checks also pass.

The implementation entering this audit passed Linux Debug ASan/UBSan, Linux
Release/internal-MOD and macOS CI. Native Apple Silicon gameplay and the private
Amiga regression are available locally. This review does not establish packaged
release artifacts, Win64 or Intel-Mac build results, or Deck/Quest hardware
results. Macintosh CD and GOG asset validation are separate from host-OS support;
their real data/playthrough matrix remains outstanding.
