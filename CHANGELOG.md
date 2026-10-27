<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Changes

Authored story demos mentioned in the local development history below are kept
private and are not included in this source tree. The editor, engine, training
fixtures and public regression tests are included.

## 1.51

- Bound hit-response rule scans before reading the next object, including empty
  tables and exhausted rule indices. Add a synthetic regression alongside all
  three asset editions' combat/editor tests.
- Release replaced SPC sprite tables on loose-file, archive and Macintosh paths,
  retaining DOS global tables across level cleanup. Exercise repeated loose and
  archived loads under LeakSanitizer.
- Separate editor level resets from one-time global resource initialization.
  Repeated native-campaign probes retain fonts, shared sprites, icons and DOS
  samples instead of leaking buffers or appending duplicate icon banks.
- Document a matched LLVM 22 sanitizer toolchain for recent macOS and isolate
  physical-controller discovery in headless leak checks without suppressions.

## 1.50

- Keep disabled doors visible and prevent native animation strides crossing
  narrow authored gates, preserving native approach/ledge alignment and original
  campaign physics. Check both directions, room edges, open gates and jumps.
- Restore standalone checkpoints with their puzzle state and quest inventory;
  keep fallback anchors valid at room boundaries. Reject contradictory dialogue
  queues and malformed quest data before changing the running world.
- Pause custom gameplay and node timers after controller disconnect until a
  fresh confirm press. Consume that press without activating a nearby puzzle;
  require a fresh press for death-recovery retries too.
- Share native frame pacing with custom campaigns to compensate for scheduler
  overshoot. Exclude background time from timed story scenes and their effects.
- Expand original-campaign parity checks across all seven chapters with walking,
  running, jumping and pistol inputs, alongside the authored campaign walkthroughs.

## 1.49

- Study original campaign scenery and simplify the twenty-room starship into
  broad dark planes, sparse highlights and quieter backgrounds, retaining one
  continuous atlas with floor-aligned crops.
- Add six raised machinery platforms with two-sided ledges, elevated pickups and
  controls, three additional solid bulkheads and a two-input pressure puzzle.
  Add physical shuttle boarding while retaining the ramp interaction.
- Use 32-pixel solid doors after detecting native animation strides through
  one-cell walls. Verify closed doors, platform approaches/descent, the full
  mission and ending on Amiga, DOS/GOG and Sega CD.
- Keep previous private demo copies intact and create the new editable layout
  as `AfterTheDrift-Classic`; document the style study, geometry and walkthrough.

## 1.48

- Redesign After the Drift as twenty connected rooms sliced from one continuous
  four-deck starship cutaway, with reciprocal lifts and a raised shuttle interior.
- Add automatic anomaly release, cargo discovery, portable-cell relay, separate
  couplers, reactor/pressure interlocks, tractor isolation, clamps and keyed ignition.
- Add an editable animated shuttle launch/tractor recapture and fade-to-black ending;
  expose story-layer motion and timed whole-screen fade in the campaign editor.
- Fix smeared authored text by preserving DOS glyph shadows; use compact checkpoint
  notices with a one-second fade. Keep the former private demo copy intact.
- Exercise all twenty rooms, puzzle interlocks, return travel, boarding, timer
  restore and cinematic rendering with Amiga, DOS/GOG and Sega CD assets.

## 1.47

- Add importable Music/Sound nodes with WAV/Ogg previews, looping, volume,
  start/stop/completion ports and campaign resume seeking. Carry audio through
  project, campaign, clipboard and portable-module copies; validate bounded audio.
- Add live sprite-strip previews to node inspectors; hide inactive animation
  nodes that have no idle sprite. Project/module schemas advance to 9/7.
- Add After the Drift: four atmospheric alien-starship rooms, original ambient
  audio, containment/sigil/reactor puzzles, a sentinel, checkpoint and escape ending.
  Include artwork provenance, generator, walkthrough and `--starship` launchers.
- Verify full starship traversal on Amiga, DOS/GOG and Sega CD, plus audio decoder,
  mixer, lifecycle and asset-copy regressions.

## 1.46

