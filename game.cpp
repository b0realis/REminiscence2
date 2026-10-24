#include "atomic_file.h"
#include <filesystem>

/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <time.h>
#include "decode_mac.h"
#include "decode_sega.h"
#include "edition.h"
#include "file.h"
#include "fs.h"
#include "game.h"
#include "save_catalog.h"
#include "gameplay_identity.h"
#include "screenshot.h"
#include "seq_player.h"
#include "systemstub.h"
#include "util.h"

Game::Game(SystemStub *stub, FileSystem *fs, FileSystem *tuneFs, const char *savePath, int level, ResourceType ver, Language lang, WidescreenMode widescreenMode, bool autoSave, uint32_t cheats)
	: _cut(&_res, stub, &_vid), _menu(&_res, stub, &_vid),
	_mix(tuneFs, stub), _res(fs, ver, lang), _seq(stub, &_mix), _vid(&_res, stub, widescreenMode),
	_stub(stub), _fs(fs), _savePath(savePath) {
	_stateSlot = 1;
	_inp_demPos = 0;
	_skillLevel = _menu._skill = kSkillNormal;
	_currentLevel = _menu._level = level;
	_demoBin = -1;
	_currentRoom = 0;
	_score = 0;
	_deathCutsceneCounter = 0;
	_vid._unkPalSlot1 = _vid._unkPalSlot2 = 0;
	_widescreenMode = widescreenMode;
	_autoSave = autoSave;
	_rewindPtr = -1;
	_rewindLen = 0;
	_cheats = cheats;
	_preferences.readCompatibility(g_options);
	_vid._conradShirt = _preferences.conradShirt;
	_preferences.rewind = autoSave;
	_preferences.widescreen = widescreenMode;
}

void Game::initCutsceneResources() {
	_res.init();
	_res.load_TEXT();

	switch (_res._type) {
	case kResourceTypeSega:
		_res.load("FONT8", Resource::OT_FNT);
		break;
	case kResourceTypeAmiga:
		_res.load("FONT8", Resource::OT_FNT, "SPR");
		if (_res._isDemo) {
			_cut._patchedOffsetsTable = Cutscene::_amigaDemoOffsetsTable;
		}
		break;
	case kResourceTypeDOS:
		_res.load("FB_TXT", Resource::OT_FNT);
		if (g_options.use_seq_cutscenes) {
			_res._hasSeqData = _fs->exists("INTRO.SEQ");
		}
		if (_fs->exists("logosssi.cmd")) {
			_cut._patchedOffsetsTable = Cutscene::_ssiOffsetsTable;
		}
		break;
	case kResourceTypeMac:
		_res.MAC_loadClutData();
		_res.MAC_loadFontData();
		break;
	case kResourceTypePC98:
		// use _font8Jp
		break;
	}
}

void Game::run() {
	_randSeed = time(0);
	initCutsceneResources();

	if (!g_options.bypass_protection && !g_options.use_words_protection &&
	    editionTraits(_res._type).shapeProtection) {
		while (!handleProtectionScreenShape()) {
			if (_stub->_pi.quit) {
				return;
			}
		}
	}

	_mix.init();
	_mix._mod._isAmiga = _res.isAmiga();


	// global resources
	switch (_res._type) {
	case kResourceTypeAmiga:
		_res.load("ICONE", Resource::OT_ICN, "SPR");
		_res.load("ICON", Resource::OT_ICN, "SPR");
		_res.load("PERSO", Resource::OT_SPM);
		break;
	case kResourceTypeDOS:
	case kResourceTypePC98:
		_res.load("GLOBAL", Resource::OT_ICN);
		_res.load("GLOBAL", Resource::OT_SPC);
		_res.load("PERSO", Resource::OT_SPR);
		_res.load_SPR_OFF("PERSO", _res._spr1);
		break;
	case kResourceTypeSega:
		_res.load("GLOBAL", Resource::OT_ANI);
		_res.load("GLOBAL", Resource::OT_ICN);
		_res.load("GLOBAL", Resource::OT_OBJ);
		_res.load("GLOBAL", Resource::OT_SPC);
		_res.load("GLOBAL", Resource::OT_SPR);
		_res.load_SPR_OFF("GLOBAL", _res._spr1, "TAB");
		break;
	case kResourceTypeMac:
		_res.MAC_loadIconData();
		_res.MAC_loadPersoData();
		break;
	}

	// sound resources
	switch (_res._type) {
	case kResourceTypeSega:
	case kResourceTypeAmiga:
		// sounds are loaded per level
		break;
	case kResourceTypeDOS:
		_res.load_FIB("GLOBAL");
		break;
	case kResourceTypePC98:
		_res.PC98_loadSounds();
		break;
	case kResourceTypeMac:
		_res.MAC_loadSounds();
		break;
	}

	if (!g_options.bypass_protection && g_options.use_words_protection && _res.isDOS()) {
		while (!handleProtectionScreenWords()) {
			if (_stub->_pi.quit) {
				return;
			}
		}
	}

	bool resumeLoaded = false;
	if (_continueRequested) {
		std::string reason;
		resumeLoaded = continueLatest(reason);
		if (!resumeLoaded) {
			warning("Continue: %s", reason.c_str());
			_continueFailed = true;
			_stub->_pi.quit = true;
		}
	} else if (_preferences.quickResume) {
		std::string reason;
		resumeLoaded = resumeLatestSave(reason);
	}
	// Default startup presents the original opening; opted-in resume bypasses it.
	// Esc unwinds the player first: a menu load must never replace resources while
	// a cutscene decoder still holds pointers into them.
	bool openingMenuRequested = false;
	_cut._interruptForMenu = _seq._interruptForMenu = [&] {
		if (_stub->_pi.escape || _stub->consumePauseRequest()) {
			_stub->_pi.escape = false;
			openingMenuRequested = true;
		}
		return openingMenuRequested;
	};
	if (!resumeLoaded && !_stub->_pi.quit) {
		if (_res.isMac()) {
			displayTitleScreenMac(Menu::kMacTitleScreen_MacPlay);
			if (!_stub->_pi.quit && !openingMenuRequested)
				displayTitleScreenMac(Menu::kMacTitleScreen_Presage);
		}
		for (int scene : {0x40, 0x0D}) {
			if (_stub->_pi.quit || openingMenuRequested)
				break;
			playCutscene(scene);
		}
	}
	_cut._interruptForMenu = _seq._interruptForMenu = {};

	if (openingMenuRequested && !_stub->_pi.quit) {
		_startupMenu = true;
		_continued = false;
		handleConfigPanel();
		_startupMenu = false;
		resumeLoaded = _continued;
	}

	bool presentMenu =
	    !_res.isSega() && ((_res._type != kResourceTypeDOS) || _res.fileExists("MENU1.MAP"));
	while (!_stub->_pi.quit && _requestedAsset < 0) {
		if (!resumeLoaded && _stub->hasWidescreen()) {
			_stub->clearWidescreen();
		}
		if (presentMenu && !resumeLoaded) {
			_mix.playMusic(1);
			_startupMenu = true;
			if (_res.isAmiga()) {
				displayTitleScreenAmiga();
				_stub->setScreenSize(_vid._w, _vid._h);
			} else {
				_menu.handleTitleScreen();
			}
			_startupMenu = false;
			if (_menu._selectedOption == Menu::MENU_OPTION_ITEM_QUIT || _stub->_pi.quit) {
				_stub->_pi.quit = true;
				break;
			}
			if (_menu._selectedOption == Menu::MENU_OPTION_ITEM_SETTINGS) {
				_mix.stopMusic();
				_startupMenu = true;
				_continued = false;
				handleConfigPanel();
				_startupMenu = false;
				resumeLoaded = _continued;
				if (!resumeLoaded)
					continue; // Closing settings returns to the original title.
			} else if (_menu._selectedOption == Menu::MENU_OPTION_ITEM_DEMO) {
				_demoBin = (_demoBin + 1) % ARRAYSIZE(_demoInputs);
				const char *fn = _demoInputs[_demoBin].name;
				debug(DBG_DEMO, "Loading inputs from '%s'", fn);
				_res.load_DEM(fn);
				if (_res._demLen == 0)
					continue;
				_skillLevel = kSkillNormal;
				_currentLevel = _demoInputs[_demoBin].level;
				_randSeed = 0;
				if (_res.isMac()) {
					static const int ranges[] = {19, 11, 17};
					_demoRandCounter = 0;
					_demoRandRange = ranges[_demoBin];
				}
			} else {
				_demoBin = -1;
				_skillLevel = _menu._skill;
				_currentLevel = _menu._level;
			}
			_mix.stopMusic();
		}
		if (_stub->_pi.quit || _requestedAsset >= 0) {
			break;
		}
		if (!resumeLoaded && _stub->hasWidescreen()) {
			_stub->clearWidescreen();
		}
		if (_currentLevel == 7) {
			_vid.fadeOut();
			_vid.setTextPalette();
			playCutscene(0x3D);
		} else {
			_vid.setTextPalette();
			_vid.setPalette0xF();
			_stub->setOverscanColor(0xE0);
			if (!resumeLoaded) {
				_vid._unkPalSlot1 = 0;
				_vid._unkPalSlot2 = 0;
				_score = 0;
				_journal = {};
				_hints = {};
				_playTicks = _lastPersistentTick = 0;
				clearStateRewind();
				loadLevelData();
				resetGameState();
			}
			resumeLoaded = false;
			_endLoop = false;
			_frameTimestamp = _stub->getTimeStamp();
			_saveTimestamp = _frameTimestamp;
			while (!_stub->_pi.quit && !_endLoop && _requestedAsset < 0) {
				mainLoop();
				if (_demoBin != -1 && _inp_demPos >= _res._demLen) {
					debug(DBG_DEMO, "End of demo");
					// exit level
					_endLoop = true;
				}
			}
			if (_preferences.diskAutosave && _activeProgress && _playTicks != _lastPersistentTick) {
				std::string reason;
				if (!savePersistent(reason))
					warning("Exit autosave: %s", reason.c_str());
			}
			_activeProgress = false;
			// flush inputs
			_stub->_pi.dirMask = 0;
			_stub->_pi.enter = false;
			_stub->_pi.space = false;
			_stub->_pi.shift = false;
		}
	}

	_res.free_TEXT();
	_mix.free();
	_res.fini();
}

