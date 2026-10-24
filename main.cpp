
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <filesystem>
#include "cli_text.h"
#include "runtime_paths.h"
#include "config.h"
#include "cutscene_tool.h"
#include "file.h"
#include "fs.h"
#include "game.h"
#include "intern.h"
#include "systemstub.h"
#include "util.h"

static int detectVersion(FileSystem *fs) {
	static const struct {
		const char *filename;
		int type;
		const char *name;
	} table[] = {
	    {"DEMO_UK.ABA", kResourceTypeDOS, "DOS (Demo)"},
	    {"GLOB_FR.ABA", kResourceTypeDOS, "DOS"},
	    {"INTRO.SEQ", kResourceTypeDOS, "DOS CD"},
	    {"MENU1SSI.MAP", kResourceTypeDOS, "DOS SSI"},
	    {"LEVEL1.MAP", kResourceTypeDOS, "DOS"},
	    {"LEVEL1.BNQ", kResourceTypeDOS, "DOS (Demo)"},
	    {"PRESENT.LEV", kResourceTypeSega, "Sega Mega Drive (extracted data, experimental)"},
	    {"LEVEL1.LEV", kResourceTypeAmiga, "Amiga"},
	    {"DEMO.LEV", kResourceTypeAmiga, "Amiga (Demo)"},
	    {"FLASHBACK.BIN", kResourceTypeMac, "Macintosh"},
	    {"FLASHBACK.RSRC", kResourceTypeMac, "Macintosh"},
	    {"GLOBAL.PAQ", kResourceTypePC98, "PC98"},
	    {0, -1, 0}};
	for (int i = 0; table[i].filename; ++i) {
		File f;
		if (f.open(table[i].filename, "rb", fs)) {
			debug(DBG_INFO, "Detected %s version", table[i].name);
			return table[i].type;
		}
	}
	return -1;
}

static Language detectLanguage(FileSystem *fs) {
	static const struct {
		const char *filename;
		Language language;
	} table[] = {// DOS
	             {"GLOB_FR.ABA", LANG_FR},
	             {"ENGCINE.TXT", LANG_EN},
	             {"FR_CINE.TXT", LANG_FR},
	             {"RUSCINE.TXT", LANG_RU},
	             {"GERCINE.TXT", LANG_DE},
	             {"SPACINE.TXT", LANG_SP},
	             {"ITACINE.TXT", LANG_IT},
	             // Amiga
	             {"FRCINE.TXT", LANG_FR},
	             {0, LANG_EN}};
	for (int i = 0; table[i].filename; ++i) {
		File f;
		if (f.open(table[i].filename, "rb", fs)) {
			return table[i].language;
		}
	}
	warning("Unable to detect language, defaults to English");
	return LANG_EN;
}

Options g_options;
const char *g_caption = "REminiscence2";

static void parseScaler(char *name, ScalerParameters *scalerParameters) {
	char *sep = strchr(name, '@');
	if (sep) {
		*sep = 0;
		scalerParameters->factor = atoi(sep + 1);
	}
	strncpy(scalerParameters->name, name, sizeof(scalerParameters->name) - 1);
	scalerParameters->name[sizeof(scalerParameters->name) - 1] = 0;
}

static bool parseWidescreen(const char *mode, WidescreenMode &result) {
	static const struct {
		const char *name;
		WidescreenMode mode;
	} modes[] = {
	    {"adjacent", kWidescreenAdjacentRooms},
	    {"adjacent-blur", kWidescreenAdjacentRoomsBlur},
	    {"mirror", kWidescreenMirrorRoom},
	    {"blur", kWidescreenBlur},
	    {"cdi", kWidescreenCDi},
	    {"none", kWidescreenNone},
	    {0, kWidescreenNone},
	};
	for (int i = 0; modes[i].name; ++i) {
		if (strcasecmp(modes[i].name, mode) == 0) {
			result = modes[i].mode;
			return true;
		}
	}
	return false;
}

static bool makeDir(const char *name) {
	std::error_code ec;
	const auto path = std::filesystem::u8path(name);
	std::filesystem::create_directories(path, ec);
	return !ec && std::filesystem::is_directory(path, ec);
}

