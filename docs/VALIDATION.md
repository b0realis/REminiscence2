# REminiscence2 validation

Validation date: 23 October 2026. These checks cover the local enhancement tree,
not a released package or a completed playthrough.

## Environment

| Component | Local configuration |
| --- | --- |
| Host | macOS 26.5.2 build 25F84, Apple Silicon arm64 |
| Compiler | Apple Clang 17.0.0, Command Line Tools |
| CMake | 4.4.4 |
| SDL2 API | sdl2-compat 2.32.74, backed by SDL3 3.4.18 |
| libmodplug | 0.8.9.0 |
| FluidSynth | 2.6.1, optional Debug backend enabled |
| mt32emu | 2.8.3, optional Debug backend enabled |
| Dear ImGui | Pinned v1.91.9b, core and SDL2 renderer backend, MIT notices retained |
| Nuked OPL3 | Bundled version 1.8, pinned commit in third-party README |
| zlib | macOS SDK 1.2.12 headers, system libz |

Development dependencies were installed through Homebrew. No commercial game
files were downloaded. User-supplied references and unpacked Amiga assets are
kept under ignored `.local/` paths.

## Checks

| Build | Configuration | Result |
| --- | --- | --- |
| `build/debug` | libmodplug, AdLib/FluidSynth/MT-32, UBSan with halt on error, private Amiga assets | Eleven suites pass, including settings and private Amiga checks |
| `build/release` | libmodplug, bundled AdLib, optional synthesizers disabled | Nine asset-free suites pass |
| `build/internal-mod` | Internal MOD player, bundled AdLib | Nine asset-free suites pass |

The suites exercise:

- Configuration and 38 CLI scenarios, including equivalent `-h`/`--help` output,
  help without configuration or game data, malformed masks/rates, missing data,
  long paths, and default/explicit configuration lookup.
- The standalone Amiga preparation tool has 12 additional Python tests using
  synthetic LHA members: CRC/size validation, RNC result checks, failed-decoder
  cleanup, source/output preservation, unsafe paths, resource collisions and
  dependency diagnostics. These run separately from CTest and are configured
  in CI. RNC subprocess unit tests use a stand-in; real decompression is checked
  against the private archive.
- PC-98 virtual archive lookup, restored table sentinels and sorted hashes,
  memory stream growth, overwrite, byte order, EOF and invalid seeks.
- Bounded C103/C211 decoding, repeat counts, clipping, flip, transparency,
  foreground masks, and resource-fork offsets/types/indexes. Corrupt inputs
  and all truncations of the small format fixtures are rejected.
- A synthetic Macintosh resource fork and native 512×448 renderer, including
  Start, Skill, Controls, Password, Demo, credits selection and `vers` metadata.
- Concave polygon clipping, alpha palette banks, Sega column-major tiles,
  RLE character halves, and malformed reverse Bytekiller streams.
- MIDI running status, meta timing, PRF/INS parsing, fractional sample timing,
  callback partition independence, AdLib waveform generation, repeated backend
  resets, and invalid soundfont/ROM handling.
- SDL dummy audio at 8,000/44,100/48,000 Hz, callback delivery, repeated close,
  and input reset on focus loss. These are not physical-device listening tests.

The Debug music suite also synthesizes with FluidSynth's installed
`VintageDreamsWaves-v2.sf2` fixture. This soundfont is not bundled in the repo
and does not establish original Flashback instrumentation. MT-32 ROMs were
unavailable, so only its initialization failure and cleanup paths were run.

The private Amiga suite renders the opening polygon cutscenes, loads all seven
levels, renders 419 valid rooms, repeats each room with adjacent rendering,
compares active-room pixels and palette, advances 210 simulation steps, and
checks manual save/load and rewind. Representative screenshots were inspected.
This caught and fixed bank selection leaking from an adjacent room. Opening
cutscene coverage also exposed inherited negative signed shifts; fixed-point
operations now use defined multiplication/unsigned arithmetic. The final UBSan
run halts on the first error and passes.