void Game::displayTitleScreenAmiga() {
	static const char *FILENAME = "present.cmp";
	_res.load_CMP_menu(FILENAME);
	static const int kW = 320;
	static const int kH = 224;
	uint8_t *buf = (uint8_t *)calloc(1, kW * kH);
	if (!buf) {
		error("Failed to allocate screen buffer w=%d h=%d", kW, kH);
	}
	static const uint16_t kAmigaColors[] = {
		0x000, 0x123, 0x012, 0x134, 0x433, 0x453, 0x046, 0x245,
		0x751, 0x455, 0x665, 0x268, 0x961, 0x478, 0x677, 0x786,
		0x17B, 0x788, 0xB84, 0xC92, 0x49C, 0xF00, 0x9A8, 0x9AA,
		0xCA7, 0xEA3, 0x8BD, 0xBBB, 0xEC7, 0xBCD, 0xDDB, 0xEED
	};
	for (int i = 0; i < 32; ++i) {
		Color c = Video::AMIGA_convertColor(kAmigaColors[i]);
		_stub->setPaletteEntry(i, &c);
	}
	_vid.setTextPalette();
	_stub->setScreenSize(kW, kH);
	// fill with black
	_stub->copyRect(0, 0, kW, kH, buf, kW);
	_stub->updateScreen(0);
	_vid.AMIGA_decodeCmp(_res._scratchBuffer + 6, buf);
	// Preserve the original 320-pixel title artwork. Level browsing belongs in
	// the QoL overlay; arriving here never implicitly chooses a level.
	const std::vector<uint8_t> background(buf, buf + kW * kH);
	_menu._selectedOption = -1;
	int selected = 0;
	int h = 0;
	while (!_stub->_pi.quit) {
		_stub->processEvents();
		if (_stub->_pi.escape || _stub->consumePauseRequest()) {
			_stub->_pi.escape = false;
			_menu._selectedOption = Menu::MENU_OPTION_ITEM_SETTINGS;
			break;
		}
		if (_stub->_pi.dirMask & (PlayerInput::DIR_UP | PlayerInput::DIR_DOWN)) {
			selected = (selected + ((_stub->_pi.dirMask & PlayerInput::DIR_UP) ? 3 : 1)) % 4;
			_stub->_pi.dirMask &= ~(PlayerInput::DIR_UP | PlayerInput::DIR_DOWN);
		}
		const bool changeSkill =
		    selected == 1 &&
		    (_stub->_pi.enter ||
			 (_stub->_pi.dirMask & (PlayerInput::DIR_LEFT | PlayerInput::DIR_RIGHT)));
		if (changeSkill) {
			_menu._skill =
			    (_menu._skill + ((_stub->_pi.dirMask & PlayerInput::DIR_LEFT) ? 2 : 1)) % 3;
			_stub->_pi.enter = false;
			_stub->_pi.dirMask &= ~(PlayerInput::DIR_LEFT | PlayerInput::DIR_RIGHT);
		} else if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			_menu._selectedOption = selected == 0   ? Menu::MENU_OPTION_ITEM_START
			                        : selected == 2 ? Menu::MENU_OPTION_ITEM_SETTINGS
			                                        : Menu::MENU_OPTION_ITEM_QUIT;
			break;
		}
		std::copy(background.begin(), background.end(), buf);
		static const char *skills[] = {"EASY", "NORMAL", "EXPERT"};
		const std::string skill = std::string("SKILL: ") + skills[_menu._skill];
		const char *items[] = {"START", skill.c_str(), "OPTIONS (ESC)", "QUIT"};
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; items[i][j]; ++j) {
				_vid.AMIGA_drawStringChar(buf, kW, 200 + j * Video::CHAR_W, 104 + i * 24, _res._fnt,
				                          i == selected ? 0xE4 : 0xE8, items[i][j]);
			}
		}
		h = std::min(h + 2, kH / 2);
		_stub->copyRect(0, kH / 2 - h, kW, h * 2, buf, kW);
		_stub->updateScreen(0);
		_stub->sleep(30);
	}
	free(buf);
}

void Game::displayTitleScreenMac(int num) {
	CutsceneInputScope input(_stub, _preferences.skipLogosWithKeyboard);
	_menu.drawMacTitle(num);
	_vid.updateScreen();
	while (!_stub->_pi.quit) {
		_stub->processEvents();
		if ((_cut._interruptForMenu && _cut._interruptForMenu()) || _stub->consumeCutsceneSkip() ||
		    _stub->_pi.enter || _stub->_pi.escape) {
			_stub->_pi.enter = _stub->_pi.escape = false;
			break;
		}
		_stub->sleep(30);
	}
}

void Game::resetGameState() {
	_weaponFx.clear();
	_animBuffers._states[0] = _animBuffer0State;
	_animBuffers._curPos[0] = 0xFF;
	_animBuffers._states[1] = _animBuffer1State;
	_animBuffers._curPos[1] = 0xFF;
	_animBuffers._states[2] = _animBuffer2State;
	_animBuffers._curPos[2] = 0xFF;
	_animBuffers._states[3] = _animBuffer3State;
	_animBuffers._curPos[3] = 0xFF;
	_currentRoom = _res._pgeInit[0].init_room;
	_cut._deathCutsceneId = 0xFFFF;
	_deathCutsceneCounter = 0;
	_saveStateCompleted = false;
	_loadMap = true;
	pge_resetMessages();
	_blinkingConradCounter = 0;
	_pge_processOBJ = false;
	_pge_opGunVar = 0;
	_textToDisplay = 0xFFFF;
	_pge_zoomPiegeNum = 0;
	_pge_zoomCounter = 0;
	_pge_zoomX = _pge_zoomY = 0;
}

void Game::mainLoop() {
	_stub->setGameplayActive(false);
	playCutscene();
	if (_cut._id == 0x3D) {
		showFinalScore();
		_endLoop = true;
		return;
	}
	if (_deathCutsceneCounter) {
		--_deathCutsceneCounter;
		if (_deathCutsceneCounter == 0) {
			playCutscene(_cut._deathCutsceneId);
			// Recovery is a blocking QoL screen. Loading is validated before any live
			// state changes; closing it requires a recovery choice or leaving play.
			if (handleConfigPanel(true))
				_endLoop = true;
			return;
		}
	}
	_stub->setGameplayActive(true);
	if (_demoBin == -1)
		_journal.chapter(_currentLevel);
	_stub->setJournalNotice(_preferences.storyJournal && _preferences.journalNotifications &&
	                        _journal.unread());
	pge_getInput();
	if (_stub->consumePauseRequest()) {
		_stub->setGameplayActive(false);
		if (handleConfigPanel())
			_endLoop = true;
		return;
	}
	_weaponFx.tick();
	// Preserve the composed frame when a device event opens the pause overlay.
	memcpy(_vid._frontLayer, _vid._backLayer, _vid._layerSize);
	pge_prepare();
	col_prepareRoomState();
	uint8_t oldLevel = _currentLevel;
	for (uint16_t i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = _pge_liveTable2[i];
		if (pge) {
			_col_currentPiegeGridPosY = (pge->pos_y / 36) & ~1;
			_col_currentPiegeGridPosX = (pge->pos_x + 8) >> 4;
			const int previousFrame = pge->anim_number;
			pge_process(pge);
			updateConradShot(pge, previousFrame);
		}
	}
	if (oldLevel != _currentLevel) {
		if (_res._isDemo) {
			_currentLevel = oldLevel;
		}
		changeLevel();
		_pge_opGunVar = 0;
		return;
	}
	if (_cut._id != 0xFFFF) {
		_weaponFx.clear();
		// A queued cutscene owns the next presentation; do not reveal its destination room
		return;
	}
	if (_loadMap) {
		_weaponFx.clear();
		const uint8_t player_room = _pgeLive[0].room_location;
		if (_currentRoom == 0xFF || !hasLevelMap(_currentLevel, player_room)) {
			if (_demoBin != -1) {
				// Demo playback may start in a room that has no visual map (e.g. DOS
				// LEVEL3 room 39). Keep the simulation running so the player falls or
				// wraps into a valid room instead of triggering an immediate death cutscene.
				_currentRoom = player_room;
				_loadMap = false;
			} else {
				_cut._id = 6;
				_deathCutsceneCounter = 1;
			}
		} else {
			_currentRoom = player_room;
			loadLevelMap();
			_vid.fullRefresh();
			_loadMap = false;
		}
	}
	if (_res.isDOS() && (_stub->_pi.dbgMask & PlayerInput::DF_AUTOZOOM) != 0) {
		pge_updateZoom();
	}
	prepareAnims();
	drawAnims();
	drawCurrentInventoryItem();
	drawLevelTexts();
	if (g_options.enable_password_menu) {
		printLevelCode();
	}
	if (_blinkingConradCounter != 0) {
		--_blinkingConradCounter;
	}
	updateProgress();
	_vid.updateScreen();
	updateTiming();
	_stub->setGameplayActive(false);
	const bool dialogue = _textToDisplay != 0xFFFF;
	drawStoryTexts();
	if (dialogue)
		_frameTimestamp = _stub->getTimeStamp();
	_stub->setGameplayActive(true);
	if (_stub->_pi.backspace) {
		_stub->_pi.backspace = false;
		_stub->setGameplayActive(false);
		handleInventory();
		_frameTimestamp = _stub->getTimeStamp();
		return;
	}
	if (_stub->_pi.escape) {
		_stub->_pi.escape = false;
		_stub->setGameplayActive(false);
		if (_demoBin != -1 || handleConfigPanel()) {
			_endLoop = true;
			return;
		}
	}
	inp_handleSpecialKeys();
}

void Game::updateTiming() {
	// Carry 1/30-second fractions and scheduler overshoot instead of rounding
	// every frame to 33 ms. Reset at explicit pause/level boundaries or long stalls.
	if (_frameTimestamp != _timingExpected)
		_timingRemainder = 0;
	const bool fast = _stub->_pi.dbgMask & PlayerInput::DF_FASTMODE;
	const unsigned rate = 30 * _preferences.gameSpeed;
	const unsigned numerator = 100000 + _timingRemainder;
	const unsigned period = fast ? 20 : numerator / rate;
	if (!fast)
		_timingRemainder = numerator % rate;
	const uint32_t elapsed = _stub->getTimeStamp() - _frameTimestamp;
	if (elapsed < period)
		_stub->sleep(period - elapsed);
	_frameTimestamp = elapsed > period * 2 ? _stub->getTimeStamp() : _frameTimestamp + period;
	_timingExpected = _frameTimestamp;
}

