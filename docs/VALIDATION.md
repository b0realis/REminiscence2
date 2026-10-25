# REminiscence2 validation

## Patch 1.26 — offline movie upscaling tools

The saved SC1 intro uses Real-CUGAN Pro 3× with denoise 3 and a tracked repair
for the user-confirmed DOCK 25 warning. The reusable exporter/upscaler reproduced
all 691 native and upscaled frame pixel arrays, all 76 repaired frame arrays,
the lossless audio WAV and the timing table exactly against the prototype.
The saved final MP4 is byte-identical to the previously reviewed final movie;
rerendering can change the encoded MP4 bytes. Both SC1 and BRIDGE completed
upscaling and full audio/video decode validation through the new tool.

Five Python tests cover all three MCD codecs with synthetic fixtures, exact RGB
and PCM output, variable frame holds, malformed input, restoration-recipe bounds
and output protection. The movie-upscaling, Sega CD media and CLI CTest suites,
all 11 release-tool tests, and the documentation guard pass. The local exporter
and engine build successfully. Generated movies, model weights and reference
art remain private. Enhanced MP4 playback in the engine is not implemented.

## Patch 1.25 — Sega CD cosmetics and independent movie audit

Real CD assets now exercise Original, Heavy and all five Laser colours through
four standing/crouching and left/right firing poses. Replacement flashes match
the native rendered muzzle coordinates; laser traces retain their launch height.
The seven variants produce identical saved gameplay state over 220 ticks each.
Separate synthetic fixtures check object-bank selection and restoration, while
live shirt changes and 550 decoded poses check the shared-colour undershirt mask.
Original and White retain the CD's native white shirt; Red/Pink changes the
undershirt while preserving the brown jacket and other palette entries.

All 27 CTest suites pass in pinned-SDL Release and Debug/UBSan with the supplied
Amiga, GOG/DOS and Sega CD assets. Gameplay captures were visually inspected.
The local launcher uses the rebuilt 1.25 executable. Public CI checks and all
six release targets are verified against the tagged commit before publication.

