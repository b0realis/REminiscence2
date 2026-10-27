
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
#include "editor/runtime.h"
#include "editor/campaign.h"

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

static bool parseScaler(const char *text, ScalerParameters *parameters) {
	const std::string input(text);
	const auto separator = input.find('@');
	const auto name = input.substr(0, separator);
	if (name.empty() || name.size() >= sizeof(parameters->name))
		return false;
	if (separator != std::string::npos) {
		uint32_t factor;
		if (!parseUnsignedOption(input.c_str() + separator + 1, 1, 64, factor))
			return false;
		parameters->factor = factor;
	}
	memcpy(parameters->name, name.c_str(), name.size() + 1);
	return true;
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
	bool listCutscenes = false, listAssets = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--list-assets") == 0)
			listAssets = true;
		if (strcmp(argv[i], "--list-cutscenes") == 0) {
			listCutscenes = true;
		}
	}
	if (!listCutscenes && !listAssets) {
		cli::printStartup();
	}
	std::string requestedAsset, requestedMusic;

	std::string customProject, editorControl, customCampaign, campaignStart;
	editor::CampaignMode campaignMode = editor::CampaignMode::Ask;
	bool explicitMusic = false;
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
	bool autoSave = false, continueRequested = false;
	int diskInterval = -1;
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
			explicitData = true;
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
		                                  {"continue", no_argument, 0, 24},
		                                  {"disk-autosave", required_argument, 0, 25},
		                                  {"no-disk-autosave", no_argument, 0, 26},
		                                  {"list-assets", no_argument, 0, 27},
		                                  {"asset-set", required_argument, 0, 28},
		                                  {"music-source", required_argument, 0, 29},
		                                  {"project", required_argument, 0, 30},
		                                  {"editor-session", required_argument, 0, 31},
		                                  {"campaign", required_argument, 0, 32},
		                                  {"campaign-start", required_argument, 0, 33},
		                                  {"campaign-resume", no_argument, 0, 34},
		                                  {"campaign-new", no_argument, 0, 35},
		                                  {0, 0, 0, 0}};
		int index;
		const int c = getopt_long(argc, argv, "h", options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 34:
		case 35:
			if (campaignMode != editor::CampaignMode::Ask) { fputs("Choose only one campaign start mode.\n", stderr); return EXIT_FAILURE; }
			campaignMode = c == 34 ? editor::CampaignMode::Resume : editor::CampaignMode::New;
			break;
		case 32:
			customCampaign = optarg;
			if (customCampaign.empty()) { fputs("--campaign needs a campaign JSON filename.\n", stderr); return EXIT_FAILURE; }
			break;
		case 33:
			campaignStart = optarg;
			if (campaignStart.empty()) { fputs("--campaign-start needs a stage ID.\n", stderr); return EXIT_FAILURE; }
			break;
		case 31:
			editorControl = optarg;
			break;
		case 30:
			customProject = optarg;
			if (customProject.empty()) {
				fputs("--project needs a project JSON filename.\n", stderr);
				return EXIT_FAILURE;
			}
			break;
		case 27:
			listAssets = true;
			break;
		case 28:
			requestedAsset = optarg;
			break;
		case 29:
			requestedMusic = optarg;
			explicitMusic = true;
			break;
		case 24:
			continueRequested = true;
			break;
		case 25: {
			uint32_t seconds;
			if (!parseUnsignedOption(optarg, 15, 600, seconds)) {
				fprintf(stderr, "Invalid disk autosave interval: expected 15 to 600 seconds.\n");
				return EXIT_FAILURE;
			}
			diskInterval = seconds;
			break;
		}
		case 26:
			diskInterval = 0;
			break;
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
			if (!parseScaler(optarg, &scalerParameters)) {
				fprintf(stderr, "Invalid scaler: use NAME or NAME@FACTOR (1-64).\n");
				return EXIT_FAILURE;
			}
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
	if (continueRequested && (listCutscenes || playCutscene)) {
		fprintf(stderr, "--continue cannot be combined with cutscene-only commands.\n");
		return EXIT_FAILURE;
	}
	if ((!customCampaign.empty() && (!customProject.empty() || !editorControl.empty() || continueRequested || listCutscenes || playCutscene || listAssets)) || ((!campaignStart.empty() || campaignMode != editor::CampaignMode::Ask) && customCampaign.empty()) || (!campaignStart.empty() && campaignMode == editor::CampaignMode::Resume)) { fputs("--campaign cannot be combined with other playback modes; --campaign-start needs --campaign.\n", stderr); return EXIT_FAILURE; }
	if (!editorControl.empty() && customProject.empty()) {
		fputs("--editor-session requires --project.\n", stderr);
		return EXIT_FAILURE;
	}
	if (!customProject.empty() && (continueRequested || listCutscenes || playCutscene || listAssets)) {
		fputs("--project cannot be combined with --continue or asset/cutscene listing/playback.\n", stderr);
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
	if (!ignorePreferences) {
		std::string diagnostic;
		const auto path = (std::filesystem::u8path(savePath) / "preferences.cfg").u8string();
		settings::load(path, preferences, diagnostic);
		if (!diagnostic.empty()) fprintf(stderr, "Preferences: %s\n", diagnostic.c_str());
	}
	if (explicitWindow) preferences.fullscreen = fullscreen;
	else fullscreen = preferences.fullscreen;
	if (runtime.maximized)
		preferences.windowWidth = preferences.windowHeight = 0;
	if (explicitRewind) preferences.rewind = autoSave;
	else autoSave = preferences.rewind;
	if (explicitWidescreen) preferences.widescreen = widescreen;
	else widescreen = static_cast<WidescreenMode>(preferences.widescreen);
	if (!explicitScaler) {
		const char *name = settings::filterScalerName(preferences.filter);
		strncpy(scalerParameters.name, name, sizeof(scalerParameters.name) - 1);
		scalerParameters.factor = preferences.scale;
	} else {
		preferences.filter = settings::filterFromScalerName(scalerParameters.name);
		preferences.scale = CLIP(scalerParameters.factor, settings::filterMinScale(preferences.filter),
		                        settings::filterMaxScale(preferences.filter));
	}
	if (diskInterval >= 0) {
		preferences.diskAutosave = diskInterval != 0;
		if (diskInterval)
			preferences.diskAutosaveSeconds = diskInterval;
	}
	preferences.applyCompatibility(g_options);
	if (!music::backendAvailable(musicOptions.backend)) {
		fprintf(stderr, "Requested MIDI driver is not enabled in this build.\n");
		return EXIT_FAILURE;
	}
	musicOptions.correctWaveformRegister = g_options.fix_fmopl_e0_reg;
	g_debugMask = listCutscenes ? 0 : debugMask;
	const auto library = assets::discover(dataPath);
	for (const auto &message : library.diagnostics)
		fprintf(stderr, "%s\n", message.c_str());
	if (listAssets) {
		puts("Game editions (use --asset-set=ID):");
		for (const auto &source : library.games)
			printf("  %s\t%s\n", source.id.c_str(), source.label.c_str());
		puts("Music sources (use --music-source=ID, or native):");
		for (const auto &source : library.music)
			printf("  %s\t%s\n", source.id.c_str(), source.label.c_str());
		return library.games.empty() ? EXIT_FAILURE : EXIT_SUCCESS;
	}
	if (library.games.empty()) {
		fprintf(stderr,
		        "No supported Flashback game data found in '%s'.\n"
		        "Put each prepared edition in its own subfolder. REminiscence2 includes the engine "
		        "only.\n",
		        dataPath);
		return EXIT_FAILURE;
	}
	int selected =
	    assets::find(library.games, requestedAsset.empty() ? preferences.assetSet : requestedAsset);
	if (!requestedAsset.empty() && selected < 0) {
		fprintf(stderr, "Unknown or ambiguous asset set '%s'. Use --list-assets.\n",
		        requestedAsset.c_str());
		return EXIT_FAILURE;
	}
	if (explicitMusic) {
		if (requestedMusic == "native")
			preferences.musicSource.clear();
		else {
			const int musicIndex = assets::find(library.music, requestedMusic);
			if (musicIndex < 0) {
				fprintf(stderr, "Unknown or ambiguous music source '%s'. Use --list-assets.\n",
				        requestedMusic.c_str());
				return EXIT_FAILURE;
			}
			preferences.musicSource = library.music[musicIndex].id;
		}
	}
	if (!preferences.musicSource.empty() &&
	    assets::find(library.music, preferences.musicSource) < 0) {
		fprintf(stderr, "Saved music source is unavailable; using this edition's music.\n");
		preferences.musicSource.clear();
	}
	const bool choose = selected < 0 && library.games.size() > 1;
	if (choose && (listCutscenes || playCutscene || continueRequested || !customProject.empty() || !customCampaign.empty())) {
		fprintf(stderr, "Multiple asset sets found. Choose --asset-set=ID (see --list-assets).\n");
		return EXIT_FAILURE;
	}
	if (selected < 0)
		selected = 0;
	if (!customCampaign.empty()) return editor::playCampaign(std::filesystem::u8path(customCampaign), library.games[selected], campaignStart, std::filesystem::u8path(savePath), campaignMode);
	if (!customProject.empty())
		return editor::playProject(std::filesystem::u8path(customProject), library.games[selected], std::filesystem::u8path(editorControl));
	if (!listCutscenes && !playCutscene && !makeDir(savePath)) {
		fprintf(stderr, "Unable to create save directory '%s'.\n", savePath);
		return EXIT_FAILURE;
	}
	const auto preferencePath = (std::filesystem::u8path(savePath) / "preferences.cfg").u8string();
	auto rememberEdition = [&](const std::string &id) {
		settings::Preferences stored;
		stored.readCompatibility(g_options);
		std::string reason;
		settings::load(preferencePath, stored, reason);
		stored.assetSet = id;
		if (!settings::save(preferencePath, stored, reason))
			fprintf(stderr, "Could not remember edition: %s\n", reason.c_str());
	};
	if (choose) {
		std::unique_ptr<SystemStub> picker(SystemStub_SDL_create());
		picker->configure(runtime);
		picker->configurePreferences(preferences);
		picker->init(g_caption, 256, 224, fullscreen, widescreen, &scalerParameters);
		std::vector<uint8_t> black(256 * 224);
		picker->copyRect(0, 0, 256, 224, black.data(), 256);
		settings::Session session;
		session.assetPicker = session.startup = true;
		session.values = preferences;
		session.assetSources = library.games;
		session.assetIndex = selected;
		picker->showSettings(session);
		const bool quit = picker->_pi.quit || session.action == settings::Action::Quit;
		selected = session.assetIndex;
		picker->destroy();
		if (quit)
			return EXIT_SUCCESS;
		rememberEdition(library.games[selected].id);
	}
	for (;;) {
		const auto &source = library.games[selected];
		const int version = source.edition;
		preferences.assetSet = source.id;
		FileSystem fs(source.root.c_str(), source.excludedRoots);
		const bool nativeTunes = !explicitTunes && (explicitData || paths.tunes == paths.data);
		FileSystem tuneFs(nativeTunes ? source.root.c_str() : tunePath,
		                  nativeTunes ? source.excludedRoots : std::vector<std::string>{});
		const Language language =
		    forcedLanguage == -1 ? detectLanguage(&fs) : Language(forcedLanguage);
		if (listCutscenes) {
			Resource resource(&fs, ResourceType(version), language);
			resource.init(true);
			cinematics::printCatalog(stdout, cinematics::catalog(resource), resource);
			return EXIT_SUCCESS;
		}
		if (playCutscene) {
			Resource resource(&fs, ResourceType(version), language);
			resource.init(true);
			bool available = false;
			for (const auto &entry : cinematics::catalog(resource))
				available |=
				    entry.name == selectedCutscene.name && selectedCutscene.part < entry.parts;
			if (!available) {
				fprintf(stderr,
				        "Cutscene '%s:%u' is unavailable. Use --list-cutscenes to inspect the "
				        "assets.\n",
				        selectedCutscene.name.c_str(), selectedCutscene.part);
				return EXIT_FAILURE;
			}
		}
		if (continueRequested && progress::scan(savePath, version).empty()) {
			fprintf(stderr, "No valid autosave is available for this asset edition.\n");
			return EXIT_FAILURE;
		}
		std::unique_ptr<SystemStub> stub(SystemStub_SDL_create());
		stub->configure(runtime);
		auto game =
		    std::make_unique<Game>(stub.get(), &fs, &tuneFs, savePath, levelNum,
			                       ResourceType(version), language, widescreen, autoSave, cheats);
		game->_assetSources = library.games;
		game->_musicSources = library.music;
		game->_assetIndex = selected;
		game->configurePreferences(preferences);
		game->_continueRequested = continueRequested;
		switch (musicOptions.backend) {
		case music::Backend::None:
			game->_settingsAudioBackend = "Disabled";
			break;
		case music::Backend::Adlib:
			game->_settingsAudioBackend = "AdLib / OPL3";
			break;
		case music::Backend::FluidSynth:
			game->_settingsAudioBackend = "FluidSynth";
			break;
		case music::Backend::Mt32:
			game->_settingsAudioBackend = "MT-32";
			break;
		}
		game->_mix.configureMusic(musicOptions, &fs);
		game->selectMusicSource(preferences.musicSource);
		stub->init(g_caption, game->_vid._w, game->_vid._h, preferences.fullscreen,
		           preferences.widescreen, &scalerParameters);
		bool success;
		if (playCutscene) {
			game->initCutsceneResources();
			success = cinematics::play(*game, selectedCutscene);
		} else {
			game->run();
			success = !game->_continueFailed;
		}
		const int next = game->_requestedAsset;
		const bool quit = stub->_pi.quit;
		preferences = game->_preferences;
		game.reset();
		stub->destroy();
		if (next < 0 || quit || playCutscene)
			return success ? EXIT_SUCCESS : EXIT_FAILURE;
		selected = next;
		continueRequested = false;
		rememberEdition(library.games[selected].id);
		const char *filter = settings::filterScalerName(preferences.filter);
		snprintf(scalerParameters.name, sizeof(scalerParameters.name), "%s", filter);
		scalerParameters.factor = preferences.scale;
		autoSave = preferences.rewind;
		widescreen = static_cast<WidescreenMode>(preferences.widescreen);
	}
}
