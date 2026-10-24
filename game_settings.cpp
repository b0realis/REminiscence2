// SPDX-License-Identifier: GPL-3.0-or-later
#include "game.h"
#include "file.h"
#include "fs.h"
#include "systemstub.h"
#include "save_catalog.h"
#include "screenshot.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <vector>

void Game::selectMusicSource(const std::string &id) {
	const int index = assets::find(_musicSources, id);
	std::unique_ptr<FileSystem> files;
	if (index >= 0) {
		const auto &source = _musicSources[index];
		files = std::make_unique<FileSystem>(source.root.c_str(), source.excludedRoots);
		_mix.selectMusicSource(files.get(), source.edition);
	} else {
		_mix.selectMusicSource(nullptr, -1);
	}
	_selectedMusicFiles = std::move(files);
}

void Game::configurePreferences(const settings::Preferences &preferences) {
	if (_preferences.pistolStyle != preferences.pistolStyle ||
	    _preferences.laserColor != preferences.laserColor) {
		_weaponFx.clear();
		_vid.fullRefresh(); // Erase any old beam when gameplay resumes.
	}
	_preferences = preferences;
	_vid._conradShirt = preferences.conradShirt;
	_stub->setInputProfileDirectory(_savePath);
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
	if (!_preferences.contextualHints && preferences.contextualHints)
		_hints = {};
	if (_preferences.diskAutosave != preferences.diskAutosave) {
		_persistentDue = true;
		_nextPersistentAttempt = 0;
	}
	const bool newMusic = _preferences.musicSource != preferences.musicSource;
	const bool redrawSides = _widescreenMode != preferences.widescreen;
	const bool redrawShirt = _preferences.conradShirt != preferences.conradShirt;
	if (_autoSave && !preferences.rewind) {
		clearStateRewind();
	}
	_autoSave = preferences.rewind;
	if (_preferences.pistolStyle != preferences.pistolStyle ||
	    _preferences.laserColor != preferences.laserColor) {
		_weaponFx.clear();
		_vid.fullRefresh(); // Erase any old beam when gameplay resumes.
	}
	if (newMusic) {
		selectMusicSource(preferences.musicSource);
		_mix.playMusic(_startupMenu ? 1 : Mixer::MUSIC_TRACK + _gameLevels[_currentLevel].track);
	}
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
	_demoBin = -1;
	_currentLevel = _menu._level = level;
	_skillLevel = _menu._skill = skill;
	_score = 0;
	_journal = {};
	_hints = {};
	_playTicks = _lastPersistentTick = 0;
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

bool Game::handleConfigPanel(bool recovery) {
	settings::Session session;
	session.startup = _startupMenu;
	session.assetSources = _assetSources;
	session.musicSources = _musicSources;
	session.assetIndex = _assetIndex;
	session.experimentalPistolAvailable = _res.isAmiga() || _res.isDOS() || _res.isMac();
	session.recovery = recovery;
	session.browseLevel = _currentLevel;
	session.autosaves = progress::scan(_savePath, _res._type);
	for (int age = 0; !_startupMenu && age < _rewindLen; ++age) {
		const auto &record = _rewindRecords[(_rewindPtr - age + kRewindSize) % kRewindSize];
		session.rewindPoints.push_back({record.preview, unsigned((_playTicks - record.tick) / 30),
		                                record.room, record.health});
	}
	session.journalEntries = _journal.entries();
	session.objectives = journal::objectives(_journal, _currentLevel);
	session.readJournalEntry = [&](unsigned id) { _journal.markRead(id); };
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
	session.health = !_startupMenu && _res._pgeNum ? _pgeLive[0].life : 0;
	session.rewindCount = _startupMenu ? 0 : _rewindLen;
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
	session.lastSaveMs = _lastDiskSaveMs;
	session.worstSaveMs = _worstDiskSaveMs;
	session.diagnostics.push_back("Detected assets: " + session.edition + " / " + session.language);
	session.diagnostics.push_back("Saves and profiles: " + std::string(_savePath));
	session.diagnostics.push_back("Voice source: " +
	                              std::string(_res.fileExists("VOICE.VCE")
	                                              ? "VOICE.VCE found"
												  : "No VOICE.VCE in this asset set (optional)"));
	session.diagnostics.push_back(
	    "Cutscenes: " + std::string(_res._hasSeqData
	                                    ? "PC CD SEQ replacements available"
										: "Original polygon scenes / available edition resources"));
	session.diagnostics.push_back(
	    "State preflight: edition, level, actor scripts, animation and gameplay fingerprint");
	for (int i = 0; i < 7; ++i)
		session.diagnostics.push_back(
		    "Level " + std::to_string(i + 1) + ": " +
		    (session.levelAvailable[i] ? "primary resources found" : "not found in these assets"));
	session.canSave =
	    !_startupMenu && _res._pgeNum && _pgeLive[0].life > 0 && !_deathCutsceneCounter;
	session.canRewind = !_startupMenu && _rewindLen > 0;
	session.apply = [&](const settings::Preferences &p, std::string &reason) {
		const bool applied = applyPreferences(p, reason);
		session.canRewind = !_startupMenu && _rewindLen > 0;
		session.rewindCount = _startupMenu ? 0 : _rewindLen;
		return applied;
	};
	session.persist = [&](const settings::Preferences &p, std::string &reason) {
		return settings::save(_preferencePath, p, reason);
	};
	session.hasSave = [&](int slot) {
		char name[40];
		makeGameStateName(slot, name, sizeof(name));
		File file;
		return file.open(name, "rb", _savePath) ||
		       file.open(saves::name(-1, _currentLevel, slot).c_str(), "rb", _savePath);
	};
	session.refreshSaves = [&] {
		session.saves = saves::scanAll(_savePath, _res._type);
		session.autosaves = progress::scan(_savePath, _res._type);
		session.autosaveIndex = std::min<int>(session.autosaveIndex,
		                                      std::max<int>(0, int(session.autosaves.size()) - 1));
	};
	session.refreshSaves();
	for (const auto &info : session.saves)
		if (info.level == session.browseLevel && info.slot == session.saveSlot) {
			snprintf(session.saveName.data(), session.saveName.size(), "%s", info.label.c_str());
			session.protectSave = info.protectedSave;
		}
	session.applyCheats = [&](uint32_t cheats, uint8_t flags) {
		_cheats = cheats & (kCheatOneHitKill | kCheatNoHit | kCheatLifeCounter);
		if (!_res.isDOS())
			flags &= ~PlayerInput::DF_AUTOZOOM;
		_stub->_pi.dbgMask = flags & (PlayerInput::DF_FASTMODE | PlayerInput::DF_DBLOCKS |
		                              PlayerInput::DF_SETLIFE | PlayerInput::DF_AUTOZOOM);
	};
	session.command = [&](settings::Action action, int slot, std::string &message) {
		if (action == settings::Action::SwitchAssets) {
			if (session.assetIndex < 0 || size_t(session.assetIndex) >= _assetSources.size()) {
				message = "Choose an available edition.";
				return false;
			}
			_requestedAsset = session.assetIndex;
			_preferences.assetSet = _assetSources[_requestedAsset].id;
			message = "Switching edition.";
			return true;
		}
		if (action == settings::Action::ContinueLatest)
			return continueLatest(message);
		if (action == settings::Action::LoadAutosave) {
			if (session.autosaveIndex < 0 ||
			    size_t(session.autosaveIndex) >= session.autosaves.size()) {
				message = "No autosave selected.";
				return false;
			}
			return loadPersistent(session.autosaves[session.autosaveIndex], message);
		}
		if (action == settings::Action::Rewind) {
			const bool loaded = loadStateRewind(session.rewindIndex);
			if (loaded)
				refreshRestoredFrame();
			message = loaded ? "Rewind restored." : "No rewind snapshot is available.";
			return loaded;
		}
		if (action == settings::Action::StartLevel) {
			const bool loaded =
			    startSettingsLevel(session.targetLevel, session.targetSkill, message);
			if (loaded && _startupMenu)
				_continued = true;
			return loaded;
		}
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
		if (action == settings::Action::UpdateSaveDetails) {
			const bool saved =
			    progress::updateManual(_savePath, _res._type, session.browseLevel, slot,
				                       session.saveName.data(), session.protectSave, message);
			if (saved) {
				message = "Save name and protection updated.";
				session.refreshSaves();
			}
			return saved;
		}
		if (slot < 0 || slot > 99 ||
		    (action == settings::Action::SaveGame && (!session.canSave || slot == 0))) {
			message = "Choose a manual slot from 1 to 99 while Conrad is alive.";
			return false;
		}
		if (slot > 0)
			_stateSlot = slot;
		if (action == settings::Action::SaveGame) {
			if (session.browseLevel != _currentLevel) {
				message =
				    "Choose the current level to save; other levels can be browsed and loaded.";
				return false;
			}
			const bool saved =
			    saveManualState(slot, session.saveName.data(), session.protectSave, message);
			_stub->notify(saved ? "Game saved." : message, !saved);
			if (saved && g_options.play_gamesaved_sound) {
				_mix.play(Resource::_gameSavedSoundData, Resource::_gameSavedSoundLen, 8000,
				          Mixer::MAX_VOLUME);
			}
			if (saved)
				message = "Game saved in slot " + std::to_string(slot) + '.';
			else if (message.empty())
				message = "Could not save the game. Check the save directory.";
			if (saved)
				session.refreshSaves();
			session.lastSaveMs = _lastDiskSaveMs;
			session.worstSaveMs = _worstDiskSaveMs;
			return saved;
		}
		if (action == settings::Action::LoadGame) {
			const bool loaded = loadManualState(session.browseLevel, slot, message);
			if (loaded && _startupMenu)
				_continued = true;
			_stub->notify(message, !loaded);
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