void Game::playCutscene(int id) {
	if (id != -1) {
		_cut._id = id;
	}
	if (_cut._id != 0xFFFF) {
		const bool skipWithKeyboard = _cut._id == 0x40 ? _preferences.skipLogosWithKeyboard
		                                               : _preferences.skipCutscenesWithKeyboard;
		_cut._skipWithKeyboard = _seq._skipWithKeyboard = skipWithKeyboard;
		ToggleWidescreenStack tws(_stub, false);
		_mix.stopMusic();
		if (_res._hasSeqData && _cut._id != 0x40) {
			int num = 0;
			switch (_cut._id) {
			case 0x02: {
					static const uint8_t tab[] = { 1, 2, 1, 3, 3, 4, 4 };
					num = tab[_currentLevel];
				}
				break;
			case 0x05: {
					static const uint8_t tab[] = { 1, 2, 3, 5, 5, 4, 4 };
					num = tab[_currentLevel];
				}
				break;
			case 0x0A: {
					static const uint8_t tab[] = { 1, 2, 2, 2, 2, 2, 2 };
					num = tab[_currentLevel];
				}
				break;
			case 0x10: {
					static const uint8_t tab[] = { 1, 1, 1, 2, 2, 3, 3 };
					num = tab[_currentLevel];
				}
				break;
			case 0x3C: {
					static const uint8_t tab[] = { 1, 1, 1, 1, 1, 2, 2 };
					num = tab[_currentLevel];
				}
				break;
			case 0x40:
				return;
			case 0x4A:
				return;
			}
			if (SeqPlayer::_namesTable[_cut._id]) {
			        char name[16];
			        snprintf(name, sizeof(name), "%s.SEQ", SeqPlayer::_namesTable[_cut._id]);
				char *p = strchr(name, '0');
				if (p) {
					*p += num;
				}
			        if (playCutsceneSeq(name)) {
					    if (_demoBin == -1 && _cut._id == Cutscene::kCineMemo && _seq._completed)
						    _journal.memory();
					    if (_cut._id == 0x3D) {
						    playCutsceneSeq("CREDITS.SEQ");
						    _cut._interrupted = false;
					    } else {
						    _cut._id = 0xFFFF;
					    }
					_mix.stopMusic();
					return;
				}
			}
		}
		_mix.playCutsceneMusic(_cut._id, _res.isAmiga());
		const auto playedId = _cut._id;
		_cut.play();
		if (_demoBin == -1 && playedId == Cutscene::kCineMemo && _cut._completed)
			_journal.memory();
		if (id == 0xD && !_cut._interrupted) {
			if (editionTraits(_res._type).extendedIntroduction) {
				_cut._id = 0x4A; // second part of the introduction cutscene
				_cut.play();
			}
		}
		if (_res.isMac() && !(id == 0x48 || id == 0x49)) { // continue or score screens
			// restore palette entries modified by the cutscene player (0xC and 0xD)
			Color palette[32];
			_res.MAC_copyClut16(palette, 0, 0x37);
			_res.MAC_copyClut16(palette, 1, 0x38);
			for (int i = 0; i < 32; ++i) {
				_stub->setPaletteEntry(0xC0 + i, &palette[i]);
			}
		}
		if (_cut._id == 0x3D) {
			_mix.playMusic(Mixer::MUSIC_TRACK + 9);
			_cut.playCredits();
		}
		_mix.stopMusic();
	}
}

bool Game::playCutsceneSeq(const char *name) {
	File f;
	if (f.open(name, "rb", _fs)) {
		_seq.setBackBuffer(_res._scratchBuffer);
		const bool played = _seq.play(&f);
		_vid.fullRefresh();
		return played;
	}
	return false;
}

void Game::inp_handleSpecialKeys() {
	if (_stub->_pi.dbgMask & PlayerInput::DF_SETLIFE) {
		_pgeLive[0].life = 0x7FFF;
	}
	if (_stub->_pi.load) {
		loadGameState(_stateSlot);
		_stub->_pi.load = false;
	}
	if (_stub->_pi.save) {
		const bool canSave = _pgeLive[0].life > 0 && !_deathCutsceneCounter;
		// saveGameState reports storage/protection failures with their specific reason.
		const bool saved = canSave && saveGameState(_stateSlot);
		if (saved && g_options.play_gamesaved_sound) {
			_mix.play(Resource::_gameSavedSoundData, Resource::_gameSavedSoundLen, 8000, Mixer::MAX_VOLUME);
		}
		if (!canSave)
			_stub->notify("Save failed: Conrad must be alive.", true);
		_stub->_pi.save = false;
	}
	if (_stub->_pi.stateSlot != 0) {
		int8_t slot = _stateSlot + _stub->_pi.stateSlot;
		if (slot >= 1 && slot < 100) {
			_stateSlot = slot;
			info("Current game state slot is %d", _stateSlot);
		}
		_stub->_pi.stateSlot = 0;
	}
	if (_stub->_pi.rewind) {
		if (_rewindLen != 0) {
			if (loadStateRewind()) {
				// Restore room/collision caches before the next simulation tick.
				refreshRestoredFrame();
				_stub->notify("Rewind restored.");
			} else {
				_stub->notify("Could not restore the rewind snapshot.", true);
			}
		} else {
			info("Rewind buffer is empty");
			_stub->notify("No rewind snapshots yet.", true);
		}
		_stub->_pi.rewind = false;
	}
}

void Game::drawCurrentInventoryItem() {
	uint16_t src = _pgeLive[0].current_inventory_PGE;
	if (src != 0xFF) {
		_currentIcon = _res._pgeInit[src].icon_num;
		drawIcon(_currentIcon, 232, 8, 0xA);
	}
}

void Game::showFinalScore() {
	if (_stub->hasWidescreen()) {
		_stub->clearWidescreen();
	}
	playCutscene(0x49);
	char buf[50];
	snprintf(buf, sizeof(buf), "SCORE %08u", _score);
	_vid.drawString(buf, (Video::GAMESCREEN_W - strlen(buf) * Video::CHAR_W) / 2, 40, 0xE5);
	const char *str = _menu.getLevelPassword(7, _skillLevel);
	_vid.drawString(str, (Video::GAMESCREEN_W - strlen(str) * Video::CHAR_W) / 2, 16, 0xE7);
	while (!_stub->_pi.quit) {
		_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
		_stub->updateScreen(0);
		_stub->processEvents();
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			break;
		}
		_stub->sleep(100);
	}
}

bool Game::handleContinueAbort() {
	if (_stub->hasWidescreen()) {
		_stub->clearWidescreen();
	}
	playCutscene(0x48);
	int timeout = 100;
	int current_color = 0;
	uint8_t colors[] = { 0xE4, 0xE5 };
	uint8_t color_inc = 0xFF;
	Color col;
	_stub->getPaletteEntry(0xE4, &col);
	memcpy(_vid._tempLayer, _vid._frontLayer, _vid._layerSize);
	while (timeout >= 0 && !_stub->_pi.quit) {
		const char *str;
		str = _res.getMenuString(LocaleData::LI_01_CONTINUE_OR_ABORT);
		_vid.drawString(str, (Video::GAMESCREEN_W - strlen(str) * Video::CHAR_W) / 2, 64, 0xE3);
		str = _res.getMenuString(LocaleData::LI_02_TIME);
		char buf[50];
		snprintf(buf, sizeof(buf), "%s : %d", str, timeout / 10);
		_vid.drawString(buf, 96, 88, 0xE3);
		str = _res.getMenuString(LocaleData::LI_03_CONTINUE);
		_vid.drawString(str, (Video::GAMESCREEN_W - strlen(str) * Video::CHAR_W) / 2, 104, colors[0]);
		str = _res.getMenuString(LocaleData::LI_04_ABORT);
		_vid.drawString(str, (Video::GAMESCREEN_W - strlen(str) * Video::CHAR_W) / 2, 112, colors[1]);
		snprintf(buf, sizeof(buf), "SCORE  %08u", _score);
		_vid.drawString(buf, 64, 154, 0xE3);
		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			if (current_color > 0) {
				SWAP(colors[current_color], colors[current_color - 1]);
				--current_color;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			if (current_color < 1) {
				SWAP(colors[current_color], colors[current_color + 1]);
				++current_color;
			}
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			return (current_color == 0);
		}
		_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
		_stub->updateScreen(0);
		static const int COLOR_STEP = 8;
		static const int COLOR_MIN = 16;
		static const int COLOR_MAX = 256 - 16;
		if (col.b >= COLOR_MAX) {
			color_inc = 0;
		} else if (col.b < COLOR_MIN) {
			color_inc = 0xFF;
		}
		if (color_inc == 0xFF) {
			col.b += COLOR_STEP;
			col.g += COLOR_STEP;
		} else {
			col.b -= COLOR_STEP;
			col.g -= COLOR_STEP;
		}
		_stub->setPaletteEntry(0xE4, &col);
		_stub->processEvents();
		_stub->sleep(100);
		--timeout;
		memcpy(_vid._frontLayer, _vid._tempLayer, _vid._layerSize);
	}
	return false;
}

void Game::printLevelCode() {
	if (_printLevelCodeCounter != 0) {
		--_printLevelCodeCounter;
		if (_printLevelCodeCounter != 0) {
			char buf[32];
			snprintf(buf, sizeof(buf), "CODE: %s", _menu.getLevelPassword(_currentLevel, _skillLevel));
			_vid.drawString(buf, (Video::GAMESCREEN_W - strlen(buf) * Video::CHAR_W) / 2, 16, 0xE7);
		}
	}
}

void Game::printSaveStateCompleted() {
	if (_saveStateCompleted) {
		const char *str = _res.getMenuString(LocaleData::LI_05_COMPLETED);
		_vid.drawString(str, (176 - strlen(str) * Video::CHAR_W) / 2, 34, 0xE6);
	}
}

void Game::drawLevelTexts() {
	LivePGE *pge = &_pgeLive[0];
	int8_t obj = col_findCurrentCollidingObject(pge, 3, 0xFF, 0xFF, &pge);
	if (obj == 0) {
		obj = col_findCurrentCollidingObject(pge, 0xFF, 5, 9, &pge);
	}
	_nearInteraction = obj > 0;
	if (obj > 0) {
		_printLevelCodeCounter = 0;
		if (_textToDisplay == 0xFFFF) {
			uint8_t icon_num = obj - 1;
			drawIcon(icon_num, 80, 8, 0xA);
			uint8_t txt_num = pge->init_PGE->text_num;
			const uint8_t *str = _res.getTextString(_currentLevel, txt_num);
			drawString(str, 176, 26, 0xE6, true);
			if (icon_num == 2) {
				printSaveStateCompleted();
				return;
			}
		} else {
			_currentInventoryIconNum = obj - 1;
		}
	}
	_saveStateCompleted = false;
}

static int getLineLength(const uint8_t *str) {
	int len = 0;
	while (*str && *str != 0xB && *str != 0xA) {
		++str;
		++len;
	}
	return len;
}