- Save and resume campaign progress atomically, including native actors, queued
  messages, authored puzzles, timers and dialogue. Add Resume/New controls,
  campaign-aware native save/recovery UI and stateful return visits.
- Carry keyed items and flags between modules/levels; add Set/Test campaign flag
  nodes (24 types), conditional routes and health/native-inventory carry options.
  Native transfers use matching destination templates, preserving local scripts.
- Fix return-visit player script initialization, restore legacy NPC/computer
  dialogue notices, apply photo opacity, standalone flag resets and above-cap
  healing; store campaign checkpoints promptly and clarify the counter inspector.
- Add Relay Station: seven original rooms, three levels, carried-coil quest,
  friendly crawler, timed reactor state machine, route unlock and ending.
  Include a reproducible generator, walkthrough and `--demo` local launchers.
- Add strict save/corruption regressions and real-input demo playthroughs for
  Amiga, DOS/GOG and Sega CD. Project/module/campaign schemas are now 8/6/2.

## 1.45

- Expand the authored palette to 22 nodes with health pickups, damage, counters,
  item tests, custom-scene checkpoints, teleporting, named level exits and startup
  events. Clarify the existing quest pickup versus its visual effect.
- Animate signal direction and state changes; add per-node/module help, inputs,
  outputs and designer comments, direct module management controls and reversible
  native puzzle deletion with protected shared actors.
- Add a fourth Campaign workspace with linked/imported levels, forks, catalog
  movies, endings, and text/photo screens with placement, color, typewriter/fade
  effects and a draggable layout preview. Add `--campaign`, stage playtesting and
  an original campaign example. New native imports include a route document.
- Introduce project schema 7/module schema 5 and campaign schema 1. Campaign
  visits start fresh; persistent campaign progress and arbitrary MP4 playback
  remain future work.

## 1.44

- Expose the complete seven-level native campaign as connected module groups,
  with typed actor/room ports, visible shared dependencies, independent behavior
  copies, complete-bundle movement/duplication and undo/redo. Include incoming
  activators, hidden helpers and owned authored extension graphs in relocation.
- Add native connections graphs, delivered-message traces, live actor state and
  single-frame stepping. Preserve original sprite/animation identity when private
  rule tables are installed; reject new incompatible enemy-bank arrangements.
- Add schema 6 template provenance, guarded native reference/inventory/collision
  operations, transactional seven-level import and `./edit-local.sh --campaign`.
  Preserve original actor IDs and report capacity limits, including full level 2.
- Verify original simulation parity across all seven levels in Amiga, DOS/GOG
  and Sega CD and audit every copyable bundle's references. Native copies remain
  local to their source level/edition; dynamic/global campaign links stay shared.

## 1.43

- Add explicit State nodes with initial states, mutually exclusive machine groups,
  entry/exit signals and active-state progress gates. Add a state-machine airlock
  recipe and graph-only Start, Play, Pause, Step and Reset controls using the same
  evaluator as gameplay. Highlight active nodes, delivered wires and bounded
  signal traces; inspect running custom or native extension graphs.
- Add actor nodes using native crawler assets in custom levels, with patrol
  bounds, friendly interaction, wired behavior controls and hurt/death/endpoint
  events. Native actor bindings retain their original artwork and scripts and
  connect native rule execution and messages to authored dialogue/state graphs.
- Expose native state-machine conditions, actions, operands and transitions for
  all seven levels. Group reviewed actor dependencies into named puzzle modules;
  support regrouping, disabled rules, appended branches and live execution traces.
  Preserve stable actor IDs and hidden dependencies; logic edits require restart.
- Add schema 5 projects / schema 4 prefabs, bounded native table aliases and trace
  storage, transactional native program compilation, legacy migration and guarded
  rule iteration. Original assets stay read-only. Native actor cloning and
  opaque dependency rewriting remain outside this patch.

## 1.42

- Separate the editor into **Level Editing**, **Module Placement**, **Module
  Editing**, and a top **Help** tab. Geometry tools and module gestures stay in
  their own workspaces; F1 still opens help while playtesting.
