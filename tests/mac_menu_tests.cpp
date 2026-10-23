// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic resources exercise Macintosh menu paths without commercial assets.
#include "config.h"
#include "cutscene_tool.h"
#include "cutscene.h"
#include "resource.h"
#include "menu.h"
#include "video.h"
#include "headless_system.h"
#include "fs.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
Options g_options;
const char *g_caption = "Mac menu tests";
using Bytes = std::vector<uint8_t>;
static void be16(Bytes &b, size_t p, unsigned v) {
	b.at(p) = v >> 8;
	b.at(p + 1) = v;
}
static void be32(Bytes &b, size_t p, uint32_t v) {
	be16(b, p, v >> 16);
	be16(b, p + 2, v);
}
static Bytes picture(int frames = 1) {
	const size_t offset = 8 + frames * 4;
	Bytes b(offset + 8);
	be16(b, 0, 0xC211);
	be16(b, 2, frames);
	for (int i = 0; i < frames; ++i) {
		be32(b, 8 + i * 4, offset);
	}
	be16(b, offset, 16);
	be16(b, offset + 2, 16);
	for (int y = 0; y < 16; ++y) {
		b.push_back(y ? 0xD0 : 0x50);
		b.push_back(0xC1);
	}
	b.push_back(0x41);
	return b;
}
static Bytes compressed(const Bytes &raw) {
	Bytes b(4);
	be32(b, 0, raw.size());
	for (size_t p = 0; p < raw.size();) {
		b.push_back(0);
		for (int i = 0; i < 8 && p < raw.size(); ++i) {
			b.push_back(raw[p++]);
		}
	}
	return b;
}
struct Entry {
	std::string name;
	Bytes bytes;
};
static Bytes fixture() {
	std::map<std::string, std::vector<Entry>> groups;
	groups["PPSS"].push_back({"Font", compressed(picture(96))});
	for (int i = 1; i <= 6; ++i) {
		groups["PPSS"].push_back(
		    {"Title " + std::to_string(i), i == 6 ? compressed(picture()) : picture()});
	}
	Bytes colors(8 + 1023 * 8);
	be16(colors, 6, 1023);
	for (int i = 0; i < 1023; ++i) {
		be16(colors, 8 + i * 8, i);
		be16(colors, 10 + i * 8, 0x7777);
	}
	groups["clut"].push_back({"Flashback colors", colors});
	groups["STRS"].push_back({"Font", Bytes{99}}); // Ambiguous name, wrong type.
	groups["STRS"].push_back({"Movie strings", Bytes(4)});
	groups["PMOV"].push_back({"intro1 movie", compressed(Bytes{0, 2, 0, 6, 0, 8, 0, 0x80, 0, 0x80})});
	groups["POLY"].push_back({"intro1 polygons", compressed(Bytes(20))});
	groups["vers"].push_back(
	    {"Version", Bytes{1,   2,   0x80, 0,   0,   0,   3,   '1', '.', '2', 10,
		                  'F', 'i', 'x',  't', 'u', 'r', 'e', ' ', 'C', 'D'}});
	Bytes data;
	std::map<std::string, std::vector<size_t>> offsets;
	for (const auto &group : groups) {
		for (const auto &entry : group.second) {
			offsets[group.first].push_back(data.size());
			const size_t pos = data.size();
			data.resize(pos + 4);
			be32(data, pos, entry.bytes.size());
			data.insert(data.end(), entry.bytes.begin(), entry.bytes.end());
		}
	}
	const size_t types = groups.size();
	size_t total = 0;
	for (const auto &g : groups) {
		total += g.second.size();
	}
	const size_t names = 28 + 2 + types * 8 + total * 12;
	Bytes map(names);
	be16(map, 24, 28);
	be16(map, 26, names);
	be16(map, 28, types - 1);
	size_t typeAt = 30, entryAt = 30 + types * 8;
	unsigned id = 128;
	for (const auto &group : groups) {
		std::copy_n(group.first.data(), 4, map.begin() + typeAt);
		be16(map, typeAt + 4, group.second.size() - 1);
		be16(map, typeAt + 6, entryAt - 28);
		typeAt += 8;
		for (size_t i = 0; i < group.second.size(); ++i) {
			const auto &entry = group.second[i];
			be16(map, entryAt, id++);
			be16(map, entryAt + 2, map.size() - names);
			be32(map, entryAt + 4, offsets[group.first][i]);
			entryAt += 12;
			map.push_back(entry.name.size());
			map.insert(map.end(), entry.name.begin(), entry.name.end());
		}
	}
	Bytes file(256);
	be32(file, 0, 256);
	be32(file, 4, 256 + data.size());
	be32(file, 8, data.size());
	be32(file, 12, map.size());
	file.insert(file.end(), data.begin(), data.end());
	file.insert(file.end(), map.begin(), map.end());
	return file;
}
struct Scripted final : HeadlessSystem {
	enum { Down = 1, Enter, Character };
	std::vector<std::pair<int, char>> actions;
	size_t next = 0;
	void queue(std::vector<std::pair<int, char>> script) {
		actions = std::move(script);
		next = 0;
		_pi = {};
	}
	void processEvents() override {
		_pi.dirMask = 0;
		_pi.enter = false;
		_pi.lastChar = 0;
		if (next == actions.size()) {
			_pi.quit = true;
			return;
		}
		const auto action = actions[next++];
		if (action.first == Down) {
			_pi.dirMask = PlayerInput::DIR_DOWN;
		} else if (action.first == Enter) {
			_pi.enter = true;
		} else {
			_pi.lastChar = action.second;
		}
	}
};
static int failures = 0;
static void check(bool value, const char *message) {
	if (!value) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}
