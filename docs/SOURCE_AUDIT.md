# REminiscence2 source audit

The selected engine is a useful foundation for preservation. Its platform
boundary is already concentrated in SDL, and it has working abstractions for
resources, video, sound, and game state. The first priority is reproducible
builds and observable behavior, followed by reliable startup and saves.
Replacing the gameplay interpreter before recording its behavior would make
compatibility harder to assess.

This audit describes the source imported on 23 October 2026 and the first
local modernization changes. Later implementation and validation are recorded
in the [0.5.6 integration ledger](UPSTREAM_056.md). Findings marked as risks are code review
findings, rather than demonstrated failures with commercial game data.

## Source lineage

| Source | Pinned revision | Role |
| --- | --- | --- |
| [chermenin/REminiscence](https://github.com/chermenin/REminiscence) | `2200e18c6e5eb710b05c5095a751882591c21cf8`, 9 July 2026 | Imported snapshot; 71 commits retained upstream and in local comparison refs |
| [cyxx/REminiscence](https://github.com/cyxx/REminiscence) | `947a4dce261aacc4f6ec0c2c90cb9d7e37712c7b`, 22 July 2026 | Comparison source, identifies itself as 0.5.6 |
| [Historical homepage](http://cyxdown.free.fr/reminiscence/) | Historical distribution site | Project origin; this run could not retrieve its contents |

The chosen fork's README identifies REinforced 0.5.3, dated 30 August 2024,
and a foundation in original REminiscence 0.4.6. Its Git history also contains
2026 fixes. These version numbers are specific to their lineages; REinforced
0.5.3 and original REminiscence 0.5.3 are not interchangeable releases.

The cyxx repository is archived on GitHub. Its
[changelog](https://github.com/cyxx/REminiscence/blob/947a4dce261aacc4f6ec0c2c90cb9d7e37712c7b/CHANGES.txt)
lists newer Macintosh decoding and cutscene fixes, Sega data support, and MIDI
work including FluidSynth. A comparison against our baseline changes 70 files.
Each relevant fix needs individual review and data-specific testing. A bulk
replacement would also remove fork-specific features and notices.

Locally, `upstream` tracks the chosen fork and `cyxx` holds the comparison
history. REminiscence2's `main` starts with one fresh root commit; the imported
commits are not ancestors of this branch. The source links and pinned revisions
above retain the provenance record. No engine release is implied by the source
snapshot.

## Architecture

| Area | Main source | Responsibilities and implications |
| --- | --- | --- |
| Startup | `main.cpp`, new `config.cpp` | Paths, edition and language detection, command line, boolean settings |
| Simulation | `game.cpp`, `piege.cpp`, `collision.cpp` | Game loop, entity script opcodes, movement and collision; the most sensitive compatibility boundary |
| Resources | `resource.cpp`, `resource_aba.cpp`, `resource_mac.cpp`, `resource_paq.cpp` | DOS, Amiga, Macintosh and PC-98 formats, archives and decoded data |
| Files | `file.cpp`, `fs.cpp`, `unpack.cpp` | Stdio, gzip, memory streams, recursive case-insensitive lookup, decompression |
| Video | `video.cpp`, `graphics.cpp`, `scaler.cpp` | Indexed palettes, sprites, backgrounds, scaling and widescreen borders |
| Cinematics | `cutscene.cpp`, `seq_player.cpp` | Polygonal sequences and DOS CD video; separate timing paths |
| Sound | `mixer.cpp`, `mod_player.cpp`, `sfx_player.cpp`, `ogg_player.cpp`, `cpc_player.cpp` | Music and effects, tracker modules, Vorbis and alternative music sources |
| Platform | `systemstub.h`, `systemstub_sdl.cpp` | Window, renderer, input, audio, clock and dynamic scaler loading |
| Tables | `staticres.cpp`, `staticres_controllers.cpp` | Engine tables, text and controller mappings; keep provenance with changes |

The code uses C++17 in the inherited Makefile but largely follows older C++
ownership patterns: raw buffers, manual allocation, global settings and
mutable structures shared between subsystems. A careful migration can improve
these boundaries without changing every file at once.

`Game::updateTiming` requests a nominal 30 Hz using integer milliseconds.
The CD sequence player uses a separate 25 Hz schedule. Rendering refresh rate
must not become the simulation rate when display support is modernized.

## Confirmed defects addressed in the first pass

| Finding | Evidence in the imported tree | Change and coverage |
| --- | --- | --- |
| Dependency lookup fails on this Apple Silicon setup | `Makefile` obtains SDL flags but hardcodes modplug linkage without its include path | New CMake target discovers each dependency; local arm64 builds succeed |
| Release compilation fails with `NDEBUG` | `util.h` defines `debug` as a macro, which also replaces the definition in `util.cpp` | Undefine that macro in the implementation; Release build and tests pass |
| PC-98 existence lookup recurses indefinitely | `ResourcePaq::hasEntry` calls itself | Call `findEntry`; test known, missing and lowercase names through `ResourceArchive` |
| Configuration depends on executable spelling and truncates paths | `initOptions` formats `argv[0]` plus `.cfg` into 16 bytes | Stable default `fb.cfg`, explicit `--config`, long-path and absolute-launch tests |
| Configuration keys match by prefix | `strncmp` compares only the known key length | Exact key match, whitespace handling, validated booleans and regression coverage |
| Documented `--widescreen=none` selects blur | `parseWidescreen` omits `none` and falls through | Explicit `none`; reject unknown modes |
| Invalid arguments can report success or reach unchecked game state | Unknown options return zero; level uses `atoi` before resource indexing | Explicit help/version, failure status for invalid options, validate levels 0 through 6 |
| Long debug messages can overflow a stack buffer | `debug` uses `vsprintf` into 1024 bytes | Bounded `vsnprintf`; long-message regression case |

`makeDir` now also rejects an existing regular file instead of treating it as a
directory. Full directory creation and platform user paths remain future work.

## Remaining priorities

| Priority | Finding | Next change and evidence required |
| --- | --- | --- |
| High | Save writes go directly to the destination; close errors are not surfaced through `File` | Temporary file, validated flush/close and atomic replacement, with interrupted-write tests |
| High | `loadState` mutates live state while consuming unchecked offsets; `loadGameState` accepts versions newer than its writer | Decode into validated temporary state, reject unsupported versions, test truncated and corrupt synthetic saves |
| High | Memory stream seeks and lengths are unchecked; zero-capacity growth and allocation failure need handling | Harden `MemoryBufferFile` before expanding rewind, with boundary and failure tests |
| High | Several decompressors and archive parsers trust sizes and references; `readByte` returns an uninitialized value after failed reads | Bounded reads and allocation limits, synthetic malformed inputs, then fuzzing under ASan |
| High | `fs.cpp` follows directories recursively without cycle detection; Windows lookup checks a handle for truth rather than `INVALID_HANDLE_VALUE` | Safe traversal, clear errors, symlink and missing-directory fixtures, Windows compilation |
| Medium | `--autosave` is a five-second in-memory snapshot ring, up to 120 entries; it does not survive exit | Preserve rewind and add separate persistent autosave with visible recovery behavior |
| Medium | SDL initialization, window, renderer and texture failures are not consistently checked | Return actionable errors and add an asset-free platform smoke harness |
| Medium | Controllers are opened at startup; hotplug events are not handled and mappings are embedded | Action bindings, hotplug, device identity checks and configurable dead zones |
| Medium | Focus loss pauses the loop but does not explicitly clear held input | Release held actions on focus loss, test focus and reconnect event sequences |
| Medium | Audio requests 4096 frames at 22050 Hz, approximately 186 ms of buffer duration | Measure actual latency, then negotiate device output and buffer size; keep mixer timing stable |
| Medium | Paths depend on the working directory; screenshot and config paths lack a unified policy | Standard writable user paths, explicit portable mode and a game-folder picker |
| Medium | CLI scaler and cheat arguments still use permissive legacy parsing | Centralize parsing and document constraints before adding a launcher |

These priorities do not establish that every malformed file is exploitable.
They identify concrete bounds and state-management work needed for dependable
releases and future features.

## Modernization approach

Keep the existing interpreter and SDL2 interface as the initial behavioral
reference. First capture repeatable input, simulation state and representative
frames with original game data. Fix file and save handling independently of
rendering. Then migrate the platform boundary to SDL3 while comparing those
same runs. Homebrew's SDL2 compatibility library is useful for this first build
but does not establish that the engine has a native SDL3 backend.

Use the DOS edition for the first complete playthrough matrix, then expand to
Amiga, Macintosh and PC-98 with edition-specific fixtures. Retain the fork's
Russian text and optional restored content. Consider newer upstream fixes
individually, retaining source and license attribution.

The next milestone is a validated first launch and safe persistent progress.
Native packaging and broader visual options should build on that foundation.
