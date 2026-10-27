<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# REminiscence2 screenshots

Select an image to view its full resolution. The new sections use synthetic
session data and original preview patterns; the older gameplay captures below
were made with player-supplied Amiga assets in `0.1.0-dev`.

## Display modes and xBRZ (1.30)

Production-overlay captures with synthetic session data. Windowed, borderless,
exclusive fullscreen and monitor resolution choices work alongside Point,
Linear, ScaleNx, xBRZ and whole-native-pixel fitting. The compact layout scrolls
its controls while keeping Resume and EXIT GAME visible.

![Desktop Graphics controls](qol-graphics-1.30.png)
![Original-resolution Graphics controls](qol-native-graphics-1.30.png)

## Sega CD release captures (1.25)

Preserved from the 1.25 release: real Sega CD gameplay with nearest-neighbour
enlargement and the production keyboard action guide.

![Sega CD gameplay](segacd-gameplay-1.25.png)
![Keyboard guide from 1.25](qol-keyboard-1.25.png)

## Recovery, manual milestones and objectives (1.10)

Verified production-overlay captures with synthetic save metadata and original
color grids. Actual play captures the player's game frame in each preview.
Native recovery places the three main actions above expandable preview details;
all pages retain their fixed footer and scroll when necessary.

![Recovery choices and previews](qol-recovery-1.10.png)
![Native recovery](qol-native-recovery-1.10.png)
![Cross-level manual browser and protected milestone](qol-manual-browser-1.10.png)
![Native manual browser](qol-native-manual-browser-1.10.png)
![Objectives notebook with folded hint](qol-objectives-1.10.png)
![Native objectives notebook](qol-native-objectives-1.10.png)

## Continue, autosaves and rewind (1.08)

Production-overlay captures with synthetic data and an original color-grid
preview, without commercial artwork. During actual play the preview is the
captured game frame. Native views use scrolling content and keep EXIT GAME
visible; the welcome screen offers New game in the other footer button.

![Welcome-back Continue](qol-welcome-1.08.png)
![Persistent autosave generations](qol-autosaves-1.08.png)
![Rewind timeline](qol-rewind-1.08.png)
![Native welcome](qol-native-welcome-1.08.png)
![Native autosaves](qol-native-autosaves-1.08.png)
![Native rewind](qol-native-rewind-1.08.png)

## Joypad and journal (1.07)

Production-overlay captures using synthetic session data, without game artwork.
The smaller views are the actual 256×224 layout, with scrolling content and
permanently visible Resume / EXIT GAME controls.

![Joypad mappings and stick settings](qol-joypad-1.07.png)
![Optional story journal](qol-journal-1.07.png)
![Native joypad page](qol-native-joypad-1.07.png)
![Native journal page](qol-native-journal-1.07.png)

## High-resolution QoL menu

The production SDL renderer and Dear ImGui overlay draw these **1920×1080**
captures over a paused game. The save browser shows two actual manual saves.
The images are lossless PNG conversions of captured renderer output.

### Enhancements

Conrad's T-shirt choices, rewind, inventory behavior and cutscene presentation.
**Resume game**, **Save settings** and the turquoise **EXIT GAME** button remain
at the bottom of the overlay.

![Enhancements page at 1920×1080](qol-highres.png)

### Save / Load

Manual slots, room and modification time, checkpoint loading and stored saves.

![Save and load page at 1920×1080 with two saved slots](qol-save-load.png)

### Graphics

Display controls, pixel filtering, window scale, side content and menu text size.

![Graphics page at 1920×1080](qol-graphics.png)

## Original-resolution QoL menu

At **256×224**, a page selector replaces the desktop tabs. The contents scroll
while **Resume** and **EXIT GAME** stay visible. This PNG retains its native
resolution.

![Compact QoL menu at the original 256×224 resolution](qol-native.png)

## Keyboard / action guide

Controls shows a full keyboard with colored primary action keys, darker unused
keys and arrows pointing to the corresponding actions. Select a colored key or
callout to rebind it. Highlights follow the actual preferences, including custom
profiles, resets and changes made during play.

![Live keyboard diagram with the default controls at 1920×1080](qol-keyboard.png)

<details>
<summary>Compare a custom WASD layout</summary>

This capture applies WASD movement, K for actions, E to use, R for the weapon,
I for inventory and P for Pause. The old arrow, Shift, Space, Tab and Escape
primary keys become dark; the colored keys and arrows move to the new bindings.
The fixed F10 recovery shortcut remains available.

![The same keyboard diagram after changing to WASD and custom action keys](qol-keyboard-wasd.png)

</details>

At the original resolution, the guide keeps each key and its action readable
in scrollable rows instead of shrinking the entire keyboard.

![Colored key caps and action callouts at 256×224](qol-keyboard-native.png)

## Amiga gameplay