int main(int argc, char **argv) {
	if (argc != 2) {
		return 1;
	}
	std::filesystem::create_directories(argv[1]);
	const auto file = std::filesystem::path(argv[1]) / "Flashback.rsrc";
	const Bytes bytes = fixture();
	{
		std::ofstream output(file, std::ios::binary);
		output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
	}
	setDefaultOptions(g_options);
	FileSystem files(argv[1]);
	Scripted system;
	Resource resource(&files, kResourceTypeMac, LANG_EN);
	resource.init();
	resource.load_TEXT();
	resource.MAC_loadClutData();
	resource.MAC_loadFontData();
	Video video(&resource, &system, kWidescreenNone);
	Menu menu(&resource, &system, &video);
	check(video._w == 512 && video._h == 448, "Mac renderer retains native resolution");
	check(resource._mac->versionDescription() == "1.2 (Fixture CD)",
	      "bounded version resource description");
	const auto cutscenes = cinematics::catalog(resource);
	check(cutscenes.size() == 1 && cutscenes[0].name == "intro1" && cutscenes[0].parts == 2,
	      "Mac cutscene catalog uses typed movie and polygon resources");
	Cutscene cutscene(&resource, &system, &video);
	system.queue({{Scripted::Enter, 0}});
	cutscene._id = 13;
	cutscene._creditsSequence = false;
	cutscene.prepare();
	check(cutscene.load(7) && resource._cmdSize == 10 && resource._polSize == 20,
	      "Mac cutscene decoded lengths retained");
	cutscene.mainLoop(1);
	check(cutscene._cmdPtr == resource._cmd + 10, "Mac part starts at its absolute resource offset");
	cutscene.unload();
	using S = Scripted;
	system.queue({{S::Down, 0},
	              {S::Enter, 0},
	              {S::Down, 0},
	              {S::Enter, 0},
	              {S::Down, 0},
	              {S::Down, 0},
	              {S::Down, 0},
	              {S::Enter, 0},
	              {S::Enter, 0},
	              {S::Enter, 0}});
	menu.handleTitleScreen();
	check(!system._pi.quit && menu._skill == 2 &&
	          menu._selectedOption == Menu::MENU_OPTION_ITEM_START,
	      "Mac Skill and Controls return to Start");
	std::vector<std::pair<int, char>> password;
	for (const char *p = menu.getLevelPassword(1, 2); *p; ++p) {
		password.push_back({S::Character, *p});
	}
	password.push_back({S::Enter, 0});
	system.queue(password);
	check(menu.handlePasswordScreen() && menu._level == 1 && menu._skill == 2,
	      "Mac password uses common text entry and edition table");
	menu._level = 0;
	std::vector<std::pair<int, char>> credits(7, {S::Down, 0});
	credits.push_back({S::Enter, 0});
	system.queue(credits);
	check(menu.handleLevelScreen() && menu._level == 7,
	      "credits selection stays separate from playable level indexes");
	system.queue({{S::Down, 0}, {S::Down, 0}, {S::Down, 0}, {S::Down, 0}, {S::Enter, 0}});
	menu.handleTitleScreen();
	check(!system._pi.quit && menu._selectedOption == Menu::MENU_OPTION_ITEM_DEMO,
	      "Mac Demo menu action");
	std::cout << (failures ? "Mac menu checks failed\n" : "Mac menu checks passed\n");
	return failures ? 1 : 0;
}
