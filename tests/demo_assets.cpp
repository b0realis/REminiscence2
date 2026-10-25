// SPDX-License-Identifier: GPL-3.0-or-later
// Optional private-DOS regression: execute the actual recorded demo and engine loop.
#include "config.h"
#include "fs.h"
#include "game.h"
#include "headless_system.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

Options g_options;
const char *g_caption = "Missing-map demo regression";
static void check(bool ok, const char *why) {
	if (!ok)
		throw std::runtime_error(why);
}

struct DemoSystem : HeadlessSystem {
	std::function<void()> poll;
	std::function<void(settings::Session &)> menu;
	int polls = 0;
	bool pause = false;
	void processEvents() override {
		check(++polls < 20000, "Demo launch exceeded its event budget");
		poll();
	}
	bool consumePauseRequest() override {
		const bool requested = pause;
		pause = false;
		return requested;
	}
	void showSettings(settings::Session &session) override { menu(session); }
};

static void checkLaunches(FileSystem &files, const std::filesystem::path &root) {
	// All three selections from an interrupted opening, then from gameplay,
	// title settings and the original title's Demo item. Exit through Esc,
	// replay exhaustion and recorded death without entering save recovery.
	for (int scenario = 0; scenario < 6; ++scenario) {
		const std::string savePath = (root / ("launch-" + std::to_string(scenario))).string();
		std::filesystem::create_directories(savePath);
		DemoSystem platform;
		auto game = std::make_unique<Game>(&platform, &files, &files, savePath.c_str(), 0,
		                                   kResourceTypeDOS, LANG_EN, kWidescreenNone, true, 0);
		game->_preferences.diskAutosave = true;
		game->_preferences.skipLogosWithKeyboard = true;
		game->_preferences.skipCutscenesWithKeyboard = true;
		const int selected = scenario < 3 ? scenario : scenario == 3 ? 2 : scenario == 4 ? 1 : 0;
		const int firstInput = selected == 2 ? 1 : 0;
		bool started = false, pausedDemo = false, titleReturned = false, opening = false;
		bool enteredGameplay = false, leaving = false;
		platform.menu = [&](settings::Session &s) {
			check(!s.recovery, "Demo opened gameplay death recovery");
			check(s.demoAvailable == std::array<bool, 3>{true, true, true},
			      "DOS demo choices were not detected");
			std::string reason;
			if (s.demoPlaying) {
				check(!s.canSave && !s.canRewind, "Demo exposes save or rewind");
				check(!s.command(settings::Action::SaveGame, 99, reason),
				      "Overlay saved demo playback");
			} else if (scenario == 3 && !enteredGameplay) {
				s.targetLevel = 0;
				check(s.command(settings::Action::StartLevel, 1, reason), "Could not start game");
				enteredGameplay = true;
			} else {
				check(s.startup == (scenario != 3), "Wrong menu origin for demo launch");
				s.demoIndex = selected;
				check(s.command(settings::Action::WatchDemo, 1, reason), "Demo command failed");
			}
			s.action = settings::Action::Resume;
		};
		platform.poll = [&] {
			if (game->_startupMenu) {
				if (started) {
					titleReturned = true;
					platform._pi.quit = true;
				} else if (scenario == 5) {
					for (int i = 0; i < (g_options.enable_password_menu ? 4 : 3); ++i)
						platform._menuInput.push(MenuInput::Down);
					platform._menuInput.push(MenuInput::Confirm);
				} else {
					platform._pi.escape = true;
				}
				return;
			}
			if (!game->_res._pgeNum) {
				if (scenario < 4 && !opening) {
					platform._pi.escape = true;
					opening = true;
				} else {
					platform._pi.cutsceneSkip = true;
				}
				return;
			}
			if (game->_demoBin < 0) {
				platform._pi.cutsceneSkip = true;
				platform.pause = true;
				return;
			}
			started = true;
			check(game->_demoBin == selected, "Title defaults replaced selected demo");
			check(game->_playTicks == 0 && game->_rewindLen == 0, "Demo accumulated progress");
			std::string reason;
			check(!game->saveManualState(99, "demo", false, reason), "Demo wrote a manual save");
			if (!pausedDemo && game->_inp_demPos >= firstInput + 2) {
				platform.pause = true;
				pausedDemo = true;
			} else if (!leaving && game->_inp_demPos >= firstInput + 6) {
				leaving = true;
				if (scenario == 1 || scenario == 4)
					game->_inp_demPos = game->_res._demLen - 1;
				else if (scenario == 2)
					game->_deathCutsceneCounter = 1;
				else
					platform._pi.escape = true;
			}
		};
		game->run();
		check(started && pausedDemo && leaving && titleReturned, "Demo did not return to title");
		check(std::filesystem::is_empty(savePath), "Demo wrote persistent progress");
	}
}
int main(int argc, char **argv) {
	if (argc != 3)
		return 1;
	try {
		setDefaultOptions(g_options);
		g_options.bypass_protection = true;
		std::filesystem::create_directories(argv[2]);
		FileSystem files(argv[1]);
		checkLaunches(files, argv[2]);
		const auto incomplete = std::filesystem::path(argv[2]) / "empty-recording";
		std::filesystem::create_directories(incomplete);
		std::ofstream(incomplete / "level3.pge").close();
		std::ofstream(incomplete / "demo3.bin").close();
		FileSystem incompleteFiles(incomplete.string().c_str());
		for (bool demo : {true, false}) {
			HeadlessSystem platform;
			auto game =
			    std::make_unique<Game>(&platform, &files, &files, argv[2], 2, kResourceTypeDOS,
				                       LANG_EN, kWidescreenNone, false, 0);
			game->initCutsceneResources();
			game->_res.load("GLOBAL", Resource::OT_ICN);
			game->_res.load("GLOBAL", Resource::OT_SPC);
			game->_res.load("PERSO", Resource::OT_SPR);
			game->_res.load_SPR_OFF("PERSO", game->_res._spr1);
			game->_res.load_FIB("GLOBAL");
			game->_res.load_DEM("demo3.bin");
			check(game->_res._dem && game->_res._demLen > 1, "DOS demo3.bin missing");
			game->_demoBin = demo ? 2 : -1;
			game->_preferences.diskAutosave = false;
			game->_randSeed = 1337;
			game->_mix.init();
			check(!game->demoAvailable(-1) && !game->demoAvailable(3),
			      "Invalid demo index accepted");
			std::string reason;
			const auto *recording = game->_res._dem;
			check(!game->startDemo(99, reason) && game->_res._dem == recording &&
			          game->_demoBin == (demo ? 2 : -1) && game->_currentLevel == 2,
			      "Unavailable demo changed the current session");
			const int recordingSize = game->_res._demLen;
			game->_res._fs = &incompleteFiles;
			check(game->demoAvailable(2) && !game->startDemo(2, reason) &&
			          game->_res._dem == recording && game->_res._demLen == recordingSize &&
			          game->_demoBin == (demo ? 2 : -1) && game->_currentLevel == 2,
			      "An empty recording destroyed the current replay or session");
			std::filesystem::resize_file(incomplete / "demo3.bin", 4 * 1024 * 1024 + 1);
			check(!game->startDemo(2, reason) && game->_res._dem == recording &&
			          game->_res._demLen == recordingSize &&
			          game->_demoBin == (demo ? 2 : -1) && game->_currentLevel == 2,
			      "An oversized recording destroyed the current replay or session");
			std::filesystem::resize_file(incomplete / "demo3.bin", 0);
			game->_res._fs = &files;
			game->loadLevelData();
			game->resetGameState();
			game->_cut._id = 0xFFFF;
			game->_endLoop = false;
			game->_frameTimestamp = game->_saveTimestamp = 0;
			// Current GOG data starts in room56. Recreate the historical room39
			// start explicitly; keep actor membership coherent when moving Conrad.
			auto &player = game->_pgeLive[0];
			const int startRoom = player.room_location;
			const int startX = player.pos_x, startY = player.pos_y;
			auto move = [&](uint8_t room, int x, int y) {
				auto **link = &game->_pge_liveTable1[player.room_location];
				while (*link && *link != &player)
					link = &(*link)->next_PGE_in_room;
				check(*link == &player, "Player missing from its actor list");
				*link = player.next_PGE_in_room;
				player.room_location = room;
				player.next_PGE_in_room = game->_pge_liveTable1[room];
				game->_pge_liveTable1[room] = &player;
				player.pos_x = x;
				player.pos_y = y;
				game->_currentRoom = room;
				game->_loadMap = true;
			};
			check(game->hasLevelMap(2, startRoom) && !game->hasLevelMap(2, 39),
			      "Fixture needs a valid start and missing LEVEL3 room39");
			move(39, 0, 0);
			const int inputStart = game->_inp_demPos;
			game->mainLoop();
			if (demo) {
				check(!game->_deathCutsceneCounter && game->_cut._id == 0xFFFF && !game->_loadMap,
				      "Missing-map demo was killed or left waiting for a map");
				check(game->_inp_demPos == inputStart + 1, "Demo input did not advance");
				game->mainLoop();
				check(game->_inp_demPos == inputStart + 2 && !game->_deathCutsceneCounter &&
				          !game->_endLoop,
				      "Missing-map demo stopped advancing input");
				// A subsequent valid-room transition must decode normally. This
				// controls the transition, not the historical demo's entire trajectory.
				move(startRoom, startX, startY);
				game->mainLoop();
				check(game->hasLevelMap(2, game->_currentRoom) && !game->_loadMap &&
				          !game->_deathCutsceneCounter && game->_inp_demPos == inputStart + 3,
				      "Demo did not recover normal map loading");
				std::cout << "Room39 demo continues input and recovers valid-room loading\n";
			} else {
				check(game->_deathCutsceneCounter == 1 && game->_cut._id == 6,
				      "Demo exception leaked into ordinary gameplay");
			}
			game->_mix.free();
		}
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	return 0;
}