void Game::drawStoryTexts() {
	if (_textToDisplay != 0xFFFF) {
		uint8_t textColor = 0xE8;
		const uint8_t *str = _res.getGameString(_textToDisplay);
		memcpy(_vid._tempLayer, _vid._frontLayer, _vid._layerSize);
		int textSpeechSegment = 0;
		int textSegmentsCount = 0;
		while (!_stub->_pi.quit) {
			drawIcon(_currentInventoryIconNum, 80, 8, 0xA);
			int yPos = 26;
			if (_res._type == kResourceTypeMac) {
				if (textSegmentsCount == 0) {
					textSegmentsCount = *str++;
				}
				int len = *str++;
				if (len >= 2 && (*str == '@' || *str == '~')) {
					switch (str[1]) {
					case '1':
						textColor = 0xE9;
						break;
					case '2':
						textColor = 0xEB;
						break;
					default:
						warning("Unhandled MAC text color code 0x%x", str[1]);
						break;
					}
					str += 2;
					len -= 2;
				}
				for (; len > 0; yPos += 8) {
					const uint8_t *next = (const uint8_t *)memchr(str, 0x7C, len);
					if (!next) {
						_vid.drawStringLen((const char *)str, len, (176 - len * Video::CHAR_W) / 2, yPos, textColor);
						// point 'str' to beginning of next text segment
						str += len;
						break;
					}
					const int lineLength = next - str;
					_vid.drawStringLen((const char *)str, lineLength, (176 - lineLength * Video::CHAR_W) / 2, yPos, textColor);
					str = next + 1;
					len -= lineLength + 1;
				}
			} else {
				if (*str == 0xFF) {
					if (_res._lang == LANG_JP) {
						switch (str[1]) {
						case 0:
							textColor = 0xE9;
							break;
						case 1:
							textColor = 0xEB;
							break;
						default:
							warning("Unhandled JP text color code 0x%x", str[1]);
							break;
						}
						str += 2;
					} else {
						textColor = str[1];
						// str[2] is an unused color (possibly the shadow)
						str += 3;
					}
				}
				while (1) {
					const int len = getLineLength(str);
					str = (const uint8_t *)_vid.drawString((const char *)str, (176 - len * Video::CHAR_W) / 2, yPos, textColor);
					if (*str == 0 || *str == 0xB) {
						break;
					}
					++str;
					yPos += 8;
				}
			}
			uint8_t *voiceSegmentData = 0;
			uint32_t voiceSegmentLen = 0;
			_res.load_VCE(_textToDisplay, textSpeechSegment++, &voiceSegmentData, &voiceSegmentLen);
			if (voiceSegmentData) {
				_mix.play(voiceSegmentData, voiceSegmentLen, 32000, Mixer::MAX_VOLUME);
			}
			_vid.updateScreen();
			while (!_stub->_pi.backspace && !_stub->_pi.quit) {
				if (voiceSegmentData && !_mix.isPlaying(voiceSegmentData)) {
					break;
				}
				inp_update();
				_stub->sleep(80);
			}
			if (voiceSegmentData) {
				_mix.stopAll();
				free(voiceSegmentData);
			}
			_stub->_pi.backspace = false;
			if (_res._type == kResourceTypeMac) {
				if (textSpeechSegment == textSegmentsCount) {
					break;
				}
			} else {
				if (*str == 0) {
					break;
				}
				++str;
			}
			memcpy(_vid._frontLayer, _vid._tempLayer, _vid._layerSize);
		}
		_textToDisplay = 0xFFFF;
	}
}

void Game::drawString(const uint8_t *p, int x, int y, uint8_t color, bool hcenter) {
	const char *str = (const char *)p;
	int len = 0;
	if (_res._type == kResourceTypeMac) {
		len = *p;
		++str;
	} else {
		len = strlen(str);
	}
	if (hcenter) {
		x = (x - len * Video::CHAR_W) / 2;
	}
	_vid.drawStringLen(str, len, x, y, color);
}

