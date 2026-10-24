// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "runtime_options.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace settings {
namespace {
struct Field {
	std::string name;
	int minimum, maximum;
	std::function<int()> get;
	std::function<void(int)> put;
};
std::vector<Field> fields(Preferences &p) {
	std::vector<Field> result;
	auto integer = [&](const char *name, int &value, int lo, int hi) {
		result.push_back(
		    {name, lo, hi, [&value] { return value; }, [&value](int n) { value = n; }});
	};
	auto boolean = [&](const char *name, bool &value) {
		result.push_back(
		    {name, 0, 1, [&value] { return int(value); }, [&value](int n) { value = n != 0; }});
	};
	boolean("fullscreen", p.fullscreen);
	boolean("vsync", p.vsync);
	boolean("pause_on_focus_loss", p.pauseOnFocusLoss);
	boolean("show_fps", p.showFps);
	boolean("show_menu_button", p.showMenuButton);
	integer("menu_layout", p.menuLayout, 0, 1);
	result.push_back(
	    {"filter", 0, 2, [&p] { return int(p.filter); }, [&p](int n) { p.filter = Filter(n); }});
	integer("window_scale", p.scale, 1, 6);
	integer("widescreen", p.widescreen, 0, 5);
	integer("menu_scale", p.uiScale, 75, 175);
	integer("master_volume", p.masterVolume, 0, 100);
	integer("music_volume", p.musicVolume, 0, 100);
	integer("effects_volume", p.effectsVolume, 0, 100);
	boolean("muted", p.muted);
	result.push_back({"conrad_shirt", 0, 2, [&p] { return int(p.conradShirt); },
	                  [&p](int n) { p.conradShirt = appearance::Shirt(n); }});
	result.push_back({"pistol_style", 0, 2, [&p] { return int(p.pistolStyle); },
	                  [&p](int n) { p.pistolStyle = weapon::Style(n); }});
	result.push_back({"laser_color", 0, 4, [&p] { return int(p.laserColor); },
	                  [&p](int n) { p.laserColor = weapon::LaserColor(n); }});
	boolean("rewind", p.rewind);
	boolean("fade_transitions", p.fadeTransitions);
	boolean("inventory_original", p.inventoryOriginal);
	boolean("save_chime", p.saveChime);
	boolean("restore_memo", p.restoreMemo);
	boolean("text_cutscenes", p.textCutscenes);
	boolean("seq_cutscenes", p.seqCutscenes);
	boolean("skip_logos_with_keyboard", p.skipLogosWithKeyboard);
	boolean("skip_cutscenes_with_keyboard", p.skipCutscenesWithKeyboard);
	boolean("extra_asc", p.extraAsc);
	boolean("extra_caillou", p.extraCaillou);
	boolean("extra_carte", p.extraCarte);
	boolean("extra_metro", p.extraMetro);
	boolean("extra_serrure", p.extraSerrure);
	integer("stick_deadzone", p.stickDeadzone, 5, 50);
	boolean("right_stick_movement", p.rightStickMovement);
	boolean("story_journal", p.storyJournal);
	boolean("journal_notifications", p.journalNotifications);
	boolean("disk_autosave", p.diskAutosave);
	boolean("quick_resume", p.quickResume);
	integer("disk_autosave_seconds", p.diskAutosaveSeconds, 15, 600);
	boolean("contextual_hints", p.contextualHints);
	boolean("sticky_action", p.stickyAction);
	boolean("reduce_shake", p.reduceShake);
	boolean("reduce_blink", p.reduceBlink);
	boolean("input_buffer", p.inputBuffer);
	integer("game_speed", p.gameSpeed, 50, 100);
	boolean("objectives_notebook", p.objectivesNotebook);
	for (size_t i = 0; i < PadBindingCount; ++i) {
		result.push_back({"pad_button_" + std::to_string(i), -1, 33,
		                  [&p, i] { return p.padButtons[i]; },
		                  [&p, i](int n) { p.padButtons[i] = n; }});
		result.push_back({"joystick_button_" + std::to_string(i), -1, 31,
		                  [&p, i] { return p.joystickButtons[i]; },
		                  [&p, i](int n) { p.joystickButtons[i] = n; }});
	}
	static const char *const bindings[] = {"key_left",   "key_right",     "key_up",
	                                       "key_down",   "key_action",    "key_use",
	                                       "key_weapon", "key_inventory", "key_pause"};
	for (size_t i = 0; i < BindingCount; ++i) {
		result.push_back({bindings[i], 1, 511, [&p, i] { return p.keys[i]; },
		                  [&p, i](int n) { p.keys[i] = n; }});
	}
	return result;
}
std::string trim(const std::string &text) {
	const size_t begin = text.find_first_not_of(" \t\r\n");
	return begin == std::string::npos
	           ? ""
			   : text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}
} // namespace

