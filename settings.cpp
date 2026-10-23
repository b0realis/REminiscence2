// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "runtime_options.h"
#include <SDL_scancode.h>
#include <SDL_keyboard.h>
#include <algorithm>
#include <tuple>

namespace settings {
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
	                extraSerrure, conradShirt, keys) ==
	       std::tie(o.fullscreen, o.vsync, o.pauseOnFocusLoss, o.showFps, o.showMenuButton,
	                o.menuLayout, o.filter, o.scale, o.widescreen, o.uiScale, o.masterVolume,
	                o.musicVolume, o.effectsVolume, o.muted, o.rewind, o.fadeTransitions,
	                o.inventoryOriginal, o.saveChime, o.restoreMemo, o.textCutscenes,
	                o.seqCutscenes, o.extraAsc, o.extraCaillou, o.extraCarte, o.extraMetro,
	                o.extraSerrure, o.conradShirt, o.keys);
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
	if (int(p.filter) < 0 || int(p.filter) > 2 || p.scale < 1 || p.scale > 6 ||
	    (p.filter == Filter::Scale && (p.scale < 2 || p.scale > 4)) || p.widescreen < 0 ||
	    p.widescreen > 5 || p.uiScale < 75 || p.uiScale > 175 || p.masterVolume < 0 ||
	    p.masterVolume > 100 || p.musicVolume < 0 || p.musicVolume > 100 || p.effectsVolume < 0 ||
	    p.effectsVolume > 100 || p.menuLayout < 0 || p.menuLayout > 1 || int(p.conradShirt) < 0 ||
	    int(p.conradShirt) > 2) {
		reason = "A display, appearance or volume setting is outside its supported range.";
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
