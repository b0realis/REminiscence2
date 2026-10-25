// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "runtime_options.h"
#include <SDL_scancode.h>
#include <SDL_keyboard.h>
#include <SDL_gamecontroller.h>
#include <algorithm>
#include <tuple>

namespace settings {
const char *filterScalerName(Filter filter) {
	switch (filter) {
	case Filter::Point:
		return "point";
	case Filter::Linear:
		return "linear";
	case Filter::Xbrz:
		return "xbrz";
	default:
		return "scale";
	}
}
Filter filterFromScalerName(const char *name) {
	if (name && !strcmp(name, "point"))
		return Filter::Point;
	if (name && !strcmp(name, "linear"))
		return Filter::Linear;
	if (name && (!strcmp(name, "xbrz") || !strcmp(name, "xbr")))
		return Filter::Xbrz;
	return Filter::Scale;
}
int filterMinScale(Filter filter) {
	return filter == Filter::Scale || filter == Filter::Xbrz ? 2 : 1;
}
int filterMaxScale(Filter filter) {
	return filter == Filter::Scale ? 4 : 6;
}

const char *presentationName(Presentation preset) {
	switch (preset) {
	case Presentation::ClassicPixels: return "Classic pixels";
	case Presentation::WidescreenScenery: return "Widescreen scenery";
	case Presentation::SmoothWidescreen: return "Smooth widescreen";
	default: return "Custom";
	}
}
void applyPresentation(Preferences &p, Presentation preset) {
	switch (preset) {
	case Presentation::ClassicPixels:
	case Presentation::WidescreenScenery:
	case Presentation::SmoothWidescreen:
		p.filter = preset == Presentation::SmoothWidescreen ? Filter::Xbrz : Filter::Point;
		p.scale = 3;
		p.integerScale = preset == Presentation::ClassicPixels;
		p.widescreen = preset == Presentation::ClassicPixels ? kWidescreenNone : kWidescreenAdjacentRooms;
		break;
	default:
		break;
	}
}
Presentation presentation(const Preferences &p) {
	for (auto preset : {Presentation::ClassicPixels, Presentation::WidescreenScenery,
	                    Presentation::SmoothWidescreen}) {
		auto expected = p;
		applyPresentation(expected, preset);
		if (p == expected)
			return preset;
	}
	return Presentation::Custom;
}

void Preferences::readCompatibility(const Options &o) {
	conradShirt = o.use_white_tshirt ? appearance::Shirt::White : appearance::Shirt::Original;
	fadeTransitions = o.fade_out_palette;
	inventoryOriginal = o.order_inventory_original;
	saveChime = o.play_gamesaved_sound;
	restoreMemo = o.restore_memo_cutscene;
	textCutscenes = o.use_text_cutscenes;
	seqCutscenes = o.use_seq_cutscenes;
	extraAsc = o.play_asc_cutscene;
	extraCaillou = o.play_caillou_cutscene;
	extraCarte = o.play_carte_cutscene;
	extraMetro = o.play_metro_cutscene;
	extraSerrure = o.play_serrure_cutscene;
}
void Preferences::applyCompatibility(Options &o) const {
	o.use_white_tshirt = conradShirt == appearance::Shirt::White;
	o.fade_out_palette = fadeTransitions;
	o.order_inventory_original = inventoryOriginal;
	o.play_gamesaved_sound = saveChime;
	o.restore_memo_cutscene = restoreMemo;
	o.use_text_cutscenes = textCutscenes;
	o.use_seq_cutscenes = seqCutscenes;
	o.play_asc_cutscene = extraAsc;
	o.play_caillou_cutscene = extraCaillou;
	o.play_carte_cutscene = extraCarte;
	o.play_metro_cutscene = extraMetro;
	o.play_serrure_cutscene = extraSerrure;
}

bool Preferences::operator==(const Preferences &o) const {
	return std::tie(fullscreen, vsync, pauseOnFocusLoss, showFps, showMenuButton, menuLayout,
	                filter, scale, widescreen, uiScale, masterVolume, musicVolume, effectsVolume,
	                muted, rewind, fadeTransitions, inventoryOriginal, saveChime, restoreMemo,
	                textCutscenes, seqCutscenes, extraAsc, extraCaillou, extraCarte, extraMetro,
	                extraSerrure, conradShirt, keys, padButtons, joystickButtons, stickDeadzone,
	                rightStickMovement, storyJournal, journalNotifications, diskAutosave,
	                contextualHints, diskAutosaveSeconds, stickyAction, reduceShake, gameSpeed,
	                objectivesNotebook, reduceBlink, inputBuffer, pistolStyle, laserColor,
	                skipLogosWithKeyboard, skipCutscenesWithKeyboard, quickResume, assetSet,
	                musicSource, exclusiveFullscreen, integerScale, windowWidth, windowHeight,
	                fullscreenWidth, fullscreenHeight) ==
	       std::tie(o.fullscreen, o.vsync, o.pauseOnFocusLoss, o.showFps, o.showMenuButton,
	                o.menuLayout, o.filter, o.scale, o.widescreen, o.uiScale, o.masterVolume,
	                o.musicVolume, o.effectsVolume, o.muted, o.rewind, o.fadeTransitions,
	                o.inventoryOriginal, o.saveChime, o.restoreMemo, o.textCutscenes,
	                o.seqCutscenes, o.extraAsc, o.extraCaillou, o.extraCarte, o.extraMetro,
	                o.extraSerrure, o.conradShirt, o.keys, o.padButtons, o.joystickButtons,
	                o.stickDeadzone, o.rightStickMovement, o.storyJournal, o.journalNotifications,
	                o.diskAutosave, o.contextualHints, o.diskAutosaveSeconds, o.stickyAction,
	                o.reduceShake, o.gameSpeed, o.objectivesNotebook, o.reduceBlink, o.inputBuffer,
	                o.pistolStyle, o.laserColor, o.skipLogosWithKeyboard,
	                o.skipCutscenesWithKeyboard, o.quickResume, o.assetSet, o.musicSource,
	                o.exclusiveFullscreen, o.integerScale, o.windowWidth, o.windowHeight,
	                o.fullscreenWidth, o.fullscreenHeight);
}

const char *padBindingName(size_t i) {
	static const char *const names[] = {
	    "Action / run / fire", "Use / confirm", "Draw / holster", "Inventory",
	    "Pause / settings",    "Quick save",    "Quick load",     "Rewind"};
	return i < PadBindingCount ? names[i] : "Unknown";
}
const char *padButtonName(int button, int style) {
	if (button == 32)
		return "LT / L2";
	if (button == 33)
		return "RT / R2";
	if (style == 1) {
		static const char *const playstation[] = {
		    "Cross", "Circle", "Square", "Triangle", "Share / Create", "PS", "Options",
		    "L3",    "R3",     "L1",     "R1"};
		if (button >= 0 && button < 11)
			return playstation[button];
	}
	static const char *const names[] = {
	    "A",       "B",       "X",        "Y",        "Back",     "Guide",    "Start",
	    "L-stick", "R-stick", "LB",       "RB",       "D-up",     "D-down",   "D-left",
	    "D-right", "Misc",    "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "Touchpad"};
	return button >= 0 && button < int(sizeof(names) / sizeof(names[0])) ? names[button]
	                                                                     : "Unassigned";
}
bool validPadBinding(int button, const Preferences &p, size_t index, bool raw,
                     std::string &reason) {
	if (index >= PadBindingCount || (raw && button == 9 && index != 4) || button < -1 ||
	    (raw ? button >= 32
	         : (button >= SDL_CONTROLLER_BUTTON_MAX && button != 32 && button != 33)) ||
	    (!raw && button >= SDL_CONTROLLER_BUTTON_DPAD_UP &&
	     button <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ||
	    (!raw && (button == SDL_CONTROLLER_BUTTON_GUIDE ||
	              (button == SDL_CONTROLLER_BUTTON_START && index != 4)))) {
		reason = "That button is reserved for movement or the system/menu. Choose another button.";
		return false;
	}
	const auto &buttons = raw ? p.joystickButtons : p.padButtons;
	for (size_t i = 0; i < buttons.size(); ++i)
		if (i != index && button >= 0 && buttons[i] == button) {
			reason = std::string("Already assigned to ") + padBindingName(i) + '.';
			return false;
		}
	reason.clear();
	return true;
}

const char *bindingName(size_t index) {
	static const char *const names[] = {
	    "Move left",     "Move right",     "Move up",   "Move down",       "Action / run / fire",
	    "Use / confirm", "Draw / holster", "Inventory", "Pause / settings"};
	return index < BindingCount ? names[index] : "Unknown";
}

bool validBinding(int key, const Preferences &p, size_t index, std::string &reason) {
	if (index >= BindingCount || key <= SDL_SCANCODE_UNKNOWN || key >= SDL_NUM_SCANCODES ||
	    !SDL_GetScancodeName(static_cast<SDL_Scancode>(key))[0] || key == SDL_SCANCODE_F10 ||
	    key == SDL_SCANCODE_LCTRL || key == SDL_SCANCODE_RCTRL || key == SDL_SCANCODE_LALT ||
	    key == SDL_SCANCODE_RALT || key == SDL_SCANCODE_LGUI || key == SDL_SCANCODE_RGUI ||
	    (key >= SDL_SCANCODE_F1 && key <= SDL_SCANCODE_F3)) {
		reason = "That key is reserved for display/engine shortcuts. F10 always opens settings.";
		return false;
	}
	for (size_t i = 0; i < BindingCount; ++i) {
		if (i != index && p.keys[i] == key) {
			reason = std::string("Already assigned to ") + bindingName(i) +
			         ". Choose another key first.";
			return false;
		}
	}
	return true;
}

bool validate(const Preferences &p, std::string &reason) {
	for (const auto size : {Resolution{p.windowWidth, p.windowHeight},
	                        Resolution{p.fullscreenWidth, p.fullscreenHeight}}) {
		if ((size.width || size.height) &&
		    (size.width < 160 || size.height < 120 || size.width > 16384 || size.height > 16384)) {
			reason = "Choose Automatic or a complete resolution between 160x120 and 16384x16384.";
			return false;
		}
	}
	for (const auto *source : {&p.assetSet, &p.musicSource}) {
		if (source->size() > 4096 || source->find_first_of("\r\n") != std::string::npos ||
		    source->find('\0') != std::string::npos) {
			reason = "Source selection is too long or contains a line break.";
			return false;
		}
	}
	if (int(p.pistolStyle) < 0 || int(p.pistolStyle) > 2 || int(p.laserColor) < 0 ||
	    int(p.laserColor) > 4 || int(p.filter) < 0 || int(p.filter) > int(Filter::Xbrz) ||
	    p.scale < filterMinScale(p.filter) || p.scale > filterMaxScale(p.filter) ||
	    p.widescreen < 0 || p.widescreen > 5 || p.uiScale < 75 || p.uiScale > 175 ||
	    p.masterVolume < 0 || p.masterVolume > 100 || p.musicVolume < 0 || p.musicVolume > 100 ||
	    p.effectsVolume < 0 || p.effectsVolume > 100 || p.menuLayout < 0 || p.menuLayout > 1 ||
	    int(p.conradShirt) < 0 || p.diskAutosaveSeconds < 15 || p.diskAutosaveSeconds > 600 ||
	    int(p.conradShirt) > 2 || p.gameSpeed < 50 || p.gameSpeed > 100 || p.stickDeadzone < 5 ||
	    p.stickDeadzone > 50) {
		reason = "A display, appearance or volume setting is outside its supported range.";
		return false;
	}
	for (size_t i = 0; i < PadBindingCount; ++i) {
		if (!validPadBinding(p.padButtons[i], p, i, false, reason) ||
		    !validPadBinding(p.joystickButtons[i], p, i, true, reason))
			return false;
	}
	for (size_t i = 0; i < BindingCount; ++i) {
		if (!validBinding(p.keys[i], p, i, reason)) {
			return false;
		}
	}
	reason.clear();
	return true;
}
} // namespace settings