bool load(const std::string &path, Preferences &preferences, std::string &diagnostic) {
	diagnostic.clear();
	std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
	if (!input) {
		return false;
	}
	input.seekg(0, std::ios::end);
	if (input.tellg() < 0 || input.tellg() > 65536) {
		diagnostic = "Preferences file exceeds 64 KiB; using current defaults.";
		return false;
	}
	input.seekg(0);
	std::map<std::string, std::string> values;
	std::string line;
	while (std::getline(input, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}
		const auto split = line.find('=');
		if (split == std::string::npos ||
		    !values.emplace(trim(line.substr(0, split)), trim(line.substr(split + 1))).second) {
			diagnostic = "Ignored malformed or duplicate preferences entries.";
		}
	}
	if (input.bad() || values["version"] != "1") {
		diagnostic = "Unreadable or unsupported preferences version; using current defaults.";
		return false;
	}
	Preferences parsed = preferences;
	for (const auto &entry : {std::make_pair("asset_set", &parsed.assetSet),
	                          std::make_pair("music_source", &parsed.musicSource)}) {
		const auto value = values.find(entry.first);
		if (value != values.end()) {
			std::istringstream stream(value->second);
			std::string decoded;
			if ((stream >> std::quoted(decoded)) && (stream >> std::ws).eof() &&
			    decoded.size() <= 4096 && decoded.find_first_of("\r\n") == std::string::npos &&
			    decoded.find('\0') == std::string::npos)
				*entry.second = decoded;
			else
				diagnostic = "Ignored invalid source selection: " + std::string(entry.first);
		}
	}
	for (const auto &field : fields(parsed)) {
		const auto found = values.find(field.name);
		if (found == values.end()) {
			continue;
		}
		uint32_t number;
		if (field.minimum == -1 && found->second == "-1") {
			field.put(-1);
		} else if (parseUnsignedOption(found->second.c_str(), std::max(0, field.minimum),
		                               field.maximum, number)) {
			field.put(number);
		} else {
			diagnostic = "Ignored an out-of-range preferences value: " + field.name;
		}
	}
	if (parsed.filter == Filter::Scale) {
		parsed.scale = std::max(2, std::min(4, parsed.scale));
	}
	std::string reason;
	if (!validate(parsed, reason)) {
		parsed.keys = preferences.keys;
		parsed.padButtons = preferences.padButtons;
		parsed.joystickButtons = preferences.joystickButtons;
		diagnostic = "Ignored invalid input bindings: " + reason;
	}
	preferences = parsed;
	return true;
}

bool save(const std::string &path, const Preferences &preferences, std::string &diagnostic) {
	if (!validate(preferences, diagnostic)) {
		return false;
	}
	Preferences copy = preferences;
	std::ostringstream text;
	text
	    << "# REminiscence2 player preferences; command-line overrides take priority.\nversion=1\n";
	for (const auto &field : fields(copy)) {
		text << field.name << '=' << field.get() << '\n';
	}
	text << "asset_set=" << std::quoted(copy.assetSet) << '\n'
	     << "music_source=" << std::quoted(copy.musicSource) << '\n';
	const std::string bytes = text.str();
	const auto destination = std::filesystem::u8path(path);
	static std::atomic<unsigned> serial{0};
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	auto temporary = destination;
	temporary += ".tmp-" + std::to_string(nonce) + '-' + std::to_string(++serial);
#ifdef _WIN32
	FILE *file = _wfopen(temporary.c_str(), L"wbx");
#else
	FILE *file = fopen(temporary.c_str(), "wbx");
#endif
	if (!file) {
		diagnostic = "Cannot create a preferences file. Check that the save directory is writable.";
		return false;
	}
	bool good = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size() && fflush(file) == 0;
#ifndef _WIN32
	if (good) {
		good = fsync(fileno(file)) == 0;
	}
#endif
	good = fclose(file) == 0 && good;
	std::error_code failure;
	if (good) {
#ifdef _WIN32
		good = MoveFileExW(temporary.c_str(), destination.c_str(),
		                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		std::filesystem::rename(temporary, destination, failure);
		good = !failure;
#endif
	}
	if (!good) {
		std::filesystem::remove(temporary, failure);
		diagnostic = "Could not save preferences; the previous file was kept.";
		return false;
	}
	diagnostic = "Settings saved for your next launch.";
	return true;
}
} // namespace settings