- Add named module creation, module-owned image libraries, and reusable templates
  in the project's `modules/` folder. Export/import carries images and internal
  connections; each placed instance starts with independent state.
- Add standalone dialogue nodes with speakers, pagination and two optional
  choice branches. Conversations queue without replacing each other. Completion
  runs after dismissal; Stop/Disable cancels queued or active conversations.
- Add triggered animation, pickup-effect animation and timer nodes with Started,
  Completed and Stopped outputs; Activate, Stop, Reset and one-shot controls.
  Include branching-dialogue/door and pickup-effect/door palette recipes.
- Migrate older projects to schema 4 and modules to schema 3. Add regressions for
  asynchronous actions, cancellation, choice navigation, owned assets, real
  workspace tabs, module creation and library placement across supported assets.

## 1.41

- Add Editor Settings with direct native campaign project creation, persistent
  Play/F5 start mode and native actor-marker visibility. Native campaign mode
  plays from the selected level's authored start; current-room mode uses TEST.
  Shift+F5 forces authored start and Ctrl/Cmd+F5 forces the current room.
- Keep settings in a bounded, atomically written folder-local sidecar. Prevent
  play shortcuts and file drops through its modal; protect hidden/off-room actor
  records from accidental canvas dragging.
- Explain native event coverage explicitly in the GUI and documentation. Original
  actors/helpers are selectable, but their events are not traced or editable as
  nodes yet. Name the custom palette Puzzle Modules and record the whole-puzzle
  ownership contract for future native conversion.

## 1.40

- Add PLAY HERE / F5 to test the currently edited room immediately. Graph tests
  use the selected node's room; a session-only TEST marker chooses the position
  without moving the authored player start. Shift+F5 tests the authored start.
- Support direct starts throughout native campaigns with the original actors,
  hidden helpers and script dependencies intact. Skip the arrival animation and
  cinematic, equip the original pistol, and reset simulation state consistently.
  Keep Pause / Apply live / Resume for testing with earned puzzle progress.
- Validate test positions against collision and closed custom doors; reject
  native slots without maps and malformed command coordinates before mutation.
- Cover current-room/graph/marker shortcuts in actual widget tests, direct starts
  in all seven levels of three editions, repeatability and native launch commands.

## 1.39

- Edit all seven native campaign levels with the complete original actor table,
  hidden helpers, puzzles, inventory, dialogue and scripts retained. Use a native
  player window with Pause / Apply live / Resume / Restart / Stop controls.
- Merge safe art, collision and exit edits between frames; reject script conflicts
  and new collision over active actors. Require restart for actor setup changes.
  Protect native IDs and operands; keep preview saves inside the project.
- Add custom-module multi-selection, box selection, group movement and copy/paste
  with fresh IDs and remapped internal links. Add independent visual-layer and
  collision locks, graph pan/zoom/frame-all, and clickable validation locations.
- Add schema-3 migration, native campaign/command protocol documentation and
  regressions for all seven levels in Amiga, DOS/GOG and Sega CD, actual editor
  widgets and the native player process.

## 1.38

- Add built-in Guidelines (F1): native room dimensions, large-image slicing,
  art layers, collision/ledge coordinates, sprite strips, style advice and
  editor/playtest controls. Add palette search and Fit / 1x / 2x room zoom.
- Keep module/node selection and the graph view through undo/redo. Preserve
  redo on empty ledge-removal clicks and align left-facing ledge hit targets.
  Open dropped project JSON files correctly; block file drops during play/modal
  operations and keep play paused while reading guidelines.
- Open original level one as a read-only reference using Amiga, DOS/GOG or
  Sega CD assets: native map slots, collision, exits, actor records and initial
  rendering. Create private, editable scenery-study projects with exact collision
  and room links, original player start, decoded Back art and a Front priority
  mask. Native campaign scripts/actors are not converted to modules; live
  campaign hot reload remains future work.
- Reuse the native loader for reference and custom playtests, export lossless
  RGBA PNGs atomically and reject overwriting existing scenery-copy directories.
  Add real-edition decoding/export/playtest and actual-widget regressions.

## 1.37

