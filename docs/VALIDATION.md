# REminiscence2 validation

Full story demos and their walkthrough sources are private. Historical results
below include those local checks; a public checkout runs the editor training and
engine regressions without either authored story campaign.

Before publishing 1.51, a fresh export of the staged public tree built successfully
and passed **26/26 asset-free CTest checks** in **16.72 seconds**. Packaging-tool
checks passed **17/17**. Neither story demo, its generator, walkthrough source nor
its screenshot was present in the exported tree. The public Ogg regression uses
an independent synthetic tone; optional private walkthroughs remain available
locally. Documentation boundaries and staged whitespace checks passed.

## Patch 1.51 — working ASan retry and memory fixes

The Apple Clang startup failure recorded below is resolved with a matched
**LLVM 22.1.8 compiler/runtime**, tested on macOS 26.5.2 arm64. A minimal program
reaches `main`, and an intentional heap-use-after-free probe correctly fails with
an ASan report and source location. The engine build uses `-O1 -g` with assertions,
AddressSanitizer and UndefinedBehaviorSanitizer, plus leak and stack-use-after-return
detection. See the reproducible command in [Development](../README.md#development).

The first complete ASan run found issues missed by the previous UBSan/release runs:

| Finding | Fix and regression |
| --- | --- |
| Hit-response scans read one object past the final native rule. | Check the index before reading the rule type; reject exhausted starting indices. A synthetic exact-size table checks misses, final-rule hits, state boundaries and empty tables. All three asset editions' editor cases exposed the original fault. |
| Amiga level changes replaced SPC sprite tables without freeing them. | Release on replacement for loose files, archives and Macintosh level resources. Synthetic reloads verify contents, archive replacement and DOS global-table survival; Amiga integration checks exercise saves and level changes. |
| Native audit probes repeatedly initialized globals on the same Game. | Separate one-time initialization from simulation resets. Retain fonts, shared sprites, icon banks and DOS samples; stop playback before level resources change. Check global ownership during repeated seven-chapter probes. |

The full combined ASan/UBSan suite passed **63/63**, **198.70 seconds**, with leak
and stack-use-after-return detection enabled and **no sanitizer diagnostics**.
The six focused ASan/UBSan/LSan regressions also passed in **5.99 seconds**. The
rebuilt local Apple Silicon Release suite passed **63/63**, **55.66 seconds**, with
normal controller discovery enabled. Native probes compare **34,127 ticks across
21 chapter/edition cases**. Both suites include the complete authored starship and
relay walkthroughs, native campaign/editor workflows, saves, combat, presentation,
input/audio, decoders and tooling. The local player reports **1.51** via `play-local.sh`.

For the headless sanitizer run only, set `SDL_JOYSTICK_MFI=0`,
`SDL_JOYSTICK_IOKIT=0` and `SDL_JOYSTICK_HIDAPI=0`. A standalone SDL init/quit probe
reproduced the Apple GameController/Objective-C worker allocations without any
engine code; disabling physical-device discovery removed those reports. Virtual
controller hotplug/input checks still execute. **No leak suppressions or disabled
leak checks are used.** These environment settings do not change shipped defaults.

Logs and minimal probes: `build/audit-1.50/asan-retry/`; sanitizer build:
`build/asan-llvm22/`. Automated tests use private Amiga, DOS/GOG and Sega CD assets
and dummy video/audio. Macintosh assets, physical controllers/speakers and other
operating systems have not been exercised in this retry. The Macintosh replacement
path was reviewed; its real asset integration remains untested.

## Patch 1.50 — gameplay bug hunt, recovery and pacing

Local Apple Silicon Release build: **63/63 CTest checks passed**, 117.09 seconds.
Full Debug UndefinedBehaviorSanitizer build: **63/63 passed**, 977.69 seconds,
with `halt_on_error=1` and no undefined-behavior diagnostics.
The engine reports version **1.50** and is available through `play-local.sh`.

Findings and fixes:

| Finding | Correction and regression coverage |
| --- | --- |
| Disabling a closed door hid its artwork but retained collision. | Keep the panel visible; compare rendered enabled/disabled closed gates. |
| Native animation strides could cross a 16-pixel authored door. | Sweep actor cell centers before graph feedback. Check walking/running from both directions, room edges, open gates and clear jumps. |
| Restarting a standalone checkpoint reset puzzle state and quest items. | Capture the full scene and local quest values; kill/restart downstream of an opened gate and verify recovery. Clamp fallback anchors at room edges. |
| Contradictory saved dialogue pages/requests could leave an invalid page index. | Validate queue ownership, generation and active state before restoring. Check rejection without changing the world, strict quest types/keys and older snapshot compatibility. |
| Custom playback resumed almost immediately after controller disconnect. | Freeze actors and node timers until a fresh confirm press; test held input, resume and subsequent switch use. Show the pause prompt even during dialogue. |
| Custom playback accumulated scheduler overshoot. | Share the native timing accumulator. Verify 300 ticks, 50% speed, deliberate oversleep, clock wrap and long-stall recovery. |
| Timed story screens could finish while the player was away. | Honor focus pause and subtract paused time from effects/advance timers. Exercise SDL background-time accounting. |
| Holding confirm could repeatedly retry a failed death recovery. | Require a fresh press for each retry. |

The initial door guard disturbed ledge alignment by snapping native pose coordinates.
The full starship walkthrough caught this; the final guard retains the preceding
horizontal pose and lets native scripts handle the approach. All six raised
platforms remain climbable from both sides, with backtracking and descent checked.

Coverage spans original Amiga, DOS/GOG and Sega CD data, native campaign editing,
both authored campaign walkthroughs, graph/state-machine execution, media nodes,
real-process campaign routing, editor GUI workflows, saves, input/audio, scalers,
combat, startup, decoder/fuzz fixtures and extraction tools. The expanded native
probes compare complete actor state with and without authoring hooks for **34,127
ticks across 21 chapter/edition cases**, using walking, running, jumping and pistol
inputs. These are deterministic probes, not solutions to all original puzzles.

At the time of this pass, AddressSanitizer could not start on this host (resolved
in the 1.51 retry above): Apple's ASan runtime deadlocked
during allocator/shadow-memory initialization before `main`. A process sample
records that failure; it is not an ASan pass. GUI/player automation used SDL dummy
video/audio and software rendering. Physical controller/speaker testing,
Macintosh game assets and other operating systems were not exercised here.

Logs: `build/audit-1.50/`; final walkthrough captures:
`build/package-arm64/starship-{amiga,dos,segacd}/`. Original game data and generated
captures remain private. Documentation boundaries and whitespace checks pass.

## Patch 1.49 — restrained scenery and physical room variety

- Local Apple Silicon release build: **63/63 CTest checks passed**, 122.42 seconds.
  After expanding the door checks to walking, running and jump attempts, the
  final three edition-specific starship walkthroughs passed again (3.60 seconds).
- Focused UndefinedBehaviorSanitizer content/starship/campaign suites: **7/7
  passed**, 74.67 seconds. The final expanded starship checks passed **3/3** again,
  56.92 seconds, with no sanitizer diagnostics.
- All twenty rooms are traversed with original Amiga, DOS/GOG and Sega CD actor
  scripts. New checks climb and descend both edges of all six machinery platforms,
  operate raised pickups/controls, enforce distinct pressure/coupler inputs and
  verify closed bulkheads from ground and platform approaches.
- A one-cell shutter initially failed the movement test. The demo now uses
  matching 32-pixel art/collision bounds, and closed-door movement checks pass.
  This is an authored geometry correction; native movement rules are unchanged.
- Visual review covers native gameplay frames, the assembled twenty-room map,
  source atlas and the actual editor workspace. Floor crops were corrected to
  the redraw's source edges. The new private project/campaign validates and
  opens in the editor under SDL dummy video/audio with software rendering.
- All twenty generated project/art/audio files match between the checked-in
  example and fresh private copy. Previous private copies remain intact.
  Launcher syntax, documentation boundaries and whitespace checks pass.
- This is automated input/rendering validation, not a hands-on controller or
  speaker session. Screen-link reachability notes remain expected for decks
  connected through teleport/lift nodes; the full traversal proves those routes.

Logs and original-art study exports: `build/editor-study/style-1.49/`.
Final gameplay captures: `build/package-arm64/starship-{amiga,dos,segacd}/`.
The private demo style study records the observations and measurements; its
private artwork notes contain the built-in imagegen prompts and source hash. Original reference
images and game-character captures remain private.

## Patch 1.48 — connected twenty-room starship

- Local release build: **63/63 CTest checks passed**, 121.88 seconds.
- Focused UndefinedBehaviorSanitizer build: **7/7 passed**, 51.24 seconds; no
  sanitizer diagnostics. Covers content/rendering, all three full starship
  walkthroughs and real campaign-player routing on Amiga, DOS/GOG and Sega CD.
- Walkthroughs use original movement/input and visit every room. Check automatic
  release, reciprocal art/exits, return lifts, distinct two-switch requirement,
  cell/manifest interlocks, timer save/restore, boarding/disembarking/reboarding,
  cockpit platform height, flight-key requirement and unread dialogue before launch.
- Rendering checks preserve DOS glyph shadows, fade notices completely, validate
  motion/opacity bounds and confirm the exact final cinematic frame is black.
- The actual SDL player ran the full timed launch → recapture → ending sequence
  and wrote a finished campaign save. Dummy video/audio and software rendering
  were used; this is not a hands-on desktop/controller or speaker test.
- Regeneration reproduces project, campaign, background, effect and audio bytes;
  existing output folders are refused. Private demo validation, launch-script
  syntax/version checks and public documentation boundary checks pass.
- Visual review caught the native 72-pixel standing-platform constraint. The
  shuttle artwork was edited with imagegen, then cabin collision and anchors
  aligned to y=144 so Conrad stands inside, rather than below, the shuttle.

Logs: `build/editor-study/release-1.48.log`, `ubsan-1.48.log`, and
`starship-finale-1.48/player.log`. Actual room captures, their reassembled map,
checkpoint and ending captures are in `build/package-arm64/starship-{amiga,dos,segacd}`.
Generated source scenery and the final prompt set are in
`examples/editor/alien-starship/ARTWORK.md`; game-character captures stay private.

## Patch 1.47 — media nodes and After the Drift

Apple Silicon Release build: **63/63 CTest checks passed**. Editor UBSan suites:
**28/28 passed**, with **3/3** final media/UI checks after inspector polish,
and no undefined-behavior diagnostics. Coverage includes:

- Real-input traversal of all four starship rooms with Amiga, DOS/GOG and Sega CD
  assets: containment counter, sentinel override, living sigil, reactor hand-in,
  mid-mission save/restore, hangar checkpoint and escape.
- WAV mono/stereo decoding and Ogg Vorbis decoding; bad/truncated WAV rejection;
  imported duration checks; audio dependency export/import; loop, completion and
  stop behavior; independent music/effects/master/mute gains; resume seeking and
  callback detachment before freeing samples.
- Real editor widgets: import an audio file through the picker, Listen, Stop
  listening, save/reload the node, and open an animated sprite-strip preview.
- Regenerated starship project validation and original campaign/editor regressions.

Logs are under `build/editor-study/`: `release-final-1.47.log`, `ubsan-1.47.log`,
`media-final-ui-1.47.log` and `ubsan-final-ui-1.47.log`. Gameplay PNGs are under `build/package-arm64/starship-*`;
UI captures are in `build/editor-study/media-final-ui-1.47/`.

Rendering/input tests used SDL dummy video/audio and software rendering. These
checks do not substitute for a hands-on speaker/controller session. The generated
background atlas is retained unmodified with its prompt and provenance in
`examples/editor/alien-starship/ARTWORK.md`; character assets remain private.

## Patch 1.46 — persistent campaigns and Relay Station

Local Apple Silicon Release build: **59/59 CTest checks passed**. The complete
editor UBSan run passed **24/24**, followed by **13/13** targeted checks after
resource-aware save validation was tightened. Final scene/checkpoint regressions
also passed **7/7** under UBSan. No undefined-behavior diagnostics were reported.

Coverage includes:

- Campaign save/resume and ending resume through the actual SDL player, including
  original movie/audio playback with Amiga, DOS/GOG and Sega CD assets.
- Scene snapshots containing timers, multi-page dialogue, NPC/computer notices,
  door state and queued native messages; resumption after destroying/recreating
  the scene; all seven native levels and unique-template inventory transfer.
- Truncated documents, missing fields, wrong asset/content identity, invalid
  numbers/keys/links, invalid cinematic/text/icon IDs, failed-save preservation
  and rejection without modifying live state.
- Return-visit player script initialization, standalone flag reset, health above
  the normal cap, immediate campaign checkpoint requests and photo opacity.
- **Relay Station** actual-input walkthroughs on all three editions: movement,
  two-cycle airlock, carried/consumed coil, friendly crawler, mid-dialogue disk
  resume, reactor state machine, stateful return visit and final departure.
- Real editor-widget regression using the new demo; a world-view capture is in
  `docs/screenshots/editor-relay-station-1.46.png`. The private example
  contains only original artwork and text. A private editable copy is prepared
  under `.local/editor-projects/RelayStation`.

Build/test logs: `build/editor-study/final-1.46.log`, `ubsan-1.46.log`,
`ubsan-final-1.46.log` and `ubsan-scene-final-1.46.log`. Actual gameplay frames
are generated by the playthrough tests under `build/package-arm64/relay-*`.
The desktop was locked: tests used SDL dummy video/audio and the software renderer.
No hands-on controller/GPU session or Macintosh-asset test was performed.

## Patch 1.44 — modular native campaign

Local Release build: **53/53 CTest tests passed**. Debug UBSan editor suites:
**18/18 passed**, with no `runtime error` or UndefinedBehaviorSanitizer diagnostics.
The original native-test timeout was extended for the exhaustive bundle audit;
unchanged source-table validation now avoids rebuilding identical operand sets.

Coverage includes:

- All seven levels in each installed Amiga, DOS/GOG and Sega CD edition: 1,097
  original actors per edition, complete ownership coverage, source-template and
  alias serialization, initial-state equality and 45 simulated frames compared
  against an unmodified native game.
- Every copyable fixed-dependency bundle: internal/literal reference relocation,
  source provenance, unchanged original instances, private rule ownership and
  rejected actor-capacity overflow without partial edits. New Washington's
  256-actor table is retained and rejects added copies.
- Representative copied bundles in the actual native runtime, 60 simulated
  frames, full InitPGE template preservation, original-versus-private enemy
  sprite-bank selection, actor preview restoration, legacy project previews,
  live monitor revision matching and delivered native messages.
- Public, newly authored synthetic scripts test actor versus scalar operands,
  literal actor ports, room remapping, offsets, mixed-use rejection, provenance,
  inventory self-reference rejection and copied authored Actor bindings. These
  regressions need no original game data in public CI.
- Actual ImGui widgets open native connections and the placement dialog, copy a
  complete puzzle, and undo/redo it. Actual standalone player processes exercise
  pause, one-frame step, stable paused state, apply/reject/restart/resume/stop.
- Seven-level CLI import produces a complete private campaign; a second import
  into the same destination is rejected and all file hashes remain unchanged.
  `./edit-local.sh --campaign --validate` reopens the prepared project.

Evidence is local under `build/editor-study/`: `final-1.44.log`,
`ubsan-final-1.44.log`, and `ui-1.44-final/` actual-widget captures. Native captures
and generated campaign JSON remain private. Original assets were not modified.
The desktop was locked: UI verification used SDL software/dummy rendering, not a
physical GPU/controller playthrough. The short deterministic simulations do not
claim completion of every story branch. Macintosh remains untested without assets.

## Patch 1.43 — state machines, actors and native event editing

Built the local Apple Silicon game and editor. Release checks passed 53/53;
editor regressions passed 18/18 under UndefinedBehaviorSanitizer. Actual SDL
widget tests exercise Start, single-wire Step, state exclusivity, dialogue
activation and the native rule Enabled control. Captures use SDL dummy/software
rendering; this is not a physical mouse/GPU play session.

New model/scene tests cover exclusive state entry/exit, active-state progress,
matching stepped and automatic evaluation, bounded feedback loops and trace
storage, actor control inputs, patrol bounds, friendly conversation and single
positive-to-dead event emission. They run with Amiga, DOS/GOG and Sega CD assets.
Native tests retain initialization and 45-tick simulation parity with unmodified
campaigns for all seven levels in each edition, then check edited/disabled native
rules, unsafe transitions, legacy state validation, authored dialogue pausing,
native message delivery, overlay composition and live graph revision/state data.
This is a regression set, not a complete campaign playthrough.

The review also fixed repeated native-table export growth using bounded aliases,
end-of-table rule iteration, mixed legacy/node actor pool overflow protection,
file-size checks before atomic saves and stale live graph snapshots after edits.
Native bindings preserve stable actor IDs and opaque dependencies. Automatic
native puzzle cloning, extension-state savegames and Macintosh validation remain
outside this patch. Source assets and native captures stay under ignored paths.

## Patch 1.42 — separate workflows and dialogue/action modules

Built the local game and editor on Apple Silicon. The final Release suite passed
53/53; all 18 editor suites passed under UndefinedBehaviorSanitizer. Actual-widget
regressions run through SDL's dummy/software renderer, with inspected captures.
The locked desktop prevented a physical mouse/keyboard/GPU session.

New model checks cover deferred dialogue completion, choice branches, held-input
edges, ordered conversations, Stop/Disable cancellation, stale activation
callbacks, one-shot Reset, finite Started/Stopped cycles, animation duration and
looping, timer restart deadlines, whitespace-only dialogue, portable owned images,
invalid dialogue/timer fields and path traversal. Scene regressions repeat with
Amiga, DOS/GOG and Sega CD resources, including asynchronous door collision and
legacy module chains. Native campaign tests still exercise all seven levels.

Actual UI checks click each workflow, create a named module, add a Dialogue node,
save it to the local library and place a second independent instance. They verify
that Level Editing cannot delete or drag the placement selection. Existing graph,
clipboard, validation, live-session and start-preference tests remain enabled.

The public screenshot contains only custom node/UI content and newly written
sample dialogue. The updated guide covers schema-4 projects, schema-3 templates,
node ports, animation timing, cancellation, library ownership and limits. Native
campaign event graph conversion remains unfinished. Documentation boundary and
Git whitespace checks passed. No release published by this patch.

## Patch 1.41 — GUI campaign settings and puzzle ownership

Built the game/editor on Apple Silicon. The full Release suite passed 53/53 and
all 18 editor suites passed under UndefinedBehaviorSanitizer. After the final
settings-modal opacity/close-priority adjustment, the actual-widget smoke run
was repeated and its SDL software capture inspected. This does not claim a
physical mouse/keyboard/GPU session on the locked desktop.

Actual ImGui input checks the Settings button, both preference controls, atomic
sidecar persistence/reopening, F5 obeying authored-start mode, Ctrl+F5 forcing
current-room mode, blocked F5/file drops behind the modal, native campaign
creation through Settings and its folder picker, all seven sibling files, and
preventing hidden actor-marker dragging. Model checks reject oversized/deep,
wrong-schema and non-boolean preferences and verify recovery by a valid write.
Existing native script/state and process regressions still pass for all three
available editions. Source assets remain read only.

Reviewed `InitPGE`/`Object`, `Game::pge_execute`, native palette/inspector and
session status. Original actor records and scripts are retained, but there is no
native per-event overlay, event trace or editable native graph bridge. The UI
and coverage audit state those limits. The whole-puzzle module ownership rule
is documented for native conversion; custom connected graph modules already
exist. Native puzzle conversion was not implemented by this settings patch.

The public Settings capture contains only authored Workshop graph/UI content.
Native Settings captures remain in ignored build folders. Preferences are in an
ignored project-folder sidecar, with no machine asset paths. No release published.

## Patch 1.40 — playtest the current room

Built the game and editor locally on Apple Silicon. The full Release suite passed
53/53; all 18 editor suites passed under UndefinedBehaviorSanitizer. After the
final TEST-label placement adjustment, the actual-widget smoke run was repeated
and its SDL capture inspected. These are software/dummy renderer and simulation
checks, not a complete human campaign playthrough or a physical-input/GPU test.

For each of all seven levels in Amiga, DOS/GOG and Sega CD, started directly in a
mapped room other than the authored start. Checked exact initial position, native
standing state, skipped arrival cinematic, preserved actor counts/behavior IDs,
pistol inventory dependency, unchanged project JSON and repeatable fresh state
after ticking and restarting. Invalid positions reject without changing state.
The pass found and fixed random-seed carryover between repeated tests.

Actual editor input covers Place TEST marker, its snapped position, F5, graph
PLAY HERE in a node's different room, Shift+F5, and returning to unchanged room
selection/document state. Model checks cover supported automatic placement and
closed-door obstruction. Separate native processes test initial play-here launch,
later room/level changes, authored-start playback, pause/apply/resume/stop and
oversized, fractional and negative command coordinates. Oversized JSON integers
are rejected before conversion, avoiding truncation into valid coordinates.

The new editor screenshot contains only authored Workshop artwork and UI. Source
assets are read only. Native tests preserve original script dependencies but do
not synthesize earlier quest progress; testing that state uses controlled live
apply. Macintosh editor assets and full manual campaign validation remain outside
this check. No release was published.

## Patch 1.39 — native campaign live editing and editor QoL

Built the editor and game locally on Apple Silicon. The full Release suite passed
53/53. The 18 editor suites passed under UndefinedBehaviorSanitizer; after the
final paused-render/cinematic fix, the six native model/process suites were run
again under UBSan. Editor and CLI checks were also rerun on the final Release
build. These are automated SDL software/dummy and resource/simulation tests,
not a complete human campaign playthrough or physical GPU/controller test.

For Amiga, DOS/GOG and Sega CD, exported/reloaded all seven native levels and
retained actor counts 107, 256, 176, 190, 90, 109 and 169. Compared initial saved
state and 45 simulation frames per level against the normal loader. Verified
unchanged script execution, rejection of dependency mutations and live actor
relocation, preservation of unrelated runtime puzzle collision and actor/link
state, rejection of conflicting CT edits, and explicit edited-level restart.
Authored art retains native HUD pixels; pausing/resuming clears the compositor
before a possible cinematic. Full normal campaign playthrough remains a manual
validation task, and Macintosh editor assets remain unsupported.

Actual ImGui input tests cover multi-selection, keyboard copy/paste, locked
layer deletion/dragging, transformed graph dragging, pan/zoom/frame-all,
clickable node validation and the full-campaign import folder picker. Model
tests cover clipboard ID/link remapping, relative group offsets, cross-project
art relocation, atomic failed paste and image diagnostic locations. Separate
native player processes exercise pause, apply, rejection, level restart, resume
and stop with paths containing spaces. Original files remain read only; process
fixtures and saves live in temporary/private folders.

Inspected actual SDL captures of graph navigation and the native editor. Public
editor screenshots contain only project-authored UI/example material. Native
campaign captures stay in ignored build folders. No GitHub release or target
publication is part of this patch.

## Patch 1.38 — editor guidelines, QoL and original-level study

Built the native editor and game on Apple Silicon. The full configured Release
suite passed 47/47; all 12 editor suites passed under UndefinedBehaviorSanitizer.
The native fixture tests also decode rooms in reverse browsing order and compare
both scenery and actor pixels, to check palette/cache independence.

For the supplied Amiga, DOS/GOG and Sega CD files, decoded every available
level-one map (38 / 26 / 38) and all 107 initial actor records per edition.
Exported private scenery studies, compared all raw collision cells and valid
room links, verified lossless exported pixels and PNG alpha, rejected attempts
to overwrite an existing output directory, then edited/reloaded collision and
ran a native-player playtest in the starting room. Source assets are read only.
No Macintosh reference import or full native-script conversion is claimed.

Actual ImGui input regressions exercise palette search, Fit / 1x zoom, the
Guidelines button and F1/Escape, graph-selection preservation through undo/redo,
project-drop dispatch and modal drop rejection, Original L1 and the scenery-copy
folder picker. They also verify F1 pauses a playtest and Escape closes only the
guidelines. SDL software/dummy captures were inspected; this is not a physical
mouse or GPU test of the locked desktop.

The guide screenshot contains newly authored Module Lab artwork only. Original
reference screenshots and decoded studies stay in ignored build/private folders.
No release publication or packaging changes are part of this patch.

## Patch 1.37 — editor audit and visual module graphs

Built both local Apple Silicon binaries. The configured full CTest suite passes
47/47, including 12 editor/model/widget/example cases. The 12 editor cases also
pass under UndefinedBehaviorSanitizer. Tests use the prepared private Amiga,
DOS/GOG and Sega CD editions; no Macintosh data or other OS editor build was
validated in this patch.

The audit fixes failed prefab imports consuming project IDs, selection clearing
redo history, thumbnail foreground ordering and missing activators, ambiguous
room widget IDs for duplicate names, overlapping world positions, and unsafe
closing over native actors. Graph regressions cover independent instances,
scoped IDs, missing item requirements, invalid ports/paths, bounded cycles,
distinct mission objectives, item consumption, animation bounds/hold-last-frame,
prefab artwork copying, legacy-chain compatibility and cross-room doors. A new
item cannot reuse the ID of a deleted but still-required item. Mission completion
text is kept separate from a pickup notice when collection triggers completion.

Native playtests exercise real Enter edges, held-key suppression, item pickup,
NPC hand-in, paginated dialogue pausing, terminal acceptance/completion and door
collision updates. Actual ImGui widgets connect event ports, drag graph headers,
save/reopen connections and layout, and preserve redo when selecting. Module Lab
runs the same UI/play lifecycle with all three editions and its own sprite sheets.
The graph workspace was rendered and visually checked using SDL's dummy driver;
this does not claim physical mouse/gamepad testing on the locked desktop.

The palette audit and current limitations are in [EDITOR_MODULES.md](EDITOR_MODULES.md).
The full original campaign catalog is not imported. Public examples and the new
graph screenshot contain newly authored artwork only. No game assets are added.

## Patch 1.36 — visual custom-level editor

Built the native editor and game locally on Apple Silicon. The configured
43-test CTest suite passes, including original campaign regressions and eight
new editor tests. Those eight also pass in a Debug UndefinedBehaviorSanitizer
build. The editor's source-image slicing, independent collision/ledge overlays,
project/prefab round trips, malformed numeric bounds, horizontal and vertical
room transitions, original crouched gun hits, door activation/dependency
isolation, cyclic links and foreground occlusion were exercised with private
Amiga, DOS/GOG and Sega CD files. Dense 64-crawler scenes and an over-capacity
sprite-queue regression check the expanded, bounded drawing buffers.

Real-widget tests use the native editor with SDL dummy video/audio: switch
room/world views, place a paired module at the clicked floor, undo/redo, select
a tall panel, save, start play, move Conrad and stop without changing the edited
document. Ten additional Amiga UI/audio lifecycle runs passed after correcting
the new audio adapter's byte-to-stereo-frame conversion. A guarded audio buffer
regression checks the boundary deterministically. Resource teardown stops audio
before freeing the scene's samples.

The native workspace, world overview and embedded playtest were rendered and
visually inspected. The development Mac's desktop was locked; this validates
the actual SDL renderer and injected widget input, not physical mouse/gamepad
use on an unlocked desktop. ASan could not run: the installed Apple runtime
deadlocked in its own allocator initialization before `main`. That attempt was
stopped; it is not counted as an ASan pass. No Macintosh game data or other OS
editor binaries were tested in this patch.

The public Workshop has newly authored art. No original assets were added.
Vendored JSON/stb headers and licenses match their recorded SHA-256 hashes.
Full original campaign import, every legacy object prefab, custom saves and
normal QoL/inventory integration remain outside this initial editor profile.

## Patch 1.33 — supplied Mac app icon

The repository JPEG matches the supplied attachment's SHA-256. The private Mac
builder converts it without cropping or redrawing into all ten standard icon
representations (16–1024px); extracting the `.icns` verifies each image's size.
The 256px result was visually inspected. Bundle metadata names the correct icon,
and the icon hash matches the private manifest. All 14 packaging tests pass.

The rebuilt private arm64 DMG is 146,856,305 bytes. The mounted app passes strict
signature verification, contains the same icon bytes and detects all three
editions and their music sources. The disk image verifies and was unmounted
after checking. Finder/Dock cache appearance was not visually inspected on the
locked build Mac. No game-resource or gameplay changes are part of this patch.

## Patch 1.32 — compact private Apple Silicon app

Fourteen release-tool tests pass, including native launcher execution from a
relocated app path with spaces, both bundled and adjacent asset layouts, packed
Amiga rejection and source-preserving file selection. Seven focused Release
CTest suites pass across asset discovery, music sources, three-edition startup
and the CLI. The new native-Ogg discovery regression also passes in Debug/UBSan:
converting CD WAV tracks must not change the edition ID or hide sibling movies,
SUB.BIN and fonts. Separate external soundtrack boundaries remain covered.

Built a private arm64-only app with system-library dependencies, ad-hoc signing,
transparent filesystem compression and an LZMA disk image. Each converted CD
music track decodes with the original sample count/rate/channels. The disk image
verifies, mounts read-only, and the app copied out of it passes strict signature
verification and all 551 asset SHA-256 checks. Its launcher detects exactly the
three expected game editions and their native soundtrack sources after relocation.

The relocated assets pass Amiga and DOS integration checks, startup for all
three editions, and the Sega CD audit of 17,754 decoded movie frames, eight
music tracks, seven level starts and 95 nonempty dialogue segments. The final
image is 144,910,751 bytes; the relocated app occupies about 167 MB allocated
on the test APFS filesystem (280 MB logical). Other copy tools/filesystems may
expand transparent compression. Finder launch started the correct bundled
engine, but visual desktop inspection was unavailable because the build Mac
was locked. No physical M5, notarization or public release validation is claimed.

## Patch 1.31 — presentation presets and consistent scenery filtering

All three presets are activated through the production overlay at desktop and
256×224 sizes. Their settings survive save/load; individual adjustments produce
the Custom label, and unrelated preferences remain intact. Both final layouts
were rendered and visually inspected, including the active preset highlight.

The new `presentation` regression uses the real SDL platform with dummy video
and software rendering. Synthetic scenery exercises Point, Linear, ScaleNx and
xBRZ across adjacent, blurred-adjacent, mirror, blurred-background and CD-i modes
on 256×224 and 512×448 canvases. Checks cover comparison with independently
filtered reference pixels, retained scenery when switching filters, fresh-upload
equivalence, unchanged-frame caching, renderer reset and clearing. All three
presets also render at 1920×1080 and 3840×2160, retain the chosen output size and
check Classic's largest whole-pixel fit. These are simulated output checks;
physical Full HD/4K displays and Macintosh assets were not tested.

All 32 CTest suites pass in both Release and Debug/UBSan, with the supplied
Amiga, GOG/DOS and Sega CD fixtures enabled. The local `play-local.sh` launcher
runs the rebuilt 1.31 binary, and the documentation guard passes. No new remote
release-target builds or publication were performed for this patch.

## Patch 1.30 — release audit

Reviewed the 1.27–1.29 changes across scaler strides, live display changes,
settings persistence, demo launch/exit, save protection and packaging. Hardened
DOS/PC-98 demo reads: bound size before signed conversion/allocation and expose
no input length until a complete read succeeds. Public format tests cover valid
input bits, missing/empty/oversized files and recovery; the private GOG regression
checks that a rejected oversized replacement retains the prior replay/session.

Local Release and Debug/UBSan builds each pass all 31 CTest suites, using supplied
Amiga, GOG/DOS and Sega CD fixtures. All 11 release-tool tests pass. The local
`play-local.sh` launcher reports 1.30. Desktop/native Graphics captures were
rendered through the production overlay and visually inspected. The combined
release notes retain the previous Sega CD results and distinguish simulated
display tests from hardware testing. The publisher requires same-commit public
CI and all six package builds before tagging; see the release workflow for those
results. No new physical-device or Macintosh-asset validation is claimed.

## Patch 1.29 — display modes, resolution and integer fitting

The new `display` regression uses the real SDL platform with dummy video and
software rendering. It checks windowed, borderless and exclusive requests,
reported-mode selection, rejection/rollback of unavailable sizes, remembered
fullscreen kind through Alt+Enter, and fallback from obsolete startup profiles.
Fixed window dimensions survive filter changes and native title/game canvas
changes. Tests cover 256×224 and 512×448 canvases, largest-whole-multiple fitting,
small-window downscaling, fractional fit and nearest-only pixel output after a
renderer reset. Preferences tests check new field round-trips and old fullscreen
profiles retaining borderless behavior; malformed resolution pairs reset safely.

Bundled SDL2 accepts exclusive mode in this test backend. The locally installed
SDL2 compatibility backend silently substitutes borderless; the engine now
detects this, rejects the request, and restores the prior mode. This is tested
as a supported failure path, not treated as exclusive-mode hardware validation.
Desktop/borderless/exclusive and integer-scaling UI captures were rendered at
desktop and native sizes, with representative captures visually reviewed.
Android/Quest retains its host-managed fullscreen surface; its window-mode and
resolution controls are disabled. No physical display-mode switches or remote
target builds were used for this validation.

All 31 suites pass in both Release and Debug/UBSan. The local launcher reports
1.29, the documentation guard passes, and the manual explains scaler factors,
integer pixels and existing Ctrl+S / Ctrl+L quick-save/load shortcuts. Changes
have not been committed or published in this patch.

## Patch 1.28 — QoL gameplay demos

The shared demo launcher is exercised with all three supplied GOG recordings.
Headless tests drive the actual `Game::run` and QoL command callbacks from an
interrupted opening, title settings and paused gameplay, and retain the original
title's Demo item. They verify return to the title through Esc, forced replay
exhaustion and a recorded death condition; saving is rejected through both the
menu and storage boundary, and playback leaves save directories empty. Missing
indices and empty recordings preserve the previous session and replay buffer.
These are focused launch/exit checks, not full-playthrough replay verification.

Startup tests verify replay availability for the supplied DOS, Amiga and Sega CD
sets. The latter two offer no recordings. UI tests activate the real demo controls
at desktop and 256×224 sizes, including confirmation, cancellation and missing
replay choices; both confirmation layouts were visually reviewed. Macintosh and
PC-98 retain their inherited replay loaders but have no private-asset validation.

All 30 Release CTest suites and 10 focused Debug/UBSan suites pass. The local
launcher is rebuilt at 1.28 and the manual documents the controls and save
behavior. This patch does not publish a release or verify other target hardware.

## Patch 1.27 — built-in xBRZ and missing-map demo regression

xBRZ is linked through the shared CMake engine target, including Android, and
the legacy Makefile. Existing filter IDs retain their meaning; `filter=3` adds
xBRZ. CLI aliases, settings persistence, same-scale live switching, renderer
reset and the desktop/native QoL layouts are checked. The adapter is compared
against the unchanged packed-row xBRZ core at every factor from 2 to 6, using
odd widths, one-pixel images, padded strides, guard pixels and invalid geometry.
Both 256×224 and 512×448 platform paths are exercised. Original scaler copyright
notices accompany binary packages through the shared license copier.

The new optional `demo_missing_map` test loads the private GOG/DOS assets and
executes the real `Game::mainLoop` with recorded demo input. Current GOG data
starts LEVEL3 in room 56; the test explicitly recreates the historical room 39
start, whose map is absent. Input continues without death, a controlled move to
a mapped room restores decoding, and ordinary play retains the missing-map death
path. This checks the exception and recovery, not a complete historical replay.
Without private DOS assets this test is compiled but not registered with CTest.

All 30 local CTest suites pass in Release and Debug/UBSan; packaging tests and
documentation checks pass. The local launcher runs the rebuilt 1.27 binary.
Cross-platform build recipes are updated; no new remote target-build or hardware
certification is implied by these local checks.

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

## Local 1.34 follow-up — 2026-10-25

- Release: 35/35 CTest checks passed. Debug with UndefinedBehaviorSanitizer:
  35/35 passed. Python release-tool suite: 17/17 passed. Documentation boundary
  and whitespace checks passed. Rebuilt the local arm64 command-line engine.
- QoL pointer regression clicks the actual rendered turquoise Exit button with
  SDL button events, deliberately stale hover coordinates, window/fullscreen
  layouts and 1x/2x framebuffer sizes. The backend now polls window-relative
  mouse coordinates while hovered. These are automated SDL software/dummy
  checks; the reported physical Mac fullscreen offset still needs user retesting.
- Private combat fixtures use original Amiga, GOG/DOS and Sega CD actors,
  scripts and resources. Each places a real mutant in Conrad's starting room
  and fires once, in both directions, for Original, Heavy and five laser colours.
  All 42 encounters damage the enemy; serialized gameplay state and damage timing
  remain identical across styles. Laser pixels terminate at the native hit.
  Synthetic DOS/Mac cases check terrain, room exits, room-wrapped hits and 1x/2x
  beam clipping; Mac original assets are still unavailable.
- Standalone compaction of the three owned editions produced 551 runtime files,
  263.0 MiB logical data, including all movies/voices. Ogg music conversion checks
  original sample counts after a complete decode. Source files were not modified.
- Built and verified a private 1.34 Apple Silicon app and 147,166,149-byte DMG
  with the three compact editions. All 551 bundled asset hashes were checked;
  the relocated native launcher lists Amiga, DOS/GOG and Sega CD correctly.
- Saved the transparent PNG in packaging/icons, retaining the supplied JPEG.
  All ten decoded ICNS sizes (16–1024px) have zero-alpha corners and retain the
  bright interior. The private app's ICNS matches this inspected icon family.
- The complete public arm64 release recipe produced an asset-free ZIP and DMG.
  A separate asset-free DMG with the optional transparent icon was also built.
  Mounted-image signature/inventory checks passed. Publisher tests reject a
  changed DMG digest and packaging rejects private bundle assets before imaging.
  The Intel recipe shares this path but was not rebuilt on this arm64 host.
  These local packages have not been published to GitHub.

## Local 1.35 notification fixes — 2026-10-25

- Rebuilt the arm64 engine: all 35 Release CTest suites passed. Seven focused
  Debug/UBSan suites passed (progress, settings, platform, input/audio and all
  three edition startup fixtures). Documentation and whitespace checks passed.
- Timer regressions cover continuous publication of unread entries, a second
  new entry, reading/disable dismissal, error priority, expiry and the final
  750ms fade across the SDL clock wrap. Expiry leaves unread journal bits intact.
- Software-rendered HUD checks cover 256, 640 and 1280 pixel widths, normal and
  enlarged text, with/without the FPS counter, solo/simultaneous notices and
  long feedback. Geometry checks detect overlap, clipping and scrolling;
  rendered pixel comparisons verify both text and backgrounds fade to nothing.
  Native and desktop captures were visually inspected. Physical GPU/fullscreen
  rendering remains for the user's local play-test.
- Refreshed the private Apple Silicon app/DMG using the existing compact Amiga,
  DOS/GOG and Sega CD assets, Ogg music and transparent icon. Verified all 551
  bundled asset hashes, app signature, disk image and launcher edition discovery.
  The local play script launches 1.35. No GitHub release was published.

## 1.45 — module mechanics and campaign authoring (2026-10-26)

- Local macOS arm64 player/editor build: **56/56 CTest checks passed**.
  All 21 editor checks also passed with UBSan and `halt_on_error=1`. Six full-native
  UI cases exceeded the previous 60-second harness limit under instrumentation;
  they passed when rerun with 120 seconds (observed 67–72 seconds). The regular
  native UI cases take approximately 6–10 seconds. CTest allows 140 seconds for
  this instrumented UI work; native dependency audits retain 300 seconds.
- Regression coverage: all 22 node help entries; real pickups versus visual
  effects; health/damage, counters, consumed-item branches, teleport/checkpoint
  behavior, a delayed checkpoint after leaving valid rooms, named exits, and
  comment round trips/runtime identity. Native checks cover reversible actor
  deletion, shared-service protection, restart requirements, transition routing
  and original seven-level parity for Amiga, DOS/GOG and Sega CD.
- Campaign tests cover draft serialization, integer/path/rectangle bounds,
  missing/dangling routes, actual level exits, nonlinear forks and loops,
  multipart movie names, the asset movie catalog, photo/text compositing,
  stable typewriter layout and fade timing. Real SDL player runs exercise
  story -> named custom-level exit -> original movie/audio -> ending on all
  three asset editions, including repeated window/audio teardown.
- Actual ImGui widget tests exercise campaign stage creation, port connections,
  save/reload, node help, the larger story layout window, dragging text and undo.
  Captures are in `docs/screenshots/editor-*-1.45.png`; all show authored content.
  Fresh seven-level native campaign export and its route document validate.
- Full playthroughs of every possible authored campaign, physical GPU/controller
  testing and Macintosh assets were not tested. The desktop was locked; UI
  validation used SDL dummy video/audio and the software renderer.
- Logs: `build/editor-study/final-1.45.log`, `ui-final-1.45.log`,
  `ubsan-final-1.45.log`, `ubsan-retry-1.45.log`. Known remaining boundaries:
  campaign visits initialize fresh state; no campaign progress persistence;
  movie stages use the existing catalog rather than arbitrary MP4 files.
