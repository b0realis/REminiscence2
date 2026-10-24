<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Sega CD: preparation, playback and format specification

The engine supports the supplied **US Sega CD edition** as a separate asset set:
level data, sprites, effects, title screen, MCD movies with embedded sound, VCE
dialogue and CD-DA music. Source files remain unchanged. The revision-specific
font/icon/text adapter checks `SUB.BIN` before reading embedded data; another
pressing needs verified offsets and a new revision entry.

See [all supported editions](ASSETS.md), [format index](formats/README.md), and
the [complete extracted file inventory](formats/SEGACD_FILES.md).

## Prepare and launch

Python 3.9+ is sufficient; this tool has no pip, mounting or emulator dependency.
Keep the CUE beside the BIN file(s) it references. Choose a **new** output folder:

```sh
python3 tools/extract_segacd.py "/path/to/Flashback.cue" --list
python3 tools/extract_segacd.py "/path/to/Flashback.cue" --output assets/segacd
./build/release/reminiscence2 --datapath=assets --list-assets
./build/release/reminiscence2 --datapath=assets --asset-set=sega:segacd --window
```

For the prepared private development copy, use `./play-local.sh --segacd`.
Run `./play-movies-local.sh` for a numbered movie browser, or add `--play sc1`
to watch the opening immediately.
The tested disc produces **163 ISO files and eight WAV tracks**, preserving the
original root/`DATA/` layout. Music goes in `music/track02.wav` through `music/track09.wav`.
`reminiscence2-segacd.json` records filenames, sizes, SHA256 digests, track durations,
pregaps, source CUE digest and selected audio byte order. `--json` prints the report;
`--list --json` inspects without creating files or hashes of extracted output.

`--audio-endian=little` is the default and matches the supplied BINs. If a dump
uses big-endian audio samples, explicitly choose `--audio-endian=big`; it swaps
each sample into little-endian WAV. No lossy encoding or sample-rate conversion
occurs. The tool refuses existing destinations and cleans incomplete staging
output on failure. Do not merge output into Amiga, GOG or Macintosh directories.

To use this CD soundtrack with another edition, keep both sets under `assets`
and select **QoL → Audio → Soundtrack → Sega CD**, or use:

```sh
./build/release/reminiscence2 --datapath=assets \
  --asset-set=dos:gog --music-source=sega:segacd
```

