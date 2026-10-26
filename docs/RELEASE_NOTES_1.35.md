<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

REminiscence2 brings **Flashback** to modern systems with a live QoL menu,
flexible presentation and better ways to keep your progress. This **1.35 preview**
combines the latest fixes with the features from the earlier releases—a thank-you
to Delphine Software and the people who have kept this game playable.

## Snapshots

**Six of the seven Amiga levels, captured in 1.35.** Click any image for the
full-size view. Original 256×224 pixels are enlarged 3× with nearest-neighbour
sampling.

| Level 1 | Level 2 | Level 3 |
| :---: | :---: | :---: |
| <a href="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level1-1.35.png"><img src="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level1-1.35.png" alt="Amiga level 1 in REminiscence2 1.35" width="240"></a> | <a href="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level2-1.35.png"><img src="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level2-1.35.png" alt="Amiga level 2 in REminiscence2 1.35" width="240"></a> | <a href="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-death-tower-1.35.png"><img src="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-death-tower-1.35.png" alt="Amiga level 3 in REminiscence2 1.35" width="240"></a> |
| **Level 4** | **Level 6** | **Level 7** |
| <a href="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level4-1.35.png"><img src="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level4-1.35.png" alt="Amiga level 4 in REminiscence2 1.35" width="240"></a> | <a href="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level6-1.35.png"><img src="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level6-1.35.png" alt="Amiga level 6 in REminiscence2 1.35" width="240"></a> | <a href="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level7-1.35.png"><img src="https://github.com/b0realis/REminiscence2/releases/download/v1.35/amiga-level7-1.35.png" alt="Amiga level 7 in REminiscence2 1.35" width="240"></a> |

### QoL controls and keyboard preview

Open **Esc / F10 → Controls**. Select a coloured key or its action to rebind it;
the highlighted keys and arrows follow your bindings. The preview below shows
the actual 1.35 overlay over Amiga gameplay.

