// SPDX-License-Identifier: GPL-3.0-or-later
#include "cli_text.h"
#include "music_options.h"
#include "version.h"

namespace cli {
namespace {
constexpr char kBanner[] = R"( ____  _____           _       _                             ____
|  _ \| ____|_ __ ___ (_)_ __ (_)___  ___ ___ _ __   ___ ___|___ \
| |_) |  _| | '_ ` _ \| | '_ \| / __|/ __/ _ \ '_ \ / __/ _ \ __) |
|  _ <| |___| | | | | | | | | | \__ \ (_|  __/ | | | (_|  __// __/
|_| \_\_____|_| |_| |_|_|_| |_|_|___/\___\___|_| |_|\___\___|_____|
)";

void printMusicBackends() {
	fputs("  PRF music backends enabled in this executable: adlib, none", stdout);
	if (music::backendAvailable(music::Backend::FluidSynth)) {
		fputs(", fluidsynth", stdout);
	}
	if (music::backendAvailable(music::Backend::Mt32)) {
		fputs(", mt32", stdout);
	}
	putchar('\n');
}
} // namespace

void printStartup() {
	fputs(kBanner, stdout);
	puts("\nREminiscence2 " REMINISCENCE2_VERSION
	     "\nRun with -h or --help for all options and examples.\n");
	// Show startup information before diagnostics or the graphical game window,
	// including when standard output is redirected to a file or pipe.
	fflush(stdout);
}

void printUsage(FILE *stream, const char *program) {
	fprintf(stream, "Usage: %s [OPTIONS]...\n       %s GAME_FOLDER\n", program, program);
}

void printHelp(const char *program) {
	printUsage(stdout, program);
	fputs(R"(
Play Flashback: The Quest for Identity using your original game files.
The engine opens a graphical game window. Game data is supplied separately.

GETTING STARTED
  Use --datapath to select one edition's extracted game files. The engine
  searches that folder and its subfolders and detects the edition itself.
  DOS (including classic DOS data from GOG), Amiga, Macintosh and PC-98 data
  have loaders. Sega Mega Drive extracted data is experimental, without Sega
  audio. GOG and Macintosh CD data still need real-asset validation.
  Installers, LHA archives, WHDLoad slaves and disk/ROM images cannot be used
  directly. Amiga RNC-packed files must be unpacked before use.
  Prepare a WHDLoad LHA package with tools/extract_amiga.py; run
  python3 tools/extract_amiga.py --help for its setup and usage instructions.

  Paths may be absolute or relative to your current working directory,
  including the config, tune and save paths. Quote paths containing spaces.
  Long options accept either --name=VALUE or --name VALUE. Options that take
  no value, such as --window, are enabled by including the switch.

INFORMATION
  -h, --help
      Show this complete guide and exit successfully. No game files,
      configuration file, audio device or graphics window are required.

  --version
      Show the engine version, upstream credits and enabled PRF music
      backends, then exit. No game files are required.

CUTSCENE PLAYER
  --list-cutscenes
      Inspect the selected game data and print a JSON catalog of available
      polygon cutscene files, descriptive titles and part counts, then exit.
      No banner, graphics window, audio device or save/tune folders are
      created. Part numbers start at zero. An empty catalog is valid.

  --play-cutscene=NAME[:PART]
      Play one polygon cutscene part and exit, without entering the game.
      NAME is a catalog name such as intro, memo or voyage; spelling ignores
      case. PART defaults to 0. Example: --play-cutscene=chute:1.
      Backspace skips the part; Escape or closing the window exits playback.
      Music and subtitles use this edition's files and the selected language.
      This plays the original polygon animation even if SEQ or text-summary
      replacements are enabled in fb.cfg. It does not create save files.
      Use --window for a window; --tunepath selects music as in normal play.
      tools/play_cutscenes.py provides a numbered menu around these options.

FILES AND CONFIGURATION
  --datapath=PATH                         Default: assets beside executable
      Folder containing the original game files. Keep different editions in
      separate folders. For Macintosh, retain the resource fork in a supported
      FLASHBACK.BIN or FLASHBACK.RSRC file. For GOG, select the original DOS
      data folder, rather than the Windows installer or executable.

  --savepath=PATH                         Default: user data SAVE folder
      Writable folder for manual save files and game checkpoint saves.
      It and missing parent folders are created when gameplay starts.
      Manual save slots range from 1 to 99 and are selected during play.

  --tunepath=PATH                         Default: discovered TUNES folder
      Folder for optional music files, such as MOD or OGG tracks. It is
      read without creating or modifying it.
      DOS PRF/MIDI/INS music is searched for in the game folder first,
      then this folder. Original game audio may also come from game data.

  --config=PATH                          Default: fb.cfg
      Read the compatibility switches listed below. The default fb.cfg is
      read from the working directory, then beside the executable.
      If neither file exists, built-in defaults apply.
      An explicitly requested file must be readable or startup fails.
      Saved in-game preferences can override the settings they expose.
      Paths and music backend selection use command-line options.

  --no-preferences
      Ignore the save folder's preferences.cfg (or the file in --savepath) for this launch.
      Use this to recover from unwanted saved display or keyboard choices.
      F10 always opens the in-game settings menu. Saving settings from that
      menu can still write a new preferences file.

  GAME_FOLDER (only argument)
      An existing folder passed without any switches is used for game data,
      and tunes. Saves use the normal writable user-data location.

  --portable
      Keep default assets, TUNES and SAVE folders beside the executable.
      Explicit path options take priority. The executable folder must be
      writable for saves. Without this switch, saves use:
        Windows: %%APPDATA%%/REminiscence2/SAVE
        macOS:   ~/Library/Application Support/REminiscence2/SAVE
        Linux:   $XDG_DATA_HOME/REminiscence2/SAVE, or
                 ~/.local/share/REminiscence2/SAVE
      Data discovery checks assets beside the executable, then legacy DATA.
      Music discovery checks beside the executable, then the working
      directory, then the same user-data root. Game files are read-only.

DISPLAY
  --window
      Start in a resizable window; overrides saved display preferences.
      Without saved preferences the default is fullscreen.

  --maximized
      Start with the resizable window maximized; also enables windowed mode.
      Alt+Enter toggles between windowed and fullscreen play.

  --scaler=NAME@X                        Default: scale@3
      Select pixel scaling and the initial window size multiplier X.
      Use a positive whole-number X; omitting @X keeps the default of 3.
      Built-in names:
        point   Sharp nearest-neighbor sampling by the graphics renderer.
        linear  Smooth linear sampling by the graphics renderer.
        scale   ScaleNx edge-aware pixel scaling at 2x, 3x or 4x.
      The scale factor is clamped to the scaler's supported range (2-4 for
      scale). Fullscreen output is fitted to the display. Examples:
      --scaler=point@3, --scaler=linear@2, --scaler=scale@4.
      Other names require a compiled-in scaler or a matching plugin in the
      working directory: scaler_NAME.so (scaler_NAME.dll on Windows).
      xBR and other external scalers are not included in the default build.

  --widescreen=MODE                      Default: none
      Choose the content outside the original playfield. These modes do
      not enlarge the playable area or change room transitions:
        none           Original playfield, without added side content.
        adjacent       Show backgrounds from neighboring rooms at the sides.
        adjacent-blur  Show blurred neighboring room backgrounds.
        mirror         Fill the sides with darkened mirrored room edges.
        blur           Place a blurred copy of the room behind the playfield.
        cdi            Use optional CD-i-style decorative side artwork.
      The first four added-content modes use a 16:9 layout. cdi uses fixed
      borders and needs flashp1.bob through flashp7.bob in the game data;
      missing border artwork leaves the margins empty.

GAMEPLAY
  --levelnum=NUM                         Default: 0
      Initial level, numbered 0 through 6 (the game's seven playable levels).
      This does not skip opening movies or menus. A password or menu level
      selection can subsequently change the level.

  --language=LANG                        Default: auto-detect, else English
      Select fr (French), en (English), de (German), sp (Spanish), it
      (Italian), jp (Japanese) or ru (Russian). Codes ignore letter case.
      Text and cutscene translations depend on the files in your edition;
      this option does not download or install a language pack.

  --autosave                            Default: disabled
      Keep an in-memory rewind snapshot about every 5 seconds while alive,
      up to 120 snapshots (about 10 minutes). Ctrl+R restores the previous
      snapshot; Continue after death can restore the latest one.
      Rewind history is cleared on level changes and lost when you quit.
      It does not create a persistent autosave file. Use manual saves or
      game checkpoints to retain progress between sessions.

  --cheats=MASK                          Default: 0 (off)
      Enable any combination of these bit flags by adding their values:
        1 (0x1)  One-hit kills for enemies.
        2 (0x2)  Ignore enemy hit messages directed at Conrad.
        4 (0x4)  Prevent ordinary decreases of Conrad's life counter.
      For example, --cheats=3 combines 1 and 2; --cheats=7 enables all three.
      These flags do not prevent every scripted death or environmental hazard.
      MASK accepts an unsigned decimal or 0x hexadecimal value up to
      4294967295. Bits other than those listed have no effect.

AUDIO AND MUSIC
  --outputrate=HZ                        Default: 22050
      Requested audio sample rate, from 8000 through 192000 Hz inclusive.
      The audio device may negotiate another rate. This changes audio output,
      not gameplay speed. For example, --outputrate=48000 requests 48 kHz.

  --mididriver=NAME                      Default: adlib
      Synthesizer for original DOS PRF/MIDI/INS music:
        adlib      Built-in OPL3 FM synthesis; no external ROMs or soundfont.
        fluidsynth General MIDI synthesis; requires --soundfont and a build
                   with FluidSynth enabled. MT-32 programs are mapped to GM.
        mt32       MT-32/CM-32L emulation; requires --rompath and a build with
                   the MT-32 backend enabled.
        none       Disable PRF synthesis. Other music and effects still play.
      Selecting a backend that was not built in fails at startup.
      Existing OGG/CPC/MOD music choices retain priority where applicable;
      this option affects the PRF fallback and does not force those tracks
      to use a synthesizer. Set use_prf_music=false in fb.cfg to disable
      this fallback altogether.

  --soundfont=PATH                       Default: none
      Your General MIDI soundfont file for --mididriver=fluidsynth, such as
      a compatible .sf2 file. Ignored by the other music backends.

  --rompath=PATH                         Default: current working directory
      Folder for --mididriver=mt32. Supply one compatible pair named:
        MT32_CONTROL.ROM and MT32_PCM.ROM, or
        CM32L_CONTROL.ROM and CM32L_PCM.ROM.
      The engine does not supply ROMs. Ignored by the other music backends.

)",
	      stdout);
	printMusicBackends();
	fputs(R"(
DIAGNOSTICS
  --debug=MASK                           Default: 1 (general information)
      Select diagnostic categories in either Debug or Release builds.
      Add values to combine categories, or use a 0x hexadecimal mask:
           1  0x0001  General information
           2  0x0002  Resources           4  0x0004  Menus
           8  0x0008  Unpacking         16  0x0010  Object scripting
          32  0x0020  Video             64  0x0040  Gameplay
         128  0x0080  Collision        256  0x0100  Sound
         512  0x0200  Cutscenes       1024  0x0400  MOD music
        2048  0x0800  Sound effects   4096  0x1000  File access
        8192  0x2000  Demo replay    16384  0x4000  PC-98 PAQ archives
      Examples: --debug=65 (general + gameplay), --debug=0x1040 (gameplay
      + files), --debug=32767 (all listed categories). --debug=0 disables
      category logging; startup text, warnings and errors remain visible.
      The maximum accepted mask is 4294967295; unlisted bits have no effect.

IN-GAME SETTINGS
  Escape, F10, controller Start or the on-screen Menu [F10] button opens the
  paused QoL overlay. Eight pages provide a save/load/checkpoint browser,
  rewind, live display/audio, enhancements, keyboard rebinding, all engine
  cheats and level/difficulty selection. Enhancements includes Conrad's
  Original, Red/Pink or White T-shirt inside the jacket. The jacket, shoes and
  other artwork retain their colours. Save settings keeps the choice.
  Session also offers level restart,
  health/rewind counts and clean BMP screenshots saved under --savepath.
  Compact layout fits the original 256x224 resolution without window resizing.
  Resume and a turquoise EXIT GAME button remain visible; exit asks first.
  Cheats are session-only; Save settings does not preserve them.
  Mouse, keyboard navigation and SDL gamepad navigation are supported.
  Tab/Shift+Tab changes focus; arrows adjust; Enter/Space activates.
  Controller shoulders switch tabs, A confirms and B closes.
  Rebinding captures one physical key. Escape cancels capture. F10 is fixed
  so settings remain reachable even after changing the Pause binding.

  Changes apply to the current session. Save settings writes preferences.cfg
  in --savepath using an atomic replacement. Startup reads fb.cfg, then saved
  preferences, then applies explicit display/rewind command-line overrides.
  --no-preferences skips the saved layer. Undo settings changes restores the
  opening preferences; Restore default settings resets built-in choices.
  Neither changes the saved file until Save settings is chosen.
  Output sample rate, MIDI driver and resource paths remain startup options.

KEYBOARD CONTROLS (DEFAULT BINDINGS)
  Arrow keys           Move Conrad or navigate menus.
  Shift                Action, interact, fire, or run with a direction.
  Space                Draw or holster the weapon.
  Enter                Use the selected item or confirm a menu choice.
  Backspace / Tab      Open the inventory.
  Escape               Open in-game settings or leave a menu.
  F10                  Always open in-game settings (fixed recovery key).
  Ctrl+S / Ctrl+L       Save / load the current manual slot (initially 1).
  Ctrl+PageUp/PageDown  Select the next / previous manual slot, within 1-99.
  Ctrl+R               Rewind one snapshot when --autosave is enabled.
  Alt+Enter            Toggle fullscreen.
  Alt+PageUp/PageDown   Increase / decrease supported pixel scaling.
  F1 / F2 / F3         Select point / linear / scale rendering.
  Alt+S                Save a BMP screenshot in the working directory.
  Alt+X                Quit the game.

CONFIGURATION SWITCHES
  The --config file uses one case-sensitive name=value entry per line.
  Values true/false and 1/0 are accepted, ignoring value case and surrounding
  whitespace. Blank lines and lines starting with # are ignored. Unknown
  keys or invalid values produce a warning and keep the previous value.
  Defaults below are built-in; the supplied fb.cfg changes some of them.
  Resource-dependent switches only apply when the required files exist.

  bypass_protection=true
      Skip the original game's copy-protection questions.
  enable_password_menu=true
      Offer the original password-based level selection interface.
  enable_language_selection=false
      Allow Left/Right to change the language at the title menu.
  fade_out_palette=true
      Fade screen transitions to black; false uses palette blending.
  use_tile_data=false
      Prefer DOS BNQ/LEV tile backgrounds over MAP backgrounds.
  use_text_cutscenes=false
      Use available text summaries in place of polygon cutscenes.
  use_seq_cutscenes=true
      Play available PC CD SEQ movies in place of polygon cutscenes.
  use_words_protection=false
      When protection is enabled, use the DOS manual-word lookup screen.
  use_white_tshirt=false
      Select Conrad's White T-shirt preset. A saved QoL shirt choice
      overrides this legacy boolean; the menu also offers Original and Red/Pink.
  play_asc_cutscene=false
      Enable the optional ASC cutscene.
  play_caillou_cutscene=false
      Enable the optional CAILLOU-F.SET cutscene.
  play_carte_cutscene=false
      Enable the optional CARTE cutscene.
  play_metro_cutscene=false
      Enable the optional METRO cutscene.
  play_serrure_cutscene=false
      Enable the optional SERRURE cutscene.
  play_gamesaved_sound=false
      Play the checkpoint/manual-save confirmation sample.
  restore_memo_cutscene=true
      Restore the additional MEMO cutscene content.
  order_inventory_original=false
      When true, place newly collected items at the bottom of the inventory.
  fix_fmopl_e0_reg=false
      Correct the original AdLib waveform-register quirk in PRF synthesis.
  use_prf_music=true
      Allow original DOS PRF/MIDI/INS music as a fallback.

EXAMPLES
)",
	      stdout);
	fprintf(stdout, R"(  Play your DOS data in a window:
    %s --datapath="/games/Flashback/DATA" --window

  Play unpacked Amiga data with separate saves and rewind enabled:
    %s --datapath="/games/Flashback/Amiga" \
      --savepath="/games/Flashback/Saves" --window --autosave

  Use neighboring-room side content and sharp pixel sampling:
    %s --datapath="/games/Flashback/DATA" \
      --widescreen=adjacent --scaler=point@3 --maximized

  Use FluidSynth for DOS PRF music (requires an enabled backend):
    %s --datapath="/games/Flashback/DATA" \
      --mididriver=fluidsynth --soundfont="/music/General MIDI.sf2"

  Read this guide in a terminal pager, where 'less' is available:
    %s -h | less

EXIT STATUS
  0 means a normal exit, including help and version requests.
  A nonzero value indicates invalid options, missing data or another error.
)",
	        program, program, program, program, program);
}

void printVersionDetails() {
	puts("Based on REminiscence / REinforced by Gregory Montoir and Alex Chermenin.");
	printMusicBackends();
}
} // namespace cli
