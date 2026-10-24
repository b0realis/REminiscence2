<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Recovery and accessibility — 1.10

This local patch follows the four-campaign source review. The published GitHub
release remains v1.06; this document does not claim new platform releases.

## Recovery without losing your place

After the death scene, a paused recovery screen offers rewind moments, the
current checkpoint, all retained autosave generations, and a cross-level manual
save browser. Desktop recovery shows three preview cards. At 256×224, the three
main recovery buttons come first; **Previews / older moments** expands details.
The first available recovery action receives keyboard/controller focus.

Escape, F10 and controller Back/Start cannot resume a dead game. Choose a recovery
point, restart the current level, return to title or use the turquoise **EXIT GAME**
button. Restart/title/exit still ask before leaving the current session.
Failed loading keeps recovery open and preserves the live state.

## Manual saves and milestones

**Save / Load** browses all seven levels without starting them. Choose a level
and slot, or select an occupied entry in the list. Load switches levels only
after serialized state and the destination's actual resources pass preflight.
Saving is allowed in the current playing level, while Conrad is alive.

Expand **Name / milestone protection** to enter an optional name (up to 96
printable ASCII characters), and choose **Protect from overwrite**. For an
existing save, **Update name / protection** changes only its metadata. To replace
a protected milestone, clear protection and explicitly Update first. Quick save
also refuses protected slots. Names and protection are independent of autosave
rotation; milestones occupy normal manual slots 1–99 until deliberately unlocked.
Slot 00 remains the in-game checkpoint and cannot be renamed or locked here.

New manual/checkpoint files use names such as
`reminiscence2-0-level1-manual-7.r2m`. A checksum covers the complete state,
preview, timestamp, name and protection flag; replacement is atomic. The game
state payload remains version 4. Existing v2/v3/v4 `.state` files still load,
including across levels. Legacy files have no preview/name/protection, and v2/v3
cannot identify their asset revision. A modern bundle takes precedence over an
older `.state` file in the same edition/level/slot. Neither is deleted on import.

Save files and their captured game artwork are private player data. `.state`,
`.r2s` and `.r2m` are excluded from tracked source and release packages.

## Visible feedback and diagnostics

Save, load, screenshot, autosave and device messages appear in a bounded HUD
notice. Failures stay visible for eight seconds; routine autosave success cannot
hide them. Other success notices last 3.5 seconds. The overlay also retains command
errors until another action changes its status.

**Diagnostics** shows the detected edition/language, save directory, primary
level-resource presence, optional voice/movie availability, active controller
profile, audio connection and measured last/worst save duration. Presence checks
are a quick diagnosis, not a replacement for the full loader's validation.

## Controllers

Unplugging the active pad during gameplay clears held input and opens a blocking
pause before the next simulation step. Reconnection never resumes by itself;
use Resume when ready, or continue with the keyboard. Unrelated devices cannot
control gameplay or binding capture.

Mapped controllers support LT/L2 and RT/R2 bindings in addition to buttons.
Select an action in **Joypad**, then squeeze a trigger. Capture uses 16000/10000
press/release thresholds to resist jitter. D-pad and Start remain reserved.
Raw joysticks retain their independent numbered-button bindings.

**Save settings** also stores pad mappings, dead zone and right-stick preference
for the connected SDL GUID, separately for mapped/raw devices. On reconnect the
matching `controller-<GUID>-mapped.cfg` or `-raw.cfg` is loaded from the save
folder. Other display, keyboard and audio preferences stay global. An unseen
controller inherits the configured global pad bindings. Identical devices
sharing an SDL GUID share a profile. PlayStation controllers use their detected
Cross/Circle/Square/Triangle labels; other mapped pads use SDL's A/B/X/Y names.

## Optional assists and notebook

All gameplay assists default off; gameplay speed defaults to 100%.

| Enhancements setting | Behavior |
| --- | --- |
| Sticky Action button | Press to hold Action, press again to release. Affects run and firing a drawn weapon. Pause/focus loss/disconnect clear the latch. |
| Short input buffer | Keep Up/jump, Use and Weapon taps for at least 100 ms during gameplay, including down/up events delivered in one poll. Pause/focus loss/disconnect clear buffered input. |
| Reduce screen shake | Present gameplay without scripted camera displacement. Simulation and random-number consumption remain unchanged. |
| Steady shield-hit sprite | Keep Conrad visible during shield-hit blinking. The original invulnerability timer still runs; cutscenes/movies retain their artwork. |
| Gameplay speed, 50–100% | Give more reaction time by pacing unchanged simulation steps more slowly. Music and cutscenes retain normal speed; Fast mode still overrides pacing. Autosave/rewind/hint clocks measure simulation time and slow with this setting. |

Graphics already provides menu text scaling from 75–175%, capped at 125% at
native resolution so navigation remains visible. These options do not promise
to eliminate flashing from original cutscene/movie assets.

**Objectives** is an optional chapter guide alongside **Journal**. It shows only
chapters reached in the current save and a memory-viewing note after that event.
Gentle hints remain collapsed until opened. Rewind and loading an older save
restore this knowledge automatically. It is a companion guide, not a puzzle-by-
puzzle completion tracker or a source of future-story revelations.

## Audio output loss

A missing/disconnected output no longer terminates the game. With no device
thread, the main thread advances the mixer into a bounded discard buffer, so
voice playback and dialogue do not wait indefinitely. Focus/background pauses
also pause this silent clock. Reconnecting an output retries the existing mixer
sample rate; failure keeps the silent fallback and displays a notice.

## Validation

The regression suites cover protected overwrite refusal, atomic metadata
updates, damaged/truncated bundles, legacy imports, cross-level real-Amiga
loading, death recovery, trigger capture/hysteresis, sticky/buffered input,
slower pacing, profile isolation, notebook visibility and clock-wrap notices.
SDL virtual pads and dummy audio cover disconnect/reconnect. The production
overlay is rendered at desktop and native resolutions, including multiple
simultaneous preview textures and renderer reset.

Local Debug/UBSan write samples, including capture/validation/write, stayed below
one normal 33.3 ms gameplay step in the recorded run. The observed worst sample
was about 10.2 ms on this Mac. The Diagnostics counter remains available for
measurement on actual player storage. A background writer was not introduced
without evidence of problematic stalls; Deck/Quest profiling is still required.
Physical device testing, Windows/Linux/Quest runtime checks, actual GOG DOS/Mac
CD assets and full playthroughs remain outstanding. ASan is unavailable on this
host; UBSan is the working sanitizer used here.
