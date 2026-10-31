<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Writing the Systems Lab

The lab is an original fan-written interlude, not a missing chapter of the
published campaign. Its premise draws on Conrad's loss of memory, the jungle
on Titan, his return towards Earth, and the original campaign's questions about
identity and appearances. The publisher's [original Flashback synopsis](https://www.microids.com/game-flashback/)
describes the memory-loss/Titan/Earth premise. New Washington and the Death Tower
are original campaign locations; the personal ship, its crew, records and
experiments here are inventions for this editor example. This story does not
adopt the later Flashback 2 weapon or plot.

## Three voices

- **Conrad** notices things he can touch: a weld, a worn contact, an impact in
  a target plate. He is a scientist trying to reconstruct his reasoning as well
  as a survivor trying to trust his habits. Uncertainty makes him careful; it
  does not make every observation a joke.
- **AURORA** distinguishes a measurement from a conclusion. She states what a
  test can establish, what remains unknown, and what action the player is about
  to authorize. Her limits are part of the relationship Conrad has chosen.
- **R-03** remembers routine decisions whose context Conrad has lost. He is
  patient and precise. His damaged joint and intact records let a maintenance
  conversation become a conversation about memory without inventing a cure.
  **T-01**, the passive training android, brings the same question into the
  range: a familiar face is insufficient evidence of intent.

## Threads to follow

| Place | Narrative purpose |
| --- | --- |
| Maintenance catwalks | Conrad wakes expecting jungle leaves; the ship's imperfect machinery is familiar enough to steady him. Navigation carries an unverified docking record. |
| Calibration bay | T-01 explains the discarded recognition profile. The range's physical safety isolation, archived shots and incomplete service history give each prop a reason to be examined. |
| Workshop | R-03 recalls an erased entry. Conrad's archive questions what a recorded voice can prove; his research notes preserve conclusions whose reasoning he cannot recover. |
| Optical and sample benches | A repeating carrier and a layered crystal invite investigation. AURORA refuses to label either as proof of Morph activity. These are unresolved observations, not extra objectives. |
| Pressure lock | An accepted identity does not replace a pressure test. Independent pumps, an emergency kit and an empty suit reflect practical survival habits. |
| Target gallery | The Death Tower's audience contrasts with private practice, physical evidence of fatigue and the freedom to stop. |
| Debrief | Leaving is Conrad's decision. The text does not claim that optional repairs, research or shooting practice were completed. |

The droid exchange is authored as connected **Dialogue** nodes:
`r03-talk-log → r03-conrad-reply → r03-memory-reply → r03-conrad-close`.
Each **Completed** output activates the next turn. T-01 and its disabled chassis
use the same pattern. Select a node in **Module Editing** to read the text,
speaker and intent comments, or follow the animated signals while playtesting.

## Reading at native resolution

The project contains 41 dialogue nodes: 35 rewritten entries and six added
speaker turns. Each node has one speaker, and the body does not repeat its name.
The current renderer wraps at 30 characters and allows five lines per page,
or three when the node has choices. The first page also includes the speaker.
The authored line breaks end pages on complete thoughts; blank lines finish
that page's remaining space rather than beginning a new thought halfway down it.
Do not remove them blindly when editing long passages.

Choices keep their existing ordering and connections, with labels tailored to
the device. Inspecting logs does not add items, scanning R-03 does not fully
repair his joint, and illuminating a monitor does not fly the ship. The target
computer still owns target motion; the separate analysis screen only controls
its display. A cancellation remains a cancellation.

The text can be changed directly in the editor. This revision changes no room
art, geometry, damage, weapon trajectory or experiment duration. Speaker-colour
and text-backing improvements remain separate presentation work.
