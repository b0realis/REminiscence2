// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"
#include "fs.h"
#include "game.h"
#include "headless_system.h"
#include "screenshot.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

Options g_options;
const char *g_caption = "DOS asset tests";
static void check(bool ok, const char *message) {
	if (!ok)
		throw std::runtime_error(message);
}
static void init(Game &game) {
	game.initCutsceneResources();
	game._res.load("GLOBAL", Resource::OT_ICN);
	game._res.load("GLOBAL", Resource::OT_SPC);
	game._res.load("PERSO", Resource::OT_SPR);
	game._res.load_SPR_OFF("PERSO", game._res._spr1);
	game._res.load_FIB("GLOBAL");
	game._mix.init();
	game._preferences.diskAutosave = false;
	game._randSeed = 1337;
	game.loadLevelData();
	game.resetGameState();
	game._cut._id = 0xffff;
}

int main(int argc, char **argv) {
	if (argc != 3)
		return 1;
	try {
		setDefaultOptions(g_options);
		FileSystem files(argv[1]);
		const std::filesystem::path output(argv[2]);
		std::filesystem::create_directories(output);
		std::vector<std::vector<uint8_t>> baseline;
		std::vector<std::array<int, 3>> origins(220, {-1, -1, 0});
		for (int variant = 0; variant < 7; ++variant) {
			HeadlessSystem platform;
			auto game =
			    std::make_unique<Game>(&platform, &files, &files, argv[2], 0, kResourceTypeDOS,
				                       LANG_EN, kWidescreenNone, false, 0);
			init(*game);
			game->_preferences.pistolStyle = static_cast<weapon::Style>(std::min(variant, 2));
			game->_preferences.laserColor =
			    static_cast<weapon::LaserColor>(std::max(0, variant - 2));
			const int pistol = game->_res._pgeInit[0].data[0];
			const int actor = game->_res._pgeInit[pistol].data[0];
			unsigned directions = 0;
			bool styledSound = false;
			for (int tick = 0; tick < 220; ++tick) {
				platform._pi.space = tick == 35;
				platform._pi.shift = (tick >= 55 && tick < 79) || (tick >= 105 && tick < 122) ||
				                     (tick >= 145 && tick < 170) || tick >= 200;
				if (tick == 95 || tick == 190) {
					game->_pgeLive[0].flags ^= 1;
					game->pge_setupDefaultAnim(&game->_pgeLive[0]);
				}
				platform._pi.dirMask = tick >= 130 && tick < 140 ? PlayerInput::DIR_DOWN : 0;
				game->mainLoop();
				const auto &effect = game->_pgeLive[actor];
				if (effect.anim_number == 287 || effect.anim_number == 288) {
					if (variant == 0) {
						std::vector<uint8_t> frame(game->_vid._frontLayer,
						                           game->_vid._frontLayer + game->_vid._layerSize);
						std::fill_n(game->_vid._frontLayer, game->_vid._layerSize, 7);
						const bool erase = game->_eraseBackground;
						game->_eraseBackground = false;
						const auto *object =
						    game->_res._spc +
						    READ_BE_UINT16(game->_res._spc + effect.anim_number * 2);
						game->drawObject(object, effect.pos_x + 8, effect.pos_y + 2, effect.flags);
						int left = 256, right = -1, top = 224, bottom = -1;
						for (int y = 0; y < 224; ++y)
							for (int x = 0; x < 256; ++x)
								if (game->_vid._frontLayer[y * 256 + x] != 7) {
									left = std::min(left, x);
									right = std::max(right, x);
									top = std::min(top, y);
									bottom = std::max(bottom, y);
								}
						check(left <= right, "DOS native flash rendered no pixels");
						origins[tick] = {(effect.flags & 1) ? left : right, (top + bottom) / 2,
						                 (effect.flags & 1) ? 1 : -1};
						game->_eraseBackground = erase;
						std::copy(frame.begin(), frame.end(), game->_vid._frontLayer);
					} else {
						const auto &fx = game->_weaponFx;
						check(fx.next && fx.poseAge < 2, "DOS native flash was not replaced");
						const auto &shot = fx.shots[(fx.next - 1) % fx.shots.size()];
						check(shot.flashX == origins[tick][0] && shot.flashY == origins[tick][1] &&
						          shot.direction == origins[tick][2],
						      "DOS replacement origin differs from native pixels");
						directions |= 1u << ((tick >= 130 ? 2 : 0) + (shot.direction < 0 ? 1 : 0));
					}
				}
				for (const auto &channel : game->_mix._channels) {
					const auto tone = variant == 1 ? MixerTone::Bass : MixerTone::DeepBass;
					const int sample = variant == 1 ? 4 : 3;
					styledSound |= channel.active && channel.tone == tone &&
					               channel.chunk.data == game->_res._sfxList[sample].data;
				}
				File state;
				state.openMemoryBuffer(32768);
				game->saveState(&state);
				state.seek(0);
				std::vector<uint8_t> bytes(state.size());
				state.read(bytes.data(), bytes.size());
				if (variant == 0)
					baseline.push_back(bytes);
				else if (bytes != baseline[tick]) {
					for (size_t pos = 0; pos < std::min(bytes.size(), baseline[tick].size()); ++pos)
						if (bytes[pos] != baseline[tick][pos]) {
							std::cerr << "variant " << variant << " tick " << tick << " byte "
							          << pos << " got " << int(bytes[pos]) << " expected "
							          << int(baseline[tick][pos]) << '\n';
							break;
						}
					throw std::runtime_error("Cosmetic choice changed DOS gameplay state");
				}
				if (tick == 56 || tick == 106 || tick == 146 || tick == 201) {
					uint8_t colors[768];
					platform.getPalette(colors, 256);
					const auto name = output / ("shot-" + std::to_string(variant) + "-" +
					                            std::to_string(tick) + ".bmp");
					check(saveBMP(name.string().c_str(), game->_vid._frontLayer, colors, 256, 224),
					      "Could not capture DOS shot");
				}
			}
			check(!variant || (directions == 15 && styledSound), "Missing DOS shot pose or sound");
			if (variant == 0) {
				int rooms = 0;
				for (int level = 0; level < 7; ++level) {
					game->_currentLevel = level;
					game->loadLevelData();
					game->resetGameState();
					for (int room = 0; room < 64; ++room) {
						if (game->hasLevelMap(level, room)) {
							game->_currentRoom = room;
							game->loadLevelMap();
							++rooms;
						}
					}
				}
				std::cout << "DOS: seven levels, " << rooms << " room backgrounds.\n";
			}
			game->_mix.free();
		}
		std::cout << "DOS: native/Heavy/five laser colours, four shot poses, native source audio "
		             "and identical gameplay state across 7 x 220 ticks.\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 2;
	}
}
