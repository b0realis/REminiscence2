// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"
#include "fs.h"
#include "game.h"
#include "mcd_player.h"
#include "decode_sega.h"
#include "screenshot.h"
#include "headless_system.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
Options g_options;
const char *g_caption = "Sega CD integration tests";
static void check(bool ok, const std::string &why) {
	if (!ok)
		throw std::runtime_error(why);
}
int main(int argc, char **argv) {
	if (argc != 3)
		return 1;
	try {
		std::filesystem::create_directories(argv[2]);
		FileSystem fs(argv[1]);
		HeadlessSystem stub;
		setDefaultOptions(g_options);
		std::array<uint8_t, 256 * 128> pixels{};
		std::array<uint8_t, 768> palette{};
		size_t total = 0;
		for (const auto &name : mcd::names()) {
			mcd::Stream movie;
			check(movie.open((name + ".MCD").c_str(), &fs), "open " + name);
			const auto playback = movie.playbackFrames((name + ".MCD").c_str());
			check(playback > 0 && playback <= movie.frames.size(), "playback extent " + name);
			if (name == "HOLO2")
				check(playback == 392, "hologram header overrides event default");
			if (name == "SC2")
				check(playback == 5312, "ending event length");
			for (size_t n = 0; n < movie.frames.size(); ++n) {
				check(movie.frame(n, pixels.data(), palette.data()),
				      "decode " + name + " frame " + std::to_string(n));
				++total;
				if (n == movie.frames.size() / 2)
					saveBMP((std::string(argv[2]) + "/" + name + ".bmp").c_str(), pixels.data(),
					        palette.data(), 256, 128);
			}
			std::cout << name << ": " << movie.frames.size() << " frames\n";
		}
		auto game = std::make_unique<Game>(&stub, &fs, &fs, argv[2], 0, kResourceTypeSega, LANG_EN,
		                                   kWidescreenNone, false, 0);
		game->initCutsceneResources();
		game->_mix.init();
		game->_preferences.diskAutosave = false;
		for (int track = 2; track <= 9; ++track) {
			check(game->_mix._ogg.playTrack(track), "CD audio track " + std::to_string(track));
			std::array<int16_t, 8192> sound{};
			check(game->_mix._ogg.mix(sound.data(), sound.size() / 2), "CD audio mix");
		}
		game->_mix._ogg.stopTrack();
		for (int level = 0; level < 7; ++level) {
			game->_currentLevel = level;
			game->loadLevelData();
			game->resetGameState();
			game->_vid.setTextPalette();
			game->_cut._id = 0xffff;
			check(game->_res._pgeNum && game->_res._aniSize, "level logic");
			for (int n = 0; n < Resource::NUM_SPRITES; ++n)
				if (game->_res._sprData[n]) {
					std::array<uint8_t, 1536> sprite{};
					check(decodeSegaCDCharacter(game->_res._sprData[n], game->_res._sprDataSizes[n],
					                            sprite.data(), sprite.size()),
					      "sprite " + std::to_string(n) + " level " + std::to_string(level));
				}
			for (int tick = 0; tick < 100; ++tick) {
				game->_cut._id = 0xffff;
				game->mainLoop();
			}
			check(game->saveGameState(7), "save level " + std::to_string(level));
			check(game->loadGameState(7), "load level " + std::to_string(level));
			stub.getPalette(palette.data(), 256);
			saveBMP((std::string(argv[2]) + "/level" + std::to_string(level + 1) + ".bmp").c_str(),
			        game->_vid._frontLayer, palette.data(), 256, 224);
			std::cout << "Level " << level + 1 << " simulated\n";
		}
		game->_currentLevel = 1;
		game->loadLevelData();
		game->resetGameState();
		game->_currentRoom = game->_pgeLive[0].room_location = 28;
		stub.mode = kWidescreenAdjacentRooms;
		game->_widescreenMode = kWidescreenAdjacentRooms;
		game->loadLevelMap();
		check(game->_res._levNum == 2, "direct entry to city bank two");
		check(stub.leftCopies && stub.rightCopies, "city adjacent previews");
		check(game->saveGameState(7), "save interior city room");
		game->loadLevelData();
		check(game->loadGameState(7), "restore city room after fresh resource load");
		game->refreshRestoredFrame();
		check(game->_res._levNum == 2 && game->_currentRoom == 28,
		      "city save selects its graphics bank");
		game->_currentRoom = 1;
		game->loadLevelMap();
		check(game->_res._levNum == 1, "return to city bank one");
		game->_menu.loadPicture("menu1");
		stub.getPalette(palette.data(), 256);
		saveBMP((std::string(argv[2]) + "/title.bmp").c_str(), game->_vid._frontLayer,
		        palette.data(), 256, 224);
		unsigned voices = 0;
		for (int n = 0; n < 45; ++n) {
			const auto *p = Resource::_voicesOffsetsTable + Resource::_voicesOffsetsTable[n] / 2;
			for (int segment = 0; segment < p[1]; ++segment) {
				uint8_t *voice = nullptr;
				uint32_t size = 0;
				game->_res.load_VCE(n, segment, &voice, &size);
				check(size == p[segment + 2] / 5 * 2048, "voice " + std::to_string(n));
				if (size) {
					check(voice, "voice allocation");
					++voices;
				}
				free(voice);
			}
		}
		std::cout << voices << " nonempty dialogue segments checked\n";
		std::cout << "Decoded " << total
		          << " movie frames, eight tracks, seven levels and dialogue voices\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
