// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "settings_overlay.h"
#include "keyboard_diagram.h"
#include "config.h"
#include "fs.h"
#include "game.h"
#include "headless_system.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "save_catalog.h"
#include <SDL.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

Options g_options;
const char *g_caption = "settings tests";
static int failures = 0;
static void check(bool good, const char *message) {
	if (!good) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}
static void write(const std::filesystem::path &path, const std::string &text) {
	std::ofstream file(path);
	file << text;
}
static bool testMusic(void *, int16_t *samples, int frames) {
	std::fill_n(samples, frames * 2, 10000);
	return true;
}
static settings::Preferences remappedKeyboard() {
	settings::Preferences p;
	p.keys = {{SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_K,
	           SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_I, SDL_SCANCODE_P}};
	return p;
}
static void keyboardModel() {
	const settings::Preferences defaults;
	for (size_t i = 0; i < settings::BindingCount; ++i)
		check(keyboard::bindingForKey(defaults, defaults.keys[i]) == static_cast<int>(i),
		      "default action has its own highlighted physical key");
	const auto remapped = remappedKeyboard();
	check(keyboard::bindingForKey(remapped, SDL_SCANCODE_A) == 0 &&
	          keyboard::bindingForKey(remapped, SDL_SCANCODE_W) == 2 &&
	          keyboard::bindingForKey(remapped, SDL_SCANCODE_K) == 4 &&
	          keyboard::bindingForKey(remapped, SDL_SCANCODE_P) == 8,
	      "WASD and alternate action keys move the live highlights");
	for (int old : defaults.keys)
		check(keyboard::bindingForKey(remapped, old) == -1,
		      "previously bound keys become dark after remapping");
	auto unusual = remapped;
	unusual.keys[0] = SDL_SCANCODE_F24;
	unusual.keys[4] = SDL_SCANCODE_INTERNATIONAL3;
	unusual.keys[8] = SDL_SCANCODE_AUDIOMUTE;
	for (const auto &preferences : {defaults, remapped, unusual}) {
		const auto keys = keyboard::layout(preferences);
		for (int code : preferences.keys)
			check(std::count_if(keys.begin(), keys.end(),
			                    [&](const keyboard::Key &key) { return key.scancode == code; }) ==
			          1,
			      "every binding, including extra and media keys, has exactly one key cap");
		for (size_t a = 0; a < keys.size(); ++a) {
			check(keys[a].width > 0 && keys[a].height > 0 && !keys[a].legend.empty(),
			      "physical key has a usable size and legend");
			for (size_t b = a + 1; b < keys.size(); ++b) {
				const auto &x = keys[a], &y = keys[b];
				check(x.scancode != y.scancode, "physical key scancodes are unique");
				check(!(x.x + .001f < y.x + y.width && y.x + .001f < x.x + x.width &&
				        x.y + .001f < y.y + y.height && y.y + .001f < x.y + x.height),
				      "keyboard and extra-key rows do not overlap");
			}
		}
	}
}
struct PausedSystem : HeadlessSystem {
	bool applied = false, persisted = false, redOnReopen = false;
	int openings = 0;
	bool applyPreferences(const settings::Preferences &p, std::string &) override {
		mode = static_cast<WidescreenMode>(p.widescreen);
		return true;
	}
	void showSettings(settings::Session &s) override {
		if (openings++)
			redOnReopen = s.values.conradShirt == appearance::Shirt::RedPink;
		s.values.masterVolume = 41;
		s.values.rewind = true;
		s.values.widescreen = kWidescreenAdjacentRooms;
		s.values.inventoryOriginal = true;
		s.values.conradShirt = appearance::Shirt::RedPink;
		std::string message;
		applied = s.apply(s.values, message);
		persisted = s.persist(s.values, message);
		s.saveSlot = 7;
		s.cheats = kCheatOneHitKill | kCheatNoHit | kCheatLifeCounter;
		s.debugFlags = PlayerInput::DF_FASTMODE | PlayerInput::DF_SETLIFE;
		s.applyCheats(s.cheats, s.debugFlags);
		_pi.shift = _pi.space = _pi.enter = true;
		_pi.dirMask = PlayerInput::DIR_RIGHT;
		clock += 5000;
		s.action = settings::Action::Resume;
	}
};