int main(int argc, char *argv[]) {
	// The catalog is a JSON interface for small front ends. Its stdout must stay
	// machine-readable; normal launches retain the startup banner.
	bool listCutscenes = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--list-cutscenes") == 0) {
			listCutscenes = true;
		}
	}
	if (!listCutscenes) {
		cli::printStartup();
	}
	bool playCutscene = false;
	cinematics::Selection selectedCutscene;

	const char *dataPath = "DATA";
	const char *tunePath = "TUNES";
	const char *savePath = "SAVE";
	const char *configPath = "fb.cfg";
	bool explicitConfig = false, explicitData = false, explicitTunes = false, explicitSaves = false, portable = false;
	bool positionalDataPath = false;
	int levelNum = 0;
	bool fullscreen = true;
	bool autoSave = false;
	bool explicitWindow = false, explicitScaler = false, explicitWidescreen = false;
	bool explicitRewind = false, ignorePreferences = false;
	RuntimeOptions runtime;
	music::Options musicOptions;
	uint32_t debugMask = DBG_INFO;
	uint32_t cheats = 0;
	WidescreenMode widescreen = kWidescreenNone;
	ScalerParameters scalerParameters = ScalerParameters::defaults();
	int forcedLanguage = -1;
	if (argc == 2) {
		// data path as the only command line argument
		struct stat st;
		if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
			dataPath = tunePath = argv[1];
			explicitData = explicitTunes = true;
			positionalDataPath = true;
		}
	}
	while (1) {
		static struct option options[] = {{"datapath", required_argument, 0, 1},
		                                  {"savepath", required_argument, 0, 2},
		                                  {"tunepath", required_argument, 0, 3},
		                                  {"levelnum", required_argument, 0, 4},
		                                  {"window", no_argument, 0, 5},
		                                  {"scaler", required_argument, 0, 6},
		                                  {"language", required_argument, 0, 7},
		                                  {"widescreen", required_argument, 0, 8},
		                                  {"autosave", no_argument, 0, 9},
		                                  {"cheats", required_argument, 0, 10},
		                                  {"help", no_argument, 0, 'h'},
		                                  {"version", no_argument, 0, 12},
		                                  {"config", required_argument, 0, 13},
		                                  {"maximized", no_argument, 0, 14},
		                                  {"outputrate", required_argument, 0, 15},
		                                  {"debug", required_argument, 0, 16},
		                                  {"mididriver", required_argument, 0, 17},
		                                  {"soundfont", required_argument, 0, 18},
		                                  {"rompath", required_argument, 0, 19},
		                                  {"list-cutscenes", no_argument, 0, 20},
		                                  {"play-cutscene", required_argument, 0, 21},
		                                  {"no-preferences", no_argument, 0, 22},
		                                  {"portable", no_argument, 0, 23},
		                                  {0, 0, 0, 0}};
		int index;
		const int c = getopt_long(argc, argv, "h", options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 1:
			dataPath = optarg; explicitData = true;
			break;
		case 2:
			savePath = optarg; explicitSaves = true;
			break;
		case 3:
			tunePath = optarg; explicitTunes = true;
			break;
		case 4: {
			char *end = 0;
			errno = 0;
			const long value = strtol(optarg, &end, 10);
			if (errno || end == optarg || *end || value < 0 || value > 6) {
				fprintf(stderr, "Invalid level '%s': expected an integer from 0 to 6.\n", optarg);
				return EXIT_FAILURE;
			}
			levelNum = static_cast<int>(value);
		} break;
		case 5:
			fullscreen = false;
			explicitWindow = true;
			break;
		case 6:
			parseScaler(optarg, &scalerParameters);
			explicitScaler = true;
			break;
		case 7: {
			static const struct {
				int lang;
				const char *str;
			} languages[] = {{LANG_FR, "FR"}, {LANG_EN, "EN"}, {LANG_DE, "DE"}, {LANG_SP, "SP"},
			                 {LANG_IT, "IT"}, {LANG_JP, "JP"}, {LANG_RU, "RU"}, {-1, 0}};
			int i = 0;
			for (; languages[i].str; ++i) {
				if (strcasecmp(languages[i].str, optarg) == 0) {
					forcedLanguage = languages[i].lang;
					break;
				}
			}
			if (!languages[i].str) {
				fprintf(stderr, "Invalid language '%s'.\n", optarg);
				return EXIT_FAILURE;
			}
		} break;
		case 8:
			if (!parseWidescreen(optarg, widescreen)) {
				fprintf(stderr, "Invalid widescreen mode '%s'.\n", optarg);
				return EXIT_FAILURE;
			}
			explicitWidescreen = true;
			break;
		case 9:
			autoSave = true;
			explicitRewind = true;
			break;
		case 10:
			if (!parseUnsignedOption(optarg, 0, UINT32_MAX, cheats)) {
				fprintf(stderr, "Invalid cheats mask '%s'.\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'h':
			cli::printHelp(argv[0]);
			return EXIT_SUCCESS;
		case 12:
			cli::printVersionDetails();
			return EXIT_SUCCESS;
		case 13:
			configPath = optarg;
			explicitConfig = true;
			break;
		case 14:
			runtime.maximized = true;
			fullscreen = false;
			explicitWindow = true;
			break;
		case 15: {
			uint32_t rate = 0;
			if (!parseUnsignedOption(optarg, 8000, 192000, rate)) {
				fprintf(stderr, "Invalid output rate '%s': expected 8000-192000 Hz.\n", optarg);
				return EXIT_FAILURE;
			}
			runtime.outputSampleRate = static_cast<int>(rate);
			break;
		}
		case 16:
			if (!parseUnsignedOption(optarg, 0, UINT32_MAX, debugMask)) {
				fprintf(stderr, "Invalid debug mask '%s'.\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 17:
			if (!strcasecmp(optarg, "adlib")) {
				musicOptions.backend = music::Backend::Adlib;
			} else if (!strcasecmp(optarg, "fluidsynth")) {
				musicOptions.backend = music::Backend::FluidSynth;
			} else if (!strcasecmp(optarg, "mt32")) {
				musicOptions.backend = music::Backend::Mt32;
			} else if (!strcasecmp(optarg, "none")) {
				musicOptions.backend = music::Backend::None;
			} else {
				fprintf(stderr, "Invalid MIDI driver '%s'.\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 18:
			musicOptions.soundFont = optarg;
			break;
		case 19:
			musicOptions.romDirectory = optarg;
			break;
		case 20:
			listCutscenes = true;
			break;
		case 21:
			if (!cinematics::parseSelection(optarg, selectedCutscene)) {
				fputs("Invalid cutscene selection: expected NAME or NAME:PART (part 0..65535).\n", stderr);
				return EXIT_FAILURE;
			}
			playCutscene = true;
			break;
		case 23:
			portable = true;
			break;
		case 22:
			ignorePreferences = true;
			break;
		default:
			cli::printUsage(stderr, argv[0]);
			fputs("Run with -h or --help for all options and examples.\n", stderr);
			return EXIT_FAILURE;
		}
	}
	if (listCutscenes && playCutscene) {
		fputs("--list-cutscenes and --play-cutscene cannot be combined.\n", stderr);
		return EXIT_FAILURE;
	}
	if (optind < argc && !positionalDataPath) {
		fprintf(stderr, "Unexpected argument '%s'. Use --datapath=PATH to select game files.\n",
		        argv[optind]);
		return EXIT_FAILURE;
	}
	const auto paths = runtimePaths(portable);
	if (!explicitData) dataPath = paths.data.c_str();
	if (!explicitTunes) tunePath = explicitData ? dataPath : paths.tunes.c_str();
	if (!explicitSaves) savePath = paths.saves.c_str();
	if (!explicitConfig) configPath = paths.config.c_str();
	setDefaultOptions(g_options);
	if (!loadConfiguration(configPath, g_options) && explicitConfig) {
		fprintf(stderr, "Unable to read configuration file '%s'.\n", configPath);
		return EXIT_FAILURE;
	}
	settings::Preferences preferences;
	preferences.readCompatibility(g_options);
	if (!ignorePreferences && !listCutscenes) {
		std::string diagnostic;
		const auto path = (std::filesystem::u8path(savePath) / "preferences.cfg").u8string();
		settings::load(path, preferences, diagnostic);
		if (!diagnostic.empty()) fprintf(stderr, "Preferences: %s\n", diagnostic.c_str());
	}
	if (explicitWindow) preferences.fullscreen = fullscreen;
	else fullscreen = preferences.fullscreen;
	if (explicitRewind) preferences.rewind = autoSave;
	else autoSave = preferences.rewind;
	if (explicitWidescreen) preferences.widescreen = widescreen;
	else widescreen = static_cast<WidescreenMode>(preferences.widescreen);
	if (!explicitScaler) {
		const char *name = preferences.filter == settings::Filter::Point ? "point" :
		                   preferences.filter == settings::Filter::Linear ? "linear" : "scale";
		strncpy(scalerParameters.name, name, sizeof(scalerParameters.name) - 1);
		scalerParameters.factor = preferences.scale;
	} else {
		if (strcmp(scalerParameters.name, "point") == 0) preferences.filter = settings::Filter::Point;
		else if (strcmp(scalerParameters.name, "linear") == 0) preferences.filter = settings::Filter::Linear;
		else preferences.filter = settings::Filter::Scale;
		preferences.scale = CLIP(scalerParameters.factor, preferences.filter == settings::Filter::Scale ? 2 : 1,
		                        preferences.filter == settings::Filter::Scale ? 4 : 6);
	}
	preferences.applyCompatibility(g_options);
	if (!music::backendAvailable(musicOptions.backend)) {
		fprintf(stderr, "Requested MIDI driver is not enabled in this build.\n");
		return EXIT_FAILURE;
	}
	musicOptions.correctWaveformRegister = g_options.fix_fmopl_e0_reg;
	g_debugMask = listCutscenes ? 0 : debugMask;
	FileSystem fs(dataPath);
	const int version = detectVersion(&fs);
	if (version == -1) {
		fprintf(stderr,
		        "No supported Flashback game data found in '%s'.\n"
		        "Provide your original DOS, Amiga, Macintosh or PC-98 files with --datapath=PATH.\n"
		        "REminiscence2 includes the engine only.\n",
		        dataPath);
		return EXIT_FAILURE;
	}
	if (!listCutscenes && !playCutscene && !makeDir(savePath)) {
		error("Unable to create directories");
		return -1;
	}
	FileSystem tuneFs(tunePath);
	const Language language =
	    (forcedLanguage == -1) ? detectLanguage(&fs) : (Language)forcedLanguage;
	if (listCutscenes) {
		Resource resource(&fs, static_cast<ResourceType>(version), language);
		resource.init(true);
		cinematics::printCatalog(stdout, cinematics::catalog(resource), resource);
		return EXIT_SUCCESS;
	}
	if (playCutscene) {
		Resource resource(&fs, static_cast<ResourceType>(version), language);
		resource.init(true);
		bool available = false;
		for (const auto &entry : cinematics::catalog(resource)) {
			available |= entry.name == selectedCutscene.name && selectedCutscene.part < entry.parts;
		}
		if (!available) {
			fprintf(stderr, "Cutscene '%s:%u' is unavailable. Use --list-cutscenes to inspect the assets.\n",
			        selectedCutscene.name.c_str(), selectedCutscene.part);
			return EXIT_FAILURE;
		}
	}
	SystemStub *stub = SystemStub_SDL_create();
	stub->configure(runtime);
	Game *g = new Game(stub, &fs, &tuneFs, savePath, levelNum, (ResourceType)version, language,
	                   widescreen, autoSave, cheats);
	g->configurePreferences(preferences);
	switch (musicOptions.backend) {
	case music::Backend::None: g->_settingsAudioBackend = "Disabled"; break;
	case music::Backend::Adlib: g->_settingsAudioBackend = "AdLib / OPL3"; break;
	case music::Backend::FluidSynth: g->_settingsAudioBackend = "FluidSynth"; break;
	case music::Backend::Mt32: g->_settingsAudioBackend = "MT-32"; break;
	}
	g->_mix.configureMusic(musicOptions, &fs);
	stub->init(g_caption, g->_vid._w, g->_vid._h, fullscreen, widescreen, &scalerParameters);
	bool success = true;
	if (playCutscene) {
		g->initCutsceneResources();
		success = cinematics::play(*g, selectedCutscene);
	} else {
		g->run();
	}
	delete g;
	stub->destroy();
	delete stub;
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
