<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Cinematic formats

[Index](README.md) · [Cutscene browser](../CUTSCENE_PLAYER.md)

There are three different runtime paths: polygon command streams (Amiga/DOS/Mac),
PC CD SEQ replacement movies, and Sega CD MCD movies. Matching file names do not
make their formats interchangeable. Catalog support differs from automatic
in-game replacement selection.

## Amiga CMP

[cutscene_assets.cpp](../../cutscene_assets.cpp) reads two consecutive blocks:
polygons first, commands second. Each begins with s32be length. A negative
length means raw bytes of its absolute length; positive means Bytekiller-packed
bytes whose expanded size comes from the trailer. Length must be nonzero, even
and bounded; maximum 16 MiB packed/expanded per block. Padding may follow block 2.
The results are equivalent to a POL/CMD pair, not independent bitmap frames.

**Title-picture CMP is a different wrapper:** `load_CMP_menu()` reads one u32be
packed length and Bytekiller block, then `AMIGA_decodeCmp()` interprets a palette
and planar picture. Do not send menu CMP to the movie two-block parser.

## CMD / PMOV parts and instructions

Commands use BE words even on DOS. The leading u16 count defines a part directory;
body starts at `(count+1)*2`. Non-Mac part 0 begins at the body start and subsequent
u16 offsets are body-relative. Count 0 is accepted as a single body. Mac PMOV uses
nonzero count and absolute payload-relative offsets, including part 0. The parser
rejects offsets before the command body or outside the buffer.

An instruction byte with bit 7 set terminates. Otherwise `byte >> 2` selects the
opcode below; the low two bits do not select a different opcode. Variable-length
instructions must be decoded sequentially, not searched as raw byte signatures.
Dispatch: `Cutscene::_opcodeTable` in [staticres.cpp](../../staticres.cpp), handlers
and special-case timing in [cutscene.cpp](../../cutscene.cpp).

| Opcode | Handler / operands |
| --- | --- |
| 00, 05 | `markCurPos`: update command bookmark and frame presentation |
| 01 | `refreshScreen`: u8 clear-screen flag |
| 02 | `waitForSync`: u8 delay parameter; timing differs for credits |
| 03 | `drawShape`: u16 shape/flags, optional s16 X/Y if bit 15 |
| 04 | `setPalette`: u8 palette index, u8 destination selector |
| 06 | `drawCaptionText`: u16 text ID, `ffff` clears |
| 07 | No operands / no-op |
| 08 | Skip three bytes |
| 09 | Refresh all, then the key-branch table used by opcode 0e |
| 0a | Scaled shape: shape/optional position, u16 zoom delta added to 512, u8 pivot X/Y |
| 0b | Rotated/scaled shape: shape/optional position; optional zoom word if bit 14; pivot bytes; first angle word; optional second/third angles if bits 13/12 (defaults 180/90) |
| 0c | Copy/display page; special memory/credits behavior |
| 0d | Positioned text: u16 ID/color; unless `ffff`, s8 X/Y measured in 8-pixel units |
| 0e | Key-condition/branch table terminated by mask `ff`; see `op_handleKeys()` for conditional jumps and loop/bookmark semantics |

Game events map to filename plus part number using edition tables; file-part
numbers are not game event IDs. Some parts wait for keys or deliberately loop.
Captions and credits also have edition-specific timing; a parser that renders
shapes but ignores synchronization will not reproduce the scene.

## POL / POLY shapes

Header u16be fields are offsets relative to the polygon-buffer start:

| Header offset | Points to |
| --- | --- |
| 02 | Shape offset table (u16be relative to shape data) |
| 06 | Palette banks, 32 bytes per 16-color bank |
| 0a | Vertex offset table (u16be relative to vertex data) |
| 0e | Shape data |
| 12 | Vertex data |

Shape index uses low 11 bits of the command's shape word. Each shape begins with
u16be primitive count. A primitive starts with u16be vertex-index/flags; low 14
bits select the vertex table, bit 15 adds s16be translation X/Y, bit 14 enables
the engine's alpha-color behavior. One following byte selects color. The command
state chooses the screen's `c0`/`d0` palette range.