static void renderPages(const std::filesystem::path &root, const char *assets) {
	const std::string saveRoot = root.string();
	check(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0,
	      "initialize software UI renderer");
	SDL_Window *window = SDL_CreateWindow("Settings rendering", 0, 0, 1280, 800, SDL_WINDOW_HIDDEN);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	check(window && renderer, "software UI window and renderer");
	if (!renderer) {
		if (window)
			SDL_DestroyWindow(window);
		SDL_Quit();
		return;
	}
	SDL_Texture *background = nullptr;
	settings::Session session;
	session.edition = "Amiga";
	session.assetSources = {{"amiga:amiga", "Amiga / amiga", "amiga", kResourceTypeAmiga, {}},
	                        {"dos:gog", "DOS / GOG / gog", "gog", kResourceTypeDOS, {}}};
	session.musicSources = session.assetSources;
	session.musicSources.push_back(
	    {"music:music/Sega CD", "Soundtrack / music/Sega CD", "music/Sega CD", -1, {}});
	session.experimentalPistolAvailable = true;
	session.language = "English";
	session.audioBackend = "AdLib / OPL3";
	session.level = 0;
	session.room = 0;
	session.score = 1250;
	session.outputRate = 22050;
	session.values.fullscreen = false;
	session.displayWidth = 1920;
	session.displayHeight = 1080;
	session.windowResolutions = {{256, 224}, {768, 672}, {1280, 720}, {1920, 1080}};
	session.fullscreenResolutions = {{1280, 720}, {1920, 1080}};
	session.values.rewind = true;
	session.canRewind = true;
	session.canLoad = true;
	auto preview = std::make_shared<progress::Preview>();
	preview->width = 128;
	preview->height = 112;
	preview->rgba.resize(128 * 112 * 4);
	for (unsigned y = 0; y < 112; ++y)
		for (unsigned x = 0; x < 128; ++x) {
			auto *pixel = &preview->rgba[(y * 128 + x) * 4];
			pixel[0] = x;
			pixel[1] = y * 2;
			pixel[2] = (x / 16 + y / 16) % 2 ? 180 : 70;
			pixel[3] = 255;
		}
	progress::Snapshot autosave;
	autosave.preview = preview;
	autosave.level = 1;
	autosave.room = 6;
	autosave.health = 3;
	autosave.timestamp = 1792843200000ULL;
	session.autosaves = {autosave, autosave, autosave};
	session.rewindPoints = {{preview, 5, 6, 3}, {preview, 10, 5, 2}, {preview, 15, 5, 3}};
	session.rewindCount = 3;
	session.preferencePath = (root / "preferences.cfg").string();
	settings::SaveSlot named;
	named.slot = 1;
	named.level = 0;
	named.room = 6;
	named.exists = named.supported = named.modern = named.protectedSave = true;
	named.label = "Before the lift";
	named.modified = "2026-10-24 12:00 UTC";
	auto manualPreview = std::make_shared<progress::Preview>(*preview);
	for (size_t i = 0; i < manualPreview->rgba.size(); i += 4)
		manualPreview->rgba[i] = 200;
	named.preview = manualPreview;
	auto checkpoint = named;
	checkpoint.slot = 0;
	checkpoint.label = "Checkpoint";
	checkpoint.protectedSave = false;
	auto checkpointPreview = std::make_shared<progress::Preview>(*preview);
	for (size_t i = 0; i < checkpointPreview->rgba.size(); i += 4)
		checkpointPreview->rgba[i + 1] = 200;
	checkpoint.preview = checkpointPreview;
	session.saves = {named, checkpoint};
	session.protectSave = true;
	snprintf(session.saveName.data(), session.saveName.size(), "%s", named.label.c_str());
	session.diagnostics = {"Detected assets: synthetic fixture", "Levels: primary resources found",
	                       "Saves: writable fixture directory"};
	if (assets) {
		FileSystem files(assets);
		HeadlessSystem platform;
		auto game = std::make_unique<Game>(&platform, &files, &files, saveRoot.c_str(), 0,
		                                   kResourceTypeAmiga, LANG_EN, kWidescreenNone, false, 0);
		game->initCutsceneResources();
		game->_res.load("ICONE", Resource::OT_ICN, "SPR");
		game->_res.load("ICON", Resource::OT_ICN, "SPR");
		game->_res.load("PERSO", Resource::OT_SPM);
		game->_mix.init();
		game->_score = 0;
		game->_randSeed = 1;
		game->loadLevelData();
		game->resetGameState();
		game->_cut._id = 0xFFFF;
		game->_frameTimestamp = game->_saveTimestamp = platform.clock;
		for (int frame = 0; frame < 12; ++frame)
			game->mainLoop();
		session.room = game->_currentRoom;
		session.score = game->_score;
		session.health = game->_pgeLive[0].life;
		uint8_t palette[768];
		platform.getPalette(palette, 256);
		std::vector<uint32_t> pixels(game->_vid._layerSize);
		for (size_t i = 0; i < pixels.size(); ++i) {
			const uint8_t color = game->_vid._frontLayer[i];
			pixels[i] = 0xFF000000U | (uint32_t(palette[color * 3]) << 16) |
			            (uint32_t(palette[color * 3 + 1]) << 8) | palette[color * 3 + 2];
		}
		background = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
		                               game->_vid._w, game->_vid._h);
		SDL_UpdateTexture(background, nullptr, pixels.data(), game->_vid._w * 4);
		SDL_SetTextureScaleMode(background, SDL_ScaleModeNearest);
		game->_mix.free();
	}
	{
		SettingsOverlay overlay;
		check(overlay.attach(window, renderer), "attach ImGui SDL renderer");
		overlay.open(session.values);
		static const char *const pages[] = {"session",      "saves",    "graphics", "audio",
		                                    "enhancements", "controls", "cheats",   "levels",
		                                    "journal",      "joypad"};
		for (int capture = 0; capture < 54; ++capture) {
			const bool native = (capture >= 28 && capture % 2) || capture == 26 || capture == 27 ||
			                    (capture >= 10 && capture < 20) || capture == 21;
			const bool small = capture == 8 || capture == 9 || capture == 23;
			SDL_SetWindowSize(window,
			                  native  ? 256
			                  : small ? 640
			                          : 1280,
			                  native  ? 224
			                  : small ? 480
			                          : 800);
			session.values.keys = (capture == 20 || capture == 21) ? remappedKeyboard().keys
			                                                       : settings::Preferences{}.keys;
			session.values.uiScale = capture == 9 || capture == 18 || capture == 22 ? 175 : 100;
			session.values.menuLayout = capture == 19 || capture == 23 ? 1 : 0;
			session.values.pistolStyle = weapon::Style::Laser;
			session.startup = capture >= 32 && capture < 34;
			session.assetPicker = capture >= 44 && capture < 46;
			session.recovery = capture == 34 || capture == 35;
			const int page = capture >= 48            ? 2
			                 : capture >= 46          ? 3
			                 : capture >= 44          ? 0
			                 : capture >= 42          ? 4
			                 : capture >= 40          ? 12
			                 : capture >= 38          ? 11
			                 : capture >= 36          ? 1
			                 : capture >= 34          ? 0
			                 : capture >= 32          ? 0
			                 : capture >= 30          ? 1
			                 : capture >= 28          ? 10
			                 : capture >= 24          ? (capture % 2 ? 8 : 9)
			                 : capture >= 20          ? 5
			                 : capture < 8            ? capture
			                 : native && capture < 18 ? capture - 10
			                                          : 2;
			std::string name = capture >= 28   ? std::string(capture % 2 ? "native-" : "") +
			                                         (capture >= 32   ? "welcome"
			                                          : capture >= 30 ? "autosaves"
			                                                          : "rewind")
			                   : capture >= 24 ? std::string(capture >= 26 ? "native-" : "") +
			                                         (capture % 2 ? "journal" : "joypad")
			                   : capture < 8   ? pages[page]
			                   : capture == 8  ? "compact"
			                   : capture == 9  ? "large-text"
			                   : capture == 18 ? "native-large-text"
			                   : capture == 19 ? "native-compact"
			                   : capture == 20 ? "controls-remapped"
			                   : capture == 21 ? "native-controls-remapped"
			                   : capture == 22 ? "controls-large-text"
			                   : capture == 23 ? "controls-compact"
			                                   : std::string("native-") + pages[page];
			if (capture >= 34)
				name =
				    std::string(capture % 2 ? "native-" : "") + (capture >= 42   ? "accessibility"
					                                             : capture >= 40 ? "diagnostics"
					                                             : capture >= 38 ? "objectives"
					                                             : capture >= 36 ? "manual-browser"
					                                                             : "recovery");
			if (capture >= 44)
				name = std::string(capture % 2 ? "native-" : "") +
				       (capture >= 46 ? "music-sources" : "asset-picker");
			if (capture >= 48)
				name = std::string(capture % 2 ? "native-" : "") +
				       (capture >= 52 ? "integer-scaling" : capture >= 50 ? "exclusive" : "borderless");
			session.values.fullscreen = capture >= 48 && capture < 52;
			session.values.exclusiveFullscreen = capture == 50 || capture == 51;
			session.values.integerScale = capture >= 52;
			journal::State story;
			// Capture the new choice at desktop and original game resolution.
			session.values.filter = capture >= 52 ? settings::Filter::Point
			                       : page == 2 ? settings::Filter::Xbrz : settings::Filter::Scale;
			session.values.scale = 3;
			story.chapter(0);
			session.journalEntries = story.entries();
			session.objectives = journal::objectives(story, 0);
			overlay.selectPage(page);
			for (int frame = 0; frame < 3; ++frame) {
				SDL_SetRenderDrawColor(renderer, 20, 35, 52, 255);
				SDL_RenderClear(renderer);
				if (background)
					SDL_RenderCopy(renderer, background, nullptr, nullptr);
				overlay.beginFrame(session.values);
				overlay.draw(session);
				overlay.render();
				check(ImGui::GetDrawData()->TotalVtxCount > 100, "menu emits visible geometry");
				if (frame == 2) {
					int width, height;
					SDL_GetRendererOutputSize(renderer, &width, &height);
					SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
					                                                      SDL_PIXELFORMAT_ARGB8888);
					check(SDL_RenderReadPixels(renderer, nullptr, surface->format->format,
					                           surface->pixels, surface->pitch) == 0,
					      "read rendered menu pixels");
					int turquoise = 0;
					for (int y = height / 2; y < height; ++y)
						for (int x = width / 2; x < width; ++x) {
							uint8_t r, g, b, a;
							SDL_GetRGBA(static_cast<uint32_t *>(surface->pixels)[y * width + x],
							            surface->format, &r, &g, &b, &a);
							if (r >= 28 && r <= 33 && g >= 206 && g <= 212 && b >= 190 && b <= 197)
								++turquoise;
						}
					check(turquoise > 100,
					      "turquoise EXIT GAME stays visible on every page and resolution");
					const auto path = root / (std::string("settings-") + name + ".bmp");
					check(SDL_SaveBMP(surface, path.string().c_str()) == 0,
					      "save rendered menu capture");
					SDL_FreeSurface(surface);
				}
				SDL_RenderPresent(renderer);
			}
		}
		session.startup = session.recovery = session.assetPicker = false;
		// Click the real native-resolution footer and its confirmation dialog.
		SDL_SetWindowSize(window, 256, 224);
		session.values.keys = settings::Preferences{}.keys;
		session.values.uiScale = 100;
		session.values.menuLayout = 0;
		session.action = settings::Action::None;
		overlay.open(session.values);
		std::vector<uint32_t> captured;
		int capturedWidth = 0, capturedHeight = 0;
		auto frame = [&](bool capturePixels = false) {
			SDL_SetRenderDrawColor(renderer, 20, 35, 52, 255);
			SDL_RenderClear(renderer);
			overlay.beginFrame(session.values);
			overlay.draw(session);
			overlay.render();
			if (capturePixels) {
				SDL_GetRendererOutputSize(renderer, &capturedWidth, &capturedHeight);
				captured.resize(capturedWidth * capturedHeight);
				check(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
				                           captured.data(), capturedWidth * 4) == 0,
				      "capture keyboard for input and live-highlight checks");
			}
			SDL_RenderPresent(renderer);
		};
		auto click = [&](float x, float y) {
			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(x, y);
			io.AddMouseButtonEvent(0, true);
			frame();
			io.AddMousePosEvent(x, y);
			io.AddMouseButtonEvent(0, false);
			frame();
			frame();
		};
		// Find an actual filled key cap in the rendered image. A 5x5 patch
		// excludes thin callout borders and leader arrows of the same color.
		auto coloredCap = [&](uint32_t rgb) {
			for (int y = 2; y < capturedHeight - 2; y += 2)
				for (int x = 2; x < capturedWidth - 2; x += 2) {
					bool filled = true;
					for (int dy = -2; dy <= 2 && filled; ++dy)
						for (int dx = -2; dx <= 2; ++dx)
							if ((captured[(y + dy) * capturedWidth + x + dx] & 0xFFFFFF) != rgb)
								filled = false;
					if (filled)
						return ImVec2(static_cast<float>(x), static_cast<float>(y));
				}
			return ImVec2(-1, -1);
		};
		auto keyEvent = [&](SDL_Scancode code) {
			SDL_Event event = {};
			event.type = SDL_KEYDOWN;
			event.key.keysym.scancode = code;
			overlay.processEvent(event, &session);
		};
		SDL_SetWindowSize(window, 1280, 800);
		overlay.selectPage(5);
		frame();
		frame(true);
		const auto originalAction = coloredCap(0xFFBE4B);
		check(originalAction.x >= 0, "default action key is visibly highlighted");
		click(originalAction.x, originalAction.y);
		keyEvent(SDL_SCANCODE_RETURN);
		check(session.statusError && session.values.keys[4] == SDL_SCANCODE_LSHIFT,
		      "clicking a diagram key enters capture and rejects duplicate bindings");
		keyEvent(SDL_SCANCODE_K);
		check(!session.statusError && session.applyRequested &&
		          session.values.keys[4] == SDL_SCANCODE_K,
		      "valid key after a rejected duplicate rebinds the clicked diagram action");
		frame(true);
		const auto newAction = coloredCap(0xFFBE4B);
		check(newAction.x >= 0 && newAction.y < originalAction.y &&
		          (captured[static_cast<int>(originalAction.y) * capturedWidth +
		                    static_cast<int>(originalAction.x)] &
		           0xFFFFFF) != 0xFFBE4B,
		      "highlight moves to K and the old Shift key becomes dark");

		session.values.keys = settings::Preferences{}.keys;
		session.status.clear();
		overlay.open(session.values);
		frame();
		frame();
		click(345, 643); // The Space / Draw-holster callout, below the desktop keyboard.
		keyEvent(SDL_SCANCODE_Q);
		check(session.values.keys[6] == SDL_SCANCODE_Q,
		      "action callout buttons also enter the shared rebind capture");

		SDL_SetWindowSize(window, 256, 224);
		session.values.keys = settings::Preferences{}.keys;
		session.status.clear();
		overlay.open(session.values);
		frame();
		frame(true);
		const auto compactLeft = coloredCap(0x46A0FF);
		check(compactLeft.x >= 0, "compact guide has a readable colored movement key");
		click(compactLeft.x, compactLeft.y);
		keyEvent(SDL_SCANCODE_A);
		check(session.values.keys[0] == SDL_SCANCODE_A,
		      "native-resolution colored key caps can rebind their action");
		std::string keyReason;
		settings::Preferences restored;
		const auto keyProfile = (root / "keyboard-remapped.cfg").string();
		check(settings::save(keyProfile, session.values, keyReason) &&
		          settings::load(keyProfile, restored, keyReason) &&
		          keyboard::bindingForKey(restored, SDL_SCANCODE_A) == 0 &&
		          keyboard::bindingForKey(restored, SDL_SCANCODE_LEFT) == -1,
		      "diagram reflects a saved and restored keybinding profile");
		session.status.clear();
		frame(true);
		const auto reboundCompactLeft = coloredCap(0x46A0FF);
		click(reboundCompactLeft.x, reboundCompactLeft.y);
		keyEvent(SDL_SCANCODE_ESCAPE);
		check(session.values.keys[0] == SDL_SCANCODE_A &&
		          session.action == settings::Action::None &&
		          session.status == "Key capture canceled.",
		      "Escape cancels diagram rebinding without resuming or changing a key");

		session.values.keys = settings::Preferences{}.keys;
		session.status.clear();
		overlay.open(session.values);
		overlay.selectPage(0);
		frame();
		frame();
		click(205, 210);
		ImGuiWindow *exitPopup = ImGui::FindWindowByName("Exit game?");
		check(exitPopup && exitPopup->Active && session.action == settings::Action::None,
		      "native EXIT GAME asks before quitting");
		if (exitPopup && exitPopup->Active) {
			ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
			frame();
			ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
			frame();
			check(session.action == settings::Action::None,
			      "Escape cancels exit confirmation without quitting or resuming");
			click(205, 210);
			if (exitPopup->Active) {
				click(exitPopup->Pos.x + ImGui::GetStyle().WindowPadding.x + 20,
				      exitPopup->Pos.y + exitPopup->Size.y - ImGui::GetStyle().WindowPadding.y -
				          ImGui::GetFrameHeight() / 2);
				check(session.action == settings::Action::Quit,
				      "confirmed native EXIT GAME queues quit");
			}
		}
		// Drive the actual demo button and bounded confirmation in both layouts.
		for (bool native : {false, true}) {
			SDL_SetWindowSize(window, native ? 256 : 1280, native ? 224 : 800);
			session.action = settings::Action::None;
			session.startup = true;
			session.demoAvailable = {true, false, true};
			session.demoIndex = 1; // A missing replay must fall back to an available one.
			overlay.open(session.values);
			overlay.selectPage(0);
			frame();
			frame();
			ImGuiWindow *content = nullptr;
			for (auto *candidate : ImGui::GetCurrentContext()->Windows)
				if (candidate->Active && strstr(candidate->Name, "Page content"))
					content = candidate;
			check(content && session.demoIndex == 0, "Demo picker chooses an available replay");
			if (!content)
				continue;
			ImGui::ActivateItemByID(content->GetID("Watch demo"));
			frame();
			check(session.action == settings::Action::WatchDemo,
			      "Title demo button queues playback in both layouts");
			session.action = settings::Action::None;
			session.startup = false;
			frame();
			ImGui::ActivateItemByID(content->GetID("Watch demo..."));
			frame();
			frame();
			auto *popup = ImGui::FindWindowByName("Watch demo?");
			check(popup && popup->Active && session.action == settings::Action::None,
			      "Watching a demo asks before replacing gameplay");
			if (popup && popup->Active) {
				check(popup->Size.x <= (native ? 256 : 1280) &&
				          popup->Size.y <= (native ? 224 : 800),
				      "Demo confirmation fits the original resolution");
				frame(true);
				SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
				    captured.data(), capturedWidth, capturedHeight, 32, capturedWidth * 4,
				    SDL_PIXELFORMAT_ARGB8888);
				check(surface && SDL_SaveBMP(surface, (root / (native ? "demo-native.bmp"
				                                                     : "demo-desktop.bmp"))
				                                          .string().c_str()) == 0,
				      "Capture demo controls and confirmation for visual review");
				SDL_FreeSurface(surface);
				ImGui::ActivateItemByID(popup->GetID("Cancel"));
				frame();
				frame();
				check(session.action == settings::Action::None, "Cancel preserves paused game");
				ImGui::ActivateItemByID(content->GetID("Watch demo..."));
				frame();
				frame();
				ImGui::ActivateItemByID(popup->GetID("Watch"));
				frame();
				frame();
				check(session.action == settings::Action::WatchDemo,
				      "Confirmed demo request reaches the host");
			}
			session.demoAvailable = {};
			session.action = settings::Action::None;
			ImGui::ActivateItemByID(content->GetID("Watch demo..."));
			frame();
			check(session.action == settings::Action::None,
			      "Missing recordings cannot start playback");
		}
		// The preview cache must survive the same renderer resets as the game.
		session.action = settings::Action::None;
		overlay.selectPage(10);
		frame();
		frame(true);
		const auto beforeReset = captured;
		overlay.resetRenderer();
		frame(true);
		check(captured == beforeReset, "rewind preview is recreated after a renderer reset");
		session.startup = true;
		overlay.open(session.values);
		frame();
		frame();
		overlay.selectPage(7); // Levels remains reachable before gameplay.
		frame();
		frame();
		bool titlePageVisible = false;
		for (auto *window : ImGui::GetCurrentContext()->Windows)
			titlePageVisible |= window->Active && strstr(window->Name, "Page content");
		check(titlePageVisible, "title settings retain the full page navigator");
		click(60, 207);
		check(session.action == settings::Action::Resume,
		      "native Back to title closes the overlay");
		session.startup = false;
		session.action = settings::Action::None;
		// Real key capture routes through the same SDL event handler as gameplay,
		// and reserved F10 always leaves the overlay usable.
		SDL_Event close = {};
		close.type = SDL_KEYDOWN;
		close.key.keysym.scancode = SDL_SCANCODE_F10;
		overlay.processEvent(close, &session);
		check(session.action == settings::Action::Resume,
		      "F10 closes the overlay independently of bindings");
		session.action = settings::Action::None;
		session.recovery = true;
		overlay.processEvent(close, &session);
		check(session.action == settings::Action::None,
		      "F10 cannot resume a dead recovery session");
		session.recovery = false;
		session.values = settings::Preferences{};
		overlay.beginPadCapture(0, false);
		SDL_Event pad = {};
		pad.type = SDL_CONTROLLERBUTTONDOWN;
		pad.cbutton.button = SDL_CONTROLLER_BUTTON_A;
		overlay.processEvent(pad, &session);
		check(session.statusError && session.values.padButtons[0] == SDL_CONTROLLER_BUTTON_X,
		      "joypad capture rejects duplicate gameplay binding");
		pad.cbutton.button = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
		overlay.processEvent(pad, &session);
		check(!session.statusError && session.applyRequested &&
		          session.values.padButtons[0] == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER &&
		          session.action == settings::Action::None,
		      "joypad capture queues binding without resuming");
		overlay.beginPadCapture(0, false);
		pad.cbutton.button = SDL_CONTROLLER_BUTTON_START;
		overlay.processEvent(pad, &session);
		check(session.action == settings::Action::None &&
		          session.status == "Button capture canceled.",
		      "Start safely cancels joypad capture");
		overlay.beginPadCapture(0, false);
		pad.type = SDL_CONTROLLERAXISMOTION;
		pad.caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
		pad.caxis.value = 20000;
		overlay.processEvent(pad, &session);
		check(session.values.padButtons[0] == 32,
		      "overlay captures a trigger as a bindable action");
		overlay.beginPadCapture(0, true);
		pad.type = SDL_JOYBUTTONDOWN;
		pad.jbutton.button = 8;
		overlay.processEvent(pad, &session);
		check(session.values.joystickButtons[0] == 8,
		      "raw joystick has an independent capture profile");
		overlay.detach();
		check(overlay.attach(window, renderer),
		      "renderer backend can reattach without losing UI context");
	}
	if (background)
		SDL_DestroyTexture(background);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

