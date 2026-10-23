# Flashback technical format references

These notes support format research and comparisons with the original game.
The cutscene reference maps directly to the existing renderer. The 3DO notes
describe related resource formats and edition-specific presentation features
for future research; 3DO game data is not a supported engine target yet.

| Document | Pages | Contents |
| --- | --- | --- |
| `flashback_cutscene.pdf` (private reference) | 2 | PC POL/CMD resources, Amiga CMP packaging, opcode behavior, page buffers, polygon/ellipse/pixel primitives, transformations, palette blending and protection shapes. |
| `flashback_3do.pdf` (private reference) | 3 | Tile-based room rendering, resource extensions, external language text, Cinepak movies, subtitle timing/bitmaps, sound samples, checkpoint feedback, menu animation encodings and demo-specific music behavior. |

The source PDFs are held in ignored `.local/reference/restricted/technical/`
storage. They are not distributed with these project-written research notes.

## Cutscene work

The POL offset tables and CMD descriptions help define bounds checks for shape,
vertex, palette and command reads. The buffer diagrams and timing descriptions
provide useful scenarios for testing background reuse, palette changes,
captions, transformations and frame presentation.

Relevant source:

- [cutscene.cpp](../../../cutscene.cpp) and [cutscene.h](../../../cutscene.h):
  command interpretation, timing, transforms and page buffers.
- [staticres.cpp](../../../staticres.cpp): opcode dispatch and embedded tables.
- [graphics.cpp](../../../graphics.cpp): drawing primitives and clipping.
- [resource.cpp](../../../resource.cpp) and [unpack.cpp](../../../unpack.cpp):
  POL/CMD loading, Amiga CMP containers and Bytekiller decompression.
- [protection.cpp](../../../protection.cpp): reuse of the cutscene renderer for
  the shape challenge described in [protection settings](../copy-protection/README.md).

The PDF says there are 14 opcodes, but its table lists 0 through 14: 15 entries,
matching `Cutscene::NUM_OPCODES` and the current dispatch table. Slot 12 is named
`drawCreditsText` in the PDF and `op_copyScreen` in this tree. Compare behavior
as well as names when turning these notes into regression cases.

The PDF's 240-by-128 buffer description identifies the animation region. The
current `Cutscene::prepare()` clips that region within the larger video layers,
scaled for the active edition. Preserve those allocation and coordinate
distinctions when applying the descriptions to Macintosh rendering.

## 3DO research

The resource and rendering notes can guide comparisons with the Amiga adapters
in [resource.cpp](../../../resource.cpp) and [video.cpp](../../../video.cpp).
Their extension table names MBK tiles while the rendering text mentions BNQ;
the RP entry has no description. Actual 3DO files are needed to resolve those
details before implementing a loader.

The Cinepak movies and DAT/SUB subtitle information describe a potential video
path distinct from the current DOS SEQ player in
[seq_player.cpp](../../../seq_player.cpp). The text and menu-animation sections
also identify additional edition resources to inspect.

The checkpoint section documents the spoken save confirmation already reflected
in our `play_gamesaved_sound` option. The US-edition observations about music
during demos could inform an optional background-music mode, with gameplay and
demo behavior compared separately. These notes do not establish compatibility
with the current music or sound loaders.

## Provenance

Both PDFs were supplied by the project owner on 2026-10-23. Their titles and page
counts were checked against the supplied files. The PDFs are preserved byte for
byte; only the attachment system's numeric filename prefixes were removed.

No author or redistribution license is identified in the supplied copies.
The PDFs and their extracted/rendered review material remain private and are
excluded from public source/release archives. The engine source code's GPL does
not relicense these documents.
