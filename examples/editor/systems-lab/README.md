<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Conrad Systems Lab

An editable five-room starship laboratory for learning the REminiscence2 editor.
Every interaction is authored with ordinary nodes and connections. Room art,
collision, climbable ledges, animation, actors, dialogue and campaign routes are
separate so you can inspect and modify each part.

## Play or edit

After building the local player and editor, and extracting your own game assets:

```sh
./play-local.sh --gog --lab --campaign-new
./edit-local.sh --lab
```

Use `--amiga` or `--segacd` instead of `--gog`. The local scripts use assets in
`.local/assets`. Editing creates a working copy in
`.local/editor-projects/SystemsLab` on first use and preserves subsequent edits.
The player uses that working copy when present, otherwise this public example.
Use a new campaign after substantial layout changes; existing saves describe the
project as it was when saved.

For a different build folder or asset location:

```sh
build/editor/reminiscence2 --datapath=/path/to/extracted/assets \
  --campaign=examples/editor/systems-lab/campaign.r2campaign.json --campaign-new
build/editor/reminiscence2-editor \
  examples/editor/systems-lab/project.r2project.json \
  --campaign=examples/editor/systems-lab/campaign.r2campaign.json \
  --assets=/path/to/extracted/assets
```

The direct editor command edits this source example. Use **Save As** to work on
another copy. Select a room or node and press **PLAY HERE / F5** to test there;
use **Shift+F5** for the authored start. The campaign view connects the playable
lab to its closing debrief screen.

## Explore the rooms

| Room | Experiments and route |
| --- | --- |
| Maintenance catwalks | Run, roll and climb the two 72-pixel steps; toggle the fan and task light; inspect the flight service panel. |
| Calibration bay | Talk to the training droid, test shooting, visit the upper charging and diagnostics stations, and approach the sliding gallery door. Enter travels through it. |
| Conrad's workshop | Repair R-03, test a contained laser pulse, inspect the powered exoskeleton, and use the upper research, server, containment and lighting controls. |
| Pressure lock | Use AURORA to seal the inner hatch, equalize pressure and unlock the outer hatch. The two doors cannot open together. Leaving right finishes the campaign. |
| Moving-target gallery | Shoot the moving plate from either firing height. Each actual hit flashes the green lamp; misses do not. Pause/resume the target at its computer and return through the doorway. |

The gallery is optional. Upper cabins are accessible by climbing the left ledge
to the middle deck, then the inward-facing ledges beside the central shaft.
Return through those shafts in controlled 72-pixel drops. Missing screen links
are sealed by the engine.

Default controls: arrows move; Space draws/holsters the pistol; Shift fires or
performs the native action; Enter uses a nearby device. Tab/Backspace or Enter
advances dialogue; Up/Down selects a response. Esc/F10 opens the QoL menu in the
standalone player. Embedded playtests use Escape/Stop to return to the editor.
See the [complete device inventory](INTERACTIONS.md) for the 33 additional
interactive control groups, alongside the repair, laser-test and airlock terminals.

## Learn from the graphs

- **Level Editing:** inspect the five 256×224 room slices, separate collision
  grid, room links and ledge handles. The atlas is `art/deck.png` (1280×224).
- **Module Placement:** select a complete connected mechanism. The project has
  eight gameplay modules and five room-ambience modules.
- **Module Editing:** follow Button/Zone, Dialogue, State, Timer, Animation and
  Sound connections; step through signals and inspect node help and comments.
  The range doorway demonstrates approach/departure events. The airlock shows
  mutual exclusion and a timed sequence. The moving target exposes native
  pistol-hit feedback. Original, Heavy and Laser variants use the same hit test.
- **Campaign:** inspect the lab's `complete` route and the text-only debrief.

Conrad is roughly 40 native pixels tall. Main floors sit at y216, middle decks
at y144 and top cabins at y72. Desks, switches and doors are sized around his
reach. Each room uses at most 26 authored background colours. Lights, fans,
monitor refresh, dust and pressure mist are editable background nodes; they
freeze with the simulation and retain their phase through saved progress.

## Included content and provenance

Newly authored project data, dialogue, artwork, animation strips, synthesized
sounds and documentation in this folder are provided under **GPL-3.0-or-later**;
see the repository [license](../../../LICENSE). The industrial backgrounds and
training-droid artwork were generated for this project, then registered to the
native room grid and reduced to a shared palette. Small effects and control
strips were authored programmatically; the WAV effects are synthesized.

This folder contains no extracted Flashback resource banks, original player
sprites, original sound recordings or publisher reference scans. Playtesting
uses Conrad, native animation/logic and other game resources from the user's
own Amiga, DOS/GOG or Sega CD asset set. Those original files keep their own
rights and are not relicensed by this example.

This fan-made lab is a learning example and a homage to Delphine Software, not
part of the original campaign. Its current dialogue pagination is functional
but remains a candidate for presentation improvements.

## Validation

`project.r2project.json` and `campaign.r2campaign.json` can be validated without
original game assets. The existing native-input walkthrough is published as
[`tests/systems_lab_tests.cpp`](../../../tests/systems_lab_tests.cpp).
`interaction-tests.json` enumerates the controls and their expected effects.

With the three asset paths configured in CMake, run:

```sh
ctest --test-dir build/editor -R editor_systems_lab --output-on-failure
```

The walkthrough checks the connected route, climbing and upper cabins,
approach/departure doors, droid combat, moving-target hits, repeatable controls,
dialogue choices/cancellation, airlock interlocks and saved animation state.
Asset-backed cases are registered only for the editions supplied to CMake;
project/campaign validation remains available without them. Test screenshots
are written under the build directory and are not part of the source example.
