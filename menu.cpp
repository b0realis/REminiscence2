
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "game.h"
#include "decode_mac.h"
#include "file.h"
#include <vector>
#include "menu.h"
#include "resource.h"
#include "systemstub.h"
#include "util.h"
#include "video.h"

Menu::Menu(Resource *res, SystemStub *stub, Video *vid)
	: _res(res), _stub(stub), _vid(vid) {
	_skill = kSkillNormal;
	_level = 0;
}

void Menu::drawString(const char *str, int16_t y, int16_t x, uint8_t color) {
	debug(DBG_MENU, "Menu::drawString()");
	uint8_t v1b = _vid->_charFrontColor;
	uint8_t v2b = _vid->_charTransparentColor;
	uint8_t v3b = _vid->_charShadowColor;
	switch (color) {
	case 0:
		_vid->_charFrontColor = _charVar1;
		_vid->_charTransparentColor = _charVar2;
		_vid->_charShadowColor = _charVar2;
		break;
	case 1:
		_vid->_charFrontColor = _charVar2;
		_vid->_charTransparentColor = _charVar1;
		_vid->_charShadowColor = _charVar1;
		break;
	case 2:
		_vid->_charFrontColor = _charVar3;
		_vid->_charTransparentColor = 0xFF;
		_vid->_charShadowColor = _charVar1;
		break;
	case 3:
		_vid->_charFrontColor = _charVar4;
		_vid->_charTransparentColor = 0xFF;
		_vid->_charShadowColor = _charVar1;
		break;
	case 4:
		_vid->_charFrontColor = _charVar2;
		_vid->_charTransparentColor = 0xFF;
		_vid->_charShadowColor = _charVar1;
		break;
	case 5:
		_vid->_charFrontColor = _charVar2;
		_vid->_charTransparentColor = 0xFF;
		_vid->_charShadowColor = _charVar5;
		break;
	}

	drawString2(str, y, x);

	_vid->_charFrontColor = v1b;
	_vid->_charTransparentColor = v2b;
	_vid->_charShadowColor = v3b;
}

void Menu::drawString2(const char *str, int16_t y, int16_t x) {
	debug(DBG_MENU, "Menu::drawString2()");
	int w = Video::CHAR_W;
	int h = Video::CHAR_H;
	int len = 0;
	switch (_res->_type) {
	case kResourceTypeAmiga:
		for (; str[len]; ++len) {
			_vid->AMIGA_drawStringChar(_vid->_frontLayer, _vid->_w, Video::CHAR_W * (x + len), Video::CHAR_H * y, _res->_fnt, _vid->_charFrontColor, (uint8_t)str[len]);
		}
		break;
	case kResourceTypeDOS:
	case kResourceTypePC98:
	case kResourceTypeSega:
		for (; str[len]; ++len) {
			_vid->DOS_drawChar((uint8_t)str[len], y, x + len, true);
		}
		break;
	case kResourceTypeMac:
		for (; str[len]; ++len) {
			_vid->MAC_drawStringChar(_vid->_frontLayer, _vid->_w, Video::CHAR_W * (x + len), Video::CHAR_H * y, _res->_fnt, _vid->_charFrontColor, (uint8_t)str[len]);
		}
		break;
	}
	_vid->markBlockAsDirty(x * w, y * h, len * w, h, _vid->_layerScale);
}

