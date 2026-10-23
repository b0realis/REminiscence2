// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_SETTINGS_H
#define REMINISCENCE2_SETTINGS_H

#include "intern.h"
#include "appearance.h"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace settings {
enum class Filter { Point, Linear, Scale };
enum class Binding { Left, Right, Up, Down, Action, Use, Weapon, Inventory, Pause, Count };
constexpr size_t BindingCount = static_cast<size_t>(Binding::Count);

struct Preferences {
	bool fullscreen = true, vsync = false, pauseOnFocusLoss = true, showFps = false;
	bool showMenuButton = true;
	int menuLayout = 0; // 0: automatic, 1: compact; small windows always use compact.
	Filter filter = Filter::Scale;
	int scale = 3, widescreen = 0, uiScale = 100;
	int masterVolume = 100, musicVolume = 100, effectsVolume = 100;
	bool muted = false, rewind = false;
	appearance::Shirt conradShirt = appearance::Shirt::Original;
	bool fadeTransitions = true, inventoryOriginal = false, saveChime = false;
	bool restoreMemo = true, textCutscenes = false, seqCutscenes = true;
	bool extraAsc = false, extraCaillou = false, extraCarte = false;
	bool extraMetro = false, extraSerrure = false;
	// Stable SDL scancodes, independent of keyboard layout. Display names and
	// capture belong to the SDL frontend; the model/storage never opens devices.
	std::array<int, BindingCount> keys = {{80, 79, 82, 81, 225, 40, 44, 43, 41}};

	void readCompatibility(const Options &options);
	void applyCompatibility(Options &options) const;
	bool operator==(const Preferences &other) const;
	bool operator!=(const Preferences &other) const { return !(*this == other); }
};

const char *bindingName(size_t index);
bool validate(const Preferences &preferences, std::string &reason);
bool validBinding(int scancode, const Preferences &preferences, size_t index, std::string &reason);
// Missing files are normal. Invalid fields retain their previous values; an
// unsupported schema leaves the entire input object untouched.
bool load(const std::string &path, Preferences &preferences, std::string &diagnostic);
bool save(const std::string &path, const Preferences &preferences, std::string &diagnostic);

enum class Action {
	None,
	Resume,
	SaveGame,
	LoadGame,
	Rewind,
	Heal,
	Screenshot,
	StartLevel,
	Title,
	Quit
};
struct SaveSlot {
	int slot = 0, room = -1;
	bool exists = false, supported = false;
	std::string modified;
};
struct Session {
	Preferences values;
	std::string preferencePath, edition, language, audioBackend;
	std::string status;
	bool statusError = false, canSave = true, canLoad = true, canRewind = false;
	int level = 0, room = 0, saveSlot = 1, outputRate = 0;
	int commandSlot = -1; // One-shot override for checkpoint loading; retain the manual slot.
	uint32_t score = 0;
	int health = 0, rewindCount = 0, skill = 1, targetLevel = 0, targetSkill = 1;
	uint32_t cheats = 0;
	uint8_t debugFlags = 0;
	bool cheatsRequested = false, refreshRequested = false, autoZoomAvailable = false;
	std::array<bool, 7> levelAvailable = {{true, true, true, true, true, true, true}};
	std::vector<SaveSlot> saves;
	bool applyRequested = false, saveRequested = false;
	Action action = Action::None;
	std::function<bool(const Preferences &, std::string &)> apply;
	std::function<bool(const Preferences &, std::string &)> persist;
	std::function<bool(Action, int, std::string &)> command;
	std::function<bool(int)> hasSave;
	std::function<void(uint32_t, uint8_t)> applyCheats;
	std::function<void()> refreshSaves;
};
} // namespace settings
#endif
