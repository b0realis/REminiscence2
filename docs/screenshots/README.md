<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# REminiscence2 screenshots

Select an image to view its full resolution. The new sections use synthetic
session data and original preview patterns; the older gameplay captures below
were made with player-supplied Amiga assets in `0.1.0-dev`.

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

## Image credits

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