void Menu::loadPicture(const char *prefix) {
	debug(DBG_MENU, "Menu::loadPicture('%s')", prefix);
	if (_res->isSegaCD()) {
		_res->clearLevelRes();
		_res->load("PRESENT", Resource::OT_MBK);
		_res->load("PRESENT", Resource::OT_PAL);
		_res->load("PRESENT", Resource::OT_LEV);
		_res->clearBankData();
		_vid->AMIGA_decodeLev(0, 0);
		File title;
		std::array<uint8_t, 32 + 28 * 6 * 32> logo{};
		if (title.open("TITLE.BIN", "rb", _res->_fs) &&
		    title.read(logo.data(), logo.size()) == logo.size()) {
			for (unsigned i = 0; i < 16; ++i) {
				const Color c =
				    Video::AMIGA_convertColor(READ_BE_UINT16(logo.data() + i * 2), true);
				_stub->setPaletteEntry(0xC0 + i, &c);
			}
			for (unsigned y = 0; y < 48; ++y)
				for (unsigned x = 0; x < 224; ++x) {
					const unsigned index = (y / 8 * 28 + x / 8) * 64 + y % 8 * 8 + x % 8;
					const unsigned pixel = (logo[32 + index / 2] >> (index % 2 ? 0 : 4)) & 15;
					if (pixel)
						_vid->_frontLayer[(y + 16) * 256 + x + 16] = 0xC0 + pixel;
				}
		}
		const Color white{255, 255, 255}, turquoise{0, 230, 215};
		_stub->setPaletteEntry(0xE1, &white);
		_stub->setPaletteEntry(0xE2, &turquoise);
		memcpy(_vid->_backLayer, _vid->_frontLayer, _vid->_layerSize);
		_vid->updateWidescreen();
		return;
	}
	if (_res->isMac()) {
		int title = kMacTitleScreen_Flashback;
		if (!strcmp(prefix, "menu2")) {
			title = kMacTitleScreen_RightEye;
		} else if (!strcmp(prefix, "menu3")) {
			title = kMacTitleScreen_LeftEye;
		} else if (!strncmp(prefix, "instr", 5)) {
			title = kMacTitleScreen_Controls;
		}
		if (title == kMacTitleScreen_Controls) {
			drawMacTitle(title);
			std::vector<uint8_t> overlay(_vid->_frontLayer, _vid->_frontLayer + _vid->_layerSize);
			drawMacTitle(kMacTitleScreen_LeftEye);
			for (int i = 0; i < _vid->_layerSize; ++i) {
				if (overlay[i]) {
					_vid->_frontLayer[i] = overlay[i];
				}
			}
		} else {
			drawMacTitle(title);
		}
		memcpy(_vid->_backLayer, _vid->_frontLayer, _vid->_layerSize);
		_vid->updateWidescreen();
		return;
	}
	static const int kPictureW = 256;
	static const int kPictureH = 224;
	if (_res->isPC98()) {
		_res->load_MAP_menu(prefix, _vid->_frontLayer);
	} else {
		_res->load_MAP_menu(prefix, _res->_scratchBuffer);
		for (int i = 0; i < 4; ++i) {
			for (int y = 0; y < kPictureH; ++y) {
				for (int x = 0; x < kPictureW / 4; ++x) {
					_vid->_frontLayer[i + x * 4 + kPictureW * y] = _res->_scratchBuffer[0x3800 * i + x + 64 * y];
				}
			}
		}
	}
	memcpy(_vid->_backLayer, _vid->_frontLayer, _vid->_layerSize);
	_res->load_PAL_menu(prefix, _res->_scratchBuffer);
	_stub->setPalette(_res->_scratchBuffer, 256);
	_vid->updateWidescreen();
}

void Menu::handleInfoScreen() {
	MenuInputScope input(_stub);
	debug(DBG_MENU, "Menu::handleInfoScreen()");
	_vid->fadeOut();
	if (_res->_lang == LANG_FR) {
		loadPicture("instru_f");
	} else {
		loadPicture("instru_e");
	}
	if (_res->isSegaCD()) {
		drawString("REMINISCENCE2 CONTROLS", 11, 3, 3);
		drawString("ESC: OPEN THE QOL MENU", 14, 3, 2);
		drawString("CONTROLS: YOUR KEY BINDINGS", 16, 3, 2);
		drawString("HELP: KEYBOARD AND MANUAL", 18, 3, 2);
		drawString("ENTER OR CLICK TO RETURN", 22, 3, 3);
	}
	_vid->fullRefresh();
	_vid->updateScreen();
	do {
		_stub->sleep(EVENTS_DELAY);
		_stub->processEvents();
		for (auto event = _stub->_menuInput.next(); event.type != MenuInput::None;
		     event = _stub->_menuInput.next()) {
			if (event.type == MenuInput::Confirm || event.type == MenuInput::Cancel ||
			    event.type == MenuInput::Click)
				return;
		}
	} while (!_stub->_pi.quit);
}

