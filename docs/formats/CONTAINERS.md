<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Containers and compression

[Index](README.md) · [Required files](../ASSETS.md)

## Loose files and preparation

The runtime opens logical basenames through [fs.cpp](../../fs.cpp). Case-insensitive
basename lookup is independent of archive lookup and is not an archive extractor.
Paths and edition isolation are specified in [ASSETS.md](../ASSETS.md).

LHA/WHDLoad preparation belongs to [extract_amiga.py](../../tools/extract_amiga.py).
`lhafile` supplies LHA member decoding; the tool checks member paths, lengths and
CRC and selects the subtree containing `FONT8.SPR` plus required game files.
It neither executes nor needs the Amiga program. It creates a new output tree
transactionally and refuses an existing destination.

Inner RNC streams use the standard 18-byte header:

| Offset | Field |
| --- | --- |
| 0 | ASCII `RNC` |
| 3 | Method byte: only 1 and 2 accepted |
| 4 | u32be expanded length |
| 8 | u32be packed payload length |
| 12 | u16be expanded CRC |
| 14 | u16be packed CRC |
| 16–17 | Leeway/chunk metadata, interpreted by Ancient |
| 18 | Packed payload |

CRC is CRC-16/ARC, initial zero, reflected polynomial `0xa001`. Header lengths,
both CRCs and decoded length are checked; expanded files are capped at 16 MiB.
Ancient performs method-specific decompression. The runtime accepts the resulting
original file, not an RNC envelope. Do not confuse RNC with Bytekiller below.

CUE/BIN and ISO9660 preparation are specified separately in [SEGA_CD.md](../SEGA_CD.md).

## ABA

Parser: [resource_aba.cpp](../../resource_aba.cpp).
Header: u16be entry count, u16be record size (must be 30).
Each 30-byte directory entry:

| Relative offset | Field |
| --- | --- |
| 0 | 14-byte NUL-terminated, nonempty filename |
| 14 | u32be payload file offset |
| 18 | u32be packed length |
| 22 | u32be expanded length |
| 26 | u32be `0x442e4d2e` (`D.M.`) |

Entries must be ordered without overlapping the directory or preceding payload.
Payloads with equal lengths are raw; otherwise they are Bytekiller streams.
Both lengths are capped at 64 MiB. The loader combines up to three ABA files and
indexes names case-insensitively. Game demo/full-release selection happens in
`Resource::init()`, not by guessing the meaning of an arbitrary ABA filename.

## PC-98 PAQ

Parser: [resource_paq.cpp](../../resource_paq.cpp); filename directory:
`ResourcePaq::_entries` and `_names` in [staticres.cpp](../../staticres.cpp).
Each record is 12 bytes: u32le file offset, u32le packed length, u32le expanded
length. There is no runtime-discovered filename table: the compiled mapping gives
the PAQ number and record number for a logical filename. Lookup hash begins at
5381; for each ASCII-uppercased byte, `hash = (hash * 33) XOR byte`, modulo 2³².

Equal lengths mean raw bytes. Otherwise use the PC-98 codec below; each size is
limited to 64 MiB. PGE and GLOBAL.ICN have edition-specific nested wrappers:

- PGE: decoded header u16le at 10 gives inner expanded size, u16le at 17 gives
  inner compressed start. Decode again, then remove the first `0x5f` bytes.
- GLOBAL.ICN: u16le at 7 and 14 describe successive inner streams while start
  equals `0x112`; limit eight layers. The terminal decoded wrapper skips `0x7d0`.
- `LEVELN.PAQ` is loaded directly as the room MAP buffer (see the DOS MAP
  layout); `SFX.PAQ` uses the special effect directory described in AUDIO.md.
  Neither goes through the generic name directory.

These wrappers are implementation knowledge for the supported release, not a
general PAQ interchange standard.

## Macintosh wrappers and resource fork

Parser: [resource_mac.cpp](../../resource_mac.cpp). All fields here are big-endian.
The accepted outer files are `Flashback.bin` and `Flashback.rsrc`.

- Raw fork: the file begins with the resource-fork header below.
- MacBinary: data-fork length is u32 at offset 83; resource-fork length at 87.
  Resource start is `128 + align_up(dataForkLength, 128)`.
- AppleDouble: signature `0x00051607`; u16 entry count at 24; 12-byte entries
  from 26 contain u32 ID, offset and length. Entry ID 2 is the resource fork.

Fork-relative header at 0: four u32 fields for data offset, map offset, data
length and map length. Within the map, u16 at 24 is the type-list offset and u16
at 26 is the name-list offset. Type list begins with u16 `(typeCount - 1)`;
each eight-byte type record contains four-character ID, u16 `(resourceCount - 1)`
and u16 reference-list offset relative to the type list.

Each 12-byte reference contains u16 resource ID, u16 name offset (`ffff` means
unnamed), four bytes combining attributes/high byte and 24-bit data offset, then
four reserved handle bytes. Data offset is relative to the data area; at that
location a u32 length precedes the payload. Names are Pascal strings relative to
the name list (u8 length, then bytes). The loader validates all extents before
publishing the directory, with a 256 MiB fork cap and at most 65535 resources.

Some payloads are raw; others have an outer compression envelope:
u32be expanded length followed by the C103 LZSS codec in
[GAME_DATA.md](GAME_DATA.md#macintosh-ppss-images). This is separate from an
image payload's own `C103`/`C211` signature.

## Bytekiller

Decoder: [unpack.cpp](../../unpack.cpp), `bytekiller_unpack()`.
The last 12 bytes are u32be initial bit reservoir, u32be XOR checksum, u32be
expanded length. Output is constructed backwards. Bit words are consumed
backwards through the input; bits leave the reservoir least-significant first.
When depleted, read the preceding u32be, take its low bit and install the remaining
31 bits with a high sentinel bit. Multi-bit values accumulate in read order,
most-significant result bit first. XOR the initial reservoir and each refill word
into the stored checksum; success requires zero.

| Prefix bits | Operation |
| --- | --- |
| `00` | Read 3-bit length plus 1; emit that many literal bytes, each read as 8 bits |
| `01` | Copy 2 bytes, distance from next 8 bits |
| `100` | Copy 3 bytes, 9-bit distance |
| `101` | Copy 4 bytes, 10-bit distance |
| `110` | Read 8-bit length plus 1, then 12-bit distance; copy |
| `111` | Read 8-bit length plus 9; literals |

For a reference, decrement the output cursor and copy from `cursor + distance`,
allowing overlap. Distance must be nonzero and cannot exceed already-decoded
output. Runs clamp at the declared output length. A bank/room may store an offset
to the **end** of its compressed stream, including this trailer; passing that as
an unbounded start pointer is incorrect.

## PC-98 compressed stream

Decoder: `pc98_unpack()` in [unpack.cpp](../../unpack.cpp).
First byte is a rolling key; remaining bits are MSB-first. A zero flag is followed
by an eight-bit encoded literal: output `literal XOR key`, then replace key with
the encoded literal rotated left one bit. A one flag introduces a reference:
begin length-bit count at one, increment for each following one bit up to the
decoder's bound, then read that many bits and add 3 for the copy length; next
13 bits plus 1 give distance. Copy forward from already-produced bytes with
overlap. The outer record supplies output size; there is no stream end marker.
Reject references before output start or beyond remaining output capacity.

## Compatibility limits

These parsers intentionally reject unsupported wrappers and corrupt lengths.
They do not extract installers, ZIP/7z packs, ROMs, ADFs, CHDs or arbitrary HFS
images. Preparing a supported wrapper externally does not certify the contained
game revision. Keep original files and extraction manifests for comparison.
