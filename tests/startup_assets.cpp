// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"
#include "fs.h"
#include "game.h"
#include "headless_system.h"
#include "screenshot.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

Options g_options;
const char *g_caption = "startup asset tests";

static void check(bool ok, const char *message) {
	if (!ok)
		throw std::runtime_error(message);
}

struct StartupSystem : HeadlessSystem {
	std::function<void()> poll;
	std::function<void(settings::Session &)> menu;
	int events = 0, width = 256, height = 224;
	bool pause = false;
	std::vector<uint8_t> screen = std::vector<uint8_t>(256 * 224);
	void setScreenSize(int w, int h) override {
		width = w;
		height = h;
		screen.assign(w * h, 0);
	}
	void copyRect(int x, int y, int w, int h, const uint8_t *src, int pitch) override {
		for (int row = 0; row < h; ++row)
			if (y + row >= 0 && y + row < height && x >= 0 && x + w <= width)
				std::copy_n(src + (y + row) * pitch + x, w, screen.data() + (y + row) * width + x);
	}
	void processEvents() override {
		check(++events < 100000, "Startup exceeded the event budget");
		if (poll)
			poll();
	}
	void showSettings(settings::Session &s) override {
		check(bool(menu), "Unexpected automatic startup overlay");
		menu(s);
	}
	bool consumePauseRequest() override {
		const bool result = pause;
		pause = false;
		return result;
	}
	void capture(const std::filesystem::path &path) {
		uint8_t colors[768];
		getPalette(colors, 256);
		check(saveBMP(path.string().c_str(), screen.data(), colors, width, height),
		      "Could not capture the original title screen");
	}
};

int main(int argc, char **argv) {
	if (argc != 4)
		return 1;
	try {
		const auto type = std::string(argv[3]) == "dos" ? kResourceTypeDOS : kResourceTypeAmiga;
		const std::filesystem::path output(argv[2]);
		std::filesystem::create_directories(output);
		FileSystem files(argv[1]);
		setDefaultOptions(g_options);
		g_options.bypass_protection = true;
		// First create real progress via immediate Esc -> Levels. A following
		// normal launch must still show both openings, even with that autosave.
		for (int scenario = 0; scenario < 11; ++scenario) {
			StartupSystem platform;
			const auto saveDirectory = scenario == 9 ? output / "empty-startup" : output;
			std::filesystem::create_directories(saveDirectory);
			const std::string savePath = saveDirectory.string();
			if (scenario == 8)
				std::ofstream(output / progress::manualFilename(type, 2, 9), std::ios::binary)
				    << "broken";
			auto game = std::make_unique<Game>(&platform, &files, &files, savePath.c_str(), 0, type,
			                                   LANG_EN, kWidescreenNone, false, 0);
			std::vector<int> scenes;
			int titlePolls = 0, gameplayPolls = 0, menus = 0;
			bool requested = false;
			game->_preferences.skipLogosWithKeyboard = scenario != 0;
			game->_preferences.skipCutscenesWithKeyboard = scenario != 0;
			game->_continueRequested = scenario == 6;
			game->_preferences.quickResume = scenario >= 7 && scenario <= 9;
			game->_assetSources = {{"first", "First", argv[1], type, {}},
			                       {"second", "Second", argv[1], type, {}}};
			platform.menu = [&](settings::Session &s) {
				++menus;
				check(s.startup && !s.canSave && !s.canRewind, "Title exposes in-game save/rewind");
				check(s.levelAvailable[0] && s.levelAvailable[6], "Title cannot browse levels");
				if (scenario == 10) {
					s.assetIndex = 1;
					std::string reason;
					check(s.command(settings::Action::SwitchAssets, 0, reason),
					      "Could not switch edition from opening");
				}
				if (scenario == 0 || scenario == 5) {
					s.targetLevel = 2;
					s.targetSkill = kSkillExpert;
					std::string reason;
					check(s.command(settings::Action::StartLevel, 1, reason),
					      "Could not start selected level");
				}
				s.action = settings::Action::Resume;
			};
			platform.poll = [&] {
				if (game->_startupMenu) {
					++titlePolls;
					if (scenario == 4 && !requested) {
						platform._pi.escape = true;
						requested = true;
					}
					if (titlePolls == 70) {
						platform.capture(output / ("title-" + std::to_string(scenario) + ".bmp"));
						platform._pi.quit = true;
					}
					return;
				}
				if (game->_res._pgeNum) {
					if (scenario == 6 && gameplayPolls == 4) {
						game->_score = 4242;
						std::string reason;
						check(game->saveManualState(9, "Newest quick save", false, reason),
						      "Could not create quick resume fixture");
						game->_preferences.diskAutosave = false;
					}
					if (++gameplayPolls >= 8)
						platform._pi.quit = true;
					return;
				}
				const int scene = game->_cut._id;
				if (scenes.empty() || scenes.back() != scene)
					scenes.push_back(scene);
				if ((scenario == 0 || scenario == 5 || scenario == 10 ||
				     (scenario == 3 && scene == 0x0D)) &&
				    !requested) {
					if (scenario == 5)
						platform.pause = true;
					else
						platform._pi.escape = true;
					requested = true;
				} else if (scenario >= 2) {
					platform._pi.cutsceneSkip = true;
				}
			};
			game->run();
			check(!game->_continueFailed, "Explicit Continue failed");
			if (scenario == 10) {
				check(game->_requestedAsset == 1 && !titlePolls && !gameplayPolls && menus == 1,
				      "Edition switch failed to unwind the opening at a safe boundary");
			} else if (scenario == 0 || scenario == 5 || (scenario >= 6 && scenario <= 8)) {
				check(gameplayPolls && !titlePolls && game->_currentLevel == 2 &&
				          game->_skillLevel == kSkillExpert,
				      "Selected/continued level was replaced by title defaults");
				check(menus == (scenario >= 6 ? 0 : 1), "Wrong startup menu count");
				if (scenario >= 6)
					check(scenes.empty(), "Quick resume did not skip title sequence");
				if (scenario == 7)
					check(game->_score == 4242, "Quick resume ignored newest manual save");
				if (scenario < 6)
					check(scenes == std::vector<int>{0x40}, "Immediate menu did not stop logos");
			} else {
				check(titlePolls && !gameplayPolls, "Normal launch did not end at title");
				check(scenes.size() >= 2 && scenes[0] == 0x40 && scenes[1] == 0x0D,
				      "Saved progress bypassed the opening sequence");
				check(menus == ((scenario == 3 || scenario == 4) ? 1 : 0),
				      "Startup opened an unrequested overlay");
			}
			check(!game->_cut._interruptForMenu && !game->_seq._interruptForMenu &&
			          !platform._cutsceneSkipEnabled,
			      "Startup input handler leaked into gameplay");
		}
		std::cout << "Startup passed: full/skip openings with saved progress, immediate Esc, "
		             "intro/title Esc, pause shortcut, level selection, edition switch, explicit "
					 "Continue and latest-compatible-save resume.\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 2;
	}
}
