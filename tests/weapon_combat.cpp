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
const char *g_caption = "Edition weapon asset tests";
static void check(bool ok, const char *message) {
	if (!ok)
		throw std::runtime_error(message);
}
static void init(Game &game) {
	game.initCutsceneResources();
	if (game._res.isAmiga()) {
		game._res.load("ICONE", Resource::OT_ICN, "SPR");
		game._res.load("ICON", Resource::OT_ICN, "SPR");
		game._res.load("PERSO", Resource::OT_SPM);
	} else if (!game._res.isSegaCD()) {
		game._res.load("GLOBAL", Resource::OT_ICN);
		game._res.load("GLOBAL", Resource::OT_SPC);
		game._res.load("PERSO", Resource::OT_SPR);
		game._res.load_SPR_OFF("PERSO", game._res._spr1);
		game._res.load_FIB("GLOBAL");
	}
	game._mix.init();
	game._preferences.diskAutosave = false;
	game._randSeed = 1337;
	game.loadLevelData();
	game.resetGameState();
	game._cut._id = 0xffff;
}

int main(int argc, char **argv) {
	if (argc != 4)
		return 1;
	try {
		setDefaultOptions(g_options);
		FileSystem files(argv[1]);
		std::filesystem::create_directories(argv[2]);
		const auto edition = std::string(argv[3]) == "segacd"  ? kResourceTypeSega
		                     : std::string(argv[3]) == "amiga" ? kResourceTypeAmiga
		                                                       : kResourceTypeDOS;
		for (bool right : {false, true}) {
			std::vector<std::vector<uint8_t>> baseline;
			for (int variant = 0; variant < 7; ++variant) {
				HeadlessSystem platform;
				auto game = std::make_unique<Game>(&platform, &files, &files, argv[2], 0, edition,
				                                   LANG_EN, kWidescreenNone, false, 0);
				init(*game);
				game->_preferences.pistolStyle = weapon::Style(std::min(variant, 2));
				game->_preferences.laserColor = weapon::LaserColor(std::max(0, variant - 2));
				// Use the original mutant's scripts/sprites/health. Move it into
				// Conrad's starting room to make this a short reproducible encounter,
				// without modifying imported asset files or synthesizing damage.
				int target = -1;
				for (int i = 1; i < game->_res._pgeNum; ++i)
					if (game->_pgeLive[i].init_PGE->object_type == 10 &&
					    game->_pgeLive[i].life > 0) {
						target = i;
						break;
					}
				check(target >= 0, "No living mutant in level fixture");
				auto &enemy = game->_pgeLive[target];
				auto &player = game->_pgeLive[0];
				const int startingHealth = enemy.life;
				bool damaged = false, traced = false, impactPixels = false;
				for (int tick = 0; tick < 90; ++tick) {
					if (tick == 50) {
						const int oldRoom = enemy.room_location;
						enemy.room_location = player.room_location;
						enemy.pos_x = right ? 192 : 16;
						enemy.pos_y = player.pos_y;
						game->pge_addToCurrentRoomList(&enemy, oldRoom);
						enemy.flags |= 4;
						game->_pge_liveTable2[target] = &enemy;
						if (right)
							player.flags ^= 3;
					}
					platform._pi.space = tick == 35;
					platform._pi.shift = tick == 55; // One shot, no automatic follow-up.
					game->mainLoop();
					damaged |= enemy.life < startingHealth;
					if (variant >= 2 && game->_weaponFx.next) {
						const auto &shot = game->_weaponFx.shots[0];
						if (shot.impact && shot.age < 12) {
							traced = true;
							check(shot.rayLength > 0 && shot.rayLength < 192,
							      "Beam missed native enemy endpoint");
							const int end = shot.x + shot.direction * shot.rayLength;
							const auto *row = game->_vid._frontLayer + shot.y * game->_vid._w;
							impactPixels |= row[end] == weapon::PaletteBase + 5;
							for (int x = end + shot.direction; x >= 0 && x < game->_vid._w;
							     x += shot.direction)
								check(row[x] < weapon::PaletteBase + 3 ||
								          row[x] > weapon::PaletteBase + 5,
								      "Laser pixels passed through the enemy collision");
						}
					}
					File state;
					state.openMemoryBuffer(32768);
					game->saveState(&state);
					state.seek(0);
					std::vector<uint8_t> bytes(state.size());
					state.read(bytes.data(), bytes.size());
					if (!variant)
						baseline.push_back(bytes);
					else
						check(bytes == baseline[tick],
						      "Weapon style changed combat simulation or damage timing");
					if (tick == 63) {
						uint8_t palette[768];
						platform.getPalette(palette, 256);
						check(saveBMP((std::filesystem::path(argv[2]) /
						               (std::string(right ? "combat-right-" : "combat-left-") +
						                std::to_string(variant) + ".bmp"))
						                  .string()
						                  .c_str(),
						              game->_vid._frontLayer, palette, 256, 224),
						      "Capture combat frame");
					}
				}
				check(damaged, "Real pistol shot did not damage the enemy");
				check(variant < 2 || (traced && impactPixels),
				      "Laser did not visibly terminate on its native hit");
				game->_mix.free();
			}
		}
		std::cout << "Original, Heavy and five laser colours: same enemy damage and full gameplay "
		             "state; visible laser collision verified.\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
