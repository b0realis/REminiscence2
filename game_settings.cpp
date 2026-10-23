// SPDX-License-Identifier: GPL-3.0-or-later
#include "game.h"
#include "file.h"
#include "systemstub.h"
#include "save_catalog.h"
#include "screenshot.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <vector>

void Game::configurePreferences(const settings::Preferences &preferences) {
	_preferences = preferences;
	_vid._conradShirt = preferences.conradShirt;
	_stub->configurePreferences(preferences);
	_preferences.applyCompatibility(g_options);
	_autoSave = preferences.rewind;
	_preferencePath = (std::filesystem::u8path(_savePath) / "preferences.cfg").u8string();
	_mix.setVolumes(preferences.masterVolume, preferences.musicVolume, preferences.effectsVolume,
	                preferences.muted);
}

bool Game::applyPreferences(const settings::Preferences &preferences, std::string &reason) {
	if (!settings::validate(preferences, reason) || !_stub->applyPreferences(preferences, reason)) {
		return false;
	}
	const bool redrawSides = _widescreenMode != preferences.widescreen;
	const bool redrawShirt = _preferences.conradShirt != preferences.conradShirt;
	if (_autoSave && !preferences.rewind) {
		clearStateRewind();
	}
	_autoSave = preferences.rewind;
	_preferences = preferences;
	_vid._conradShirt = preferences.conradShirt;
	_preferences.applyCompatibility(g_options);
	_mix.setVolumes(preferences.masterVolume, preferences.musicVolume, preferences.effectsVolume,
	                preferences.muted);
	_widescreenMode = static_cast<WidescreenMode>(preferences.widescreen);
	_vid._widescreenMode = _widescreenMode;
	if (redrawSides && _res._pgeNum) {
		// Rebuild the room backgrounds and borders, retaining the already composed
		// frame (including sprites) beneath the paused overlay.
		std::vector<uint8_t> front(_vid._frontLayer, _vid._frontLayer + _vid._layerSize);
		loadLevelMap();
		memcpy(_vid._frontLayer, front.data(), front.size());
		_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
		_vid.fullRefresh();
	}
	if ((redrawShirt || redrawSides) && _res._pgeNum) {
		// Clothing pixels already use private palette aliases. Refresh the RGB
		// presentation immediately, without redrawing actors or advancing play.
		_vid.refreshConradShirtPalette();
		_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
		_vid.fullRefresh();
	}
	return true;
}

bool Game::settingsLevelAvailable(int level) {
	if (level < 0 || level >= 7)
		return false;
	if (_res._isDemo)
		return level == _currentLevel;
	if (_res.isMac()) {
		static const char *const numbers[] = {"1", "2", "3", "4-1", "4-2", "5-1", "5-2"};
		const std::string name = std::string("Level ") + numbers[level] + " objects";
		return _res._mac && _res._mac->findEntry(name.c_str());
	}
	const auto &data = _gameLevels[level];
	const std::string name = _res.isAmiga() ? data.nameAmiga : data.name2;
	return _res.fileExists((name + ".pge").c_str());
}

bool Game::startSettingsLevel(int level, int skill, std::string &reason) {
	if (skill < kSkillEasy || skill > kSkillExpert || !settingsLevelAvailable(level)) {
		reason = "That level is unavailable in these assets, or the difficulty is invalid.";
		return false;
	}
	// Reuse the normal level loader at the paused boundary. No stale rewind,
	// checkpoint-valid flag, inventory pointers or death script survive the jump.
	clearStateRewind();
	_mix.stopMusic();
	_currentLevel = _menu._level = level;
	_skillLevel = _menu._skill = skill;
	_score = 0;
	loadLevelData();
	resetGameState();
	_pge_opGunVar = 0;
	_vid.setPalette0xF();
	_vid.setTextPalette();
	loadLevelMap();
	memcpy(_vid._frontLayer, _vid._backLayer, _vid._layerSize);
	_vid.fullRefresh();
	_frameTimestamp = _saveTimestamp = _stub->getTimeStamp();
	reason = "Level started.";
	return true;
}