The cutscene browser adds bounded CMP container and part-table checks, real
DOS command/polygon pair discovery, and typed Macintosh movie/polygon resource
fixtures with absolute part offsets. Nine Python checks cover catalog parsing,
selection, failed inspection, literal argument boundaries, and a full frontend
launch with synthetic assets and SDL dummy devices. Python uses only the
standard library. The catalog itself opens no SDL devices and creates no
save/tune folders; the synthetic playback test verifies asset bytes remain
unchanged.

The separate private cutscene test renders all **27 files / 68 parts** from the
supplied Amiga package, advances interactive panels and feeds the real MOD
mixer through the headless platform. The final run completed all parts without
using its preview skip fallback. This exposed an inherited signed overflow in
GENEXP's large ellipse error terms; 64-bit accumulators and a clipping regression
fixture now cover it. Strict UBSan passes the full catalog. Representative
intro and memory-scene frames were inspected. This is automated rendering and
mixing coverage, not an auditory review of every track or a full game playthrough.

The executable is Mach-O arm64. Help/version work without assets. The inherited
LICENSE hash remains equal to the imported baseline.

## In-game settings overlay

The new settings suite round-trips all preferences, rejects out-of-range and
colliding bindings, checks unsupported schema handling and atomic replacement
failure, and verifies independent master/music/effect gains and silent playback
advancement. A scripted game boundary confirms that settings reach the engine
without moving the player, that persistence succeeds, and that menu input and
pause time do not leak into gameplay.

SDL dummy-device tests apply new textures, scale and side content, rebind and
release a movement key, reject the old primary key, and open/close the complete
overlay through F10. SDL tests also click the on-screen shortcut, dispatch cheat
and healing actions, and confirm the menu retains a 256×224 window. Twenty-four
software-rendered captures cover all eight pages at desktop and native
resolution, compact layout and enlarged text. Pixel checks verify the turquoise
EXIT GAME button stays visible; actual mouse/key input exercises exit
confirmation, cancellation and acceptance. The private Amiga check rejects
truncated, invalid-link and future-version saves without mutating serialized
state, exercises menu save/load/healing/cheat/screenshot commands, and starts
all seven levels with selected difficulty and cleared rewind/death state. It
also changes side content while retaining the composed frame, player
position/life and random state. Captures over actual
Amiga gameplay were inspected. GPU/high-DPI, physical controller navigation and
Windows behavior remain to be checked on their respective devices.

Keyboard-diagram checks cover default and WASD bindings, extra function/media
keys, unique non-overlapping physical caps, and desktop, compact and enlarged
text captures. Actual mouse clicks on a colored cap and an arrow callout enter
the shared SDL rebind path. A duplicate is rejected, a subsequent valid key is
accepted, and pixel checks verify the highlight moves while the old key becomes
dark. Native-size caps also rebind, saved profiles restore the correct highlights,
and Escape cancels capture without resuming the game.

Shirt appearance checks cover Original/Red-Pink/White profile round trips,
invalid enum rejection, legacy boolean import and reopening with Red/Pink.
Rendering checks exercise all four character orientations, NPC preservation,
foreground occlusion and synthetic Macintosh shirt blits. Private Amiga tests
reserve only palette entries 0x6C/0x6D, check all 419 room images for collisions,
and change all three styles on each of seven levels without changing the
indexed frame, room background, shared palette, score, position, life, random
seed or pause timers. Native-resolution captures show the new control. Actual
DOS/GOG, Sega and Macintosh asset appearance needs visual checks with those
editions; synthetic paths do not replace that testing.

The Amiga jacket/T-shirt regression checks actual decoded player poses and
accepts recolouring only light fabric pixels. Brown jacket, skin, hair and
trouser pixels must remain identical across every original Conrad sprite.
Named landmarks check the visible T-shirt, a shoe sole and the held weapon's
metal highlights. Rotated synthetic poses include separate boots and equipment
using the same light shades. Gameplay captures turn Conrad toward the camera
so the actual undershirt is visible, then compare all pixels outside the
T-shirt palette aliases across Original, Red/Pink and White.

## Documentation cleanup

The player field guide was rewritten as original REminiscence2 documentation.
The earlier publisher-manual conversion, its image manifest and all 54 PNGs
were removed from public documentation. Six other supplied image/PDF references
without recorded redistribution grants were also moved into ignored local
storage. All 66 preserved files, including the superseded text and provenance
notes, retain their recorded sizes and SHA-256 hashes.