- Add a visual interactable module editor: draggable nodes, typed event ports,
  gold item-requirement links, independent part placement, sprite strips and
  idle/active animations with looping or hold-last-frame playback.
- Add reusable button/door, keycard, NPC retrieval, mission computer and area
  trigger templates; bounded cyclic signal delivery, distinct mission objectives,
  item consumption, native-resolution paginated dialogue and pickup notices.
- Preserve old projects with schema-1 migration, convert paired doors into
  graphs, keep legacy chains working, and copy all node artwork when exporting
  prefabs or saving a project elsewhere. Include an original-art Module Lab.
- Fix failed prefab imports advancing project IDs, selection clearing redo,
  foreground ordering and missing activators in world thumbnails, overlapping
  world-room placement, and closing doors over live enemies. Document the
  original campaign palette's actual coverage and remaining gaps.

## 1.36

- Add a native visual level editor with whole-level image slicing, Back /
  Playfield / Front layers, independent collision and ledges, linked rooms and
  a world overview. Imported images are stored once with reusable crop references.
- Add self-contained door/activator pairs, independent duplication, portable
  module files and chained door outputs; include animated props, health pickups
  and the original crawler as the first reusable palette entries.
- Play custom rooms with the original player simulation using Amiga, DOS/GOG
  or Sega CD assets. Preserve pistol controller dependencies and explicit sprite
  coverage, bound and expand sprite queues for crowded authored rooms, and stop
  audio before releasing scene resources.
- Add atomic project saving, undo/redo, bounded input validation, an original-art
  Workshop example, `edit-local.sh`, `--project`, an authoring guide and schema
  documentation. Custom campaign saves and the complete object catalog remain
  future work. Add original-asset and real-widget editor regressions.

## 1.35

- Show each new journal entry notification for 3.5 seconds, fading out over the
  last 0.75 seconds without marking the entry read. Leaving entries unread no
  longer keeps the banner on screen.
- Place autosave and other feedback at the same upper-left edge. Simultaneous
  journal and save notices sit side by side, wrapping within their cards at
  native resolution. Fade feedback too, retaining the longer error duration.

## 1.34

- Fix macOS QoL pointer polling to use content-relative coordinates while over
  the window, and take click coordinates directly from button events.
- Stop cosmetic laser beams at the original gun ray's enemy/wall/range endpoint,
  showing a brief impact without changing damage, hit rules or game scripts.
- Add a transparent PNG icon variant and retain the supplied JPEG original.
- Add asset-free Mac DMGs to both release targets, a GUI assets-folder prompt,
  and reusable Python asset compaction and app packaging tools. Keep public
  assets outside signed bundles and verify disk-image inventories before release.

## 1.33

- Add optional native macOS app icons to the private package builder, with all
  standard 16–1024px sizes and Finder/Dock bundle metadata before signing.
- Preserve the user-supplied original icon image and its provenance in the repo.

## 1.32

- Let the macOS GUI launcher use assets inside its app bundle, retaining the
  adjacent-assets layout for public releases. Include a private-package builder
  for a compact Apple Silicon app with separate Amiga, DOS/GOG and Sega CD sets,
  optional Ogg CD music and a compressed drag-to-Applications disk image.
- Keep an edition's sibling music directory attached when it contains Ogg
  tracks, so converted Sega CD music no longer hides movies and embedded assets
  or causes detection as cartridge data. Preserve separate soundtrack boundaries.

## 1.31

- Add one-click Classic pixels, Widescreen scenery and Smooth widescreen
  presentation presets to both Graphics layouts. Expand them into normal saved
  preferences, preserve window mode/fixed resolution, and show Custom after edits.
- Apply Point, Linear, ScaleNx and xBRZ consistently to the playfield and side
  scenery. Cache filtered backgrounds until they change; retain native scenery
  across live filter switches and renderer resets instead of losing the borders.
- Check every side-content mode, preset persistence and native-menu activation,
  plus 1080p/4K rendering with 256×224 and 512×448 game canvases.

## 1.30

- Audit the new demo and graphics features. Reject oversized or incompletely
  read DOS/PC-98 demo recordings before playback, retaining the previous live
  session on a rejected launch. Add public malformed-input and private-session
  preservation regressions.