void Menu::handleSkillScreen() {
	MenuInputScope input(_stub);
	debug(DBG_MENU, "Menu::handleSkillScreen()");
	static const uint8_t colors[3][3] = {
		{ 2, 3, 3 }, // easy
		{ 3, 2, 3 }, // normal
		{ 3, 3, 2 }  // expert
	};
	_vid->fadeOut();
	loadPicture("menu3");
	_vid->fullRefresh();
	drawString(_res->getMenuString(LocaleData::LI_12_SKILL_LEVEL), 12, 4, 3);
	int currentSkill = _skill;
	do {
		drawString(_res->getMenuString(LocaleData::LI_13_EASY),   15, 14, colors[currentSkill][0]);
		drawString(_res->getMenuString(LocaleData::LI_14_NORMAL), 17, 14, colors[currentSkill][1]);
		drawString(_res->getMenuString(LocaleData::LI_15_EXPERT), 19, 14, colors[currentSkill][2]);

		_vid->updateScreen();
		_stub->sleep(EVENTS_DELAY);
		_stub->processEvents();

		for (auto event = _stub->_menuInput.next(); event.type != MenuInput::None;
		     event = _stub->_menuInput.next()) {
			bool confirm = event.type == MenuInput::Confirm;
			if (event.type == MenuInput::Up && currentSkill > kSkillEasy)
				--currentSkill;
			if (event.type == MenuInput::Down && currentSkill < kSkillExpert)
				++currentSkill;
			const int scale = _vid->_layerScale;
			for (int i = 0; i < 3; ++i) {
				if (event.inside(108 * scale, (116 + i * 16) * scale, 64 * scale, 16 * scale)) {
					currentSkill = i;
					confirm = event.type == MenuInput::Click;
					break;
				}
			}
			if (event.type == MenuInput::Cancel)
				return;
			if (confirm) {
				_skill = currentSkill;
				return;
			}
		}
	} while (!_stub->_pi.quit);
	_skill = 1;
}

bool Menu::handlePasswordScreen() {
	MenuInputScope input(_stub);
	debug(DBG_MENU, "Menu::handlePasswordScreen()");
	_vid->fadeOut();
	_vid->_charShadowColor = _charVar1;
	_vid->_charTransparentColor = 0xFF;
	_vid->_charFrontColor = _charVar4;
	_vid->fullRefresh();
	char password[7];
	int len = 0;
	do {
		loadPicture("menu2");
		drawString2(_res->getMenuString(LocaleData::LI_16_ENTER_PASSWORD1), 15, 3);
		drawString2(_res->getMenuString(LocaleData::LI_17_ENTER_PASSWORD2), 17, 3);

		for (int i = 0; i < len; ++i) {
			char glyph[] = {password[i], 0};
			drawString2(glyph, 21, i + 15);
		}
		drawString2(" ", 21, len + 15);

		_vid->markBlockAsDirty(15 * Video::CHAR_W, 21 * Video::CHAR_H, (len + 1) * Video::CHAR_W, Video::CHAR_H, _vid->_layerScale);
		_vid->updateScreen();
		_stub->sleep(EVENTS_DELAY);
		_stub->processEvents();
		char c = _stub->_pi.lastChar;
		if (c != 0) {
			_stub->_pi.lastChar = 0;
			if (len < 6) {
				if ((c >= 'A' && c <= 'Z') || (c == 0x20)) {
					password[len] = c;
					++len;
				}
			}
		}
		for (auto event = _stub->_menuInput.next(); event.type != MenuInput::None;
		     event = _stub->_menuInput.next()) {
			if (event.type == MenuInput::Erase) {
				if (len > 0) {
					--len;
				}
			}
			if (event.type == MenuInput::Cancel) {
				return false;
			}
			if (event.type == MenuInput::Confirm) {
				password[len] = '\0';
				for (int level = 0; level < 8; ++level) {
					for (int skill = 0; skill < 3; ++skill) {
						if (strcmp(getLevelPassword(level, skill), password) == 0) {
							_level = level;
							_skill = skill;
							return true;
						}
					}
				}
				return false;
			}
		}
	} while (!_stub->_pi.quit);
	return false;
}

