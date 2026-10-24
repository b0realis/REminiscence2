<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Changes

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