- Prepare a combined multiplatform preview with xBRZ, QoL demo selection,
  window/resolution/integer-scaling controls and reusable movie upscaling tools;
  retain the Sega CD features and release notes from 1.25.

## 1.29

- Add windowed, borderless desktop and exclusive fullscreen choices to Graphics,
  with independent window-size and reported fullscreen-resolution selectors.
  Preserve old fullscreen profiles as borderless and keep fixed window sizes
  across filter changes. Reject unavailable fullscreen modes without replacing
  the current display settings; fall back safely for obsolete startup profiles.
- Add whole-native-pixel integer fitting with borders and a downscale fallback
  for tiny windows. Keep Point sampling sharp after renderer resets. Document
  Ctrl+S / Ctrl+L quick save/load and the distinction between size and filtering.

## 1.28

- Add Session → Gameplay demos to both QoL layouts. Select an available original
  Jungle, Alien World or Death Tower replay from DOS/GOG or Macintosh assets.
  Confirm before replacing gameplay; unavailable editions explain missing replays.
- Share the launcher with the original title's Demo item. Keep recordings out of
  manual saves and autosaves; end recorded death at the title instead of recovery.

## 1.27

- Bundle xBRZ in standard desktop and Quest builds, with a live Graphics filter
  choice in both QoL layouts and `--scaler=xbrz@2` through `@6` (`xbr` alias).
  Preserve existing filter preferences and fix padded-row handling in the adapter.
- Add a DOS demo regression for a missing LEVEL3 room39 map: demo input continues,
  valid-room loading recovers, and ordinary gameplay retains its death path.

## 1.26

- Save reusable offline Sega CD movie upscaling tools: export through the engine
  decoder, run Real-CUGAN Pro, preserve original frame holds and audio, validate
  the MP4, and record source/model hashes. Existing outputs are never replaced.
- Add optional source-bound tracked-region repairs and the SC1 sign recipe.
  Keep restored movies, reference images and model binaries in local storage.

## 1.25

- Enable gameplay T-shirt choices and Heavy/Laser pistol styles for Sega CD,
  including all five laser colours. Select the CD object bank and tile decoder
  when finding the native muzzle position; restore the caller's bank afterward.
- Share the selective undershirt mask with Amiga. Preserve the jacket, footwear,
  equipment, source palettes and imported files; Original keeps native colours.
- Audit all 70 MCD files against an independent disc extraction and all 17754
  indexed frames against the original 68000 decompressor. Their dithering is
  original artwork. Add patterned tile/palette and CD cosmetic regressions.

## 1.24

- Fix Sega CD movie timing after focus/background pauses: video waits for the
  resumed audio instead of racing through the elapsed wall time.
- Invalidate WAV streams after any failed open, reject Windows device filenames
  in extracted disc paths, and bound CUE reads before allocation.
- Add `play-movies-local.sh` for the private Sega CD movie browser. Refresh its
  help and format documentation; retain the local Amiga/DOS/Sega CD play choices.
- Package the title-input fixes and Sega CD gameplay, music, movies, dialogue,
  extraction tools and asset-format reference from 1.22–1.23 for all six targets.

## 1.23

- Add a bounded Python CUE/BIN extractor for the supplied Sega CD disc: preserve
  ISO files, remove audio pregaps, write lossless WAV tracks and SHA256 manifests,
  and refuse existing output directories.
- Add the US Sega CD resource adapter, including split-stream character sprites,
  separate object/background banks, original title graphics and verified embedded
  fonts/icons/text. Derive city room-bank selection from both LEV directories so
  direct save restoration and adjacent-room previews use the right resources.
- Play MCD movies with embedded PCM, original event variants and playback lengths;
  add them to the Python cutscene browser. Preserve VCE dialogue and decode large
  CD sound effects without treating valid sizes as missing entries.
- Stream PCM16 WAV backgrounds through the existing music selector and resampler;
  extracted CD music can accompany another game's asset edition. Add `--segacd`
  to the local launcher. Original imported assets remain unchanged.
