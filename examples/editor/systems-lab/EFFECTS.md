<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Lab particles and their controls

The lab uses small native-pixel effects to make machinery feel active. They are
ordinary **Background machinery** nodes, placed on the **Back** layer. Conrad,
foreground art and physical objects draw afterward. There is no extra damage,
collision, audio or screen shake. Player/editor **1.78 or later** is required
for the Electrical sparks style.

| Where | Node | Visible effect | Starts / stops with |
| --- | --- | --- | --- |
| Maintenance fan | `ambience-1 / vent-drift` | Six light dust motes within the fan recess, above the platform | Fan switch and original fan start signal |
| Charging cradle | `ambience-2 / charging-sparks` | Brief warm flecks at the coil | Accepted charging test; its two-second timer stops the emitter |
| R-03's bench | `android-repair / scan-motes` | Fine motes in the diagnostic illumination | Accepted scan; its existing timer stops them |
| Optical bench | `laser-test / receiver-sparks` | A short contained discharge at the beam receiver | Accepted test pulse; its existing timer resets the emitter |
| Sample enclosure | `workshop-life / sample-motes` | Slow particles inside the display field | Sample-field on/standby choices |
| Gallery task light | `ambience-5 / lit-dust` | Motes within the lit lower bay | Local light switch |
| Pressure lock | `airlock / mist` | Slightly fuller vent wisps during equalization | Cycling state's Entered signal; Ready stops the effect |

Existing wide, sparse dust remains in the maintenance bay, workshop and gallery.
The charging and laser sparks are brief test feedback, not perpetually broken
machines. The moving target's green hit lamp retains its existing meaning;
background particles cannot count as hits.

## Inspect or change an effect

Open the project, select its module, and switch to **Module Editing**. Select
one of the nodes above. Its inspector exposes the effect style, rectangle,
density and loop period; comments identify the control it follows. Each added
emitter receives the same incoming Activate, Stop and Reset wires as the related
fan, scan, beam or lamp. Their effects can therefore share the mechanism's timer
without creating another state machine.

X is the rectangle's left edge and Feet Y its bottom. Keep the rectangle clear
of ledge silhouettes and control labels. Dust and mist drift within an ellipse;
sparks start at its horizontal centre, one quarter down from the top, then
scatter and fade. Density 1, 2 and 3 emits 5, 7 and 9 spark trajectories. The lab
uses density 1 and a four-second loop for both discharges, with a two-second
experiment timer: each activation produces one short burst.

To restart a running effect, send **Reset**, then **Activate**. Activate alone
leaves the current phase running. **Stop** removes the effect immediately and
restores the underlying art. Particle phases follow simulation time, so pause,
dialogue and save/load preserve them; drawing more frames cannot change the
random gameplay sequence.

Use **PLAY HERE** to inspect the effect beside its device. The lab walkthrough
checks fan/light toggles, charging and sample choices, cancellation, repeated
experiments and saved phases. The engine's ambient checks also cover sparks at
the screen edge, foreground occlusion, quiet intervals and long-running phases.