void Game::prepareAnims() {
	if (_currentRoom < 0x40) {
		int8_t pge_room;
		LivePGE *pge = _pge_liveTable1[_currentRoom];
		while (pge) {
			prepareAnimsHelper(pge, 0, 0);
			pge = pge->next_PGE_in_room;
		}
		pge_room = _res._ctData[CT_UP_ROOM + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if ((pge->init_PGE->object_type != 10 && pge->pos_y > 176) || (pge->init_PGE->object_type == 10 && pge->pos_y > 216)) {
					prepareAnimsHelper(pge, 0, -216);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[CT_DOWN_ROOM + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if (pge->pos_y < 48) {
					prepareAnimsHelper(pge, 0, 216);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[CT_LEFT_ROOM + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if (pge->pos_x > 224) {
					prepareAnimsHelper(pge, -256, 0);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[CT_RIGHT_ROOM + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if (pge->pos_x <= 32) {
					prepareAnimsHelper(pge, 256, 0);
				}
				pge = pge->next_PGE_in_room;
			}
		}
	}
}

void Game::prepareAnimsHelper(LivePGE *pge, int16_t dx, int16_t dy) {
	debug(DBG_GAME, "Game::prepareAnimsHelper() dx=0x%X dy=0x%X pge_num=%ld pge->flags=0x%X pge->anim_number=0x%X", dx, dy, pge - &_pgeLive[0], pge->flags, pge->anim_number);
	const bool conrad = pge == &_pgeLive[0];
	const int frame = pge->anim_number;
	const uint8_t displayFlags = pge->flags;
	if (effectivePistolStyle() != weapon::Style::Original && _weaponFx.nativeFlashMatched &&
	    isConradShotEffect(pge) && (pge->anim_number == 287 || pge->anim_number == 288))
		return;
	if (conrad && effectivePistolStyle() != weapon::Style::Original)
		dx += _weaponFx.recoil(pge->obj_type);
	if (!(pge->flags & 8)) {
		if (pge->index != 0 && loadMonsterSprites(pge) == 0) {
			return;
		}
		const uint8_t *dataPtr = 0;
		int8_t dw = 0, dh = 0;
		switch (_res._type) {
		case kResourceTypeAmiga:
		case kResourceTypeDOS:
		case kResourceTypePC98:
		case kResourceTypeSega:
			assert(pge->anim_number < 1287);
			dataPtr = _res._sprData[frame];
			if (dataPtr == 0) {
				return;
			}
			dw = (int8_t)dataPtr[0];
			dh = (int8_t)dataPtr[1];
			break;
		case kResourceTypeMac:
			break;
		}
		uint8_t w = 0, h = 0;
		switch (_res._type) {
		case kResourceTypeAmiga:
			w = ((dataPtr[2] >> 7) + 1) * 16;
			h = dataPtr[2] & 0x7F;
			break;
		case kResourceTypeDOS:
		case kResourceTypePC98:
			w = dataPtr[2];
			h = dataPtr[3];
			dataPtr += 4;
			break;
		case kResourceTypeSega:
			w = 32;
			h = 48;
			break;
		case kResourceTypeMac:
			break;
		}
		int16_t ypos = dy + pge->pos_y - dh + 2;
		int16_t xpos = dx + pge->pos_x - dw;
		if (displayFlags & 2) {
			xpos = dw + dx + pge->pos_x;
			uint8_t _cl = w;
			if (_cl & 0x40) {
				_cl = h;
			} else {
				_cl &= 0x3F;
			}
			xpos -= _cl;
		}
		if (xpos <= -32 || xpos >= 256 || ypos < -48 || ypos >= 224) {
			return;
		}
		xpos += 8;
		if (pge == &_pgeLive[0]) {
			_animBuffers.addState(1, xpos, ypos, dataPtr, pge, w, h);
		} else if (pge->flags & 0x10) {
			_animBuffers.addState(2, xpos, ypos, dataPtr, pge, w, h);
		} else {
			_animBuffers.addState(0, xpos, ypos, dataPtr, pge, w, h);
		}
	} else {
		const uint8_t *dataPtr = 0;
		switch (_res._type) {
		case kResourceTypeAmiga:
		case kResourceTypeDOS:
		case kResourceTypePC98:
		case kResourceTypeSega:
			assert(pge->anim_number < _res._numSpc);
			dataPtr = _res._spc + READ_BE_UINT16(_res._spc + pge->anim_number * 2);
			break;
		case kResourceTypeMac:
			break;
		}
		const int16_t xpos = dx + pge->pos_x + 8;
		const int16_t ypos = dy + pge->pos_y + 2;
		if (pge->init_PGE->object_type == 11) {
			_animBuffers.addState(3, xpos, ypos, dataPtr, pge);
		} else if (pge->flags & 0x10) {
			_animBuffers.addState(2, xpos, ypos, dataPtr, pge);
		} else {
			_animBuffers.addState(0, xpos, ypos, dataPtr, pge);
		}
	}
}

void Game::drawAnims() {
	debug(DBG_GAME, "Game::drawAnims()");
	_eraseBackground = false;
	drawAnimBuffer(2, _animBuffer2State);
	drawAnimBuffer(1, _animBuffer1State);
	drawWeaponFx();
	drawAnimBuffer(0, _animBuffer0State);
	_eraseBackground = true;
	drawAnimBuffer(3, _animBuffer3State);
}

void Game::drawAnimBuffer(uint8_t stateNum, AnimBufferState *state) {
	debug(DBG_GAME, "Game::drawAnimBuffer() state=%d", stateNum);
	assert(stateNum < 4);
	_animBuffers._states[stateNum] = state;
	uint8_t lastPos = _animBuffers._curPos[stateNum];
	if (lastPos != 0xFF) {
		uint8_t numAnims = lastPos + 1;
		state += lastPos;
		_animBuffers._curPos[stateNum] = 0xFF;
		do {
			LivePGE *pge = state->pge;
			const uint8_t displayFlags = pge->flags;
			if (!(pge->flags & 8)) {
				if (pge == &_pgeLive[0]) _vid.refreshConradShirtPalette();
				if (stateNum == 1 && (_blinkingConradCounter & 1) && !_preferences.reduceBlink) {
					break;
				}
				switch (_res._type) {
				case kResourceTypeAmiga:
					_vid.AMIGA_decodeSpm(state->dataPtr, _res._scratchBuffer);
					if (pge == &_pgeLive[0]) {
						const int width = ((state->dataPtr[2] >> 7) + 1) * 16;
						const int height = state->dataPtr[2] & 0x7F;
						appearance::markAmigaTshirt(_res._scratchBuffer, width, height, Resource::kScratchBufferSize);
					}
					drawCharacter(_res._scratchBuffer, state->x, state->y, state->h, state->w,
					              displayFlags, pge == &_pgeLive[0]);
					break;
				case kResourceTypeDOS:
				case kResourceTypePC98:
					if (!(state->dataPtr[-2] & 0x80)) {
						_vid.DOS_decodeSpm(state->dataPtr, _res._scratchBuffer);
						drawCharacter(_res._scratchBuffer, state->x, state->y, state->h, state->w,
						              displayFlags, pge == &_pgeLive[0]);
					} else {
						drawCharacter(state->dataPtr, state->x, state->y, state->h, state->w,
						              displayFlags, pge == &_pgeLive[0]);
					}
					break;
				case kResourceTypeSega:
					if (decodeSegaCharacter(state->dataPtr, _res._sprDataSizes[pge->anim_number],
					                        _res._scratchBuffer, Resource::kScratchBufferSize)) {
						drawCharacter(_res._scratchBuffer, state->x, state->y, state->h, state->w,
						              displayFlags, pge == &_pgeLive[0]);
					} else {
						warning("Invalid Sega character %d", pge->anim_number);
					}
					break;
				case kResourceTypeMac:
					drawPiege(state);
					break;
				}
			} else {
				drawPiege(state);
			}
			--state;
		} while (--numAnims != 0);
	}
}

void Game::drawPiege(AnimBufferState *state) {
	LivePGE *pge = state->pge;
	switch (_res._type) {
	case kResourceTypeAmiga:
	case kResourceTypeDOS:
	case kResourceTypeSega:
	case kResourceTypePC98:
		drawObject(state->dataPtr, state->x, state->y, pge->flags);
		break;
	case kResourceTypeMac:
		if (pge->flags & 8) {
			_vid.MAC_drawSprite(state->x, state->y, _res._spc, pge->anim_number, (pge->flags & 2) != 0, _eraseBackground);
		} else if (pge->index == 0) {
			if (pge->anim_number == 0x386) {
				break;
			}
			const int frame = _res.MAC_getPersoFrame(pge->anim_number);
			_vid.MAC_drawSprite(state->x, state->y, _res._perso, frame, (pge->flags & 2) != 0,
			                    _eraseBackground, true);
		} else {
			const int frame = _res.MAC_getMonsterFrame(pge->anim_number);
			_vid.MAC_drawSprite(state->x, state->y, _res._monster, frame, (pge->flags & 2) != 0, _eraseBackground);
		}
		break;
	}
}

void Game::drawObject(const uint8_t *dataPtr, int16_t x, int16_t y, uint8_t flags) {
	debug(DBG_GAME, "Game::drawObject() dataPtr[]=0x%X dx=%d dy=%d",  dataPtr[0], (int8_t)dataPtr[1], (int8_t)dataPtr[2]);
	assert(dataPtr[0] < 0x4A);
	uint8_t slot = _res._rp[dataPtr[0]];
	uint8_t *data = _res.findBankData(slot);
	if (data == 0) {
		data = _res.loadBankData(slot);
	}
	int16_t posy = y - (int8_t)dataPtr[2];
	int16_t posx = x;
	if (flags & 2) {
		posx += (int8_t)dataPtr[1];
	} else {
		posx -= (int8_t)dataPtr[1];
	}
	int count = 0;
	switch (_res._type) {
	case kResourceTypeAmiga:
		count = dataPtr[8];
		dataPtr += 9;
		break;
	case kResourceTypeDOS:
	case kResourceTypeSega:
	case kResourceTypePC98:
		count = dataPtr[5];
		dataPtr += 6;
		break;
	case kResourceTypeMac:
		assert(0); // different graphics format
		break;
	}
	for (int i = 0; i < count; ++i) {
		drawObjectFrame(data, dataPtr, posx, posy, flags, _res.getBankDataSize(slot));
		dataPtr += 4;
	}
}

void Game::drawObjectFrame(const uint8_t *bankDataPtr, const uint8_t *dataPtr, int16_t x, int16_t y,
                           uint8_t flags, size_t bankSize) {
	debug(DBG_GAME, "Game::drawObjectFrame(%p, %d, %d, 0x%X)", dataPtr, x, y, flags);
	const size_t bankOffset = size_t(dataPtr[0]) * 32;
	if (bankOffset >= bankSize) {
		warning("Object offset exceeds graphics bank");
		return;
	}
	const uint8_t *src = bankDataPtr + bankOffset;

	int16_t sprite_y = y + dataPtr[2];
	int16_t sprite_x;
	if (flags & 2) {
		sprite_x = x - dataPtr[1] - (((dataPtr[3] & 0xC) + 4) * 2);
	} else {
		sprite_x = x + dataPtr[1];
	}

	uint8_t sprite_flags = dataPtr[3];
	if (flags & 2) {
		sprite_flags ^= 0x10;
	}

	uint8_t sprite_h = (((sprite_flags >> 0) & 3) + 1) * 8;
	uint8_t sprite_w = (((sprite_flags >> 2) & 3) + 1) * 8;

	switch (_res._type) {
	case kResourceTypeAmiga:
		_vid.AMIGA_decodeSpc(src, sprite_w, sprite_h, _res._scratchBuffer);
		break;
	case kResourceTypeDOS:
	case kResourceTypePC98:
		_vid.DOS_decodeSpc(src, sprite_w, sprite_h, _res._scratchBuffer);
		break;
	case kResourceTypeSega: {
		const size_t offset = size_t(dataPtr[0]) * 32;
		if (offset > bankSize ||
		    !decodeSegaTiles(src, bankSize - offset, sprite_w, sprite_h, _res._scratchBuffer,
		                     Resource::kScratchBufferSize)) {
			warning("Invalid Sega object tiles");
			return;
		}
		break;
	}
	case kResourceTypeMac:
		assert(0); // different graphics format
		break;
	}

	src = _res._scratchBuffer;
	bool sprite_mirror_x = false;
	int16_t sprite_clipped_w;
	if (sprite_x >= 0) {
		sprite_clipped_w = sprite_x + sprite_w;
		if (sprite_clipped_w < 256) {
			sprite_clipped_w = sprite_w;
		} else {
			sprite_clipped_w = 256 - sprite_x;
			if (sprite_flags & 0x10) {
				sprite_mirror_x = true;
				src += sprite_w - 1;
			}
		}
	} else {
		sprite_clipped_w = sprite_x + sprite_w;
		if (!(sprite_flags & 0x10)) {
			src -= sprite_x;
			sprite_x = 0;
		} else {
			sprite_mirror_x = true;
			src += sprite_x + sprite_w - 1;
			sprite_x = 0;
		}
	}
	if (sprite_clipped_w <= 0) {
		return;
	}

	int16_t sprite_clipped_h;
	if (sprite_y >= 0) {
		sprite_clipped_h = 224 - sprite_h;
		if (sprite_y < sprite_clipped_h) {
			sprite_clipped_h = sprite_h;
		} else {
			sprite_clipped_h = 224 - sprite_y;
		}
	} else {
		sprite_clipped_h = sprite_h + sprite_y;
		src -= sprite_w * sprite_y;
		sprite_y = 0;
	}
	if (sprite_clipped_h <= 0) {
		return;
	}

	if (!sprite_mirror_x && (sprite_flags & 0x10)) {
		src += sprite_w - 1;
	}

	uint32_t dst_offset = 256 * sprite_y + sprite_x;
	uint8_t sprite_col_mask = (flags & 0x60) >> 1;

	if (_eraseBackground) {
		if (!(sprite_flags & 0x10)) {
			_vid.drawSpriteSub1(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub2(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	} else {
		if (!(sprite_flags & 0x10)) {
			_vid.drawSpriteSub3(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub4(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	}
	_vid.markBlockAsDirty(sprite_x, sprite_y, sprite_clipped_w, sprite_clipped_h, _vid._layerScale);
}

void Game::drawCharacter(const uint8_t *dataPtr, int16_t pos_x, int16_t pos_y, uint8_t a, uint8_t b, uint8_t flags, bool conrad) {
	debug(DBG_GAME, "Game::drawCharacter(%p, %d, %d, 0x%X, 0x%X, 0x%X)", dataPtr, pos_x, pos_y, a, b, flags);
	bool sprite_mirror_y = false;
	if (b & 0x40) {
		b &= ~0x40;
		SWAP(a, b);
		sprite_mirror_y = true;
	}
	uint16_t sprite_h = a;
	uint16_t sprite_w = b;

	const uint8_t *src = dataPtr;
	bool var14 = false;

	int16_t sprite_clipped_w;
	if (pos_x >= 0) {
		if (pos_x + sprite_w < 256) {
			sprite_clipped_w = sprite_w;
		} else {
			sprite_clipped_w = 256 - pos_x;
			if (flags & 2) {
				var14 = true;
				if (sprite_mirror_y) {
					src += (sprite_w - 1) * sprite_h;
				} else {
					src += sprite_w - 1;
				}
			}
		}
	} else {
		sprite_clipped_w = pos_x + sprite_w;
		if (!(flags & 2)) {
			if (sprite_mirror_y) {
				src -= sprite_h * pos_x;
				pos_x = 0;
			} else {
				src -= pos_x;
				pos_x = 0;
			}
		} else {
			var14 = true;
			if (sprite_mirror_y) {
				src += sprite_h * (pos_x + sprite_w - 1);
				pos_x = 0;
			} else {
				src += pos_x + sprite_w - 1;
				var14 = true;
				pos_x = 0;
			}
		}
	}
	if (sprite_clipped_w <= 0) {
		return;
	}

	int16_t sprite_clipped_h;
	if (pos_y >= 0) {
		if (pos_y < 224 - sprite_h) {
			sprite_clipped_h = sprite_h;
		} else {
			sprite_clipped_h = 224 - pos_y;
		}
	} else {
		sprite_clipped_h = sprite_h + pos_y;
		if (sprite_mirror_y) {
			src -= pos_y;
		} else {
			src -= sprite_w * pos_y;
		}
		pos_y = 0;
	}
	if (sprite_clipped_h <= 0) {
		return;
	}

	if (!var14 && (flags & 2)) {
		if (sprite_mirror_y) {
			src += sprite_h * (sprite_w - 1);
		} else {
			src += sprite_w - 1;
		}
	}

	uint32_t dst_offset = 256 * pos_y + pos_x;
	uint8_t sprite_col_mask = ((flags & 0x60) == 0x60) ? 0x50 : 0x40;

	debug(DBG_GAME, "dst_offset=0x%X src_offset=%ld", dst_offset, src - dataPtr);

	if (!(flags & 2)) {
		if (sprite_mirror_y) {
			_vid.drawSpriteSub5(src, _vid._frontLayer + dst_offset, sprite_h, sprite_clipped_h, sprite_clipped_w, sprite_col_mask, conrad);
		} else {
			_vid.drawSpriteSub3(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask, conrad);
		}
	} else {
		if (sprite_mirror_y) {
			_vid.drawSpriteSub6(src, _vid._frontLayer + dst_offset, sprite_h, sprite_clipped_h, sprite_clipped_w, sprite_col_mask, conrad);
		} else {
			_vid.drawSpriteSub4(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask, conrad);
		}
	}
	_vid.markBlockAsDirty(pos_x, pos_y, sprite_clipped_w, sprite_clipped_h, _vid._layerScale);
}

int Game::loadMonsterSprites(LivePGE *pge) {
	debug(DBG_GAME, "Game::loadMonsterSprites()");
	InitPGE *init_pge = pge->init_PGE;
	if (init_pge->obj_node_number != 0x49 && init_pge->object_type != 10) {
		return 0xFFFF;
	}
	if (init_pge->obj_node_number == _curMonsterFrame) {
		return 0xFFFF;
	}
	if (pge->room_location != _currentRoom) {
		return 0;
	}

	const uint8_t *mList = _monsterListLevels[_currentLevel];
	while (*mList != init_pge->obj_node_number) {
		if (*mList == 0xFF) { // end of list
			return 0;
		}
		mList += 2;
	}
	_curMonsterFrame = mList[0];
	if (_curMonsterNum != mList[1]) {
		_curMonsterNum = mList[1];
		switch (_res._type) {
		case kResourceTypeSega:
		case kResourceTypeAmiga: {
			if (!_res.isSega()) {
				_res.load(_monsterNames[1][_curMonsterNum], Resource::OT_SPM);
			}
			    static const uint8_t tab[4] = { 0, 8, 0, 8 };
				const int offset = _vid._mapPalSlot3 * 16 + tab[_curMonsterNum];
				for (int i = 0; i < 8; ++i) {
					_vid.setPaletteColorBE(0x50 + i, offset + i);
				}
			}
			break;
		case kResourceTypeDOS:
		case kResourceTypePC98: {
				const char *name = _monsterNames[0][_curMonsterNum];
				_res.load(name, Resource::OT_SPRM);
				_res.load_SPR_OFF(name, _res._sprm);
				_vid.setPaletteSlotLE(5, _monsterPals[_curMonsterNum]);
			}
			break;
		case kResourceTypeMac: {
				Color palette[256];
			    _res.MAC_loadMonsterData(_monsterNames[0][_curMonsterNum], palette, _currentLevel);
			    static const int kMonsterPalette = 5;
				for (int i = 0; i < 16; ++i) {
					const int color = kMonsterPalette * 16 + i;
					_stub->setPaletteEntry(color, &palette[color]);
				}
			}
			break;
		}
	}
	return 0xFFFF;
}

bool Game::hasLevelMap(int level, int room) const {
	if (level < 0 || level >= 7 || room < 0 || room >= 64) {
		return false;
	}
	if (_res._type == kResourceTypeMac) {
		return _res.MAC_hasLevelMap(level, room);
	}
	if (_res._map) {
		return READ_LE_UINT32(_res._map + room * 6) != 0;
	} else if (_res._lev) {
		return READ_BE_UINT32(_res._lev + room * 4) > 0x100;
	}
	return false;
}

static bool isMetro(int level, int room) {
	return level == 1 && (room == 0 || room == 13 || room == 38 || room == 51);
}

void Game::decodeLevelRoom(int room) {
	switch (_res._type) {
	case kResourceTypeAmiga:
		if (_currentLevel == 1) {
			int num = 0;
			switch (room) {
			case 14:
			case 19:
			case 52:
			case 53:
				num = 1;
				break;
			case 11:
			case 24:
			case 27:
			case 56:
				num = 2;
				break;
			}
			if (num != 0 && _res._levNum != num) {
				char name[9];
				snprintf(name, sizeof(name), "level2_%d", num);
				_res.load(name, Resource::OT_LEV);
				_res._levNum = num;
			}
		}
		_vid.AMIGA_decodeLev(_currentLevel, room);
		break;
	case kResourceTypeSega:
		_vid.AMIGA_decodeLev(_currentLevel, room);
		break;
	case kResourceTypeDOS:
		_vid.DOS_decodeMap(_currentLevel, room);
		break;
	case kResourceTypeMac:
		_vid.MAC_decodeMap(_currentLevel, room);
		break;
	case kResourceTypePC98:
		_vid.PC98_decodeMap(_currentLevel, room);
		break;
	}
}

void Game::loadLevelMap() {
	if (!hasLevelMap(_currentLevel, _currentRoom)) {
		return;
	}
	_currentIcon = 0xFF;
	const int originalBank = _res._levNum;
	const bool adjacent =
	    _stub->hasWidescreen() && (_widescreenMode == kWidescreenAdjacentRooms ||
		                           _widescreenMode == kWidescreenAdjacentRoomsBlur);
	if (adjacent) {
		for (int side = 0; side < 2; ++side) {
			const int room = _res._ctData[(side ? CT_RIGHT_ROOM : CT_LEFT_ROOM) + _currentRoom];
			const uint8_t *pixels = nullptr;
			if (hasLevelMap(_currentLevel, room) && !isMetro(_currentLevel, room)) {
				decodeLevelRoom(room);
				pixels = _vid._backLayer;
			}
			if (side) {
				_stub->copyWidescreenRight(_vid._w, _vid._h, pixels,
				                           _widescreenMode == kWidescreenAdjacentRooms);
			} else {
				_stub->copyWidescreenLeft(_vid._w, _vid._h, pixels,
				                          _widescreenMode == kWidescreenAdjacentRooms);
			}
		}
	}
	// Restore current room pixels, palette and Amiga resource-bank selection
	// after decoding neighbors, which deliberately use the same adapter.
	if (_res.isAmiga() && _currentLevel == 1 && originalBank != _res._levNum) {
		char name[16];
		snprintf(name, sizeof(name), "level2_%d", originalBank);
		_res.load(name, Resource::OT_LEV);
		_res._levNum = originalBank;
	}
	decodeLevelRoom(_currentRoom);
	if (!adjacent) {
		_vid.updateWidescreen();
	}
}

void Game::loadLevelData() {
	_weaponFx.clear();
	_lastRewindTick = _playTicks;
	_persistentDue = true;
	_activeProgress = false;
	_nextPersistentAttempt = 0;
	_res.clearLevelRes();
	const Level *lvl = &_gameLevels[_currentLevel];
	switch (_res._type) {
	case kResourceTypeAmiga:
		if (_res._isDemo) {
			static const char *fname1 = "demo";
			static const char *fname2 = "demof";
			_res.load(fname1, Resource::OT_MBK);
			_res.load(fname1, Resource::OT_CT);
			_res.load(fname1, Resource::OT_PAL);
			_res.load(fname1, Resource::OT_RPC);
			_res.load(fname1, Resource::OT_SPC);
			_res.load(fname1, Resource::OT_LEV);
			_res.load(fname2, Resource::OT_PGE);
			_res.load(fname1, Resource::OT_OBJ);
			_res.load(fname1, Resource::OT_ANI);
			_res.load(fname2, Resource::OT_TBN);
			_res.load_SPL_demo();
			_res.load("level1", Resource::OT_SGD);
			break;
		}
		{
			const char *name = lvl->nameAmiga;
			if (_currentLevel == 4) {
				name = _gameLevels[3].nameAmiga;
			}
			_res.load(name, Resource::OT_MBK);
			if (_currentLevel == 6) {
				_res.load(_gameLevels[5].nameAmiga, Resource::OT_CT);
			} else {
				_res.load(name, Resource::OT_CT);
			}
			_res.load(name, Resource::OT_PAL);
			_res.load(name, Resource::OT_RPC);
			_res.load(name, Resource::OT_SPC);
			if (_currentLevel == 1) {
				_res.load("level2_1", Resource::OT_LEV);
				_res._levNum = 1;
			} else {
				_res.load(name, Resource::OT_LEV);
			}
		}
		_res.load(lvl->nameAmiga, Resource::OT_PGE);
		_res.load(lvl->nameAmiga, Resource::OT_OBC);
		_res.load(lvl->nameAmiga, Resource::OT_ANI);
		_res.load(lvl->nameAmiga, Resource::OT_TBN);
		{
			char name[8];
			snprintf(name, sizeof(name), "level%d", lvl->sound);
			_res.load(name, Resource::OT_SPL);
		}
		if (_currentLevel == 0) {
			_res.load(lvl->nameAmiga, Resource::OT_SGD);
		}
		break;
	case kResourceTypeDOS:
	case kResourceTypeSega:
	case kResourceTypePC98:
		_res.load(lvl->name, Resource::OT_MBK);
		_res.load(lvl->name, Resource::OT_CT);
		_res.load(lvl->name, Resource::OT_PAL);
		_res.load(lvl->name, Resource::OT_RP);
		if (_res.isPC98()) {
			_res.PC98_loadLevelMap(_currentLevel);
		} else if (!_res.isSega() &&
		           (_res._isDemo || g_options.use_tile_data ||
		            _res._aba)) { // use .BNQ/.LEV/(.SGD) instead of .MAP (PC demo)
			if (_currentLevel == 0) {
				_res.load(lvl->name, Resource::OT_SGD);
			}
			_res.load(lvl->name, Resource::OT_LEV);
			_res.load(lvl->name, Resource::OT_BNQ);
		} else if (_res.isSega()) {
			if (_currentLevel == 0) {
				_res.load(lvl->name, Resource::OT_SGD);
			}
			_res.load(lvl->name, Resource::OT_LEV);
		} else {
			_res.load(lvl->name, Resource::OT_MAP);
		}
		_res.load(lvl->name2, Resource::OT_PGE);
		if (!_res.isSega()) {
			_res.load(lvl->name2, Resource::OT_OBJ);
			_res.load(lvl->name2, Resource::OT_ANI);
		}
		_res.load(lvl->name2, Resource::OT_TBN);
		break;
	case kResourceTypeMac:
		_res.MAC_unloadLevelData();
		_res.MAC_loadLevelData(_currentLevel);
		break;
	}
	if (0) {
		for (int i = 0; i < 64; ++i) {
			if (hasLevelMap(_currentLevel, i)) {
				_currentRoom = i;
				loadLevelMap();
				uint8_t palette[256 * 3];
				_stub->getPalette(palette, 256);
				char name[64];
				snprintf(name, sizeof(name), "DUMP/level%d_room%02d.bmp", _currentLevel, i);
				saveBMP(name, _vid._backLayer, palette, _vid._w, _vid._h);

				memcpy(_vid._frontLayer, _vid._backLayer, _vid._layerSize);
				for (int y = 0; y < 7; ++y) {
					for (int x = 0; x < 16; ++x) {
						const uint8_t num = _res._ctData[0x100 + _currentRoom * 0x70 + y * 16 + x];
						char buf[16];
						snprintf(buf, sizeof(buf), "%d", num);
						_vid.drawString(buf, x * 16, y * 36 + 8, 0xE7);
					}
				}
				snprintf(name, sizeof(name), "DUMP/level%d_room%02d_grid.bmp", _currentLevel, i);
				saveBMP(name, _vid._frontLayer, palette, _vid._w, _vid._h);
			}
		}
	}

	_cut._id = lvl->cutscene_id;

	_curMonsterNum = 0xFFFF;
	_curMonsterFrame = 0;

	_res.clearBankData();
	_printLevelCodeCounter = 150;

	_col_slots2Cur = _col_slots2;
	_col_slots2Next = 0;

	memset(_pge_liveTable2, 0, sizeof(_pge_liveTable2));
	memset(_pge_liveTable1, 0, sizeof(_pge_liveTable1));

	_currentRoom = _res._pgeInit[0].init_room;
	uint16_t n = _res._pgeNum;
	while (n--) {
		pge_loadForCurrentLevel(n);
	}

	if (_demoBin != -1) {
		_cut._id = -1;
		if (_res.isMac()) {
			static const uint8_t positions[3][3] = {
			    {0x33, 0x60, 0x46}, {0, 0x60, 0xD6}, {0x38, 0x50, 0x8E}};
			_pgeLive[0].room_location = positions[_demoBin][0];
			_pgeLive[0].pos_x = positions[_demoBin][1];
			_pgeLive[0].pos_y = positions[_demoBin][2];
			_inp_demPos = 0;
		} else if (_demoInputs[_demoBin].room != 255) {
			_pgeLive[0].room_location = _demoInputs[_demoBin].room;
			_pgeLive[0].pos_x = _demoInputs[_demoBin].x;
			_pgeLive[0].pos_y = _demoInputs[_demoBin].y;
			_inp_demPos = 0;
		} else {
			_inp_demPos = 1;
		}
		_printLevelCodeCounter = 0;
	}

	for (uint16_t i = 0; i < _res._pgeNum; ++i) {
		if (_res._pgeInit[i].skill <= _skillLevel) {
			LivePGE *pge = &_pgeLive[i];
			// Keep off-screen actors in their 0xFE/0xFF holding lists too.
			// Spawning moves an actor out of its old list; omitting it here
			// would prevent shot effects (and inventory objects) entering a room.
			pge->next_PGE_in_room = _pge_liveTable1[pge->room_location];
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
	pge_resetMessages();
	_assetFingerprint = gameplayFingerprint(_res);

	_validSaveState = false;

	_mix.playMusic(Mixer::MUSIC_TRACK + lvl->track);

	if (_widescreenMode == kWidescreenCDi) {
		char name[16];
		snprintf(name, sizeof(name), "flashp%d.bob", _currentLevel + 1);
		File f;
		if (f.open(name, "rb", _fs)) {
			const int w = f.readUint16LE();
			const int h = f.readUint16LE();
			if (w != kWidescreenBorderCDiW || h != kWidescreenBorderCDiH) {
				warning("Unsupported CDi border w:%d h:%d", w, h);
			} else {
				f.read(_res._scratchBuffer, 256 * 3 + w * h);
				_stub->copyWidescreenCDi(w, h, _res._scratchBuffer + 256 * 3, _res._scratchBuffer);
			}
		}
	}
}

void Game::drawIcon(uint8_t iconNum, int16_t x, int16_t y, uint8_t colMask) {
	uint8_t buf[16 * 16];
	switch (_res._type) {
	case kResourceTypeAmiga:
		if (iconNum > 30) {
			// inventory icons
			switch (iconNum) {
			case 76: // cursor
				memset(buf, 0, 16 * 16);
				for (int i = 0; i < 3; ++i) {
					buf[i] = buf[15 * 16 + (15 - i)] = 1;
					buf[i * 16] = buf[(15 - i) * 16 + 15] = 1;
				}
				break;
			case 77: // up - icon.spr 4
				memset(buf, 0, 16 * 16);
				_vid.AMIGA_decodeIcn(_res._icn, 35, buf);
				break;
			case 78: // down - icon.spr 5
				memset(buf, 0, 16 * 16);
				_vid.AMIGA_decodeIcn(_res._icn, 36, buf);
				break;
			default:
				memset(buf, 5, 16 * 16);
				break;
			}
		} else {
			_vid.AMIGA_decodeIcn(_res._icn, iconNum, buf);
		}
		break;
	case kResourceTypeDOS:
	case kResourceTypePC98:
		_vid.DOS_decodeIcn(_res._icn, iconNum, buf);
		break;
	case kResourceTypeSega: {
		const size_t offset = size_t(iconNum) * 128;
		if (offset > size_t(_res._icnLen) ||
		    !decodeSegaTiles(_res._icn + offset, _res._icnLen - offset, 16, 16, buf, 256)) {
			return;
		}
		break;
	}
	case kResourceTypeMac:
		switch (iconNum) {
		case 76: // cursor
			iconNum = 32;
			break;
		case 77: // up
			iconNum = 33;
			break;
		case 78: // down
			iconNum = 34;
			break;
		}
		_vid.MAC_drawSprite(x, y, _res._icn, iconNum, false, true);
		return;
	}
	_vid.drawSpriteSub1(buf, _vid._frontLayer + x + y * _vid._w, 16, 16, 16, colMask << 4);
	_vid.markBlockAsDirty(x, y, 16, 16, _vid._layerScale);
}

void Game::playSound(uint8_t num, uint8_t softVol) {
	if (num < _res._numSfx) {
		SoundFx *sfx = &_res._sfxList[num];
		if (sfx->data) {
			const int volume =
			    (_res.isMac() ? Mixer::MAX_VOLUME / 2 : Mixer::MAX_VOLUME) >> (2 * softVol);
			_mix.play(sfx->data, sfx->len, sfx->freq, volume);
		}
	} else if (num == 66) {
		// open/close inventory (DOS)
	} else if (num >= 68 && num <= 75) {
		// in-game music
		_mix.playMusic(num);
	} else if (num == 76) {
		// metro
		playSound(8, softVol);
	} else if (num == 77) {
		// triggered when Conrad draw his gun
	} else {
		warning("Unknown sound num %d", num);
	}
}

uint16_t Game::getRandomNumber() {
	uint32_t n = _randSeed * 2;
	if (((int32_t)_randSeed) >= 0) {
		n ^= 0x1D872B41;
	}
	_randSeed = n;
	return n & 0xFFFF;
}

void Game::changeLevel() {
	_vid.fadeOut();
	clearStateRewind();
	loadLevelData();
	loadLevelMap();
	_vid.setPalette0xF();
	_vid.setTextPalette();
	_vid.fullRefresh();
}

void Game::handleInventory() {
	LivePGE *selected_pge = 0;
	LivePGE *pge = &_pgeLive[0];
	if (pge->life > 0 && pge->current_inventory_PGE != 0xFF) {
		playSound(_res.isMac() ? 52 : 66, 0);
		std::vector<InventoryItem> items;
		std::array<bool, 256> visited{};
		int num_items = 0;
		uint8_t inv_pge = pge->current_inventory_PGE;
		while (inv_pge != 0xFF) {
			if (inv_pge >= _res._pgeNum || visited[inv_pge]) {
				warning("Invalid inventory chain");
				return;
			}
			visited[inv_pge] = true;
			items.emplace_back();
			items[num_items].icon_num = _res._pgeInit[inv_pge].icon_num;
			items[num_items].init_pge = &_res._pgeInit[inv_pge];
			items[num_items].live_pge = &_pgeLive[inv_pge];
			inv_pge = _pgeLive[inv_pge].next_inventory_PGE;
			++num_items;
		}
		items.emplace_back();
		items[num_items].icon_num = 0xFF;
		int current_item = 0;
		int num_lines = (num_items - 1) / 4 + 1;
		int current_line = 0;
		const std::vector<uint8_t> gameplayFrame(_vid._frontLayer,
		                                         _vid._frontLayer + _vid._layerSize);
		bool display_score = false;
		while (!_stub->_pi.backspace && !_stub->_pi.quit) {
			static const int icon_spr_w = 16;
			static const int icon_spr_h = 16;
			switch (_res._type) {
			case kResourceTypeAmiga:
			case kResourceTypeDOS:
			case kResourceTypeSega:
			case kResourceTypePC98: {
				// draw inventory background
				int icon_num = 31;
				for (int y = 140; y < 140 + 5 * icon_spr_h; y += icon_spr_h) {
					for (int x = 56; x < 56 + 9 * icon_spr_w; x += icon_spr_w) {
						drawIcon(icon_num, x, y, 0xF);
						++icon_num;
					}
				}
			}
				if (_res._type == kResourceTypeAmiga) {
					// draw outline rectangle
					static const uint8_t outline_color = 0xE7;
					uint8_t *p = _vid._frontLayer + 140 * Video::GAMESCREEN_W + 56;
					memset(p + 1, outline_color, 9 * icon_spr_w - 2);
					p += Video::GAMESCREEN_W;
					for (int y = 1; y < 5 * icon_spr_h - 1; ++y) {
						p[0] = p[9 * icon_spr_w - 1] = outline_color;
						p += Video::GAMESCREEN_W;
					}
					memset(p + 1, outline_color, 9 * icon_spr_w - 2);
				}
				break;
			case kResourceTypeMac:
				drawIcon(31, 56, 140, 0xF);
				break;
			}
			if (!display_score) {
				int icon_x_pos = 72;
				for (int i = 0; i < 4; ++i) {
					int item_it = current_line * 4 + i;
					if (items[item_it].icon_num == 0xFF) {
						break;
					}
					drawIcon(items[item_it].icon_num, icon_x_pos, 157, 0xA);
					if (current_item == item_it) {
						drawIcon(76, icon_x_pos, 157, 0xA);
						selected_pge = items[item_it].live_pge;
						uint8_t txt_num = items[item_it].init_pge->text_num;
						const uint8_t *str = _res.getTextString(_currentLevel, txt_num);
						drawString(str, Video::GAMESCREEN_W, 189, 0xED, true);
						if (items[item_it].init_pge->init_flags & 4) {
							char buf[10];
							snprintf(buf, sizeof(buf), "%d", selected_pge->life);
							_vid.drawString(buf, (Video::GAMESCREEN_W - strlen(buf) * Video::CHAR_W) / 2, 197, 0xED);
						}
					}
					icon_x_pos += 32;
				}
				if (current_line != (g_options.order_inventory_original ? 0 : (num_lines - 1))) {
					drawIcon(77, 120, 143, 0xA); // up arrow
				}
				if (current_line != (g_options.order_inventory_original ? (num_lines - 1) : 0)) {
					drawIcon(78, 120, 176, 0xA); // down arrow
				}
			} else {
				char buf[50];
				snprintf(buf, sizeof(buf), "SCORE %08u", _score);
				_vid.drawString(buf, (114 - strlen(buf) * Video::CHAR_W) / 2 + 72, 158, 0xE5);
				snprintf(buf, sizeof(buf), "%s:%s", _res.getMenuString(LocaleData::LI_06_LEVEL), _res.getMenuString(LocaleData::LI_13_EASY + _skillLevel));
				_vid.drawString(buf, (114 - strlen(buf) * Video::CHAR_W) / 2 + 72, 166, 0xE5);
				if (0) { // if the protection screen code was not properly cracked...
					static const uint8_t kCrackerText[17] = {
						0x19, 0x08, 0x1B, 0x19, 0x11, 0x1F, 0x08, 0x67, 0x18,
						0x16, 0x1B, 0x13, 0x08, 0x1F, 0x1B, 0x0F, 0x5A
					};
					for (int i = 0; i < 17; ++i) {
						buf[i] = kCrackerText[i] ^ 0x5A;
					}
					_vid.drawString(buf, 65, 193, 0xE4);
				}
			}

			_vid.updateScreen();
			_stub->sleep(80);
			inp_update();
			if (_stub->_pi.escape) {
				_stub->_pi.escape = false;
				memcpy(_vid._frontLayer, gameplayFrame.data(), gameplayFrame.size());
				_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
				if (handleConfigPanel())
					_endLoop = true;
				return;
			}

			if (g_options.order_inventory_original) {
				if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
					_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
					if (current_line < num_lines - 1) {
						++current_line;
						current_item = current_line * 4;
					}
				}
				if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
					_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
					if (current_line > 0) {
						--current_line;
						current_item = current_line * 4;
					}
				}
			} else {
				if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
					_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
					if (current_line < num_lines - 1) {
						++current_line;
						current_item = current_line * 4;
					}
				}
				if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
					_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
					if (current_line > 0) {
						--current_line;
						current_item = current_line * 4;
					}
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_LEFT) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				if (current_item > 0) {
					int item_num = current_item % 4;
					if (item_num > 0) {
						--current_item;
					}
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_RIGHT) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				if (current_item < num_items - 1) {
					int item_num = current_item % 4;
					if (item_num < 3) {
						++current_item;
					}
				}
			}
			if (_stub->_pi.enter) {
				_stub->_pi.enter = false;
				display_score = !display_score;
			}
		}
		_vid.fullRefresh();
		_stub->_pi.backspace = false;
		if (selected_pge) {
			pge_setCurrentInventoryObject(selected_pge);
		}
		playSound(_res.isMac() ? 52 : 66, 0);
	}
}

void Game::inp_update() {
	_stub->processEvents();
	if (_demoBin != -1 && _inp_demPos < _res._demLen) {
		const int keymask = _res._dem[_inp_demPos++];
		_stub->_pi.dirMask = keymask & 0xF;
		_stub->_pi.enter = (keymask & 0x10) != 0;
		_stub->_pi.space = (keymask & 0x20) != 0;
		_stub->_pi.shift = (keymask & 0x40) != 0;
		_stub->_pi.backspace = (keymask & 0x80) != 0;
	}
}

void Game::makeGameStateName(uint8_t slot, char *buf, size_t capacity) {
	snprintf(buf, capacity, "%s", saves::name(_res._type, _currentLevel, slot).c_str());
}


static const uint32_t TAG_FBSV = 0x46425356;

void Game::writeGameState(File &f) {
	f.writeUint32BE(TAG_FBSV);
	f.writeUint16BE(kSaveVersion);
	char header[32] = {};
	snprintf(header, sizeof(header), "level=%d room=%d", _currentLevel + 1, _currentRoom);
	f.write(header, sizeof(header));
	saveState(&f);
}

bool Game::saveGameState(uint8_t slot) {
	if (slot == kAutoSaveSlot)
		return saveStateRewind();
	if (slot > 99)
		return false;
	std::string reason;
	progress::Snapshot old;
	const auto path = (std::filesystem::u8path(_savePath) /
	                   progress::manualFilename(_res._type, _currentLevel, slot))
	                      .u8string();
	progress::read(path, old, reason);
	const bool saved = saveManualState(slot, old.label, false, reason);
	if (saved && slot == kIngameSaveSlot)
		_persistentDue = true;
	_stub->notify(saved ? (slot ? "Saved slot " + std::to_string(slot) + '.' : "Checkpoint stored.")
	                    : reason,
	              !saved);
	return saved;
}

bool Game::loadGameState(uint8_t slot) {
	if (slot == kAutoSaveSlot) {
		return loadStateRewind();
	}
	std::string reason;
	const bool success = loadManualState(_currentLevel, slot, reason);
	_stub->notify(reason, !success);
	if (!success)
		warning("%s", reason.c_str());
	return success;
}

void Game::saveState(File *f) {
	f->writeByte(_skillLevel);
	f->writeUint32BE(_score);
	if (_col_slots2Cur == 0) {
		f->writeUint32BE(0xFFFFFFFF);
	} else {
		f->writeUint32BE(_col_slots2Cur - &_col_slots2[0]);
	}
	if (_col_slots2Next == 0) {
		f->writeUint32BE(0xFFFFFFFF);
	} else {
		f->writeUint32BE(_col_slots2Next - &_col_slots2[0]);
	}
	for (int i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = &_pgeLive[i];
		f->writeUint16BE(pge->obj_type);
		f->writeUint16BE(pge->pos_x);
		f->writeUint16BE(pge->pos_y);
		f->writeByte(pge->anim_seq);
		f->writeByte(pge->room_location);
		f->writeUint16BE(pge->life);
		f->writeUint16BE(pge->counter_value);
		f->writeByte(pge->collision_slot);
		f->writeByte(pge->next_inventory_PGE);
		f->writeByte(pge->current_inventory_PGE);
		f->writeByte(pge->ref_inventory_PGE);
		f->writeUint16BE(pge->anim_number);
		f->writeByte(pge->flags);
		f->writeByte(pge->index);
		f->writeUint16BE(pge->first_obj_number);
		if (pge->next_PGE_in_room == 0) {
			f->writeUint32BE(0xFFFFFFFF);
		} else {
			f->writeUint32BE(pge->next_PGE_in_room - &_pgeLive[0]);
		}
		if (pge->init_PGE == 0) {
			f->writeUint32BE(0xFFFFFFFF);
		} else {
			f->writeUint32BE(pge->init_PGE - &_res._pgeInit[0]);
		}
	}
	f->write(&_res._ctData[0x100], 0x1C00);
	for (CollisionSlot2 *cs2 = &_col_slots2[0]; cs2 < _col_slots2Cur; ++cs2) {
		if (cs2->next_slot == 0) {
			f->writeUint32BE(0xFFFFFFFF);
		} else {
			f->writeUint32BE(cs2->next_slot - &_col_slots2[0]);
		}
		if (cs2->unk2 == 0) {
			f->writeUint32BE(0xFFFFFFFF);
		} else {
			f->writeUint32BE(cs2->unk2 - &_res._ctData[0x100]);
		}
		f->writeByte(cs2->data_size);
		f->write(cs2->data_buf, 0x10);
	}
	f->writeUint16BE(_pge_opGunVar);
	// Version 4 extends the legacy payload. Keep old version 2/3 files readable.
	f->writeUint32BE(_randSeed);
	f->writeByte(_res._type);
	f->writeByte(_currentLevel);
	f->writeUint16BE(_res._pgeNum);
	f->writeUint32BE(_assetFingerprint);
	f->writeUint32BE(_journal.unlocked);
	f->writeUint32BE(_journal.read);
}

void Game::loadState(File *f, int version) {
	uint16_t i;
	uint32_t off;
	_skillLevel = f->readByte();
	_score = f->readUint32BE();
	memset(_pge_liveTable2, 0, sizeof(_pge_liveTable2));
	memset(_pge_liveTable1, 0, sizeof(_pge_liveTable1));
	off = f->readUint32BE();
	if (off == 0xFFFFFFFF) {
		_col_slots2Cur = 0;
	} else {
		_col_slots2Cur = &_col_slots2[0] + off;
	}
	off = f->readUint32BE();
	if (off == 0xFFFFFFFF) {
		_col_slots2Next = 0;
	} else {
		_col_slots2Next = &_col_slots2[0] + off;
	}
	for (i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = &_pgeLive[i];
		pge->obj_type = f->readUint16BE();
		pge->pos_x = f->readUint16BE();
		pge->pos_y = f->readUint16BE();
		pge->anim_seq = f->readByte();
		pge->room_location = f->readByte();
		pge->life = f->readUint16BE();
		pge->counter_value = f->readUint16BE();
		pge->collision_slot = f->readByte();
		pge->next_inventory_PGE = f->readByte();
		pge->current_inventory_PGE = f->readByte();
		pge->ref_inventory_PGE = f->readByte();
		pge->anim_number = f->readUint16BE();
		pge->flags = f->readByte();
		pge->index = f->readByte();
		pge->first_obj_number = f->readUint16BE();
		off = f->readUint32BE();
		if (off == 0xFFFFFFFF) {
			pge->next_PGE_in_room = 0;
		} else {
			pge->next_PGE_in_room = &_pgeLive[0] + off;
		}
		off = f->readUint32BE();
		if (off == 0xFFFFFFFF) {
			pge->init_PGE = 0;
		} else {
			pge->init_PGE = &_res._pgeInit[0] + off;
		}
	}
	f->read(&_res._ctData[0x100], 0x1C00);
	for (CollisionSlot2 *cs2 = &_col_slots2[0]; cs2 < _col_slots2Cur; ++cs2) {
		off = f->readUint32BE();
		if (off == 0xFFFFFFFF) {
			cs2->next_slot = 0;
		} else {
			cs2->next_slot = &_col_slots2[0] + off;
		}
		off = f->readUint32BE();
		if (off == 0xFFFFFFFF) {
			cs2->unk2 = 0;
		} else {
			cs2->unk2 = &_res._ctData[0x100] + off;
		}
		cs2->data_size = f->readByte();
		f->read(cs2->data_buf, 0x10);
	}
	for (i = 0; i < _res._pgeNum; ++i) {
		if (_res._pgeInit[i].skill <= _skillLevel) {
			LivePGE *pge = &_pgeLive[i];
			if (pge->flags & 4) {
				_pge_liveTable2[pge->index] = pge;
			}
			// Keep off-screen actors in their 0xFE/0xFF holding lists too.
			// Spawning moves an actor out of its old list; omitting it here
			// would prevent shot effects (and inventory objects) entering a room.
			pge->next_PGE_in_room = _pge_liveTable1[pge->room_location];
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
	resetGameState();
	if (version >= 3)
		_pge_opGunVar = f->readUint16BE();
	if (version >= 4) {
		_randSeed = f->readUint32BE();
		f->readByte();
		f->readByte();
		f->readUint16BE(); // Identity already checked by preflight.
		f->readUint32BE();
		_journal.unlocked = f->readUint32BE();
		_journal.read = f->readUint32BE();
	} else {
		_journal = {};
	}
}

void Game::clearStateRewind() {
	// info("Clear rewind state (count %d)", _rewindLen);
	for (int i = 0; i < _rewindLen; ++i) {
		int ptr = _rewindPtr - i;
		if (ptr < 0) {
			ptr += kRewindSize;
		}
		_rewindBuffer[ptr].close();
		_rewindRecords[ptr] = {};
	}
	_rewindPtr = -1;
	_rewindLen = 0;
	_lastRewindTick = _playTicks;
}

bool Game::saveStateRewind() {
	if (_rewindPtr == kRewindSize - 1) {
		_rewindPtr = 0;
	} else {
		++_rewindPtr;
	}
	static const int kGameStateSize = 16384;
	File &f = _rewindBuffer[_rewindPtr];
	f.openMemoryBuffer(kGameStateSize);
	saveState(&f);
	if (f.ioErr()) {
		f.close();
		clearStateRewind();
		return false;
	}
	_rewindRecords[_rewindPtr] = {capturePreview(), _playTicks, _currentRoom, _pgeLive[0].life};
	_lastRewindTick = _playTicks;
	if (_rewindLen < kRewindSize) {
		++_rewindLen;
	}
	// info("Save state for rewind (index %d, count %d, size %d)", _rewindPtr, _rewindLen,
	// f.size());
	return !f.ioErr();
}

bool Game::loadStateRewind(unsigned age) {
	if (age >= unsigned(_rewindLen))
		return false;
	const int ptr = (_rewindPtr - int(age) + kRewindSize) % kRewindSize;
	File &f = _rewindBuffer[ptr];
	f.seek(0);
	loadState(&f, kSaveVersion);
	if (f.ioErr())
		return false;
	_playTicks = _lastRewindTick = _rewindRecords[ptr].tick;
	_lastPersistentTick = _playTicks;
	_nextPersistentAttempt = 0;
	_persistentDue = true;
	for (unsigned n = 0; n <= age; ++n) {
		const int discard = (_rewindPtr - int(n) + kRewindSize) % kRewindSize;
		_rewindBuffer[discard].close();
		_rewindRecords[discard] = {};
	}
	_rewindPtr = (ptr + kRewindSize - 1) % kRewindSize;
	_rewindLen -= age + 1;
	return true;
}

void AnimBuffers::addState(uint8_t stateNum, int16_t x, int16_t y, const uint8_t *dataPtr, LivePGE *pge, uint8_t w, uint8_t h) {
	debug(DBG_GAME, "AnimBuffers::addState() stateNum=%d x=%d y=%d dataPtr=%p pge=%p", stateNum, x, y, dataPtr, pge);
	assert(stateNum < 4);
	AnimBufferState *state = _states[stateNum];
	state->x = x;
	state->y = y;
	state->w = w;
	state->h = h;
	state->dataPtr = dataPtr;
	state->pge = pge;
	++_curPos[stateNum];
	++_states[stateNum];
}