bool Game::handleConfigPanel() {
	settings::Session session;
	session.values = _preferences;
	session.values.readCompatibility(g_options);
	// The legacy bool cannot express the explicit Red/Pink choice.
	session.values.conradShirt = _preferences.conradShirt;
	session.values.rewind = _autoSave;
	_stub->readPreferences(session.values);
	if (_preferencePath.empty()) {
		_preferencePath = (std::filesystem::u8path(_savePath) / "preferences.cfg").u8string();
	}
	session.preferencePath = _preferencePath;
	session.edition = _res.isAmiga()  ? "Amiga"
	                  : _res.isMac()  ? "Macintosh"
	                  : _res.isSega() ? "Sega"
	                  : _res.isPC98() ? "PC-98"
	                                  : "DOS";
	static const char *const languages[] = {"French",  "English",  "German", "Spanish",
	                                        "Italian", "Japanese", "Russian"};
	session.language = languages[_res._lang];
	session.level = _currentLevel;
	session.room = _currentRoom;
	session.score = _score;
	session.health = _res._pgeNum ? _pgeLive[0].life : 0;
	session.rewindCount = _rewindLen;
	session.targetLevel = _currentLevel;
	session.targetSkill = _skillLevel;
	session.skill = _skillLevel;
	session.cheats = _cheats;
	session.debugFlags = _stub->_pi.dbgMask;
	session.autoZoomAvailable = _res.isDOS();
	for (int i = 0; i < 7; ++i)
		session.levelAvailable[i] = settingsLevelAvailable(i);
	session.saveSlot = _stateSlot;
	session.outputRate = _stub->getOutputSampleRate();
	session.audioBackend = _settingsAudioBackend;
	session.canSave = _res._pgeNum && _pgeLive[0].life > 0 && !_deathCutsceneCounter;
	session.canRewind = _rewindLen > 0;
	session.apply = [&](const settings::Preferences &p, std::string &reason) {
		const bool applied = applyPreferences(p, reason);
		session.canRewind = _rewindLen > 0;
		session.rewindCount = _rewindLen;
		return applied;
	};
	session.persist = [&](const settings::Preferences &p, std::string &reason) {
		return settings::save(_preferencePath, p, reason);
	};
	session.hasSave = [&](int slot) {
		char name[40];
		makeGameStateName(slot, name, sizeof(name));
		File file;
		return file.open(name, "rb", _savePath);
	};
	session.refreshSaves = [&] { session.saves = saves::scan(_savePath, _currentLevel); };
	session.refreshSaves();
	session.applyCheats = [&](uint32_t cheats, uint8_t flags) {
		_cheats = cheats & (kCheatOneHitKill | kCheatNoHit | kCheatLifeCounter);
		if (!_res.isDOS())
			flags &= ~PlayerInput::DF_AUTOZOOM;
		_stub->_pi.dbgMask = flags & (PlayerInput::DF_FASTMODE | PlayerInput::DF_DBLOCKS |
		                              PlayerInput::DF_SETLIFE | PlayerInput::DF_AUTOZOOM);
	};
	session.command = [&](settings::Action action, int slot, std::string &message) {
		if (action == settings::Action::StartLevel)
			return startSettingsLevel(session.targetLevel, session.targetSkill, message);
		if (action == settings::Action::Screenshot) {
			static unsigned serial = 0;
			const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
			                       std::chrono::system_clock::now().time_since_epoch())
			                       .count();
			const auto path =
			    std::filesystem::u8path(_savePath) /
			    ("flashback-" + std::to_string(stamp) + '-' + std::to_string(++serial) + ".bmp");
			uint8_t palette[768];
			_stub->getPalette(palette, 256);
			const bool saved =
			    saveBMP(path.u8string().c_str(), _vid._frontLayer, palette, _vid._w, _vid._h);
			message = saved ? "Screenshot saved: " + path.u8string()
			                : "Could not write screenshot to the save directory.";
			return saved;
		}
		if (action == settings::Action::Heal) {
			if (!session.canSave) {
				message = "Health can only be restored while Conrad is alive.";
				return false;
			}
			_pgeLive[0].life = std::max<int>(_pgeLive[0].life, _res._pgeInit[0].life);
			session.health = _pgeLive[0].life;
			message = "Health restored.";
			return true;
		}
		if (slot < 0 || slot > 99 ||
		    (action == settings::Action::SaveGame && (!session.canSave || slot == 0))) {
			message = "Choose a manual slot from 1 to 99 while Conrad is alive.";
			return false;
		}
		if (slot > 0)
			_stateSlot = slot;
		if (action == settings::Action::SaveGame) {
			const bool saved = saveGameState(slot);
			if (saved)
				_validSaveState = true;
			if (saved && g_options.play_gamesaved_sound) {
				_mix.play(Resource::_gameSavedSoundData, Resource::_gameSavedSoundLen, 8000,
				          Mixer::MAX_VOLUME);
			}
			message = saved ? "Game saved in slot " + std::to_string(slot) + '.'
			                : "Could not save the game. Check the save directory.";
			if (saved)
				session.refreshSaves();
			return saved;
		}
		if (action == settings::Action::LoadGame) {
			const bool loaded = loadGameState(slot);
			message = loaded ? "Game loaded."
			                 : "Could not load this level/slot. Check the save file and version.";
			return loaded;
		}
		if (action == settings::Action::Rewind) {
			const bool loaded = _rewindLen && loadStateRewind();
			message = loaded ? "Rewind restored." : "No rewind snapshot is available.";
			return loaded;
		}
		return false;
	};
	_stub->showSettings(session);
	if (session.saveSlot > 0 && session.saveSlot <= 99)
		_stateSlot = session.saveSlot;
	const bool quitting = _stub->_pi.quit || session.action == settings::Action::Quit;
	const uint8_t debugFlags = _stub->_pi.dbgMask;
	_stub->_pi = {};
	_stub->_pi.quit = quitting;
	_stub->_pi.dbgMask = debugFlags;
	// The overlay is a blocking pause: neither simulation nor the rewind timer
	// catches up with wall time on return. Menu inputs never leak into gameplay.
	_frameTimestamp = _saveTimestamp = _stub->getTimeStamp();
	_vid.fullRefresh();
	return session.action == settings::Action::Title;
}
