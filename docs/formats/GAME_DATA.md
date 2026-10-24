<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Game data and graphics

[Index](README.md) · [Files by edition](../ASSETS.md)

This describes the consumed fields. Legacy readers sometimes ignore trailing or
reserved fields; do not assume those fields are safe to repurpose in original data.

## PGE: initial actors

[Resource::decodePGE](../../resource.cpp) reads a two-byte count followed by 32-byte
records. Count is at most 256. Multi-byte fields follow gameplay edition byte
order (DOS/PC-98 LE; Amiga/Sega/Mac BE).

| Record offset | Field |
| --- | --- |
| 0 | u16 object/animation type |
| 2, 4 | s16 X, Y |
| 6 | u16 behavior-node index |
| 8 | s16 life |
| 10, 12, 14, 16 | Four u16 object-specific data words |
| 18 | u8 object type |
| 19 | u8 initial room |
| 20 | u8 room location |
| 21 | u8 initial flags |
| 22 | u8 colliding icon |
| 23 | u8 inventory icon |
| 24 | u8 object ID |
| 25 | u8 skill threshold |
| 26 | u8 mirror X |
| 27 | u8 flags |
| 28 | u8 collision-data length |
| 29 | Reserved/ignored byte |
| 30 | u16 text number |

Runtime `LivePGE` structures contain pointers and mutable state; they are not the
on-disk record and must not be loaded by `memcpy` from PGE files.

## OBJ / OBC: behavior nodes

[object_data.cpp](../../object_data.cpp) validates a u32 offset table followed by
nodes. Amiga/Sega OBJ have 230 table entries with no count prefix, BE offsets
relative to file start. DOS/PC-98 OBJ start with a u16le count; the following
table offsets are relative to the body after that count. Macintosh COND includes
a u16be count and offsets relative to the complete decoded payload.

Offsets are nondecreasing; equal adjacent offsets share a node. A node is u16
record count followed by exactly that many 18-byte behavior records. File end
delimits the last node. The node count cap is 255.

| Record offset | Field |
| --- | --- |
| 0 | u16 type |
| 2, 3 | s8 delta X, delta Y |
| 4 | u16 next/initial object type |
| 6, 7 | u8 opcode 2, opcode 1 (this order matters) |
| 8, 9 | u8 flags, opcode 3 |
| 10 | u16 initial object number |
| 12, 14, 16 | u16 arguments 1, 2, 3 |

Execution semantics are in [piege.cpp](../../piege.cpp), dispatched through
`Game::_pge_opcodeTable` in [staticres.cpp](../../staticres.cpp). They include
collision queries, inventory, motion and game-event cutscenes; this is not a
general-purpose VM. OBC wraps an Amiga-style OBJ body as u32be packed length
followed by a Bytekiller stream; expanded length comes from its trailer.

## ANI: animation sequences

A leading word and u16 offset directory locate sequence records. For non-Mac
editions `getAniData(n)` reads the word at `2 + n*2` and adds `2 + offset` to
the buffer base. Mac uses a u16be count, table at 2 and absolute payload-relative
offsets. A sequence starts with u16 frame count, u8 sound ID, u8 behavior value,
u16 flags, then four-byte frames: u16 sprite/flip word, s8 delta X, s8 delta Y.
`ffff` marks an absent frame; bit 15 is horizontal flip and low 15 bits select the
sprite. See `pge_setupAnim()` and `pge_playAnimSound()` in `piege.cpp`.
Sound zero means none; otherwise the sound ID is one-based.

## CT / LMAP: connectivity and collision

Loose CT is Bytekiller-compressed. Its decoded layout is 0x1d00 bytes:
256 connectivity bytes (four arrays of 64 room neighbors) followed by 64×112
collision bytes, each room a 16×7 grid. Neighbor values are signed; negative
means no room. Direction offsets are defined by the `CT_*_ROOM` constants in
`game.h`; collision-cell meanings depend on the behavior opcode consuming them.
Mac `LMAP` uses the Mac outer LZSS envelope and the same decoded layout.

## TBN and text tables

Non-Mac inventory-name files use u16 offsets from the start of the text buffer,
in the edition's byte order, into NUL-terminated strings. The first offset also
delimits the directory in the Sega CD embedded tables. Mac STRS tables use a
u16be count followed by u16be payload-relative offsets. CD-language fallback names
include `Level X names English/French` and `Game strings English/French`.
Japanese and common story/menu strings also have compiled fallback tables.
Language extensions `.TBF`, `.TBG`, `.TBS`, `.TBI` are selected where supported;
check `Resource::load()` rather than renaming text blindly.