An independent libarchive ISO9660 extraction matches all 70 MCD files byte for
byte. Executing the original US SUB.BIN decompression routines in Unicorn's
M68000 emulator matches all 17754 indexed frames (290881536 packed pixel bytes)
against this engine's decoder. The visible checkerboard dithering is in the
original image data. New patterned fixtures check tile placement, nibble order
and BGR333 palette conversion. See [Sega CD](SEGA_CD.md#movie-picture-quality)
for routine offsets and scope. Full playthrough and physical-console A/V timing
checks remain outstanding.

## Patch 1.24 — Sega CD bug pass and release preparation

Focused review fixed movie video racing ahead after focus/background pauses and
WAV readers retaining partially parsed state after a failed reopen. Synthetic
regressions cover a four-second audio/video pause, a subsequent unaffected movie,
and a malformed duplicate WAV format chunk. Disc paths now reject Windows device
names; CUE reads enforce the size bound before loading the entire input.

All 26 suites passed in Release and Debug/UBSan with the supplied Sega CD, Amiga
and DOS assets. The local movie launcher lists all 70 CD movies. The full movie,
level-start, save/load, city-bank and 95-dialogue-segment regressions from 1.23
remain part of this pass. All 11 packaging tests and documentation checks passed.
The Apple Silicon app/ZIP was built, ad-hoc signed, dependency-checked and tested
after relocation. All 386 Amiga/GOG and 171 extracted CD file hashes are unchanged.
Six-target CI artifacts are verified before publication; full playthrough and
hardware limits below remain.

## Patch 1.23 — Sega CD US assets and format reference (local)

The supplied CUE/BIN extracted to a new ignored directory: 163 ISO files, eight
lossless stereo PCM WAV tracks and a SHA256 manifest. The US adapter checks
SUB.BIN length 96400 and CRC32 80cee8d8 before reading embedded data. Original
disc/Amiga/GOG files are not edited. No release is published in this pass.

Private tests decode 17754 indexed movie frames, all sprites in seven level-start
banks, eight music tracks and 95 nonempty dialogue segments. They simulate 100
ticks in each level, round-trip saves, and restore a city interior directly into
the correct graphics/sprite bank with adjacent-room previews. Movie playback
separately honors event lengths (not all trailing indexed frames are presented).
Startup covers combined CD opening/title, skip, immediate settings, edition
switch, latest-save resume and physical/pointer menu input. Title, gameplay and
movie frames were visually inspected from headless captures.

Synthetic tests cover CUE separate/shared BIN and cooked sectors, ISO bounds and
path rejection, output atomicity, audio byte order, three MCD LZSS variants,
split-stream sprites, PCM WAV looping/resampling/source selection and movie hook
lifetime. Format references distinguish implemented subsets from untested variants.
All 26 CTest suites pass in both pinned-SDL Release and Debug/UBSan. All 11
release-tool tests and the documentation-boundary check pass. The 171 extracted
Sega CD files/audio tracks match their manifest, and the 386 original Amiga/GOG
files retain their earlier hashes. The local executable reports version 1.23.

Real Macintosh assets, other Sega CD pressings, complete game playthroughs and
comparison of CD audio/video timing against original hardware remain outstanding.

## Patch 1.22 — original-menu input and mouse skipping (local)

Amiga/DOS title input now uses queued press edges and shared hold/repeat timing.
Regression tests cover brief taps, held controls, mouse wheel/hover/click hit boxes,
and click-to-skip release consumption. The existing 22 CTest suites passed in
Release and Debug/UBSan before the Sega CD extension. This patch is retained in
the current worktree while release publication is on hold.

## Patch 1.21 — new-feature bug pass and release

Focused source review found and fixed stale Ogg/CPC background state after a
missing next-level track, inconsistent soundtrack cue selection in standalone
cutscene playback, a skipped DOS-only PRF cue (25), and Quest/Python launchers
forcing a shared tune root across editions. Scene override fallback now restores
the native cue and tempo. Catalog commands also honor the remembered edition.

All 22 suites pass in local pinned-SDL Release and Debug/UBSan. Added music
regressions cover a missing next-level track followed by a scene, native fallback
from an empty DOS override, and audible DOS cue 25 with Amiga graphics. CLI and
Python tests exercise remembered/explicit library selection without an implicit
shared tune path. All 11 release-tool tests pass. The Apple Silicon app/ZIP was
assembled, ad-hoc signed, dependency-checked and smoke-tested after relocation.
The 386 original Amiga/GOG asset hashes are unchanged. Hardware playthrough
limits from 1.20 still apply; remote target builds are verified before publishing.

## Patch 1.20 — automatic resume, edition library and independent music

Local pinned-SDL Release and Debug/UBSan cover 22 CTest suites. Production
startup tests with the supplied Amiga and GOG data verify normal openings by
default, newest manual-save auto-resume across levels, corrupt-save fallback to
an autosave, empty-save fallback to the openings, and an edition-switch request
unwinding the opening safely. The automatic-resume preference round-trips with
source IDs containing spaces and quotes.

Synthetic libraries test DOS MAP+LEV disambiguation, separate Macintosh/Amiga
markers, sibling Amiga fonts, nested editions, duplicate filenames, ambiguous
shorthand IDs, external Ogg/MOD packs and exclusion of raw RNC-packed WHDLoad
resources. CLI checks cover read-only listing and invalid/ambiguous selections.
The local launcher finds both prepared Amiga copies and the extracted GOG set;
its `--amiga` and `--gog` choices produce their respective cutscene catalogs.

Music tests use original CC0 sine-wave fixtures to verify mono/stereo Ogg,
44.1-to-22.05 kHz pitch preservation, background/cue transitions, native fallback
and source-hook cleanup. The supplied Amiga modules play through a DOS mixer;
the supplied DOS PRF cues also produce nonzero audio after source switching.
Release uses the internal MOD player, while Debug exercises libmodplug.
Edition-picker and Audio captures were visually inspected at desktop and
256×224 resolution; the turquoise exit button remains accessible.

The 386 original Amiga/GOG files in the startup manifest retain their SHA-256
hashes. No actual Genesis/Sega CD soundtrack was supplied, so only the prepared
Ogg-pack path has been tested. Raw ROM/VGM or Sega CD image playback/extraction
is not implemented. Macintosh remains limited to synthetic fixtures; no full
playthrough or physical-controller/device certification is implied.

## Patch 1.19 — opening flow and real GOG data

All 19 suites pass in local pinned-SDL Release and Debug/UBSan. Private startup
integration tests run the production `Game::run()` path with both editions:
complete openings despite saved progress, per-scene skipping, Esc during the
first logo with letter skipping disabled, Esc during the introduction and at
the title, the pause-request shortcut, level/difficulty selection without an
unintended reset, and explicit CLI Continue. Closing title settings returns to
the title. Native-size overlay tests retain the full page navigator there.

The supplied GOG Windows installer `1.0.0 (25550)` was extracted without running
its executable. Its 250 loose files under `Flashback_Data/StreamingAssets/data`
are recognized as DOS resources; the cutscene catalog finds 28 polygon files.
The new optional `REMINISCENCE2_DOS_DATA` test fixture renders 272 room backgrounds
across seven levels. It compares seven 220-tick runs (Original, Heavy and five
laser colours), checking identical serialized gameplay, native flash positions
for standing/crouched shots in both directions, and the original source samples
with the selected audio filter. This exposed uninitialized collision-buffer
padding in saves; newly allocated records now clear that padding.

The title screens and actual GOG firing frames were visually inspected. Private
captures remain in ignored build output. Recorded hashes for Amiga and GOG data
remain unchanged, including all 129 Amiga files from the earlier manifest.
The installer and extracted assets remain ignored and are not release contents.
The local launcher supports `--gog`; the plain invocation keeps the Amiga data.

This is automated engine validation and visual inspection, not a complete GOG
playthrough or physical-device certification. Real Macintosh assets are still
unavailable; their existing synthetic coverage remains the testing limit.


## Patch 1.18 — DOS/Mac shot geometry and five laser colours

Local pinned-SDL Release and Debug/UBSan each pass all 16 CTest suites. Synthetic
DOS SPC and Macintosh C211 fixtures exercise the production native sprite and
replacement-effect paths with different offsets, both directions and two shot
heights. Macintosh fixtures deliberately place the flash on odd native pixels.
The moving laser core is checked against the launch row throughout its visible
lifetime, even when the following flash frame moves vertically. Truncated
geometry produces no replacement and leaves native rendering enabled.

All five palettes are exercised in both formats. Preference tests cover Orange
as default, all five saved colours and rejection of invalid values. Desktop and
256×224 menu captures show the colour selector and retain the turquoise exit
button. Real Amiga captures of Orange, Red, Green, Blue and Purple were visually
inspected; the existing 660-tick simulation comparison still passes.

The capture run also exposed a save-test assumption about catalogue ordering.
The assertion now looks up the saved slot by level and slot number; the full
Amiga fixture passes with a fresh output directory as well as reused fixtures.
All 129 imported asset files retain their SHA-256 hashes. The launcher reports
1.18; documentation and whitespace checks pass.

Actual commercial GOG/DOS and Macintosh assets remain unavailable for visual
play testing. Synthetic coverage verifies format handling and positioning, not
the appearance of every commercial edition. Private game-derived captures remain
in ignored build output.

## Patch 1.17 — native shot positions and edition isolation

All 16 CTest suites pass in local pinned-SDL Release and Debug/UBSan. The real
Amiga tests render Original's flash in isolation and measure its pixels as an
independent reference. Both Heavy and Laser match the native flash's rear edge,
centerline and starting tick in standing/crouched shots facing both directions;
the second flash frame also follows its native position. The three 220-tick runs
continue to serialize identical gameplay state, including save/restore.

Conrad now keeps his native firing animation. Private comparison captures were
visually inspected. Non-Amiga fixtures with saved Heavy/Laser preferences verify
effective Original mode, no effect pixels or palette writes, and no sound
substitution. Those fixtures exercise the edition guard; actual GOG/Macintosh
assets and physical-device playback have not been tested.

SHA-256 manifests before and after the work match for all 129 imported Amiga
files. The local launcher reports 1.17. Documentation boundary and whitespace
checks pass; game-derived captures remain in ignored build output.

## Patch 1.16 — cutscene shortcuts and pistol rendering

Local pinned-SDL Release and Debug/UBSan pass all 16 CTest suites. SDL event tests
cover Space and every A–Z key, remapped bindings, disabled shortcuts, held-key
repeat and release after playback. Real Amiga logo/intro/story scenes exercise
the independent preferences; text, SET and synthetic SEQ fixtures cover the other
playback paths and interruption cleanup.

Three real-input weapon runs total 660 ticks and serialize identically across
Original, Heavy and Laser, including a save/restore between shooting sequences.
Standing/crouched shots in both directions are checked against the original
shot actor's motion. Isolated production sprite rendering verifies that each
new effect starts on the last metal pixel of the barrel, with an empty pixel
immediately beyond it, and includes Heavy's recoil offset.

The missing Original flash was reproduced: dormant actors were omitted from
room lists, so the existing room-transfer code could not insert them on spawn.
Tests now verify every eligible actor belongs to exactly one list, including
off-screen holding rooms, after level load, firing and save restoration. A real
rendered flash pixel guards against recurrence. Seven-level/419-room asset
checks and the 21,000-tick campaign suite also pass. Private before/after captures
were visually inspected; original shot-effect sprites are flashes, not evidence
of a separate casing-ejection animation.

The local launcher reports 1.16. Actual DOS/Macintosh/Sega barrel alignment and
physical-device playback remain unverified; the supplied Amiga assets were used
for visual validation. No game-derived captures are included in public docs.

## Patch 1.14 — deeper laser bass

All 16 CTest suites pass in local Debug/UBSan and pinned-SDL Release. Both bass
profiles pass frequency-balance and full-scale polarity/peak checks at 22050,
44100 and 48000 Hz. Real Amiga firing and the missing-laser fixture select the
deep-bass profile; the 510-tick simulation comparison remains identical.

Private Amiga mixer previews measure about 11.8 dB more laser low-band energy
using a 240 Hz one-pole analysis filter, with no full-scale clipped samples.
Original and Heavy previews are byte-identical to 1.13. Documentation and
whitespace checks pass. This remains a local audio revision; physical-device
listening and other editions' source samples remain unverified.

## Patch 1.13 — pistol audio refinement

Local Debug/UBSan and pinned-SDL Release each pass all 16 CTest suites.
New audio checks verify bass emphasis, reduced treble and bounded full-scale
positive/negative input at 22050, 44100 and 48000 Hz. Existing callback-partition
and dry-voice reset checks still pass. Real Amiga firing selects the original
laser asset; a missing-sample fixture selects the synthesized fallback.

All 510 firing/mirrored/crouching simulation states still match Original byte
for byte. Fresh private mixer previews preserve the Original WAV exactly and
contain no full-scale clipped samples. The documentation boundary and whitespace
checks pass. This patch remains local; physical-device listening and other
editions' actual sound assets remain unverified.

## Patch 1.12 — cosmetic pistol experiments

Local Debug/UBSan and pinned-SDL Release each pass all 16 CTest suites.
The new weapon suite covers default no-op rendering, foreground masking,
offscreen clipping at native/double scale, lifetime expiry, pose cancellation,
audio callback partitioning, dry/bass voice reuse and synthesized laser PCM.
Preference round trips include the selected pistol style.

Three real Amiga runs total 510 ticks of firing, mirrored aim and crouching.
Every serialized simulation state matches the Original run byte for byte.
The tests check the actual bass/laser voice selection and generate private shot
captures; standing and crouching muzzle alignment in both directions was visually
inspected. The existing seven-level/419-room and 21,000-tick checks still pass.

This experiment is local and has not been published. Its renderer supports
the edition scaling paths, but actual DOS/Macintosh/Sega visual alignment and
physical-device listening remain unverified. Preview frames and audio derived
from the player's Amiga files remain in ignored build output.

## Patch 1.11 — feature bug pass and release candidate

Local validation on 24 October 2026:

- All 15 CTest suites pass in Debug/UBSan and pinned-SDL Release, including the
  private seven-level Amiga checks and 21,000 deterministic route ticks.
- All 32 Python asset/release-tool tests pass. The documentation manifest,
  private-file boundary and whitespace checks pass.
- New tests first reproduced lost protected-save explanations and stale global
  controller defaults on hotplug. Both pass after the fixes.
- Production overlay/shortcut checks preserve protected-save failure reasons;
  disconnect pause retains the composed frame; shortcut rewind prepares the
  destination room and collision caches before another simulation tick.
- Hosted pre-release runs exposed a missing standard-library include on Windows
  and a scheduler-sensitive audio-pause test on macOS. The test now compares
  callback progress with measured foreground time and reports failed check IDs.

The release publisher requires successful Build and test and Release packages
workflows for the exact commit before tagging. It validates all six target
packages, the Quest deployment bundle, corresponding source and SHA-256 sums.
Hosted results are attached to the GitHub release; the sections below retain
the historical local validation records from earlier patches.

Physical Deck/Quest/controllers/audio devices, actual GOG/Macintosh CD assets
and complete playthroughs remain unverified. A successful native build or SDL
dummy-device test does not establish those results. Local ASan still cannot
start on this host; the Ubuntu CI configuration supplies ASan/UBSan coverage.

## Patch 1.10 — recovery, input and accessibility

See [the feature and format guide](QOL_1.10.md). This is a local patch; v1.06
remains the published release.

- Debug with UBSan and static-SDL Release: all 15 CTest suites pass.
- The 32 Python asset/release-tool tests pass, including uppercase `.R2S` and
  `.R2M` private-preview rejection. Documentation manifest and diff checks pass.
- 44 production overlay layouts render with the turquoise EXIT GAME visible.
  New desktop/native recovery, manual-browser, notebook and assist captures were
  visually inspected; they use synthetic session data and original color grids.
- New storage tests cover names/protection, checkpoint previews, failed protected
  overwrite, metadata-only updates, truncation, CRC and portable-name limits.
- Real Amiga regressions restore a protected manual milestone across levels,
  refuse its quicksave overwrite and recover through the production death path.
  The existing seven-level/419-room and 21,000-tick deterministic probes still pass.
- Virtual pads cover disconnect pause and input clearing; controller profile
  import preserves keyboard/display/audio preferences. Trigger capture and
  hysteresis, sticky-action release, 100 ms buffer expiry and 50% pacing pass.
- SDL dummy audio removal advances the callback silently, pauses that clock in
  the background, and reconnects at the existing mixer rate. This verifies the
  SDL event path, not physical audio-device or Quest lifecycle behavior.

Local synchronous save timing is reported in Diagnostics. One Debug/UBSan run
observed a worst sample of about 10.2 ms on this Mac; no cross-platform latency
claim or background writer is implied. Physical controllers/audio devices,
Windows/Linux/Deck/Quest runtime checks, actual GOG DOS/Mac CD data and complete
playthroughs still need validation. The host ASan runtime limitation remains.

## Patch 1.09 — four campaigns, 24 October 2026

See [the campaign report](CAMPAIGN_1.09.md) for scope, findings, changes and limits.
Fresh Debug/UBSan and pinned-SDL Release builds pass **15 CTest suites**. All
**32 Python tests** pass using `.local/extract-venv/bin/python` with
`REMINISCENCE2_ENGINE` set to the Release executable. The first system-Python run
failed because it lacked the declared `lhafile` dependency; the configured
extraction environment completed the full suite. The optional RWops adapter
passes a separate syntax compile. The docs boundary check passes.

New coverage includes 40,000 deterministic CPC/foreground mutations, exact save
EOF/checksum and graph validation, owner-specific music teardown, tempo effects,
26-item inventory, inventory-to-QoL access, independent background/focus pause,
and archived BNQ/LEV resource adapters. Two route probes totaling 21,000 ticks
across seven Amiga levels produce the same serialized hash, **3132929345**, in
Debug and Release. They restart around death/story scenes and are not a complete
playthrough. Seven level and three clothing captures are byte-identical to the
pre-campaign 1.08 Release outputs.

Clang analyzed 51 non-table engine/CLI translation units after the changes.
The confirmed foreground, SFX-null and ABA-table warnings are resolved. Remaining
notes are legacy dead stores and an unreproduced possible uninitialized value in
bundled `stb_vorbis.c`; this is not claimed as a confirmed bug or a clean static
analysis result. The previously recorded ASan host startup limitation remains;
physical-device, actual DOS/Mac/CD-music, and cross-platform runtime validation
remain pending. This is a local patch; no new release was published.

## Patch 1.08 — 24 October 2026

Fresh Debug/UBSan and pinned-SDL Release builds pass **14 CTest suites**. The
release-tool Python suite passes 11 tests. Documentation/private-asset checks
pass. This patch remains local; the public release is still v1.06.

The new progress suite verifies bounded envelope parsing, exact state/summary
agreement, CRC corruption and truncation rejection, three-generation rotation,
manual-save isolation, failed replacement, symlink rejection, profile round trips,
remapped hint labels, suppression, expiry, cooldown and rewind-time resets.

Private Amiga integration checks interval boundaries and pause/disabled timers,
preview capture, restoring an older rewind point while retaining earlier history,
incompatible-asset fallback, recovery in a fresh Game/Resource, all seven levels,
and both production startup flows: direct Continue and welcome-back Continue.
Fresh-process recovery exposed a derived collision cache that was not initialized;
restored-frame presentation now clears that cache before the next simulation tick.

SDL checks dispatch Continue, selected autosave and rewind through the production
paused command loop. Software rendering captures 34 overlay layouts, including
new desktop/native recovery, autosave and rewind views; it checks native welcome
buttons and recreates the rewind preview after a renderer reset. Public new
captures use synthetic session data and an original color-grid preview; actual
player frames stay in ignored build output.

All platform binaries still require CI/release packaging for a new published
release. Physical gamepads/Deck/Quest, actual GOG DOS/Macintosh CD data and a full
playthrough remain unverified. The host AddressSanitizer limitation recorded
below is unchanged; it is not counted as passing validation.

## Patch 1.07 — 24 October 2026

Fresh builds pass all **13 CTest suites** in both `build/debug` (UBSan with halt
on error; optional synthesis backends) and `build/package-arm64` (Release,
pinned static SDL/zlib, internal MOD, private Amiga data). Release-tool Python
checks pass all 11 tests; the documentation/private-assets boundary check passes.

New coverage includes independent keyboard/D-pad/stick input, Shift aliases,
joypad shortcuts, dead-zone hysteresis, raw and mapped SDL virtual-device hotplug,
joypad binding capture and persisted profiles, OGG callback partitioning, the
public internal-MOD fallback with an unspecified tempo, bounded object/SEQ
truncations and signed runs, a complete synthetic SEQ movie and corrupt fallback,
SEQ source-rate duration at 8/22.05/44.1/48/192 kHz, and exact fractional 30 Hz
pacing. Private Amiga checks verify rejected data-fingerprint mismatches, restored
random/journal state, and memory-journal unlock only after the scene completes.
Rendered captures cover the new pages at desktop and native resolution.

AddressSanitizer cannot currently start on this host: even a standalone sanitized
hello-world stalls before `main`. That run was stopped; it is not recorded as a
pass. UBSan and the ordinary Release checks above completed successfully.
Physical controller/Deck/Quest checks, real GOG/Macintosh assets and a complete
playthrough remain outstanding. New SEQ decoding/audio is covered synthetically;
original CD movie validation still needs the player's data.

The following sections retain earlier validation records.

---

## Earlier validation record

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
