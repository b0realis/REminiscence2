// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_SETTINGS_H
#define REMINISCENCE2_SETTINGS_H

#include "intern.h"
#include "appearance.h"
#include "weapon_fx.h"
#include "journal.h"
#include "progress_store.h"
#include "asset_library.h"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace settings {
enum class Filter { Point, Linear, Scale };
enum class Binding { Left, Right, Up, Down, Action, Use, Weapon, Inventory, Pause, Count };
constexpr size_t BindingCount = static_cast<size_t>(Binding::Count);
constexpr size_t PadBindingCount = 8;

struct Preferences {
	bool fullscreen = true, vsync = false, pauseOnFocusLoss = true, showFps = false;
	bool showMenuButton = true;
	int menuLayout = 0; // 0: automatic, 1: compact; small windows always use compact.
	Filter filter = Filter::Scale;
	int scale = 3, widescreen = 0, uiScale = 100;
	int masterVolume = 100, musicVolume = 100, effectsVolume = 100;
	bool muted = false, rewind = false;
	weapon::Style pistolStyle = weapon::Style::Original;
	weapon::LaserColor laserColor = weapon::LaserColor::Orange;
	appearance::Shirt conradShirt = appearance::Shirt::Original;
	bool fadeTransitions = true, inventoryOriginal = false, saveChime = false;
	bool restoreMemo = true, textCutscenes = false, seqCutscenes = true;
	bool skipLogosWithKeyboard = true, skipCutscenesWithKeyboard = true;
	bool extraAsc = false, extraCaillou = false, extraCarte = false;
	bool extraMetro = false, extraSerrure = false;
	// Stable SDL scancodes, independent of keyboard layout. Display names and
	// capture belong to the SDL frontend; the model/storage never opens devices.
	std::array<int, BindingCount> keys = {{80, 79, 82, 81, 225, 40, 44, 43, 41}};
	// SDL controller button numbers; -1 leaves an optional shortcut unassigned.
	std::array<int, PadBindingCount> padButtons = {{2, 0, 1, 3, 6, -1, -1, -1}};
	std::array<int, PadBindingCount> joystickButtons = {{1, 2, 0, 3, 9, -1, -1, -1}};
	int stickDeadzone = 15;
	bool rightStickMovement = false;
	bool storyJournal = true, journalNotifications = true;
	bool diskAutosave = true, contextualHints = false;
	bool quickResume = false;
	std::string assetSet, musicSource; // Empty: ask for edition / use this edition's music.
	bool stickyAction = false, reduceShake = false, reduceBlink = false, inputBuffer = false;
	int gameSpeed = 100;
	bool objectivesNotebook = true;
	int diskAutosaveSeconds = 60;

	void readCompatibility(const Options &options);
	void applyCompatibility(Options &options) const;
	bool operator==(const Preferences &other) const;
	bool operator!=(const Preferences &other) const { return !(*this == other); }
};

const char *bindingName(size_t index);
const char *padBindingName(size_t index);
const char *padButtonName(int button, int style = 0);
bool validPadBinding(int button, const Preferences &, size_t index, bool raw, std::string &reason);
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
	UpdateSaveDetails,
	Rewind,
	ContinueLatest,
	LoadAutosave,
	Heal,
	Screenshot,
	StartLevel,
	SwitchAssets,
	Title,
	Quit
};
struct SaveSlot {
	int slot = 0, room = -1;
	bool exists = false, supported = false;
	std::string modified;
	int level = 0;
	std::string label;
	bool modern = false, protectedSave = false;
	std::shared_ptr<const progress::Preview> preview;
};
struct RewindPoint {
	std::shared_ptr<const progress::Preview> preview;
	unsigned ageSeconds = 0;
	int room = 0, health = 0;
};
struct Session {
	Preferences values;
	bool startup = false, recovery = false;
	bool assetPicker = false;
	std::vector<assets::Source> assetSources, musicSources;
	int assetIndex = 0;
	bool experimentalPistolAvailable = false;
	int autosaveIndex = 0, rewindIndex = 0;
	std::vector<progress::Snapshot> autosaves;
	std::vector<RewindPoint> rewindPoints;
	std::string preferencePath, edition, language, audioBackend, controllerName, controllerProfile;
	bool controllerDisconnected = false, audioAvailable = true;
	int controllerStyle = 0;
	int browseLevel = 0;
	std::vector<std::string> diagnostics;
	double lastSaveMs = 0, worstSaveMs = 0;
	std::array<char, 97> saveName{};
	bool protectSave = false;
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
	std::vector<journal::Entry> journalEntries;
	std::vector<journal::Objective> objectives;
	std::function<void(unsigned)> readJournalEntry;
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