Documentation checks resolved 72 local links/anchors and matched all 21 long
command options and 19 configuration defaults against the implementation.
The release guard passed the cleaned tree and rejected injected binary/figure
fixtures. A separate scratch Git tree verified that `git archive` excluded
eight representative private/removed paths. The guide contains no embedded
images; an exact-text check found no shared 12-word passages with the removed
publisher-manual portion. These structural checks do not determine copyright
status or clear the inherited engine's separate provenance questions.

## Reproduce

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure

cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug \
  -DREMINISCENCE2_SANITIZERS=ON -DREMINISCENCE2_SANITIZER_SET=undefined \
  -DREMINISCENCE2_FLUIDSYNTH=ON -DREMINISCENCE2_MT32EMU=ON \
  -DREMINISCENCE2_AMIGA_DATA=/absolute/path/to/unpacked/amiga \
  -DREMINISCENCE2_TEST_SOUNDFONT=/absolute/path/to/reference.sf2
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
```

The private paths are CMake cache settings and are not committed. Omit them to
run without private assets; optional libraries require development packages.
The sanitizer option makes UBSan findings fail CTest rather than recover.

## AddressSanitizer runtime limitation

The combined ASan/UBSan build compiled, but both executables timed out before
entering `main()`. A native process sample placed the spin inside
`libclang_rt.asan_osx_dynamic.dylib`, following this initialization sequence:

```text
AsanInitInternal
  InitializeShadowMemory
    MemoryRangeIsAvailable
      get_dyld_hdr
        dyld_shared_cache_iterate_text_swift
          _Block_copy / malloc
            AsanInitFromRtl
              StaticSpinMutex::LockSlow
```

This is consistent with the runtime initialization issue addressed in
[LLVM change 182943](https://github.com/llvm/llvm-project/pull/182943).
It is not a passing ASan result. Use a runtime containing the upstream fix
for local ASan checks; UBSan alone passed on the existing toolchain.
The Linux CI job retains both sanitizers.


## Remaining validation

- Hosted CI covers Linux ASan/UBSan, optional MIDI backend compilation, the
  internal MOD player, and macOS. The Linux MIDI job builds libmt32emu from
  pinned Munt release `munt_2_8_2` because the runner has no development package.
  Current results are available in [GitHub Actions](https://github.com/b0realis/REminiscence2/actions).
- Actual GOG DOS/Windows and Macintosh CD asset sets remain to be supplied and
  verified. Sega and PC-98 additions also need edition-specific runtime checks.
- MT-32 playback, physical audio devices, controllers, high-DPI/GPU output,
  Windows, native SDL2, Intel macOS and Linux ARM runtime checks remain.
- Full progression, combat, inventory interactions, level transitions, endings,
  all saves and complete demo replays need recorded playthrough comparisons.
- Inherited assert-only-variable and large controller-string compiler warnings
  remain. New format modules do not remove every unsafe assumption in the
  inherited simulation and archive loaders.

No bug-free or release-ready certification follows from these checks.

## Patch 1.02 reliability and allocation pass (24 October 2026)

Local Release passes 10 suites; UBSan Debug passes 12, including all private
Amiga and cutscene checks. New synthetic tests cover deterministic duplicate
asset lookup, missing directories, a symlink cycle, atomic compressed-save
replacement and failure cleanup, buffered close errors where `/dev/full` is
available, rolling-key PC-98 decoding and every input truncation, malformed
backreferences, short PAQ wrappers/ranges, and ABA tables truncated at every
byte. SDL virtual joystick checks cover hotplug, unrelated instance rejection,
disconnect clearing, reconnect and texture-reset recovery. Enlarged-text menu
captures were inspected: Resume and turquoise EXIT GAME remain visible.

The changes reuse widescreen/blur and bounded T-shirt workspaces, cache keyboard
geometry until bindings change, and upload the main texture only when composed
pixels or texture dimensions change. These are allocation/work reductions; no
hardware speedup or battery-runtime claim has been measured. Original game
simulation and the existing T-shirt/jacket regression remain unchanged.