![Interactive QoL keyboard action guide in 1.35](https://github.com/b0realis/REminiscence2/releases/download/v1.35/qol-keyboard-1.35.png)

**One-click presentation presets.** Classic pixels, Widescreen scenery and
Smooth widescreen share the live Graphics page with display modes and filters.

![Presentation presets and display controls in 1.35](https://github.com/b0realis/REminiscence2/releases/download/v1.35/qol-presentation-1.35.png)

<details>
<summary>More snapshots: Sega CD jungle and the native-resolution QoL menu</summary>

**Sega CD jungle, running in 1.35.** Original game pixels enlarged 3×.

![Sega CD jungle gameplay in 1.35](https://github.com/b0realis/REminiscence2/releases/download/v1.35/segacd-jungle-1.35.png)

**The same presets at 256×224.** The compact menu scrolls its settings while
keeping Resume and the turquoise EXIT GAME button visible.

![Native-resolution presentation menu in 1.35](https://github.com/b0realis/REminiscence2/releases/download/v1.35/qol-native-presentation-1.35.png)

</details>

These are captures from the 1.35 engine and production overlay. Menu captures
use controlled session settings; gameplay uses player-supplied original assets.
Flashback artwork belongs to its respective rights holders.

## New in 1.31–1.35

- **Presentation presets:** Classic pixels uses integer fitting and borders;
  Widescreen scenery adds adjacent-room scenery; Smooth widescreen adds xBRZ.
  Filtering is consistent across the playfield and side scenery. The selected
  window mode and fixed resolution survive preset changes. Side scenery does
  not enlarge the playable area or stretch gameplay.
- **Quieter notifications:** a new journal notice lasts 3.5 seconds and fades
  during the final 0.75 seconds. The entry stays unread until opened. Autosave
  feedback uses the same top edge; simultaneous notices sit side by side,
  including at native resolution. Errors retain their longer display time.
- **Mac QoL mouse fix:** pointer polling and clicks use the window's content
  coordinates, addressing the reported fullscreen menu offset.
- **Pistol effects:** cosmetic laser beams stop at the original shot's enemy,
  wall or range endpoint, with a brief impact. Original damage and hit rules
  are preserved. The alternative styles remain optional.
- **Mac installation:** both Apple Silicon and Intel now have drag-to-Applications
  DMGs alongside ZIPs. The launcher helps locate the assets folder. Python tools
  compact owned assets, optionally convert Sega CD music to Ogg, and build a
  private self-contained Mac app. Private game bundles are not public downloads.

## Included from earlier releases

- **Live QoL controls:** open with **Esc, F10 or controller Start**. Adjust audio,
  graphics and controls during play; browse saves, levels, cheats and the journal.
  Both desktop and original-resolution layouts keep essential actions accessible.
- **Save and recovery tools:** **Ctrl+S / Ctrl+L** quick-save/load the selected
  manual slot; browse saves across levels, name and protect milestones, keep
  rotating autosaves, use checkpoint recovery and an optional rewind timeline.
  New saves carry previews; loading validates compatible data before replacing
  the running session. Automatic resume at launch is optional.
- **Keyboard and joypad support:** the graphical keyboard follows remapping;
  controller profiles include buttons, triggers and stick settings. Disconnect
  pauses gameplay. Optional assists include short input buffering, sticky Action,
  reduced shake, a steady shield-hit sprite and slower gameplay pacing.
- **Journal and objectives:** optional original companion passages and a chapter
  notebook reveal only reached content. Entries can be read in the paused menu.
- **Display choices:** windowed, borderless and exclusive fullscreen; selectable
  window sizes and reported fullscreen resolutions; integer pixel fitting;
  Point, Linear, ScaleNx and **xBRZ at 2×–6×**. Invalid mode changes retain or
  safely restore usable settings.
- **Original gameplay demos:** QoL Session can launch available Jungle, Alien
  World and Death Tower recordings from compatible DOS/GOG or Macintosh data.
  These are attract-mode replays, not extra levels. Demos cannot write progress;
  missing-map starts and malformed recordings have focused regressions.
- **Amiga, DOS/GOG and US Sega CD assets:** keep editions in separate folders,
  choose which to play, and select an independent soundtrack with native fallback.
  Original Macintosh support is retained; real Macintosh assets still need validation.
- **Sega CD media:** seven level resource sets, title graphics, **70 MCD movies**
  with embedded audio, **VCE voice dialogue** and **eight music tracks**. All 70
  extracted movies and 17,754 indexed frames matched independent extraction and
  the original 68000 decoder in the 1.25 audit. The checkerboard texture comes
  from the original 256×128, 16-colour dithering.
- **Original presentation and optional cosmetics:** skippable opening sequences
  lead to the original title menu. Space, alphabet keys and left-click can skip
  cutscenes; title navigation supports mouse and wheel input. Conrad's undershirt
  choices leave his jacket unchanged; optional pistol styles follow native muzzle
  positions across supported asset editions.
- **Asset and movie tools:** Python tools prepare WHDLoad data and extract Sega CD
  CUE/BIN assets, list/play cutscenes, and document the formats. The offline
  restoration workflow supports Real-CUGAN Pro 3× / denoise 3, original timing and
  audio, and optional tracked sign repair. Models and restored movies are supplied
  separately; the game continues to play original MCD movies.

## Downloads

| Platform | Choose |
| --- | --- |
| Windows x64 | `win64.zip` |
| Linux x64 | `linux64.tar.gz` — launch with `play.sh` |
| Steam Deck | `steamdeck.tar.gz` — add the launcher as a Non-Steam Game |
| Mac Apple Silicon, including M5 | `macos-arm64.dmg` or `.zip` |
| Older Intel Mac | `macos-x86_64.dmg` or `.zip` |
| Quest arm64 | Signed `.apk`, or the bundle ZIP with Linux/ADB deployment tools |

Supply your own prepared game files. Desktop packages include an `assets` folder;
Mac DMGs can use `~/Library/Application Support/REminiscence2/assets`. Public
packages contain no playable original assets. Save settings in QoL to retain
preferences. The corresponding-source archive includes matching SDL2/zlib sources
and dependency notices. **SHA256SUMS covers the ten package/source downloads**;
the gallery PNGs are separate illustrative attachments.

## Validation and remaining checks

All six target builds and the normal CI matrix passed for the tagged 1.35 source,
including Linux sanitizer checks. Local 1.35 passed **35 Release suites**, seven
focused Debug/UBSan suites and **17 packaging-tool tests**; 1.34 had also passed
all 35 Debug/UBSan suites. Uploaded package checksums were verified. Package,
binary-string, image-metadata and screenshot audits found no personal-data or
credential leaks; the fresh gallery was checked as well.

This remains a preview: full playthroughs, physical Deck/Quest testing, physical
Mac fullscreen retesting, console A/V timing comparisons and other Sega CD
pressings remain outstanding. Macintosh asset compatibility needs real data.
Mac apps are ad-hoc signed, not notarized. Quest runs as a flat Android panel.
