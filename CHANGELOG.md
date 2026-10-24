<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Changes

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