The earlier seven-level captures are preserved below at **512×448**, a 2×
nearest-neighbor presentation of the original **256×224** game frame. These
are Amiga captures; they do not demonstrate DOS/GOG or Macintosh CD assets.

<details>
<summary>Show the seven-level overview</summary>

![Contact sheet of all seven Amiga levels](amiga-contact-sheet.png)

</details>

| Level 1 | Level 2 |
| --- | --- |
| ![Amiga level 1](amiga-level1.png) | ![Amiga level 2](amiga-level2.png) |

| Level 3 | Level 4 |
| --- | --- |
| ![Amiga level 3](amiga-level3.png) | ![Amiga level 4](amiga-level4.png) |

| Level 5 | Level 6 |
| --- | --- |
| ![Amiga level 5](amiga-level5.png) | ![Amiga level 6](amiga-level6.png) |

![Amiga level 7](amiga-level7.png)

## Visual editor, 1.36

![Native level editor with paired door modules](editor-workspace-1.36.png)

This capture shows the actual SDL editor after its widget regression placed a
second door/activator pair. The Workshop artwork is newly drawn project material,
not extracted game scenery. Its provenance and GPL-3.0-or-later grant are in
`examples/editor/workshop/README.md`. Dear ImGui UI/font notices still apply.

## Module graph editor (1.37)

![NPC item requirement and door connection](editor-module-graph-1.37.png)

An actual SDL renderer capture of Module Lab's retrieval graph. The gold wire
requires the signal module; the turquoise completion wire opens the door.
The interface uses Dear ImGui; the project names and example data are original
REminiscence2 material. No original game sprites appear in this screenshot.

## Editor design guidelines (1.38)

![Built-in artwork and pixel-grid guidance](editor-guidelines-1.38.png)

Actual SDL-rendered Guidelines dialog over the original-art Module Lab example.
The text and sample artwork were created for REminiscence2. Dear ImGui UI/font
notices apply. Private original-level reference captures remain outside the repo.

## Graph navigation (1.39)

![Graph zoom, frame-all and connected module nodes](editor-graph-navigation-1.39.png)

Actual SDL-rendered graph after automated pan/zoom and Frame all input. The
button/door graph and UI are project-authored; there is no original game artwork
in this capture. Native campaign editor captures remain private. Dear ImGui
UI/font notices apply.

## Playtest from the current room (1.40)

![PLAY HERE and the session-only TEST marker](editor-play-here-1.40.png)

Actual SDL capture after clicking Place TEST marker in the Workshop UI test.
The room artwork is project-authored; the marker does not change the campaign's
START. Dear ImGui UI/font notices apply. No original game artwork is shown.

## Editor Settings (1.41)

![Campaign setup, playtest preference and event coverage](editor-settings-1.41.png)

Actual SDL-rendered settings dialog over a custom puzzle graph. It shows the
native campaign setup button and explicit event support status. Only authored
UI/Workshop content is shown; native campaign captures remain private. Dear
ImGui UI/font notices apply.

## Dialogue modules and three workspaces (1.42)

![Separate workflows and a branching dialogue/animation puzzle](editor-dialogue-module-1.42.png)

Actual SDL software-rendered Module Editing workspace after automated tab,
module-creation, node-palette and graph-frame controls. The sample connects a
button to a two-choice conversation, an animation and a door. The UI and all
sample text are project-authored. No original game imagery appears. Dear ImGui
UI/font notices apply; native campaign captures remain private.

## State-machine playback (1.43)

![State-machine playback with active nodes and a signal trace](editor-state-machine-1.43.png)

Actual SDL software-rendered editor after Start and three single-wire steps.
The authored airlock example shows the active state, running dialogue and last
connection delivered. All text and nodes are project-authored; no game imagery
or native resource tables appear. Native rule-editor captures remain private.
Dear ImGui UI/font notices apply.

## Game screenshot credits

Flashback was created by Delphine Software. Its original artwork and other game
content remain copyrighted by their respective rights holders. These screenshots
document the engine's gameplay and interface; the engine's GPL license does not
grant rights in the game artwork shown. The SPDX notice above covers this
project-written gallery text, not the PNGs.

The QoL interface is REminiscence2 work using the MIT-licensed Dear ImGui UI
and font. Dependency notices are retained in
[third_party/imgui](../../third_party/imgui/README.md).
The supplied publisher-manual illustrations, comic, protection sheets and cover
scans remain private references and are not part of this gallery. Players still
supply their own original game files.

- `editor-campaign-1.45.png`: actual fourth-workspace stage routing and story inspector.
- `editor-story-layout-1.45.png`: actual draggable 640×360 text/photo composition preview.
- `editor-node-help-1.45.png`: actual node behavior/port help and designer-comments window.
  These captures use original authored test content and include no Flashback game assets.
