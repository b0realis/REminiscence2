<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Source, fixes, smoothness and robustness campaign — 1.09

24 October 2026. This report describes the local patch. The published release
remains v1.06; 1.07–1.09 have not been published.

## 1. Source bug hunt

Reviewed the first-party C++ engine and headers, CLI, Python asset/release tools,
platform launchers and Android import boundary. Clang's analyzer covered the
50 original non-table C++ translation units and the added foreground decoder.
Static resource/controller tables were checked through their consumers and the
existing integrity tests. Bundled SDL/zlib/ImGui/synthesis/Vorbis sources were
not treated as newly authored code or comprehensively audited.

The review followed resource input through decoding and drawing, input through
simulation and pause boundaries, and save files through preflight and recovery.
It also checked callback ownership, teardown, replay timing and packaging source
lists. No private assets, publisher documents or game art were added.

Before fixes, the new regression test reproduced seven failures: null actor
initialization, duplicate actor identity, inventory cycles, trailing save data,
incorrect gzip size, and two cases where stopping one player removed another
player's callback. A complete SNDS header with a zero chunk length reproduced
an infinite parser loop; the reproduction was stopped after three seconds.

## 2. Confirmed fixes

| Area | Fault and correction | Evidence |
| --- | --- | --- |
| Save structure | Range checks allowed null initialization links, duplicate actor indices and inventory/collision cycles. Require stable actor identity, valid collision extents, terminating inventory lists and an exact end of stream. | Synthetic malformed saves, gzip CRC corruption, existing legacy imports. |
| Save scripts/animation | A matching data fingerprint did not validate serialized animation/script indices. Check them against the actual level before committing. | Private Amiga mutations reject invalid animation, script and object type without applying the save. |
| Inventory | A fixed 24-entry stack array could overflow; malformed lists could loop. Collect bounded, visited actor entries dynamically. | 26-item inventory and cycle regressions. |
| Actor initialization | Actors excluded by difficulty retained uninitialized flags/script positions. Initialize those fields before skill filtering. | Repeated loads and deterministic replay under UBSan. |
| Checkpoints | Manual quick/menu saves overwrote the checkpoint-valid flag. Preserve checkpoint bookkeeping; reject quicksave during death. | Menu-save regression and source-path checks. |
| Music ownership | OGG, CPC, MOD and action-music stop paths could clear another backend's callback. Retire only the owner's hook; synchronize shared music type/action state and teardown. | Owner-preservation, backend-replacement and repeated stop tests. |
| MOD replacement | Repeated playback could replace decoder state without retiring the active reader. Stop and unload before loading; reject negative track indices and bound libmodplug input. | Public playback, invalid selection and backend tests. |
| CPC audio | Chunk parsing could fail to advance and playback read files in the callback. Parse and decode bounded SDX2 PCM before installing the callback. | Zero-length/truncated/mutated chunks and callback partition tests. |
| Gzip | Size used an unsupported seek-to-end; save reads could omit final trailer verification. Measure bounded decoded length while preserving position; explicitly check EOF/checksum. | Size/position and damaged gzip checksum tests. |
| Screenshot/file adapter | Screenshot success preceded the buffered close result; the optional RWops adapter lacked tell and discarded read counts. Propagate close failures and complete the adapter. | File failure tests, normal screenshots and adapter compilation. |
| Scaler CLI | `atoi` silently accepted invalid scale text. Parse a bounded positive integer and report invalid input. Clamp built-in point/linear scales. | CLI malformed-value tests. |
| ABA index | Appending an empty archive needlessly reallocated the old table and could lose it on allocation failure. Keep the old table without reallocating. | Archive tests and analyzer recheck. |
| Archived resources | BNQ omitted its extent; LEV had no archive dispatch; decoded PGE input leaked. Preserve sizes, dispatch LEV and free temporary PGE bytes. | Synthetic archive adapter/bank decode and seven-level tests. |
| Release boundary | The autosave suffix was absent from the older private-file release check. Ignore player states and reject `.r2s` files in tracked source/release archives. | Uppercase `.R2S` archive rejection and documentation boundary test. |
| Makefile | The inherited source list omitted runtime path resolution. Add the required translation unit alongside the CMake build. | Source-list review; CMake remains the validated release build. |