## MBK / BNQ and RP / RPC / SPC

MBK bank records are six bytes: u32be offset and u16be tile-count/flags. Each
tile is 32 bytes (8×8×4 bits). With bit 15 set, bytes are raw at the offset;
otherwise the offset points just past a Bytekiller stream whose output length
must equal `tileCount*32`. Amiga and Sega CD raw lengths are negative signed
16-bit counts (negate); DOS masks bit 15 except BNQ's signed-count variant.
Cartridge Sega rejects negative counts. DOS masks the bank offset to 16 bits.
Decoded banks use a 0x7000-byte cache; changing banks invalidates it.

BNQ is the DOS tile-background bank, swapped into the bank reader while decoding
LEV. Sega CD has a separate `SPC*.MBK` object bank and level background MBK;
switch both pointer **and extent**, then invalidate cache.

RP/RPC is the object's bank-ID lookup, normally 74 bytes. Sega CD permits shorter
tables and fills missing entries with `ff`. SPC begins with a BE16 offset table
(first offset / 2 gives count), pointing into compound object-sprite descriptions.
`Game::drawObject()` / `drawObjectFrame()` consume the descriptors and select the
RP bank and per-piece offset/position/flags. Preserve these descriptor streams
when converting graphics; replacing only the pixel bank loses placement metadata.

## LEV: tile rooms

The first 256 bytes are 64 u32be **end offsets** of independently Bytekiller-packed
rooms. A repeated end offset can represent an absent room, particularly in the
two Sega CD city banks. Do not treat it as a second copy of the previous room.
Room output begins:

| Offset | Field |
| --- | --- |
| 0–1 | Flags; nonzero byte 1 selects SGD foreground commands |
| 2, 4, 6, 8 | Four u16be palette-bank selectors |
| 10, 12 | u16be offsets to the two tilemaps (or SGD commands at 10) |
| 14 | u16be offset to tile-bank assembly commands |

Assembly commands contain u16be bank ID (bit 15 marks last command), then u8
count: `ff` copies a complete bank; otherwise `count+1` following u8 tile IDs
select tiles from that bank. The temporary bank reserves tile zero and has
1024 tiles. Tilemaps, when present, have 32×28 u16 entries. Attribute masks and
plane compositing are in `Video::decodeLevHelper()` in [video.cpp](../../video.cpp).
Amiga tile pixels are bitplanes; DOS/Sega use packed nibbles. A shared LEV
container therefore does not imply identical bank-pixel encoding.

## MAP: DOS room images and menu images

Gameplay MAP begins with 64 six-byte records: s32le room offset and u16le size
(the old renderer does not use that size). Zero offset is absent; negative means
raw at its absolute value. Room payload starts with four u8 palette selectors.
Packed rooms contain four sections, each u16le encoded length then byte-RLE:
signed control <0 repeats the following byte `1-control` times; otherwise copy
`control+1` literal bytes. Each section expands to 14336 bytes; packed sections
are concatenated into the 256×224 indexed image. Raw sections instead store every
fourth horizontal pixel and are interleaved by the loader. PC-98 masks pixels to
four bits. Menu MAP is a separate flat 57344-byte image with no room directory;
its PAL is 768 RGB bytes. See `DOS_decodeMap()` and `load_MAP_menu()`.

## SGD: irregular foreground

[decode_sgd.cpp](../../decode_sgd.cpp) renders a command list: u16be count, then
six-byte entries (u16be tile ID, s16be X, s16be Y). `ffff` reuses the previous tile;
otherwise low 15 bits select a u32be offset in the SGD directory. Negative offsets
point to raw records with u16be byte length; positive offsets point to the Amiga
byte-RLE described below. Decoded records have width/height codes at 0/1 and u16be
plane stride at 2, followed by mask then color planes. Width units are
`(byte 0+1)/2`; height is `byte 1+1`. Amiga uses four color bitplanes; DOS/Sega pack
color nibbles behind the mask. Pixel writes clip to the 256×224 canvas.
Amiga's whole SGD file has an outer Bytekiller envelope; DOS/Sega's does not.

## Sprite families, fonts and icons