bool Menu::handleLevelScreen() {
	MenuInputScope input(_stub);
	debug(DBG_MENU, "Menu::handleLevelScreen()");
	_vid->fadeOut();
	loadPicture("menu2");
	_vid->fullRefresh();
	int currentSkill = _skill;
	int currentLevel = _level;
	do {
		for (int i = 0; i < 8; ++i) {
			drawString(getLevelName(i), 7 + i * 2, 4, (currentLevel == i) ? 2 : 3);
		}
		_vid->markBlockAsDirty(4 * Video::CHAR_W, 7 * Video::CHAR_H, 192, 7 * Video::CHAR_H, _vid->_layerScale);

		drawString(_res->getMenuString(LocaleData::LI_13_EASY),   23,  4, (currentSkill == 0) ? 2 : 3);
		drawString(_res->getMenuString(LocaleData::LI_14_NORMAL), 23, 14, (currentSkill == 1) ? 2 : 3);
		drawString(_res->getMenuString(LocaleData::LI_15_EXPERT), 23, 24, (currentSkill == 2) ? 2 : 3);
		_vid->markBlockAsDirty(4 * Video::CHAR_W, 23 * Video::CHAR_H, 192, Video::CHAR_H, _vid->_layerScale);

		_vid->updateScreen();
		_stub->sleep(EVENTS_DELAY);
		_stub->processEvents();

		for (auto event = _stub->_menuInput.next(); event.type != MenuInput::None;
		     event = _stub->_menuInput.next()) {
			bool confirm = event.type == MenuInput::Confirm;
			if (event.type == MenuInput::Up || event.type == MenuInput::Down)
				currentLevel = (currentLevel + (event.type == MenuInput::Up ? 7 : 1)) % 8;
			if (event.type == MenuInput::Left && currentSkill > kSkillEasy)
				--currentSkill;
			if (event.type == MenuInput::Right && currentSkill < kSkillExpert)
				++currentSkill;
			const int scale = _vid->_layerScale;
			for (int i = 0; i < 8; ++i) {
				if (event.inside(28 * scale, (52 + i * 16) * scale, 200 * scale, 16 * scale)) {
					currentLevel = i;
					confirm = event.type == MenuInput::Click;
					break;
				}
			}
			if (event.type == MenuInput::Click) {
				for (int i = 0; i < 3; ++i)
					if (event.inside((28 + i * 80) * scale, 180 * scale, 64 * scale, 16 * scale))
						currentSkill = i;
			}
			if (event.type == MenuInput::Cancel)
				return false;
			if (confirm) {
				_skill = currentSkill;
				_level = currentLevel;
				return true;
			}
		}
	} while (!_stub->_pi.quit);
	return false;
}