int main(int argc, char **argv) {
	if (argc < 2 || argc > 3)
		return 2;
	const std::filesystem::path root(argv[1]);
	const std::string saveRoot = root.string();
	std::filesystem::create_directories(root);
	setDefaultOptions(g_options);
	keyboardModel();
	settings::Preferences p;
	std::string reason;
	check(settings::validate(p, reason), "default settings validate");
	check(p.skipLogosWithKeyboard && p.skipCutscenesWithKeyboard, "skip keys enabled by default");
	check(p.laserColor == weapon::LaserColor::Orange, "laser defaults to orange");
	check(!p.quickResume, "quick resume defaults off");
	p.quickResume = true;
	p.assetSet = "dos:Folder with spaces";
	p.musicSource = "music:Music \"quoted\"";
	p.skipLogosWithKeyboard = p.skipCutscenesWithKeyboard = false;
	p.fullscreen = false;
	p.exclusiveFullscreen = true;
	p.integerScale = true;
	p.windowWidth = 1280;
	p.windowHeight = 720;
	p.fullscreenWidth = 1920;
	p.fullscreenHeight = 1080;
	p.filter = settings::Filter::Point;
	p.scale = 5;
	p.masterVolume = 73;
	p.musicVolume = 24;
	p.effectsVolume = 61;
	p.keys[0] = SDL_SCANCODE_A;
	p.rewind = true;
	p.showMenuButton = false;
	p.menuLayout = 1;
	p.conradShirt = appearance::Shirt::White;
	p.pistolStyle = weapon::Style::Laser;
	p.laserColor = weapon::LaserColor::Purple;
	p.extraMetro = true;
	const auto path = (root / "preferences with spaces.cfg").string();
	p.padButtons[5] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
	p.joystickButtons[6] = 5;
	p.stickDeadzone = 22;
	p.rightStickMovement = true;
	p.stickyAction = p.inputBuffer = p.reduceBlink = p.reduceShake = true;
	p.gameSpeed = 75;
	p.objectivesNotebook = false;
	p.storyJournal = false;
	p.journalNotifications = false;
	check(settings::save(path, p, reason), "save settings atomically");
	settings::Preferences loaded;
	check(settings::load(path, loaded, reason) && loaded == p, "all preferences round-trip");
	{
		settings::Preferences legacy;
		write(root / "legacy-display.cfg", "version=1\nfullscreen=1\nwindow_scale=3\n");
		check(settings::load((root / "legacy-display.cfg").string(), legacy, reason) &&
		          legacy.fullscreen && !legacy.exclusiveFullscreen && !legacy.integerScale &&
		          !legacy.windowWidth && !legacy.fullscreenWidth,
		      "old fullscreen profiles remain borderless with automatic sizing");
		for (const auto size : {settings::Resolution{1280, 0}, {-1, 720}, {20000, 1080}, {0, 720}}) {
			auto invalid = p;
			invalid.windowWidth = size.width;
			invalid.windowHeight = size.height;
			check(!settings::validate(invalid, reason), "incomplete/oversized resolution rejected");
		}
		write(root / "broken-display.cfg", "version=1\nwindow_width=1280\nwindow_height=0\n");
		check(settings::load((root / "broken-display.cfg").string(), legacy, reason) &&
		          !legacy.windowWidth && !legacy.windowHeight,
		      "incomplete stored resolution safely falls back to automatic");
	}
	for (int filter = 0; filter <= 3; ++filter) {
		auto selected = p;
		selected.filter = settings::Filter(filter);
		for (int scale = settings::filterMinScale(selected.filter);
		     scale <= settings::filterMaxScale(selected.filter); ++scale) {
			selected.scale = scale;
			settings::Preferences restored;
			check(settings::validate(selected, reason) &&
			          settings::save((root / "filters.cfg").string(), selected, reason) &&
			          settings::load((root / "filters.cfg").string(), restored, reason) && restored == selected,
			      "legacy filter IDs and every xBRZ scale round-trip");
		}
	}
	check(settings::filterFromScalerName("xbr") == settings::Filter::Xbrz &&
	          settings::filterFromScalerName("xbrz") == settings::Filter::Xbrz &&
	          std::string(settings::filterScalerName(settings::Filter::Xbrz)) == "xbrz",
	      "CLI aliases and persisted xBRZ selection agree");
	{
		auto selected = p;
		selected.filter = settings::Filter::Xbrz;
		for (int scale : {0, 1, 7}) {
			selected.scale = scale;
			check(!settings::validate(selected, reason), "invalid xBRZ scale rejected");
		}
		write(root / "xbrz-minimum.cfg", "version=1\nfilter=3\nwindow_scale=1\n");
		check(settings::load((root / "xbrz-minimum.cfg").string(), selected, reason) &&
		          selected.filter == settings::Filter::Xbrz && selected.scale == 2,
		      "loaded xBRZ preferences clamp to minimum scale");
	}
	for (int color = 0; color < 5; ++color) {
		auto selected = p;
		selected.laserColor = weapon::LaserColor(color);
		settings::Preferences restored;
		const auto colorsPath = (root / "laser-colors.cfg").string();
		check(settings::save(colorsPath, selected, reason) &&
		          settings::load(colorsPath, restored, reason) && selected == restored,
		      "all five laser colours persist");
	}
	const auto previous = loaded;
	write(root / "future.cfg", "version=9\nmaster_volume=1\n");
	check(!settings::load((root / "future.cfg").string(), loaded, reason) && loaded == previous,
	      "unknown profile version preserves current settings");
	write(root / "invalid.cfg", "version=1\nmaster_volume=999\nwindow_scale=-1\nkey_left=41\nmusic_"
	                            "volume=35\nconrad_shirt=99\nlaser_color=99\n");
	check(settings::load((root / "invalid.cfg").string(), loaded, reason) &&
	          loaded.masterVolume == p.masterVolume && loaded.scale == p.scale &&
	          loaded.keys == p.keys && loaded.musicVolume == 35 &&
	          loaded.conradShirt == p.conradShirt && loaded.laserColor == p.laserColor,
	      "invalid values and colliding bindings are rejected without dropping valid fields");
	check(!settings::validBinding(SDL_SCANCODE_F10, p, 0, reason),
	      "recovery shortcut cannot be rebound");
	check(!settings::validBinding(SDL_SCANCODE_RETURN, p, 0, reason),
	      "duplicate bindings rejected");
	std::filesystem::create_directories(root / "not-a-file");
	check(!settings::save((root / "not-a-file").string(), p, reason) &&
	          std::filesystem::is_directory(root / "not-a-file"),
	      "failed replacement preserves the original destination");
	for (const auto &file : std::filesystem::directory_iterator(root)) {
		check(file.path().filename().string().find(".tmp-") == std::string::npos,
		      "temporary preferences cleaned up");
	}
	{
		using appearance::Shirt;
		for (auto choice : {Shirt::Original, Shirt::RedPink, Shirt::White}) {
			auto saved = p;
			saved.conradShirt = choice;
			settings::Preferences restored;
			const auto profile = (root / "shirt-profile.cfg").string();
			check(settings::save(profile, saved, reason) &&
			          settings::load(profile, restored, reason) && restored == saved,
			      "each shirt style persists and restores with the complete profile");
		}
		check(appearance::shirtPixel(true, 0x47, true) == 0x47 &&
		          appearance::shirtPixel(true, 0x42, true) == 0x42,
		      "Amiga jacket and shared hair/outline colours remain intact");
		for (auto edition : {kResourceTypeAmiga, kResourceTypeDOS, kResourceTypePC98,
		                     kResourceTypeSega, kResourceTypeMac}) {
			for (int color = 0; color < 256; ++color)
				check(appearance::shirtPixel(edition == kResourceTypeAmiga, color, false) == color,
				      "other sprites keep every palette index");
			std::array<Color, 2> native{{{153, 17, 68}, {187, 34, 85}}};
			const auto original = appearance::shirtColors(Shirt::Original, native);
			check(memcmp(native.data(), original.data(), sizeof(native)) == 0,
			      "Original keeps exact asset RGB");
			FileSystem files(saveRoot.c_str());
			HeadlessSystem platform;
			Resource resource(&files, edition, LANG_EN);
			resource._isSegaCD = edition == kResourceTypeSega;
			const bool shared = resource.isAmiga() || resource.isSegaCD();
			Video video(&resource, &platform, kWidescreenNone);
			const int source = shared ? 1 : 13;
			const Color fill = shared ? Color{204, 204, 204} : native[1];
			platform.palette[0x40 + source] = fill;
			platform.palette[0x4C] = native[0];
			platform.palette[0x45] = {136, 136, 136};
			platform.palette[0x47] = {102, 68, 34};
			for (auto choice : {Shirt::White, Shirt::RedPink, Shirt::Original}) {
				video._conradShirt = choice;
				video.refreshConradShirtPalette();
				const auto color = platform.palette[appearance::ShirtLight];
				check(memcmp(&platform.palette[0x40 + source], &fill, sizeof(Color)) == 0,
				      "shirt styles never replace shared source RGB");
				if (choice == Shirt::White)
					check(color.r == color.g && color.g == color.b, "white shirt is neutral");
				if (choice == Shirt::RedPink)
					check(color.r > color.b && color.b > color.g,
					      "red/pink shirt uses the red ramp");
				if (choice == Shirt::Original)
					check(memcmp(&color, &fill, sizeof(Color)) == 0,
					      "Original restores the native shirt fill");
			}
			if (edition == kResourceTypeMac)
				continue;
			const uint8_t pixel = shared ? appearance::ShirtLight : source;
			const uint8_t npcPixel = source;
			using Draw = void (Video::*)(const uint8_t *, uint8_t *, int, int, int, uint8_t, bool);
			for (Draw draw : {&Video::drawSpriteSub3, &Video::drawSpriteSub4,
			                  &Video::drawSpriteSub5, &Video::drawSpriteSub6}) {
				uint8_t target = 0;
				(video.*draw)(&pixel, &target, 1, 1, 1, 0x40, true);
				check(target == appearance::ShirtLight,
				      "every character orientation maps player clothing");
				target = 0;
				(video.*draw)(&npcPixel, &target, 1, 1, 1, 0x40, false);
				check(target == (source | 0x40), "NPC drawing uses original clothing index");
				target = 0x80;
				(video.*draw)(&pixel, &target, 1, 1, 1, 0x40, true);
				check(target == 0x80, "shirt recolouring respects foreground occlusion");
			}
		}
		// Synthetic open jacket, light undershirt, boots and a held metal item.
		// Rotate it to exercise pose-independent segmentation before sprite flips.
		std::vector<uint8_t> pose = {0, 0, 4, 4, 0, 0, 0, 0, 7, 5, 1, 7, 0, 0, 7, 7, 5,
		                             1, 7, 7, 0, 0, 7, 5, 1, 7, 0, 0, 0, 0, 6, 6, 0, 4,
		                             1, 0, 0, 6, 6, 0, 0, 5, 0, 0, 5, 1, 0, 0, 0};
		std::vector<uint8_t> mask(49, 0);
		for (int y = 1; y <= 3; ++y)
			mask[y * 7 + 2] = mask[y * 7 + 3] = 1;
		for (int turn = 0; turn < 4; ++turn) {
			auto tagged = pose;
			check(appearance::markSharedTshirt(tagged.data(), 7, 7, tagged.size()) == 6,
			      "only undershirt fabric is tagged in each rotated pose");
			for (size_t i = 0; i < tagged.size(); ++i) {
				const uint8_t expected =
				    mask[i] ? pose[i] == 1 ? appearance::ShirtLight : appearance::ShirtShadow
					        : pose[i];
				check(tagged[i] == expected,
				      "jacket, shoes, skin, trousers and equipment remain unchanged");
			}
			auto nextPose = pose, nextMask = mask;
			for (int y = 0; y < 7; ++y)
				for (int x = 0; x < 7; ++x) {
					nextPose[x * 7 + 6 - y] = pose[y * 7 + x];
					nextMask[x * 7 + 6 - y] = mask[y * 7 + x];
				}
			pose = nextPose;
			mask = nextMask;
		}
		check(appearance::markSharedTshirt(pose.data(), 7, 7, 48) == 0,
		      "short decoded frames are rejected without accessing beyond capacity");
		const std::array<Color, 2> dark{{{77, 9, 34}, {94, 17, 43}}};
		const std::array<Color, 2> bright{{{153, 17, 68}, {187, 34, 85}}};
		check(appearance::shirtColors(Shirt::White, dark)[1].r <
		          appearance::shirtColors(Shirt::White, bright)[1].r,
		      "white shirt retains dark-room shading");
		settings::Preferences oldConfig;
		Options legacy;
		setDefaultOptions(legacy);
		legacy.use_white_tshirt = true;
		oldConfig.readCompatibility(legacy);
		check(oldConfig.conradShirt == Shirt::White,
		      "legacy white-shirt configuration imports into new selector");
		loaded.conradShirt = Shirt(99);
		check(!settings::validate(loaded, reason), "invalid shirt enum rejected");
		loaded.conradShirt = Shirt::Original;
	}
	{
		// A minimal valid state exercises the same preflight reader without game assets.
		std::vector<uint8_t> state(13 + 30 + 0x1C00 + 2, 0);
		state[0] = 1;
		state[13 + 13] = state[13 + 14] = state[13 + 15] = 255;
		std::fill_n(state.data() + 9, 4, 255);
		std::fill_n(state.data() + 13 + 22, 4, 255);
		auto fixture = [&](int version) {
			File file;
			check(file.open("fb-level1-03.state", "wb", saveRoot.c_str()),
			      "create save preflight fixture");
			file.writeUint32BE(0x46425356);
			file.writeUint16BE(version);
			char description[32] = "level=1 room=12";
			file.write(description, sizeof(description));
			file.write(state.data(), state.size());
		};
		fixture(3);
		File valid;
		check(valid.open("fb-level1-03.state", "zrb", saveRoot.c_str()), "open preflight fixture");
		int version = 0;
		check(saves::validate(valid, 1, version, reason) && version == 3 && valid.tell() == 38,
		      "valid save preflight leaves decoder at payload");
		const auto catalog = saves::scan(saveRoot.c_str(), 0);
		check(catalog.size() == 100 && catalog[3].supported && catalog[3].room == 12 &&
		          !catalog[4].exists,
		      "catalog distinguishes stored and empty slots");
		state[5] = 127;
		fixture(3);
		File corrupt;
		corrupt.open("fb-level1-03.state", "zrb", saveRoot.c_str());
		check(!saves::validate(corrupt, 1, version, reason),
		      "invalid collision count rejected before pointer construction");
		state[5] = 0;
		state.pop_back();
		fixture(3);
		File truncated;
		truncated.open("fb-level1-03.state", "zrb", saveRoot.c_str());
		check(!saves::validate(truncated, 1, version, reason), "truncated final state rejected");
	}
	{
		HeadlessSystem platform;
		Mixer mixer(nullptr, &platform);
		mixer.init();
		mixer.setPremixHook(testMusic, nullptr);
		int16_t buffer[8] = {};
		mixer.setVolumes(50, 50, 100, false);
		mixer.mix(buffer, 4);
		check(buffer[0] == 2500 && buffer[7] == 2500, "independent music/master gains");
		mixer.setPremixHook(nullptr, nullptr);
		const uint8_t sample[16] = {127, 127, 127, 127, 127, 127, 127, 127,
		                            127, 127, 127, 127, 127, 127, 127, 127};
		mixer.play(sample, sizeof(sample), 22050, Mixer::MAX_VOLUME);
		mixer.setVolumes(50, 0, 50, false);
		mixer.mix(buffer, 4);
		check(buffer[0] == 8159, "effects level does not follow music gain");
		const auto position = mixer._channels[0].chunkPos;
		mixer.setVolumes(100, 100, 100, true);
		mixer.mix(buffer, 4);
		check(buffer[0] == 0 && mixer._channels[0].chunkPos > position,
		      "mute advances playback while producing silence");
		mixer.free();
	}
	{
		FileSystem files(root.string().c_str());
		PausedSystem platform;
		platform.clock = 1000;
		auto game = std::make_unique<Game>(&platform, &files, &files, saveRoot.c_str(), 0,
		                                   kResourceTypeAmiga, LANG_EN, kWidescreenNone, false, 0);
		game->_currentRoom = 3;
		game->_score = 50;
		game->_pgeLive[0] = {};
		game->_pgeLive[0].pos_x = 123;
		game->_deathCutsceneCounter = 0;
		check(!game->handleConfigPanel() && platform.applied && platform.persisted,
		      "paused menu applies and persists through the game bridge");
		check(game->_pgeLive[0].pos_x == 123 && game->_currentRoom == 3 && game->_score == 50,
		      "opening and changing settings does not advance the simulation");
		check(!platform._pi.shift && !platform._pi.space && !platform._pi.enter &&
		          !platform._pi.dirMask && game->_frameTimestamp == platform.clock &&
		          game->_saveTimestamp == platform.clock,
		      "resume clears menu inputs and resets pause timing");
		check(game->_autoSave && game->_stateSlot == 7 && g_options.order_inventory_original &&
		          game->_widescreenMode == kWidescreenAdjacentRooms &&
		          game->_mix._masterVolume == 41 && game->_cheats == 7 &&
		          game->_preferences.conradShirt == appearance::Shirt::RedPink &&
		          game->_vid._conradShirt == appearance::Shirt::RedPink &&
		          platform._pi.dbgMask == (PlayerInput::DF_FASTMODE | PlayerInput::DF_SETLIFE),
		      "live changes reach their actual engine subsystems");
		game->handleConfigPanel();
		check(platform.redOnReopen,
		      "explicit Red/Pink survives reopening despite legacy false boolean");
	}
	renderPages(root, argc == 3 ? argv[2] : nullptr);
	std::cout << (failures ? "Settings checks failed\n" : "Settings checks passed\n");
	return failures ? 1 : 0;
}