| Format | Layout / consumer |
| --- | --- |
| DOS SPR | Skip 12-byte outer header; OFF supplies sprite offsets. Sprite dimensions, origins, packed flags and nibble/run decoding are consumed by `Game::prepareAnimsHelper()` and `Video::DOS_decodeSpm()`. Enemy SPR uses a separate buffer. |
| DOS OFF | Repeated u16le sprite ID + u32le offset; `ffff` ID terminates, `ffffffff` offset means absent. IDs reference the 1287-entry sprite directory. |
| Amiga SPM | Bytekiller-compressed actor bank; compiled `_spmOffsetsTable` locates sprites. Main Conrad bank expands to 178647 bytes; enemy offsets are relative to the following bank. |
| Amiga sprite | Origin/size metadata followed by byte-RLE at offset 3; decoded three-bitplane actor data becomes indexed pixels. `AMIGA_decodeSpm()` derives width as 16 or 32 pixels and height from byte 2. |
| Amiga byte-RLE | u16be encoded byte count masked with `7fff`; signed negative control repeats next byte `1-control` times, otherwise copies `control+1` bytes. Source and output caps are explicit in `decodeAmigaRle()`. |
| Sega SPR/TAB | SPR skips 12 bytes; TAB is 1287 u32be offsets into that body, `ffffffff` absent. Size is next greater offset or bank end. See below for cartridge vs CD compression. |
| DOS FNT | 8×8 glyphs, 4-bit pixels, 32 bytes/glyph; glyph indexing begins with space. |
| Amiga FONT8.SPR | Planar font consumed by `AMIGA_drawStringChar()`; despite extension it is not a DOS SPR bank. |
| DOS ICN | u16le offset directory; each selected entry skips two bytes then contains 128 packed bytes for a 16×16 icon. |
| Amiga ICONE/ICON.SPR | Variable planar icon records with dimensions and per-record metadata; `AMIGA_decodeIcn()` / inventory drawing interpret them. |
| Sega icons | 128 bytes per 16×16 icon, column-major 8×8 tiles; Sega CD reads verified embedded icon data. |

Sega packed tiles store high nibble first; sprite tile order is **column-major**.
A character expands to 768 packed bytes, decoded as two consecutive 32×24 blocks
to form 32×48 pixels. Cartridge compression skips two origin bytes, reads u16be
compressed length plus one, then literals except `fN,count`, which repeats nibble
N in both halves for `count+1` bytes. CD instead uses split-stream LZSS described
in [SEGA_CD.md](../SEGA_CD.md). Movie tiles are row-major, not sprite tile order.

## Macintosh PPSS images

Decoder: [decode_mac.cpp](../../decode_mac.cpp). Decoded PPSS payload begins with
u16be signature `c103` or `c211`, u16be frame count, four uninterpreted bytes,
then u32be absolute frame offsets from byte 8. Nearest greater frame offset or
buffer end bounds each frame. Header per frame: u16be width and height; C211 adds
s16be origin X and Y. Dimensions must be nonzero; decoded cap is 16 MiB.

**C103 LZSS:** LSB-first flag bytes; zero flag means one literal byte. One means
u16be reference, distance `(word & 4095)+1`, length `(word >> 12)+3`. A 4096-byte
window receives output; references cannot precede produced data. Final reference
length clamps to remaining output. Used both for C103 pixels and Mac outer
compression envelopes (which begin with u32be expanded length).

**C211 commands:** bit 7 advances a row and resets X; low 5 bits give count, or
u16be extended count when zero. Bits5–6 select: `00` begin repeat(count>1) or
end repeat(count=1); `20` skip transparent pixels; `40` fill next color, except
count 1 terminates; `60` copy literal pixels. Loops are bounded to depth 512 and an
instruction budget. Runs may not cross image width. Pixels start transparent zero.

Mac sprites use their stored origin and optional flip; never assume DOS/Amiga
origin conventions. Font pixels `c0`/`c1` are shadow/front masks, not literal
screen palette indices. Resource color tables use the classic indexed CLUT layout
read by `MAC_decodeDataCLUT()` (six-byte header, u16 count as interpreted by this loader, then index/R/G/B u16 words).

## PAL, demo input and borders

Gameplay palettes usually store 16 u16be colors per 32-byte bank. Amiga colors are
RGB444; Sega colors use BGR333 bits in positions 1–3, 5–7, 9–11. Conversion and
the legacy nibble-to-byte conversion (`n*17`, with optional R/B swap) are centralized in `AMIGA_convertColor()`; DOS menu
palette RGB triples use a different path. Keep Original presentation faithful;
shirt/weapon options operate after decoding and do not rewrite PAL or sprites.

DEMO*.BIN is recorded input bytes consumed one per simulation step; bits encode
direction/actions, not timestamps. Macintosh `Demo Level N` is LZSS-wrapped and
remaps bits 4–6 on import. It is neither a video nor an engine save.

CD-i `.BOB` borders: u16le width, u16le height, 768 RGB palette bytes, then
width×height indexed pixels. This implementation accepts **52×224** only and
loads `flashp1.bob` through `flashp7.bob` for the corresponding level.