Only background music changes. The other edition retains its own movies,
dialogue and effects. Track 08 is preserved but has no standard background-track
mapping. SC2's ending includes its own movie audio. See the
[music mapping](../manual.md#music-sources) for the remaining track numbers.

Browse movies independently:

```sh
python3 tools/play_cutscenes.py assets/segacd --engine ./build/release/reminiscence2
```

Choose `sc1` for the opening or another listed stem. The Sega CD opening contains
its publisher/developer logos inside SC1; there is no separate DOS/Amiga-style
logo movie. The normal launch plays SC1, then the title menu. Default keyboard
and mouse skipping works, and Esc opens QoL settings during startup.

## Gameplay appearance

QoL → Enhancements supports the Original/Red-Pink/White T-shirt and
Original/Heavy/Laser pistol styles. Laser offers Orange, Red, Green, Blue and
Purple. Only the gameplay undershirt changes; the jacket, shoes and equipment
retain their original colours. Prerecorded movies keep their original artwork.
Pistol effects use the CD edition's native flash coordinates and original sound
samples, with cosmetic bass processing. Simulation and imported files stay unchanged.

## Movie picture quality

The original movies store **256×128 pixels and 16 palette colours per frame**.
Their regular checkerboard texture is encoded dithering, most visible when
nearest-neighbour scaling enlarges the pixels. It is not an added engine overlay.

The supplied disc's 70 extracted MCD files match an independent libarchive
ISO9660 extraction byte for byte. All **17754 indexed frames** also match the
original US program's 68000 decompression output byte for byte. The validation
ran the original routines from the user's private SUB.BIN in Unicorn's M68000
CPU emulator: file offsets `0x14fd6` (LSB), `0x152ca` (MSB), and `0x15f98`
(split stream). The original program is never shipped with the engine. This
checks decompression; physical-console display and A/V timing remain separate
validation tasks. Synthetic tests check nonuniform tile placement, nibble order
and RGB palette conversion to guard against display-layout regressions.

## CUE/BIN and ISO9660 subset

Implementation: [extract_segacd.py](../tools/extract_segacd.py).

- One MODE1 data track followed by optional AUDIO tracks. Quoted `FILE "..."
  BINARY`, increasing track numbers 1..99, INDEX00/01 supported. Files may be
  separate or shared; shared files cannot mix 2048/2352-byte sector sizes.
- CUE times are `MM:SS:FF`, 75 frames/sectors per second. Stored pregap INDEX00
  is excluded: audio starts at INDEX01 and ends at the next stored pregap/start
  in the same file or EOF. PREGAP/POSTGAP directives describe generated silence
  and are not copied from BIN. Metadata directives are ignored; unsupported
  directives fail explicitly.
- MODE1/2048 supplies payload directly. MODE1/2352 requires sync bytes
  `00 ff ff ff ff ff ff ff ff ff ff 00` and mode byte 1 at 15; user payload is
  bytes 16..2063. EDC/ECC is not verified or repaired. AUDIO sectors are 2352 raw
  bytes, 588 stereo signed 16 frames at 44100 Hz.
- ISO logical sector size must be 2048. Primary volume descriptor is found from
  sector 16; descriptor ID/version is `CD001`/1. Volume size at 80 and block size
  at 128 are checked in both byte orders; root directory record starts at 156.
- ISO directory records: u8 length at 0, extended-attribute length at 1 (must 0),
  dual-endian extent at 2, dual-endian byte length at 10, flags at 25, interleave
  fields 26/27 (must 0), u8 name length at 32 and ASCII name at 33. `;version` is
  stripped. Zero-length directory bytes pad to the next sector; `00`/`01` names
  are self/parent and skipped.
- Bounds: 1 GiB per BIN and total extracted file bytes, 4096 directory records,
  directory depth 16 and directory byte size 4 MiB. Reject outside extents,
  cycles, case-insensitive duplicate paths, traversal, external BIN symlinks,
  unsupported flags/multiextents and collisions with generated output names.
- Only the primary ISO tree is interpreted; no Joliet/Rock Ridge/HFS, MODE2,
  CHD, compressed disc or arbitrary audio FILE types. Minimum Flashback layout
  markers are `DATA/LEVEL1.LEV`, `DATA/VOICE.VCE`, `SC1.MCD`, `SC2.MCD`.

The extractor preserves all ISO files, including files the engine does not use.
It does not certify every extracted revision as playable; the engine separately
validates the US program data identity.

## US embedded resources

[resource_segacd.cpp](../resource_segacd.cpp) reads `SUB.BIN` only when length is
**96400** and standard zlib CRC32 is **0x80cee8d8**. Offsets below are file offsets,
not 68000 runtime addresses. No program code is executed or shipped with the engine.

| Offset | Data read |
| --- | --- |
| `0x1144` | 95 font glyphs ×32 bytes, 8×8 packed4-bit pixels |
| `0xfa96` | 31 inventory icons ×128 bytes, 16×16 column-major tiles |
| `0x139be` | Chapter 4 inventory-name table |
| `0x13a3a` | Chapter 5 inventory-name table |
| `0x13b0a` | Chapter 6 inventory-name table |
| `0x13bc4` | Chapter 7 inventory-name table |

The text tables have BE16 offsets into NUL-terminated strings; the first offset
gives table byte length. The engine reads a bounded 512-byte window and validates
the table and string termination. Navigation glyphs 76–78 are engine-generated;
the original CD interface draws these separately. `TITLE.BIN` starts with 32 bytes
of Sega palette followed by a 224×48 logo in row-major 8×8 tiles (5376 packed bytes).
Additional bytes in that file are preserved but not consumed by this title path.
`PRESENT.LEV/.MBK/.PAL` provide the title background.

## Level file mapping

All names below are under `DATA/`. Letter case is retained by extraction but
runtime basename lookup is case-insensitive. Extensions follow the shared
[game-data specification](formats/GAME_DATA.md), with CD differences below.

| Chapter | PGE/OBJ/ANI/SPL | Background MBK/CT/PAL | LEV | RP/SPC | SPR/TAB | Object MBK |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | LEVEL1 | LEVEL1 | LEVEL1 | LEVEL1 | LEVEL1 | SPC1 |
| 2 | LEVEL2 | LEVEL2 | LEVEL2_1 / LEVEL2_2 | LEVEL2 | LEVEL2_1 / LEVEL2_2 | SPC2 |
| 3 | DT | DT | DT | DT | DT | SPCDT |
| 4 | LEVEL3_1 | LEVEL3 | LEVEL3 | LEVEL3_1 | LEVEL3_1 | SPC3 |
| 5 | LEVEL3_2 | LEVEL3 | LEVEL3 | LEVEL3_1 | LEVEL3_2 | SPC3 |
| 6 | LEVEL4_1 | LEVEL4 | LEVEL4 | LEVEL4_1 | LEVEL4 | SPC4 |
| 7 | LEVEL4_2 | LEVEL4 | LEVEL4 | LEVEL4_1 | LEVEL4 | SPC4 |

TBN is a file for chapters 1–3 and embedded for 4–7. Chapter 1 additionally uses
`LEVEL1.SGD`. Fonts/icons are globals from SUB rather than GLOBAL.SPR. Each level
has its own ANI/OBJ, so cartridge-global lifetime rules must not be reused.

City LEV directories contain cumulative compressed **end offsets**. A room is
present in a bank when its end is greater than the preceding end (or 256 for room 0).
The two US directories are disjoint. The engine derives a 64-entry ownership map
from both directories, allowing direct room/save entry without replaying the
console's boundary-triggered bank switches. Adjacent-room previews must restore
the active room bank; only changing the actual room switches the SPR/TAB bank.

Object and background MBKs are separate. Negative raw tile counts use the Amiga
signed-count convention. CD SPL's absent sentinel is exactly `ffff`, unlike the
Amiga high-bit rule. VCE dialogue uses the common bounded voice loader described
in [AUDIO.md](formats/AUDIO.md#voicevce-dialogue).

## CD character sprites

Decoder: `decodeSegaCDCharacter()` in [decode_sega.cpp](../decode_sega.cpp).
After the SPR's 12-byte outer header, TAB offsets select each compressed record.
Bytes 0–1 are placement metadata used by the renderer; bytes 2–3 are not used by
the new decompressor. u16be at 4 is the split-stream reference offset relative
to 6. Literal/flag stream starts at 6; reference words start at `6 + offset`.

Read flag bytes MSB-first. A 1 bit copies one literal byte from the literal stream.
A 0 consumes a u16be from the reference stream:
`distance=(word>>7)+1`, `length=(word&127)+2`. Copy from already produced bytes,
allowing overlap. Stop after exactly 768 packed bytes. Reject exhausted input,
references preceding output, or output overruns. Decode two 384-byte blocks,
each 32×24 pixels in column-major 8×8 tiles, to form 32×48. High nibble is first pixel.

This differs from cartridge Sega's nibble-run compression and MCD's 12/4-bit
distance/length references. Choosing the wrong one can produce plausible-looking
but corrupt sprites; select from the verified edition, not a per-record guess.

## MCD movies

Implementation: [mcd_player.cpp](../mcd_player.cpp). Physical groups are **10240**
bytes:8192 video bytes then2048 PCM bytes. File length must be a multiple of 10240,
nonzero and at most 1 GiB. A logical video byte offset V maps to physical
`(V/8192)*10240 + (V%8192)`. Reads crossing a group skip the audio sector.

Each frame occupies 1..7 **video** sectors of 2048 bytes; it may cross groups.

| Frame offset | Field |
| --- | --- |
| 0..31 | 16 u16be Sega palette values: red bits 1–3, green 5–7, blue 9–11 |
| 32 | Compression type:1 LZSS;0 word-RLE path (not encountered in tested disc frames) |
| 33 | Number of 2048-byte video sectors occupied by the frame |
| 34 | u16be first-frame count or sentinel `4000` for LZSS; encoded length for type 0 |
| 36 | LZSS bytes, or split-stream offset word in the split variant |

Decoded raster is 16384 packed bytes =256×128 at 4bpp. Tiles are row-major 32×16
tiles of 8×8 pixels; high nibble first. Palette channel conversion scales the
three-bit hardware value to 0..255. The engine displays this at Y48 in 256×224.

LZSS variants, all with 1=literal and 0=reference:

| Files | Flags | Streams |
| --- | --- | --- |
| `SC2.MCD`, `BRIDGE.MCD` | MSB-first | Flags, literals and references in one stream |
| `MEMORY.MCD`, `SPY.MCD` | MSB-first | u16be at 36 gives distance from 38 to reference stream; literals/flags begin 38 |
| All other supported names | LSB-first | One stream starting 36 |

Reference word is BE16: `distance=(word>>4)+1`, `length=(word&15)+2`. References
can overlap but must address existing output and finish within 16384 bytes.
For the type 0 implementation, signed 8 control selects `(abs(control)+1)` words;
negative repeats one following two-byte word, nonnegative copies literal words.
That path has no real-disc validation and should be tested before adding a variant.

PCM is mono 8-bit sign-magnitude. Decode `byte&128 ? -(byte&127) : byte`, then
multiply by 256 for signed 16 stereo output. Source rate is 30720 Hz, derived from
75 physical CD sectors/second ×1/5 audio sectors ×2048 samples. Video deadlines
use cumulative video sectors at 60/second; frames therefore have variable duration.
The player streams PCM with an independent file handle and bounded sector buffer.
Focus/background pauses are excluded from the video clock, matching suspended audio.
Original-hardware A/V timing comparison remains a validation task.

Frame counts can exclude trailing valid duplicates or unused transfer padding.
The stream index retains valid inspectable frames; playback respects explicit
first-frame count, or verified US event defaults when that field is `4000`.
SC2 is special: three joined sequences, playback limited to 5312 frames by the
original event duration. Its index contains 5332 inspectable frames. An explicit
count overrides a default (for example HOLO2 plays 392, not its default 300).

## Movie events and variants

`mcd::eventName(event, level)` is the authoritative table. Main examples:

| Event (hex) | Movie |
| --- | --- |
| 00 | DEBUT |
| 01, 04 | TKHOLO, TKGUN |
| 0d | SC1 (opening, including logos) |
| 11, 14 | HOLO2, BRIDGE |
| 19..1d | MAP, CARTE1..CARTE4 |
| 25, 27, 28, 29, 2a | MISSION1, MISSION2, MISSION4, MISSION3, MISSION5 |
| 2f, 30 | ANTIG, MEMORY |
| 34, 35, 36, 37 | VOYAGE, TELEPORT, LIFTU, LIFTD |
| 39, 3b | SPY, JOURNAL |
| 3d, 3e, 3f | SC2 (ending/credits), GENEXPLO, BADEND |
| 41 | OVER |
| 42, 44..47 | MISSEND1..MISSEND5 |

Chapter-indexed variants (chapters 1..7):

| Stem/event | Suffix sequence |
| --- | --- |
| TKKEY /02 | 1, 2, 3, 3, 3, 4, 4 |
| SHIELD /05 | 1, 2, 3, 4, 4, 5, 5 |
| TKTELEC /10 | 1, 2, 2, 2, 2, 3, 3 |
| TKRECEP /3c | 1, 1, 1, 1, 1, 2, 2 |

CHUTE1 is chapters 1/4, otherwise CHUTE2. GVID1 is chapter 2, otherwise GVID2.
GVTUNE1 and TKCREDIT are chapter 1, otherwise GVTUNE2/TKCREDI2. Event 40 has no
separate logo movie. CREATE is preserved/cataloged even though no normal event
in the current table invokes it. Null entries are not permission to invent a
DOS polygon fallback that the disc does not contain.

## Validation and remaining work

The real-asset regression decodes **17754 indexed frames across all 70 movies**, all available
sprites in the seven level-start banks, all eight CD tracks and 95 nonempty VCE
segments. It simulates 100 ticks per level, exercises save/load and direct city
bank restoration, and captures private reference frames. Startup tests cover
normal opening/title, skip, immediate settings, edition switch, quick resume and
keyboard/mouse title navigation. Synthetic tests cover codec bounds, WAV lifecycle,
CUE/ISO extraction failures and output integrity.

These are headless regressions and limited render inspections, not a complete
seven-level playthrough or an audio-fidelity certification. Real-console timing,
all dialogue triggers, long-play stability, other regions/pressings and hardware
testing on all release targets remain to be checked. Original files and generated
captures stay private. See [releases](RELEASES.md) for platform packages.
