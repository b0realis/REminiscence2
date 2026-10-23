<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Player guide provenance

[manual.md](../../manual.md) is original REminiscence2 documentation, distributed
under GNU GPL version 3 or any later version. It is written around the current
engine, its command interface and Python tools. It is not a transcription,
translation, abridgment or illustrated edition of the publisher's manual.

The guide covers launching, asset preparation, current controls, functional
movement patterns, saving, display/audio choices, cutscene selection, building,
command options, configuration and troubleshooting. Its only diagrams are
plain-text arrangements of ordinary keyboard labels. It contains no copied
comic panels, screenshots, sprites, cover artwork, story dialogue, publisher
support/warranty passages or other proprietary illustrations.

## Implementation references

- [main.cpp](../../main.cpp) and [cli_text.cpp](../../cli_text.cpp): command
  switches, bounds, defaults and built-in help.
- [systemstub_sdl.cpp](../../systemstub_sdl.cpp), [game.cpp](../../game.cpp) and
  [piege.cpp](../../piege.cpp): input bindings, save handling and gameplay input.
- [config.cpp](../../config.cpp) and [fb.cfg](../../fb.cfg): built-in and example
  compatibility choices.
- [extract_amiga.py](../../tools/extract_amiga.py) and
  [play_cutscenes.py](../../tools/play_cutscenes.py): preparation and playback.
- [Validation record](../VALIDATION.md): evidence and remaining compatibility work.

## Superseded material

An earlier local draft converted the supplied Macintosh manual and embedded
54 extracted/rendered PNGs. That draft, its image manifest and all 54 PNGs have
been removed from the public documentation tree. The original PDF was already
kept under ignored local storage and was never bundled with the player guide.

For local research continuity, the superseded conversion and its provenance
are held only in `.local/reference/restricted/manual/`. Other supplied images
and PDFs without a recorded redistribution grant are likewise held in the
private reference archive. They are not distributed under the guide's license.

The documentation check rejects those asset locations and embedded images in
the text-only player guide. `.gitattributes` excludes private reference/build
paths and the removed asset locations from `git archive` output. These are
checks for known material, not a legal determination about every project file.
See [licensing and provenance](../LICENSING.md) for the separate engine and
third-party notices and unresolved inherited-license questions.