void Menu::handleTitleScreen() {
	MenuInputScope input(_stub);
	debug(DBG_MENU, "Menu::handleTitleScreen()");

	_charVar1 = 0;
	_charVar2 = 0;
	_charVar3 = 0;
	_charVar4 = 0;
	_charVar5 = 0;

	static const int MAX_MENU_ITEMS = 6;
	Item menuItems[MAX_MENU_ITEMS];
	int menuItemsCount = 0;

	menuItems[menuItemsCount].str = LocaleData::LI_07_START;
	menuItems[menuItemsCount].opt = MENU_OPTION_ITEM_START;
	++menuItemsCount;
	if (!_res->_isDemo) {
		if (g_options.enable_password_menu) {
			menuItems[menuItemsCount].str = LocaleData::LI_08_SKILL;
			menuItems[menuItemsCount].opt = MENU_OPTION_ITEM_SKILL;
			++menuItemsCount;
			menuItems[menuItemsCount].str = LocaleData::LI_09_PASSWORD;
			menuItems[menuItemsCount].opt = MENU_OPTION_ITEM_PASSWORD;
			++menuItemsCount;
		} else {
			menuItems[menuItemsCount].str = LocaleData::LI_06_LEVEL;
			menuItems[menuItemsCount].opt = MENU_OPTION_ITEM_LEVEL;
			++menuItemsCount;
		}
	}
	menuItems[menuItemsCount].str = LocaleData::LI_10_INFO;
	menuItems[menuItemsCount].opt = MENU_OPTION_ITEM_INFO;
	++menuItemsCount;
	menuItems[menuItemsCount].str = LocaleData::LI_23_DEMO;
	menuItems[menuItemsCount].opt = MENU_OPTION_ITEM_DEMO;
	++menuItemsCount;
	menuItems[menuItemsCount].str = LocaleData::LI_11_QUIT;
	menuItems[menuItemsCount].opt = MENU_OPTION_ITEM_QUIT;
	++menuItemsCount;

	_selectedOption = -1;
	_nextScreen = SCREEN_TITLE;

	int currentEntry = 0;

	static const struct {
		Language lang;
		const uint8_t *bitmap16x12;
	} languages[] = {
		{ LANG_EN, _flagEn16x12 },
		{ LANG_FR, _flagFr16x12 },
		{ LANG_RU, _flagRu16x12 },
		{ LANG_DE, _flagDe16x12 },
		{ LANG_SP, _flagSp16x12 },
		{ LANG_IT, _flagIt16x12 },
		{ LANG_JP, _flagJp16x12 },
	};
	int currentLanguage = 0;
	for (int i = 0; i < ARRAYSIZE(languages); ++i) {
		if (languages[i].lang == _res->_lang) {
			currentLanguage = i;
			break;
		}
	}

	while (!_stub->_pi.quit) {
		if (_stub->_pi.escape || _stub->consumePauseRequest()) {
			_stub->_pi.escape = false;
			_selectedOption = MENU_OPTION_ITEM_SETTINGS;
			return;
		}

		int selectedItem = -1;
		int previousLanguage = currentLanguage;

		if (_nextScreen == SCREEN_TITLE) {
			_vid->fadeOut();
			loadPicture("menu1");
			_vid->fullRefresh();
			_charVar1 = _res->isMac() ? 0xE0 : 0;
			_charVar3 = _res->isMac() ? 0xE4 : 1;
			_charVar4 = _res->isMac() ? 0xE8 : 2;
			if (_res->isSegaCD()) {
				_charVar3 = 0xE1;
				_charVar4 = 0xE2;
			}
			currentEntry = 0;
			_nextScreen = -1;
		}

		_stub->processEvents();
		for (auto event = _stub->_menuInput.next(); event.type != MenuInput::None;
		     event = _stub->_menuInput.next()) {
			if (event.type == MenuInput::Cancel) {
				_selectedOption = MENU_OPTION_ITEM_SETTINGS;
				return;
			}
			if (g_options.enable_language_selection) {
				if (event.type == MenuInput::Left || event.type == MenuInput::Right)
					currentLanguage =
					    (currentLanguage +
						 (event.type == MenuInput::Left ? ARRAYSIZE(languages) - 1 : 1)) %
					    ARRAYSIZE(languages);
			}
			if (event.type == MenuInput::Up || event.type == MenuInput::Down)
				currentEntry =
				    (currentEntry + (event.type == MenuInput::Up ? menuItemsCount - 1 : 1)) %
				    menuItemsCount;
			const int scale = _vid->_layerScale;
			for (int i = 0; i < menuItemsCount; ++i) {
				if (event.inside(156 * scale,
				                 ((26 - menuItemsCount * 2 + i * 2) * Video::CHAR_H - 4) * scale,
				                 100 * scale, 16 * scale)) {
					currentEntry = i;
					if (event.type == MenuInput::Click)
						selectedItem = i;
					break;
				}
			}
			if (event.type == MenuInput::Confirm)
				selectedItem = currentEntry;
			if (selectedItem != -1)
				break;
		}
		if (selectedItem != -1) {
			_selectedOption = menuItems[selectedItem].opt;
			switch (_selectedOption) {
			case MENU_OPTION_ITEM_START:
				return;
			case MENU_OPTION_ITEM_SKILL:
				handleSkillScreen();
				break;
			case MENU_OPTION_ITEM_PASSWORD:
				if (handlePasswordScreen()) {
					return;
				}
				break;
			case MENU_OPTION_ITEM_LEVEL:
				if (handleLevelScreen()) {
					return;
				}
				break;
			case MENU_OPTION_ITEM_INFO:
				handleInfoScreen();
				break;
			case MENU_OPTION_ITEM_DEMO:
				return;
			case MENU_OPTION_ITEM_QUIT:
				return;
			}
			_nextScreen = SCREEN_TITLE;
			continue;
		}

		if (previousLanguage != currentLanguage) {
			_res->setLanguage(languages[currentLanguage].lang);
			// clear previous language text
			memcpy(_vid->_frontLayer, _vid->_backLayer, _vid->_layerSize);
		}

		// draw the options
		const int yPos = 26 - menuItemsCount * 2;
		for (int i = 0; i < menuItemsCount; ++i) {
			drawString(_res->getMenuString(menuItems[i].str), yPos + i * 2, 20, (i == currentEntry) ? 2 : 3);
		}

		// draw the language flag in the top right corner
		if (previousLanguage != currentLanguage) {
			_stub->copyRect(0, 0, _vid->_w, _vid->_h, _vid->_frontLayer, _vid->_w);
			static const int flagW = 16;
			static const int flagH = 12;
			const int flagX = _vid->_w - flagW - 8;
			static const int flagY = 8;
			_stub->copyRectRgb24(flagX, flagY, flagW, flagH, languages[currentLanguage].bitmap16x12);
		}
		_vid->updateScreen();
		_stub->sleep(EVENTS_DELAY);
	}
}