## 3. Smoothness campaign

- Rewind snapshots now use active simulation ticks. Inventory, dialogue, settings
  and background pauses no longer consume their five-second interval.
- F10/Escape/Start can leave inventory for the QoL overlay, which restores the
  composed game frame before showing settings. The ordinary inventory key still
  closes/selects as before.
- Inventory/dialogue returns reset the frame deadline. Original movement,
  collision and animation rules remain unchanged.
- Background and window-focus state are independent pause reasons. A focus gain
  cannot resume a backgrounded application. Event-driven waiting removes the
  old 100 ms focus-poll delay.
- MOD ticks carry fractional `2.5 / BPM` timing and apply tempo effects in the
  same callback. Action music also carries fractional tick samples.
- CPC playback advances its source clock against the negotiated output rate.
  It no longer requires matching file/device rates, and its callback performs
  no disk reads or allocations. PCM is bounded to 64 MiB.

These changes address specific stalls and timing faults. No hardware FPS,
battery-life or latency improvement is claimed without device measurements.

## 4. Robustness campaign

The foreground decoder now validates command counts, repeat state, offsets,
compressed runs, output expansion and plane extents before publishing pixels.
It clips each packed pixel independently, including the bottom-right edge.
Room decoding checks the LEV table, decoded extent, tilemap spans and assembled
bank capacity instead of relying on debug-only assertions.

Validation includes 40,000 deterministic CPC/foreground mutations, truncation
sweeps, canaries and transactional-output checks. Two route probes cover 21,000
simulation ticks across all seven Amiga levels, including repeated save/load,
room traversal, rewind capture and local restarts. Their serialized state hashes
match. These probes restart around queued death/story scenes: they are not a
complete puzzle/combat playthrough.

The seven baseline Amiga level captures and three T-shirt captures are
byte-identical to the pre-campaign v1.08 Release outputs. All 419 room renders,
existing cutscene tests and private seven-level recovery checks remain covered.

See [validation](VALIDATION.md) for final build/test results. Local raw logs and
analyzer reports are in ignored `build/campaign/`.

## Limits and follow-up

Real GOG DOS/Macintosh CD/3DO music files, physical controllers, Steam Deck,
Quest and other operating systems still need direct validation. AddressSanitizer
cannot start on this host, as recorded earlier; UBSan is used here. No
ThreadSanitizer or complete hostile-asset security certification is claimed.
Remaining analyzer notes include legacy geometry/cutscene dead stores and a
possible uninitialized Vorbis value in the bundled decoder. The Vorbis report
has not been reproduced and is not presented as a confirmed engine bug.
The disabled Tremor path also received reviewed close-ownership/reload/EOF guards;
it was not built because its optional library is absent. The default STB path
was built and tested. Some inherited resource/script paths still fail with a terminal error for
missing or corrupt required data. A successful test campaign does not prove
that every original binary format accepts arbitrary hostile input safely.

## Recommended next enhancements

1. A death/recovery screen showing the latest autosave, checkpoint, manual saves
   and rewind previews together, with a useful default controller selection.
2. A cross-level manual-save browser with thumbnails and optional slot names;
   preserve a few player-marked milestones outside automatic rotation.
3. Visible save/error feedback and an assets diagnostic page, so a player using
   a one-click launcher does not have to discover a terminal warning.
4. Optional pause on controller disconnect, reconnect prompts, trigger binding,
   per-device profiles and controller-aware button labels.
5. A small event-driven objectives/notebook page alongside the journal, with
   progressively revealed hints and no future-story spoilers.
6. Optional accessibility assists: hold/toggle Action, a short input buffer,
   readable text scaling and reduced flashing. Test gameplay-changing assists
   separately and keep the original rules as the default.
7. Measure disk-save stalls before introducing a bounded background autosave
   writer; profile decoding/upload on Deck and Quest before adding caches.
8. Audio-device loss/recovery with a silent fallback that also advances voice
   playback, preventing dialogue from waiting indefinitely for absent audio.
