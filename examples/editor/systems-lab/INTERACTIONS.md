<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Lab interaction guide

Walk near a device and read the **Enter** prompt. Adjacent controls use the nearest device; upper and lower decks do not compete. Use Up/Down for dialogue choices. Switches work immediately. All experiments are optional; the pressure lock remains the only exit gate.

| Room | Floor | Device | Interaction |
|---|---:|---|---|
| room-1 | 144 | Turn off Ventilator | toggle |
| room-1 | 216 | Turn off Task light | toggle |
| room-1 | 72 | Flight service panel | pulse |
| room-2 | 216 | Talk to training droid | read |
| room-2 | 216 | Inspect disabled droid | read |
| room-2 | 144 | Turn off Range light | toggle |
| room-2 | 72 | Charging cradle | pulse |
| room-2 | 72 | Droid diagnostics | choice |
| room-2 | 72 | Range telemetry | choice |
| room-2 | 72 | Shot recorder | read |
| room-2 | 72 | Range power rack | read |
| room-3 | 216 | Talk to R-03 | read |
| room-3 | 144 | Oscilloscope | choice |
| room-3 | 144 | Containment console | choice |
| room-3 | 144 | Exoskeleton cradle | pulse |
| room-3 | 216 | Turn off Workshop light | toggle |
| room-3 | 72 | Research terminal | choice |
| room-3 | 72 | Repair arm | pulse |
| room-3 | 72 | Ship systems server | read |
| room-3 | 72 | Personal archive | read |
| room-3 | 72 | Turn off Battery LEDs | toggle |
| room-4 | 216 | Turn off Airlock light | toggle |
| room-4 | 216 | Flight status workstation | choice |
| room-4 | 72 | Turn off Cabin blower | toggle |
| room-4 | 72 | Atmosphere controls | read |
| room-4 | 72 | Emergency locker | read |
| room-4 | 72 | EVA suit self-test | pulse |
| room-5 | 144 | Target computer | target |
| room-5 | 216 | Turn off Gallery light | toggle |
| room-5 | 72 | Spare target discs | read |
| room-5 | 72 | Range supply cabinet | read |
| room-5 | 72 | Ballistics analysis screen | choice |
| room-5 | 72 | Carriage service rack | read |

Existing repair, optical-test and AURORA terminals retain their experiments; both gallery doorways retain approach animation and Enter-to-travel. Lamps and the target-hit indicator are feedback, not switches. Computer glow is deliberately restrained and affects background scenery only.

Each floor is 72 pixels above the last. All new device anchors sit on reachable ground or a platform. The native-input walkthrough test uses interaction-tests.json to visit every authored device, exercise choices and repeated toggles, and preserve state through save/load.
