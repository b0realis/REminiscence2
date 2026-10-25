<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Supported assets and required files

This is the asset contract for the current engine. File extensions are not
interchangeable between editions. Preserve each edition's complete data tree;
the examples below identify what the loaders consume, not a promise that a
hand-picked subset will finish the game.

For byte layouts, compression and decoder entry points, start with the
[format index](formats/README.md). For preparing a disc, see
[Sega CD extraction and playback](SEGA_CD.md). Tests and their limits are recorded
in [VALIDATION.md](VALIDATION.md).

## Edition support

| Edition | Input accepted by the engine | Detection markers | Evidence and limits |
| --- | --- | --- | --- |
| Amiga | Unpacked original files; WHDLoad LHA requires preparation | `LEVEL1.LEV` or `DEMO.LEV`, without a stronger DOS/Sega marker | Supplied WHDLoad v3.4 data: seven level starts, saves, startup and all supplied polygon parts tested. No complete playthrough claim. |
| DOS / classic GOG data | Loose DOS resources or supported ABA archives | `LEVEL1.MAP`, `LEVEL1.BNQ`, `MENU1SSI.MAP`, `INTRO.SEQ`, `DEMO_UK.ABA`, or `GLOB_FR.ABA` | Supplied GOG installer 1.0.0 (25550) contains compatible DOS data; startup, levels and rendering tests pass. Other packages need checking. |
| Macintosh | `Flashback.bin` or `Flashback.rsrc` with a valid resource fork | Those filenames | Raw fork, MacBinary and AppleDouble wrappers supported; synthetic decoding/menu tests. Real Macintosh CD assets have not been supplied. Native rendering is 512×448. |
| Sega CD US | Extracted ISO files plus optional extracted CD audio | `PRESENT.LEV`; `SC1.MCD` + `SUB.BIN` distinguish CD from cartridge data | Actual US disc: movies, voices, music, seven level starts, saves and startup checked. Embedded data requires `SUB.BIN` length 96400 and CRC32 `80cee8d8`. Other pressings need a verified adapter. |
| Mega Drive / Genesis | Previously extracted engine-format files | `PRESENT.LEV` without CD markers | Experimental cartridge adapter and synthetic tile/sprite tests. No ROM loader, cartridge audio emulation or full real-asset validation. |
| PC-98 | Original PAQ collection | `GLOBAL.PAQ` | Archive/codec implementation and synthetic tests; real edition remains unvalidated. |
| 3DO / CD-i | Optional music/borders only, described below | No standalone game detector | Not supported as complete game editions. A working optional media decoder does not imply game support. |

## Directory and selection rules

```text
assets/
  amiga/          # FONT8.SPR, DATA/, MUSIC/, ...
  gog/            # DOS data, possibly nested under an extracted installation
  mac/            # Flashback.bin or Flashback.rsrc
  segacd/         # SUB.BIN, SC1.MCD, DATA/, music/, ...
  music/
    custom/       # trackNN.ogg / trackNN.wav and optional scene cues
```

`--datapath=assets --list-assets` prints exact IDs. Examples:
`--asset-set=dos:gog`, `--asset-set=sega:segacd`,
`--music-source=sega:segacd`, `--music-source=music:music/custom`.
IDs depend on folder placement. `sega` is shared by the CD and cartridge adapters;
use the full ID when both are present. `--music-source=native` removes an override.

[asset_library.cpp](../asset_library.cpp) groups editions; [fs.cpp](../fs.cpp)
indexes basenames case-insensitively using ASCII folding. Files in subdirectories
are visible. Hidden entries and directory symlinks are not traversed; depth is
limited to 64. Duplicate basenames prefer the shallowest file, then bytewise
relative-path order. Never merge editions into one directory to exploit that
priority: it can silently combine incompatible graphics and logic.

Game and soundtrack indexes are separate. The music selector changes music,
not sound effects, speech, movie audio or gameplay resources. Save compatibility
checks include the gameplay fingerprint. Different releases sharing an edition
ID should use separate `--savepath` directories to retain independent slot names.

## DOS and GOG files