- Document every supported asset family, file requirements, byte layouts,
  compression, source entry points, validation limits and the full tested CD
  inventory. Add synthetic extraction/media tests and private CD integration
  and startup regressions. Build locally; release publication remains on hold.

## 1.22

- Fix over-sensitive Amiga title navigation and missed DOS title taps. Queue
  menu presses and use a shared 400 ms hold delay / 120 ms repeat interval for
  keyboard, D-pad and stick controls. Keep arrows/Enter/Esc available after remapping.
- Add mouse hover/click and wheel navigation to original title, Skill and Level
  menus, with native-resolution hit boxes and widescreen/HiDPI coordinate mapping.
- Allow left-click to skip developer logos, intro and story cutscenes using the
  existing separate skip settings. Consume the release so a skip cannot activate
  the next menu or gameplay HUD. Update the manual and settings descriptions.

## 1.21

- Fix stale background-music state when a custom pack omits the next level's
  track; native action music and later scene cues remain available.
- Share soundtrack cue/tempo selection between gameplay and the standalone
  cutscene player, restore native cue mapping on missing overrides, and include
  the DOS-only PRF cue beyond the MOD name table.
- Keep Quest and Python-player default music scoped to the selected edition.
  Add asset/music selectors to the Python tool and honor remembered editions
  for catalog commands.
- Package the 1.12–1.21 pistol, startup, save-resume, edition-library and music
  improvements for all six release targets.

## 1.20

- Add default-off “Quicksave skips title sequence”: resume the newest compatible
  modern quick/manual/checkpoint save or autosave; try older saves when needed
  and return to normal opening/title flow when none loads.
- Discover separate editions under one assets folder, remember an initial picker
  choice, and switch editions from QoL Session after confirmation. Isolate each
  edition's files and exclude packed WHDLoad data until prepared.
- Select music independently in QoL Audio: Amiga MOD, DOS PRF or mapped external
  Ogg packs, with native fallback. Add --list-assets, --asset-set and --music-source.
- Support mono/stereo Ogg and streaming sample-rate conversion for supplied
  soundtrack recordings. Document Genesis/Sega CD Ogg packs; raw ROM/VGM and
  disc-image playback/extraction remain outside this feature.

## 1.19

- Always show developer logos and the introduction before the title on normal
  launches, including with existing autosaves. Preserve explicit `--continue`.
- Replace the Amiga title's automatic level list with a Start/Skill/Options/Quit
  menu over its original artwork. Esc, F10 and controller Start open the full QoL
  menu from the openings and title; closing it returns to the title. Level jumps
  and save loads enter the selected session without resetting it.
- Validate the supplied GOG Windows installer's DOS data, including native shot
  positions and all five laser colours. Add optional private DOS integration
  tests and `play-local.sh --gog` for the extracted local assets.
- Initialize unused collision-record bytes before serialization, fixing
  nondeterministic save payloads exposed by the DOS comparisons.

## 1.18

- Enable Heavy and Laser with DOS/GOG and Macintosh assets, deriving their
  origin from each edition's native flash geometry and timing. Preserve odd
  Macintosh pixel coordinates and keep the travelling laser on its launch row.
- Add Orange (default), Red, Green, Blue and Purple laser colours in the QoL
  menu, with matching flash/beam/light colours and saved preferences.
- Keep imported graphics and sounds unchanged. Invalid or missing flash
  geometry retains native rendering instead of hiding it behind a guessed effect.

## 1.17

- Start Heavy and Laser at the native flash's position and animation tick,
  reading its sprite geometry instead of estimating offsets from Conrad.
  Keep the original firing animation, with Heavy's extra drawing-only recoil.
- Limit the pistol experiments to Amiga assets. GOG/DOS, Macintosh, PC-98 and
  Sega keep their native pistol graphics and sound even when a saved Amiga
  preference selects Heavy or Laser. Imported resource files remain unmodified.

## 1.16

- Add separate, persistent Space/A-Z skip controls for opening developer/publisher
  logos and other cutscenes, enabled by default. Cover polygon, text, SET and SEQ
  playback; consume skip keys so holds cannot skip successive scenes or affect
  gameplay. Keep Backspace available.
