# Flashback cutscene player

Browse and play the polygon cutscenes from your original Flashback files:

```sh
python3 tools/play_cutscenes.py "/path/to/Flashback/assets"
```

The tool prints a numbered catalog. Select a number or a file name, such as
`intro`, `memo` or `voyage`. If the file contains several parts, choose its part
at the next prompt, or specify it directly as `chute:1`. Part numbers start at
zero. Playback opens an SDL window and returns to the terminal menu when the
part ends. Type `q` at the menu or press Ctrl+C to exit the tool.

During playback:

- **Backspace, Tab or Escape** skips the part and returns to the menu.
- **Enter, Space and arrow keys** operate interactive panels, including the
  journal. Some original panels wait for input or loop deliberately.
- Closing the window ends that playback and returns to the menu.
- **Alt+Enter** toggles fullscreen using the engine's normal display controls.

The catalog includes story scenes, short gameplay animations, mission cards,
and the original continue/score screen animations. A brief or static part is
normal; these are the original resources, without their surrounding game UI.
Descriptions identify the files; they are not names supplied by Delphine.
The supplied Amiga WHDLoad package provides **27 files and 68 selectable parts**.

## Requirements and data

Use **Python 3.9 or newer** and a compiled **REminiscence2 from this tree**.
Python uses only its standard library; no `pip` installation is needed.
Build the engine as described in the [project README](../README.md#build).
The tool searches `build/release`, `build` and the repository root for the
executable, then searches `PATH`. Override this with `--engine=PATH`.

Supply one edition's extracted game folder. The engine searches its subfolders
and handles its own filename case matching and supported archives:

| Edition | Cutscene resources | Validation |
| --- | --- | --- |
| Amiga | Bytekiller-compressed or raw blocks in `.CMP` files | All 27 supplied files and 68 parts rendered with strict UBSan and the MOD mixer |
| DOS / classic GOG DOS data | Matching `.CMD` and `.POL` files, including the engine's ABA support | Synthetic catalog and launch fixtures; actual GOG files remain to be supplied |
| Macintosh | Named `PMOV` and `POLY` resources in a supported `FLASHBACK.BIN` or `FLASHBACK.RSRC` resource fork | Synthetic resource/offset checks; real Macintosh CD validation remains outstanding |
| PC-98 / experimental extracted Sega data | Engine-supported command/polygon resources | No edition-specific real-asset playback validation yet |

First prepare Amiga WHDLoad archives using [the extraction tool](../tools/extract_amiga.py):

```sh
python3 tools/extract_amiga.py Flashback_v3.4_1163.lha /path/to/amiga-ready
python3 tools/play_cutscenes.py /path/to/amiga-ready
```

Keep edition folders separate. This player reads local assets without modifying
them and creates no saves. Music defaults to the asset folder, including its
subfolders; use `--tunepath=PATH` for music kept elsewhere. Subtitles use the
engine's language detection unless `--language` is specified. Available
translations and music depend on the files in your edition. Some raw parts
have no game-event music association and play silently.

This is a polygon player. It does not list PC CD `.SEQ` replacements, standalone
`.SET` animations, 3DO videos, disk images or installers. The engine plays the
selected polygon part directly, regardless of text-summary or SEQ preferences
in the configuration. A named part does not automatically chain into other
files, add the game's score overlay, or assemble the full ending-credit sequence.

## Commands

```sh
# List without opening graphics or audio.
python3 tools/play_cutscenes.py /path/to/assets --list

# Play once, then exit the Python tool.
python3 tools/play_cutscenes.py /path/to/assets --play intro
python3 tools/play_cutscenes.py /path/to/assets --play chute:1

# Filter the catalog; choose French subtitles and a larger window.
python3 tools/play_cutscenes.py /path/to/assets --filter memory --language FR --scale 4

# Use a custom build or music folder, or start full screen.
python3 tools/play_cutscenes.py /path/to/assets --engine /path/to/reminiscence2 \
  --tunepath /path/to/music --fullscreen

# Inspect the versioned JSON catalog for scripting.
python3 tools/play_cutscenes.py /path/to/assets --json
python3 tools/play_cutscenes.py --help
```

The default window uses nearest-neighbor scaling at 3×. `--scale` accepts 1–6;
Macintosh assets already have twice the base resolution. `--config=PATH` selects
a compatibility configuration; otherwise the repository's `fb.cfg` is used
when present. Paths are resolved from the caller's working directory, and
paths containing spaces should be quoted. Noninteractive use requires
`--list`, `--json` or `--play`. Failed inspection or playback returns a nonzero
exit status; an empty filtered JSON catalog is valid.

The binary also exposes these operations directly:

```sh
./build/release/reminiscence2 --datapath=/path/to/assets --list-cutscenes
./build/release/reminiscence2 --datapath=/path/to/assets \
  --tunepath=/path/to/assets --play-cutscene=intro:0 --window
```

`--list-cutscenes` emits JSON with `schema`, `edition`, and `cutscenes` fields,
without the usual banner. Each entry has `name`, `title` and `parts`; valid part
numbers run from 0 to `parts - 1`. Listing does not initialize SDL or create
save/tune directories. Missing files are omitted. Invalid command tables are
reported on stderr and omitted; invalid CMP containers fail inspection with
a diagnostic. Rebuild the engine if an older binary rejects these switches.

## Implementation and reference

The Python tool supplies selection and process management. The C++ engine is
the single source for the catalog, CMP decompression, command-part offsets,
rendering, fonts, subtitles, audio, and edition behavior. It uses the existing
game-event tables to associate music and corrections with named parts. New CMP
container checks reject truncated blocks, oversized allocations and failed
Bytekiller checksums before exposing their data. These checks do not constitute
a full validation of every inherited polygon opcode or archive loader.

The requested reference, Gregory Montoir's
[dpoly_js](https://github.com/cyxx/dpoly_js), demonstrates the same cutscene
formats through an HTML5 player. Its
[selection table](https://github.com/cyxx/dpoly_js/blob/master/index.html) and
[command player](https://github.com/cyxx/dpoly_js/blob/master/player_cmp.js)
were inspected for context. No JavaScript, hosted game files, converted music,
or other assets were imported from that project. See also the supplied
[technical format notes](reference/technical/README.md).

`tests/cutscene_tests.cpp` covers containers, part tables, actual catalog
discovery and large-ellipse clipping. `tests/test_play_cutscenes.py` checks
selection, the JSON contract, process argument boundaries, errors and a real
engine launch using synthetic data and SDL dummy devices. CTest runs these
without commercial assets when Python is available. Set the existing
`REMINISCENCE2_AMIGA_DATA` CMake option to add the private all-parts playback
test; that test advances interactive panels and can skip looping previews.
