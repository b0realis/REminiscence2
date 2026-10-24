<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Developer format reference

These are implementation specifications derived from this tree and the tested
assets. They describe the subset the engine understands, not undocumented variants
as if they were verified. Unknown fields are explicitly left unknown. File offsets
are zero-based; `u16be`/`u32le` denote width and byte order; signed values use two's
complement unless a section explicitly says sign-magnitude. Sizes are bytes unless
specified otherwise. Source links name the authoritative parser or consumer.

Start with [required files by edition](../ASSETS.md). Detailed references:

| Reference | Formats covered |
| --- | --- |
| [Containers and compression](CONTAINERS.md) | Filesystem resolution, LHA/RNC preparation, ABA, PAQ, MacBinary/AppleDouble/resource forks, Bytekiller, PC-98 compression |
| [Game data and graphics](GAME_DATA.md) | PGE, OBJ/OBC, ANI, CT, TBN, MBK/BNQ, RP/RPC/SPC, LEV/MAP, SGD, SPR/OFF/TAB/SPM, FNT/ICN, Mac PPSS, PAL/CLUT, DEMO, BOB |
| [Cinematics](CINEMATICS.md) | CMP, CMD/POL, text BIN/TXT, PMOV/POLY, SET subset, PC CD SEQ |
| [Audio](AUDIO.md) | FIB, SPL, VCE, MOD, PRF/MIDI/INS, Ogg, WAV, CPC/SDX2, Mac snd resources |
| [Sega CD](../SEGA_CD.md) | CUE/BIN, ISO9660 extraction, manifest, MCD video/PCM, split-stream sprites, US embedded resources and event/level mappings |
| [Sega CD file inventory](SEGACD_FILES.md) | Every file extracted from the tested ISO, with file sizes and use categories |

## Coordinates and edition boundaries

Most gameplay uses a 256×224 indexed-color canvas, 64 room slots per map,
16×7 collision cells per room, and up to 256 initial actors. Macintosh renders
512×448 while retaining the game's underlying logic coordinates. Movie pictures
can occupy only a 256×128 center band. Hardware upscaling does not change source
asset resolution.

DOS/PC-98 gameplay words are usually little-endian; Amiga/Sega/Mac gameplay words
are usually big-endian. **Do not apply that rule to every file:** DOS polygon
movies, palettes and graphics-bank directories use big-endian fields too.
`edition.h`, the particular decoder and the tables below define each exception.

## Validation map

| Tests | Evidence |
| --- | --- |
| `core`, `formats` | Bounded synthetic compression, resource-fork, image, archive and malformed-input fixtures |
| `music`, `music_sources`, `input_audio` | PRF/MIDI/instruments, MOD/CPC/Ogg, source selection, resampling and mixer lifetime |
| `cutscenes`, `cutscene_frontend` | CMP/command parts, listing, Python launcher contracts |
| `mac_menus` | Synthetic Mac resources and native-resolution menus |
| `segacd_media`, `segacd_extractor` | Synthetic MCD/sprite/WAV streams and CUE/ISO safety cases; no commercial files needed |
| `amiga_assets`, `dos_assets`, `cutscene_assets`, `music_source_assets` | Opt-in tests using private original data |
| `segacd_assets`, `startup_segacd` | Actual US CD files: every indexed movie frame, all CD tracks, dialogue ranges, sprites, level starts, city save/load and startup |

CTest names depend on configured private asset paths. See `CMakeLists.txt` for
`REMINISCENCE2_AMIGA_DATA`, `REMINISCENCE2_DOS_DATA`,
and `REMINISCENCE2_SEGACD_DATA` options. Do not commit
commercial fixtures; construct synthetic minimal examples or use opt-in paths.

## Extending a format

1. Record edition/revision and hash when a layout depends on an executable or
   a particular pressing. Keep payloads private.
2. Establish byte order, offset base, packed/unpacked sizes, sentinel values,
   codec termination and ownership before reusing another edition's parser.
3. Bound lengths before allocation, directory traversal, seeking and copying.
   Reject unsupported layouts explicitly. Successful extraction is not proof
   that a complete game can play.
4. Add a synthetic valid fixture plus meaningful truncation/reference tests.
   Run the private integration test when original data is available.
5. Update the asset table, this index and the relevant detailed specification.
   Record exactly what was tested, including remaining playback uncertainties.

Legacy readers still vary in defensive validation: the presence of a format
description does not mean every old code path safely handles arbitrary malformed
files. Prefer `ByteReader`/bounded spans when extending them. Do not replace the
player's original files; presentation options should transform decoded data.

Engine-owned save/config formats are documented in [SETTINGS.md](../SETTINGS.md)
and implemented in `game.cpp`, `progress_store.cpp`, `save_catalog.cpp`,
`settings_store.cpp` and `controller_profile.cpp`. They are separate from imported game assets.