const char *Menu::getLevelPassword(int level, int skill) const {
	switch (_res->_type) {
	case kResourceTypeAmiga:
		if (level < 7) {
			if (_res->_lang == LANG_FR) {
				return _passwordsFrAmiga[skill * 7 + level];
			} else {
				return _passwordsEnAmiga[skill * 7 + level];
			}
		}
		break;
	case kResourceTypeMac:
		return _passwordsMac[skill * 8 + level];
	case kResourceTypeDOS:
	case kResourceTypePC98:
	case kResourceTypeSega:
		// default
		break;
	}
	return _passwordsDOS[skill * 8 + level];
}

const char *Menu::getLevelName(int level) const {
	if (level == 7) {
		return "CREDITS SEQUENCE";
	}
	if (level < 0 || level >= 7) {
		return "";
	}
	if (_res->_lang == LANG_RU) {
		return _levelNamesRu[level];
	} else {
		return _levelNames[level];
	}
}

void Menu::drawMacTitle(int num) {
	const int w = 512;
	int h = 384;
	int clutBaseColor = 0;
	switch (num) {
	case Menu::kMacTitleScreen_MacPlay:
		break;
	case Menu::kMacTitleScreen_Presage:
		clutBaseColor = 12;
		break;
	case Menu::kMacTitleScreen_Flashback:
	case Menu::kMacTitleScreen_LeftEye:
	case Menu::kMacTitleScreen_RightEye:
		h = 448;
		break;
	case Menu::kMacTitleScreen_Controls:
		break;
	}
	DecodeBuffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.ptr = _vid->_frontLayer;
	buf.pitch = buf.w = _vid->_w;
	buf.h = _vid->_h;
	buf.x = (_vid->_w - w) / 2;
	buf.y = (_vid->_h - h) / 2;
	memset(_vid->_frontLayer, 0, _vid->_layerSize);
	_res->MAC_loadTitleImage(num, &buf);
	for (int i = 0; i < 12; ++i) {
		Color palette[16];
		_res->MAC_copyClut16(palette, 0, clutBaseColor + i);
		const int basePaletteColor = i * 16;
		for (int j = 0; j < 16; ++j) {
			_stub->setPaletteEntry(basePaletteColor + j, &palette[j]);
		}
	}
	if (num == kMacTitleScreen_MacPlay || num == kMacTitleScreen_Controls) {
		for (int bank = 0; bank < 2; ++bank) {
			Color palette[16];
			_res->MAC_copyClut16(palette, 0, 55 + bank);
			for (int i = 0; i < 16; ++i) {
				_stub->setPaletteEntry((12 + bank) * 16 + i, &palette[i]);
			}
		}
	} else if (num == Menu::kMacTitleScreen_Presage) {
		Color c;
		c.r = c.g = c.b = 0;
		_stub->setPaletteEntry(0, &c);
	} else {
		_vid->setTextPalette();
		_vid->_charShadowColor = 0xE0;
	}
	_vid->fullRefresh();
}