Vertex records start with a byte: high bit indicates an ellipse with s16be center
X/Y and u16be radii; zero indicates a point with s16be X/Y; otherwise it is a
polygon with initial s16be X/Y followed by that many signed 8 delta pairs, yielding
one more vertex. The renderer coalesces certain consecutive horizontal edges.
Scale/rotation uses the same geometry through the transform handlers.

## Captions and text files

Amiga `<language>CINE.TXT` is split into newline-delimited strings (up to 117 table
entries). DOS/Sega use `<language>CINE.BIN` offsets plus `.TXT` payload; the current
getter interprets the offset words as BE and uses compiled Japanese fallback
where applicable. Mac uses `Movie strings` STRS, with English/French CD-name
fallback. These text layouts are unrelated to RIFF or the disc program `.BIN`.

## SET subset

The engine retains `playSet()`, `drawSetShape()` and `readSetPalette()` for the
specific original vector-animation layout and embedded memory-scene restoration.
SET is not an arbitrary video container and is not listed by the Python browser.
Its code includes shape/palette offset tables and per-frame compositions; no
independent comprehensive SET corpus is tested here. Treat its exact consumer
in `cutscene.cpp` as the executable specification until dedicated fixtures exist.
Do not infer 3DO or Sega CD video support from this path.

## CRD credits

DOS optionally reads `CREDITS.CRD`, at most 65536 bytes, otherwise using built-in
credits. [credits.h](../../credits.h) validates the control stream: `ff` ends,
`00` resets the text segment, `fe` consumes one delay byte, `01` consumes X/Y
position bytes, other bytes append text (maximum 498 bytes per segment). The
renderer uses eight-pixel position units and scene-dependent delay handling.
Mac `Credit strings` has its separate STRS resource path.

## PC CD SEQ

Demuxer/player: [seq_player.cpp](../../seq_player.cpp); blocks:
[decode_seq.cpp](../../decode_seq.cpp). File cap 1 GiB. Initial 256 bytes are zero,
followed by 30 u16le staging-buffer capacities. Physical frame/chunk stride 6144.
The current demuxer preloads 100 chunks before playback. At each chunk start:

| Offset | Field |
| --- | --- |
| 0 | u16le audio offset, zero when absent |
| 2 | u16le palette offset, zero when absent |
| 4 | u8 completed video-buffer ID (`ff` means none) |
| 5–7 | Three u8 destination staging-buffer IDs |
| 8–15 | Four u16le offsets delimiting up to three video-buffer fragments |

Fragments append to their designated bounded staging buffers. A completed video
buffer begins with 128 bytes of two-bit block opcodes, LSB-first, covering 32×16
blocks of 8×8 pixels. The video is 256×128 centered at Y48 in 256×224; unchanged
blocks retain previous pixels.

| Two-bit block opcode | Data |
| --- | --- |
| 0 | Leave previous block unchanged |
| 1 | u8 mode/length. High-bit clear: palette length 1..16, palette bytes and packed indices (minimum 1 bit). High-bit set: signed 4-bit run lengths totaling 64, then run colors/literals; low 2 bits 1=row-major, 2=column-major. Negative run repeats a byte. |
| 2 | 64 raw pixel bytes, row-major |
| 3 | Position/color pairs; low 6 position bits select pixel; bit 7 on position terminates the patch list |

Palette payload is 768 RGB bytes. Audio payload is 882 **big-endian signed 16 mono**
samples per chunk, played at 22050 Hz; video scheduling is 25 Hz. Audio uses a bounded
queue, distinct from the looped background music player. `INTRO.SEQ` activates
the optional replacement path when enabled; `_namesTable` and `Game::playCutscene()`
map game events/variants. The polygon catalog still does not enumerate SEQ.

## Sega CD MCD

See [MCD byte layout, compression and timing](../SEGA_CD.md#mcd-movies).
These are complete raster/audio movies, including short pickups and mission
cards. They use their own filenames and event variants, not DOS CMD/POL parts.
The catalog exposes one selectable movie per MCD file. SC2 contains the ending
and credits as joined sequences.