- Correct cosmetic pistol direction to match the original firing script and
  retain the aiming sprite's mirror state. Anchor Heavy/laser flashes and beams
  to the actual barrel-tip pixel, including crouching and Heavy's visual recoil.
- Restore Original's native shot flash by retaining dormant actors in their
  off-screen room lists at level load and save restore. Check room membership,
  visible flash pixels and identical simulation states across all three styles.

## 1.15

- Add an executable `play-local.sh` launcher in the repository root for the
  local Apple Silicon build and prepared Amiga assets. Support launching from
  another directory and forwarding additional command-line options.

## 1.14

- Give Orange laser a dedicated deep-bass shelf on both the original game
  sample and the missing-asset fallback, with soft peak limiting. Preserve
  the source pitch, playback duration, Heavy sound and gameplay.

## 1.13

- Give Heavy / red a stronger bass shelf while retaining the original shot's
  pitch and duration; soften boosted peaks to avoid harsh clipping.
- Use the loaded edition's original laser effect for Orange laser. Replace the
  wide synthesized pitch sweep with a short electrical pulse as a fallback for
  asset sets missing that sound. Gameplay and visual effects remain unchanged.

## 1.12

- Add optional Heavy / red and Orange laser pistol presentations under QoL
  Enhancements, with Original retained as the default.
- Heavy uses red muzzle flame/casings, extra drawing-only recoil and a bass shelf
  on the original sample. Laser uses a moving orange beam, pistol-side light,
  no casings and an original synthesized laser sound.
- Keep simulation state, hit tests, damage, firing cadence and movement unchanged;
  test identical serialized states across all styles with real Amiga firing input.
- Add clipping, lifetime, foreground masking, audio partitioning and preference
  regressions. Effects are transient and never enter game saves.

## 1.11

- Preserve specific manual-save and quicksave failure messages, including
  protected milestones, instead of replacing them with a generic error.
- Let newly connected controllers inherit the most recently saved global
  bindings, including changes made after startup.
- Keep the composed gameplay frame intact when controller disconnect opens
  the pause overlay, so screenshots and save previews retain actors.
- Prepare the restored room and collision caches immediately after shortcut
  rewind, before the next simulation tick.
- Add regressions for these paths and assemble the 1.07–1.11 improvements for
  the six-target preview release.

## 1.10

- Add unified death recovery with rewind/checkpoint/autosave previews, cross-level
  manual saves and a compact layout with recovery actions immediately visible.
- Store names, thumbnails and protected milestones atomically in `.r2m` bundles;
  retain legacy `.state` loading and refuse protected quicksave overwrites.
- Display save/error/device feedback and an assets/system Diagnostics page.
- Pause on controller disconnect; add GUID profiles, trigger binding and detected
  PlayStation labels. Keep reconnect separate from Resume.
- Add opt-in Sticky Action, 100 ms tap buffering, steady shield-hit sprites,
  reduced shake and slower gameplay, plus a spoiler-limited Objectives notebook.
- Recover from audio-output loss with a silent mixer clock so voice/dialogue can
  finish, and retry audio at the existing sample rate on reconnect.
- Measure local save durations before adding background I/O; extend regression
  and desktop/native rendering coverage. See [the guide](docs/QOL_1.10.md).

## 1.09

- Reject unsafe save identities, cyclic inventory/collision links, invalid script
  and animation references, trailing data and damaged gzip trailers before restore.
- Remove the 24-item inventory limit; initialize actors excluded by difficulty;
  keep manual saves from changing checkpoint bookkeeping.
- Correct music callback ownership, decoder replacement/teardown and shared state.
  Bound and predecode CPC/SDX2 audio; resample against the negotiated device rate.
- Carry fractional MOD/action-music tick timing and apply tempo changes immediately.
- Keep rewind on active play time; open QoL settings from inventory; reset pause
  deadlines and keep focus/background pause reasons independent.
- Bound foreground/RLE decoding, clip edge pixels, and validate LEV/tile bank
  extents. Preserve archived BNQ/LEV lengths and free archived PGE input.