| Group | Logical filenames consumed |
| --- | --- |
| Global UI and actor graphics | `FB_TXT.FNT`, `GLOBAL.ICN`, `GLOBAL.SPC`, `PERSO.SPR`, `PERSO.OFF` |
| Sound effects | `GLOBAL.FIB` |
| Level backgrounds / object banks | `LEVEL1` … `LEVEL5` with `.MBK`, `.CT`, `.PAL`, `.RP`, `.MAP` |
| Logic and inventory text | Stems below with `.PGE`, `.OBJ`, `.ANI`, `.TBN` (language extensions also supported) |
| Tile-background alternative | `.LEV` + `.BNQ`, plus `LEVEL1.SGD`; used by supported demos/ABA or the tile-data option |
| Enemy sprites | `JUNKY`, `MERCENAI`, `REPLICAN`, `GLUE` sprite resources, as requested by the level's monster table |
| Title/help pictures | `MENU1`, `MENU2`, `MENU3`, `INSTRU_E`, `INSTRU_F` `.MAP`/`.PAL`; SSI title variant when present |
| Polygon movies | Matching `.CMD`/`.POL` stems from `Cutscene::_namesTableDOS`, plus supported SSI variants |
| Movie text | `ENGCINE.BIN` + `ENGCINE.TXT`; language prefixes `FR_`, `GER`, `SPA`, `ITA`, `RUS` as supplied |
| PC CD movies | `INTRO.SEQ` and the event-specific names in `SeqPlayer::_namesTable`; selected by the SEQ option |
| Native DOS music | `.PRF` records, the `.MID` names referenced by them, and referenced `.INS` patches for AdLib |
| Demo playback | `DEMO1.BIN` (Jungle / level 1), `DEMO51.BIN` (Alien World / level 6), `DEMO3.BIN` (Death Tower / level 3), plus the corresponding level data |
| Optional credits | `CREDITS.CRD` (validated text/control stream; built-in fallback) |

| Game chapter | Background stem | Logic/text stem |
| --- | --- | --- |
| 1 | `LEVEL1` | `LEVEL1` |
| 2 | `LEVEL2` | `LEVEL2` |
| 3 | `LEVEL3` | `LEVEL3` |
| 4 | `LEVEL4` | `LEVEL4_1` |
| 5 | `LEVEL4` | `LEVEL4_2` |
| 6 | `LEVEL5` | `LEVEL5_1` |
| 7 | `LEVEL5` | `LEVEL5_2` |

Supported archive arrangements are `DEMO_UK.ABA`, or the set `GLOB1_FB.ABA`,
`GLOB2_FB.ABA`, `GLOB_FR.ABA`. These contain logical resources, not a new edition.
Loose files can take precedence over archive entries. Keep the full archive set.

The tested GOG installation stores DOS data under
`Flashback_Data/StreamingAssets/data`. Its installer and Unity executable are
not engine inputs; install or extract them first, as described in the manual.

## Amiga files

| Group | Files |
| --- | --- |
| Globals | `FONT8.SPR`, `ICONE.SPR`, `ICON.SPR`, `PERSO.SPM` |
| Chapter logic | `LEVEL1`, `LEVEL2`, `DT`, `LEVEL3_1`, `LEVEL3_2`, `LEVEL4_1`, `LEVEL4_2`: `.PGE`, `.OBC`, `.ANI`, `.TBN` |
| Backgrounds/objects | `.MBK`, `.CT`, `.PAL`, `.RPC`, `.SPC`, `.LEV` requested by `Game::loadLevelData`; chapters 4/5 share graphics, chapter 7 reuses chapter 6 collision data |
| City split | Both `LEVEL2_1.LEV` and `LEVEL2_2.LEV` |
| Foreground | `LEVEL1.SGD` |
| Effects | `LEVEL1.SPL`, `LEVEL3.SPL`, `LEVEL4.SPL`; chapter-to-bank mapping in `Game::_gameLevels` |
| Enemy graphics | Corresponding `.SPM` resources, loaded as monsters appear |
| Cinematics/title | `.CMP` movies and title pictures; text such as `ENGCINE.TXT` or `FRCINE.TXT` |
| Music | Original module files, often under `MUSIC/`, with names such as `INTRO`, `OPTIONS`, `LOGO` or the supported `.MOD` names |
| Demo variant | `DEMO.*`, `DEMOF.PGE`/`.TBN`, and individual effect samples used by `load_SPL_demo()` |

`tools/extract_amiga.py` selects the game subtree, unpacks RNC files and writes
a manifest into a new directory. It uses `lhafile` for LHA and Ancient for RNC.
The `.slave`, executable, icons for Workbench, and WHDLoad itself are not run.
The engine cannot directly read an LHA, ADF or still-RNC-packed resource.