- Correct gzip sizing, screenshot close reporting, optional RWops file methods,
  scaler argument validation and the Makefile runtime-path source list.
- Exclude player states/autosave previews from tracked source and release archives.
- Add 40,000 decoder mutations, 21,000 deterministic route ticks, and the
  [four-campaign report](docs/CAMPAIGN_1.09.md).

## 1.08

- Keep three checksum-validated, atomically written autosave generations per
  edition, independently of checkpoint/manual slots. Default interval: 60 active
  seconds, adjustable from 15 to 600 seconds or disabled.
- Add a welcome-back Continue screen, `--continue`, and cross-level autosave
  recovery with asset-identity preflight and fallback to an older generation.
- Add a Rewind page with captured-frame previews, room, health and age. Restoring
  an older point discards its newer branch and retains older history.
- Add optional contextual control reminders using current keyboard/pad bindings,
  active-play timers and per-game suppression. Default off.
- Document new controls and add corruption, rotation, recovery, timeline and
  rendering regressions.

## 1.07

- Add persistent joypad/raw-joystick remapping, configurable stick dead zones,
  optional save/load/rewind buttons and independent input-source ownership.
- Add an optional, spoiler-limited story journal and unread notification.
- Extend saves with edition/gameplay identity, random seed and journal state;
  keep legacy imports and isolate new save filenames by edition.
- Correct fractional 30 Hz pacing and avoid double-throttling paused menus.
- Fix internal MOD default-tempo crashes, OGG remainder repetition, and SEQ
  audio timing across device rates; use a bounded SEQ audio queue.
- Bound object/SEQ decoding and retain failed texture uploads for retry.
- Add input/audio/decoder regressions and journal/joypad menu captures.

## 1.06

- Package public documentation from an explicit manifest, without requiring Git
  in the native Windows build shell or an extracted source archive.
- Reuse the included dependency sources in offline desktop and Android recipes.
- Require the UCRT64 runtime notice directory when assembling Windows packages.

## 1.05

- Fix Windows runtime-DLL discovery during packaging and include toolchain notices.
- Link Android system logging explicitly for the Quest host.
- Include a Quest deployment bundle with assets folder and Linux one-command helper.
- Verify same-commit CI, archive contents and source versions before tagged publishing.
- Ship corresponding SDL/zlib source, dependency manifests, checksums and public docs.

## 1.04

- Recover settings font textures alongside game textures after a renderer reset.
- Initialize the complete mirror-side texture before retaining its CPU backup.
- Test relocated-executable asset discovery and Unicode filesystem/save paths.
- Replace the obsolete Android SDK tools package in the build workflow.

## 1.03

- Add native Win64, Linux64, Steam Deck, Apple Silicon and Intel Mac packaging.
- Build pinned SDL 2.32.10 and zlib 1.3.1; remove local package-manager library paths.
- Add precreated assets folders, Linux/Deck launchers, Windows convenience launcher,
  and a native Mac app launcher alongside the command-line executable.
- Add an Android arm64 flat-panel host, bounded ZIP importer and signed APK recipe.
- Add a checksum-verified Linux ADB deployment helper that preserves saves/settings.
- Resolve Windows C++ standard, close-wrapper and path portability issues.

## 1.02

- Index assets deterministically without platform-specific handle errors, raw
  realloc arrays or recursive directory-symlink traversal.
- Write game saves atomically and propagate stream/compression close failures.
- Validate ABA/PAQ records and wrappers; bound PC-98 decompression input and output.
- Use writable user-data save defaults; retain explicit paths and portable mode.
- Handle controller hotplug, instance IDs, disconnect clearing and renderer resets.
- Size footer controls for enlarged text, retaining the turquoise EXIT GAME button.
- Reuse rendering/blur and T-shirt workspaces, cache keyboard geometry and avoid
  uploading unchanged paused frames. Simulation/gameplay timing remains unchanged.

## 1.01

- Establish the integer patch sequence, canonical version header and bump helper.
- Audit release blockers and define six distribution targets.

Device validation on Steam Deck and Quest, and full GOG/Macintosh data
playthroughs, remain separate from automated build/synthetic test coverage.