## Macintosh resource names

The resource file encapsulates the following logical assets; do not split it
into invented DOS filenames. Resource names are case-sensitive inside the fork.

| Type / names | Use |
| --- | --- |
| `PPSS`: `Font`, `Icons`, `Person` | UI and Conrad |
| `PPSS`: `Junky`, `Mercenary`, `Replicant`, `Alien` | Enemies |
| `PPSS`: `Title N`, `Objects N`, `Level N Room R` | Title panels, object sprites, rooms |
| `OBJD`: `Level X objects` | Initial actors, equivalent to PGE |
| `ANIM`: `Level X sequences` | Animation sequences |
| `COND`: `Level X conditions` | Object behavior records |
| `LMAP`: `Level N map` | Collision/connectivity |
| `STRS`: `Level X names`, `Flashback strings`, `Movie strings`, `Credit strings` | Localized text; language-specific lookup is in `decodeResourceMacText()` |
| `PMOV` / `POLY` | Command/polygon resources resolved by `MAC_loadCutscene()` |
| `snd ` | Sound effects; trailing space is part of the four-byte type |
| `Flashback colors` | Color table |
| `Demo Level N`, `vers` | Recorded input and optional version information |

`X` follows `1`, `2`, `3`, `4-1`, `4-2`, `5-1`, `5-2`; `N` is the leading
chapter number where resources are shared. Existence of these loaders does not
establish that every Macintosh CD pressing uses this exact arrangement.

## Sega and PC-98

Sega CD has its own [complete file inventory](formats/SEGACD_FILES.md) and
[level mapping / format specification](SEGA_CD.md). Keep root files and `DATA/`
together. `SUB.BIN` is read for verified font/icon/text ranges; it is never executed.

The experimental cartridge path expects `FONT8.FNT`, `GLOBAL.ANI`, `.ICN`,
`.OBJ`, `.SPC`, `.SPR`, `.TAB`, `PRESENT.LEV`, and extracted level `.MBK`,
`.CT`, `.PAL`, `.RP`, `.LEV`, `.PGE`, `.TBN`, with `LEVEL1.SGD` as needed.
Its global sprite compression differs from Sega CD's per-level sprite compression.
The project does not extract these from a ROM or emulate the Sega sound chip.

PC-98 opens `POL.PAQ`, `PGE.PAQ`, `PAL.PAQ`, `OFF.PAQ`, `OBJ.PAQ`, `CT.PAQ`,
`CMD.PAQ`, `DEMO.PAQ`, `IMG.PAQ`, `RP.PAQ`, `SPR.PAQ`, `ANI.PAQ`, `MBK.PAQ`,
`TBN.PAQ`, `GLOBAL.PAQ`, plus `LEVELN.PAQ` room maps and `SFX.PAQ` sound data.
The name-to-record directory is compiled into `staticres.cpp`; arbitrary PAQ
collections are not supported. Japanese font/string data also has compiled tables.

## Optional media and non-game inputs

| Files | Supported use |
| --- | --- |
| `track02` … `track07`, `track09` `.ogg` or `.wav` | Looping background music; Ogg wins when both exist. WAV is PCM16 mono/stereo, 8–192 kHz. `track08.wav` is preserved from the CD but has no standard background mapping. |
| Scene-name `.ogg` and original `.MOD` / module names | Scene music; [manual mapping](../manual.md#music-sources) |
| `Options.Cpc`, `Jungle.Cpc`, `Subway.Cpc`, `TVshow.Cpc`, `City.Cpc`, `Alien.Cpc` | Optional 3DO-style SDX2 soundtrack through the native tune path; not 3DO game/movie support |
| SoundFont / MT-32 ROM paths | Optional PRF synthesizer inputs handled by FluidSynth/Munt when built; not game-edition assets |
| `flashp1.bob` … `flashp7.bob` | Optional CD-i decorative widescreen borders |
| `.set` | Limited built-in polygon animation path in `cutscene.cpp`, not a general standalone catalog format |
| `.cfg`, `.r2s`, `.r2m`, `.state`, screenshots | Engine settings, saves and output; never mix them into imported source data |

Original commercial files, disc images, PDFs and scans stay in private/ignored
storage. This documentation describes formats and filenames; the engine license
does not grant redistribution rights to imported assets.
