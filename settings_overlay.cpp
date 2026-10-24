// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_overlay.h"
#include "keyboard_diagram.h"
#include "version.h"
#include "game.h"
#include "systemstub.h"
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace {
bool compactLayout = false;
const ImVec4 turquoise(0.12f, 0.82f, 0.76f, 1.0f);
const ImVec4 accent(0.24f, 0.60f, 1.0f, 1.0f);
bool match(const char *query, const char *label, const char *detail = "") {
	std::string haystack = std::string(label) + ' ' + detail;
	std::string needle(query);
	for (char &c : haystack)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	for (char &c : needle)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return haystack.find(needle) != std::string::npos;
}
void hint(const char *text) {
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.67f, 0.77f, 1));
	ImGui::TextWrapped("%s", text);
	ImGui::PopStyleColor();
}
void toggle(settings::Session &s, const char *query, const char *label, bool &value,
            const char *detail) {
	if (!match(query, label, detail))
		return;
	s.applyRequested |= ImGui::Checkbox(label, &value);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", detail);
}
void slider(settings::Session &s, const char *query, const char *label, int &value, int lo, int hi,
            const char *format) {
	if (!match(query, label))
		return;
	ImGui::PushID(label);
	if (compactLayout)
		ImGui::TextUnformatted(label);
	ImGui::SetNextItemWidth(
	    compactLayout ? -1 : std::max(140.0f, ImGui::GetContentRegionAvail().x * 0.56f));
	s.applyRequested |= ImGui::SliderInt(compactLayout ? "##value" : label, &value, lo, hi, format);
	ImGui::PopID();
}
void combo(settings::Session &s, const char *label, int &value, const char *items) {
	ImGui::PushID(label);
	if (compactLayout)
		ImGui::TextUnformatted(label);
	ImGui::SetNextItemWidth(compactLayout ? -1 : 250);
	s.applyRequested |= ImGui::Combo(compactLayout ? "##value" : label, &value, items);
	ImGui::PopID();
}
bool wideButton(const char *label) {
	const float width = std::min(
	    ImGui::GetContentRegionAvail().x,
	    std::max(240.0f, ImGui::CalcTextSize(label).x + 2 * ImGui::GetStyle().FramePadding.x));
	return ImGui::Button(label, ImVec2(compactLayout ? -1 : width, 0));
}
// Every destructive command uses a bounded popup that also fits 256 x 224.
void confirmation(const char *name, const char *message, const char *label,
                  const std::function<void()> &accept) {
	const auto size = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowSize(ImVec2(std::min(380.0f, size.x - 12), 0), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(size.x / 2, size.y / 2), ImGuiCond_Appearing,
	                        ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped("%s", message);
		if (ImGui::Button(label)) {
			accept();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
		    ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}
bool preferenceButtons(settings::Session &s, const settings::Preferences &opening) {
	bool resetCapture = false;
	if (wideButton("Save settings"))
		s.saveRequested = true;
	if (wideButton("Undo settings changes")) {
		s.values = opening;
		s.applyRequested = true;
		resetCapture = true;
	}
	if (wideButton("Restore default settings..."))
		ImGui::OpenPopup("Restore defaults?");
	confirmation("Restore defaults?",
	             "Restore built-in preferences? The saved profile changes only when you choose "
	             "Save settings.",
	             "Restore", [&] {
		             s.values = settings::Preferences{};
		             s.applyRequested = true;
		             resetCapture = true;
	             });
	return resetCapture;
}
void sessionPage(settings::Session &s, const char *query) {
	if (match(query, "Session progress health rewind screenshot restart title")) {
		ImGui::SeparatorText("Paused session");
		ImGui::BeginDisabled(s.autosaves.empty());
		if (wideButton("Continue latest autosave..."))
			ImGui::OpenPopup("Continue latest?");
		ImGui::EndDisabled();
		confirmation(
		    "Continue latest?",
		    "Restore the newest compatible autosave? Current unsaved progress will be replaced.",
		    "Continue", [&] { s.action = settings::Action::ContinueLatest; });
		ImGui::Text("%s / Level %d / Room %d", s.edition.c_str(), s.level + 1, s.room);
		ImGui::Text("Score %08u / Health %d", s.score, s.health);
		ImGui::Text("Rewind: %d snapshots", s.rewindCount);
		ImGui::BeginDisabled(!s.canRewind);
		if (wideButton("Rewind one snapshot"))
			s.action = settings::Action::Rewind;
		ImGui::EndDisabled();
		if (wideButton("Capture clean screenshot"))
			s.action = settings::Action::Screenshot;
		if (wideButton("Restart current level..."))
			ImGui::OpenPopup("Restart level?");
		confirmation("Restart level?",
		             "Restart this level at the current difficulty? Unsaved progress, score and "
		             "rewind history will be reset.",
		             "Restart", [&] {
			             s.targetLevel = s.level;
			             s.targetSkill = s.skill;
			             s.action = settings::Action::StartLevel;
		             });
		if (wideButton("Return to title..."))
			ImGui::OpenPopup("Return to title?");
		confirmation("Return to title?",
		             "Return to the title screen? Unsaved progress will be lost.", "Title",
		             [&] { s.action = settings::Action::Title; });
	}
	ImGui::SeparatorText("Everyday preferences");
	toggle(s, query, "Persistent autosaves", s.values.diskAutosave,
	       "Three rotating files, separate from manual slots. Save during living gameplay, at "
	       "checkpoints, and on clean exit.");
	slider(s, query, "Autosave interval", s.values.diskAutosaveSeconds, 15, 600, "%d seconds");
	toggle(s, query, "Contextual control hints", s.values.contextualHints,
	       "Optional short, non-spoiler reminders using your current bindings. Each appears once "
	       "per new game; pauses stop the timer.");
	toggle(s, query, "Rewind history", s.values.rewind,
	       "Keep snapshots about every five seconds. Disabling clears history.");
	toggle(s, query, "Pause when unfocused", s.values.pauseOnFocusLoss,
	       "Pause gameplay and audio when the window loses focus.");
	toggle(s, query, "Show frame rate", s.values.showFps,
	       "Show measured presentation rate; simulation speed is unchanged.");
	toggle(s, query, "Show QoL shortcut button", s.values.showMenuButton,
	       "Display a clickable Menu [F10] shortcut during gameplay.");
}
const settings::SaveSlot *saveInfo(const settings::Session &s, int slot) {
	for (const auto &info : s.saves)
		if (info.slot == slot && info.level == s.browseLevel)
			return &info;
	return nullptr;
}
void selectSave(settings::Session &s, int level, int slot) {
	s.browseLevel = level;
	s.saveSlot = slot;
	const auto *info = saveInfo(s, slot);
	s.saveName.fill(0);
	if (info)
		snprintf(s.saveName.data(), s.saveName.size(), "%s", info->label.c_str());
	s.protectSave = info && info->protectedSave;
}
void savesPage(settings::Session &s, const char *query,
               const std::function<void(std::shared_ptr<const progress::Preview>)> &preview) {
	if (!match(query, "Save load slot checkpoint progress refresh name protect milestone browser"))
		return;
	ImGui::SeparatorText(compactLayout ? "Manual saves" : "Manual save browser / all levels");
	int level = s.browseLevel, slot = s.saveSlot;
	if (!compactLayout)
		ImGui::Text("Playing level %d", s.level + 1);
	ImGui::SetNextItemWidth(-1);
	if (ImGui::Combo("##browselevel", &level,
	                 "Level 1\0Level 2\0Level 3\0Level 4\0Level 5\0Level 6\0Level 7\0"))
		selectSave(s, level, slot);
	ImGui::SetNextItemWidth(-1);
	if (ImGui::SliderInt("##slot", &slot, 1, 99, "Manual slot %02d"))
		selectSave(s, s.browseLevel, slot);
	const auto *selected = saveInfo(s, s.saveSlot);
	const bool exists = selected && selected->exists;
	const bool supported = selected && selected->supported;
	if (exists) {
		if (compactLayout) {
			ImGui::Text("Room %d%s", selected->room,
			            selected->protectedSave ? " / LOCKED"
			            : selected->supported   ? ""
			                                    : " / invalid");
		} else {
			ImGui::Text("Level %d / Room %d%s", selected->level + 1, selected->room,
			            selected->supported ? "" : " / incompatible");
			ImGui::TextWrapped("%s", selected->modified.c_str());
			if (selected->protectedSave)
				ImGui::TextColored(turquoise, "PROTECTED MILESTONE");
			if (!selected->label.empty())
				ImGui::TextWrapped("%s", selected->label.c_str());
		}
	} else
		hint("Empty slot");
	if (ImGui::BeginTable("Manual save actions", compactLayout ? 1 : 2,
	                      ImGuiTableFlags_SizingStretchSame)) {
		ImGui::TableNextColumn();
		ImGui::BeginDisabled(!supported);
		if (wideButton("Load game..."))
			ImGui::OpenPopup("Load save?");
		ImGui::EndDisabled();
		confirmation(
		    "Load save?",
		    "Restore this save, switching levels if needed? Unsaved progress will be replaced.",
		    "Load", [&] { s.action = settings::Action::LoadGame; });
		if (!s.recovery) {
			ImGui::BeginDisabled(!s.canSave || !s.saveSlot || s.browseLevel != s.level ||
			                     (selected && selected->protectedSave));
			if (wideButton("Save game")) {
				if (exists)
					ImGui::OpenPopup("Overwrite save?");
				else
					s.action = settings::Action::SaveGame;
			}
			ImGui::EndDisabled();
			confirmation("Overwrite save?", "Replace this manual save with your current progress?",
			             "Save", [&] { s.action = settings::Action::SaveGame; });
		}
		if (ImGui::CollapsingHeader("Name / milestone protection")) {
			if (compactLayout && selected) {
				ImGui::TextWrapped("%s", selected->modified.c_str());
				ImGui::TextWrapped("%s", selected->label.c_str());
			}
			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##savename", "Save name (up to 96 ASCII characters)",
			                         s.saveName.data(), s.saveName.size());
			ImGui::BeginDisabled(s.saveSlot == 0);
			ImGui::Checkbox("Protect from overwrite", &s.protectSave);
			ImGui::EndDisabled();
			ImGui::BeginDisabled(!selected || !s.saveSlot || !selected->modern || !supported);
			if (wideButton("Update name / protection..."))
				ImGui::OpenPopup("Update save details?");
			ImGui::EndDisabled();
			confirmation("Update save details?",
			             "Update this save's name and overwrite protection? Its game state and "
			             "preview are kept.",
			             "Update", [&] { s.action = settings::Action::UpdateSaveDetails; });
		}
		const auto *checkpoint = saveInfo(s, 0);
		ImGui::BeginDisabled(!checkpoint || !checkpoint->supported);
		if (wideButton("Load checkpoint..."))
			ImGui::OpenPopup("Load checkpoint?");
		ImGui::EndDisabled();
		confirmation("Load checkpoint?", "Restore the checkpoint for the browsed level?", "Load",
		             [&] {
			             s.commandSlot = 0;
			             s.action = settings::Action::LoadGame;
		             });
		ImGui::TableNextColumn();
		if (selected)
			preview(selected->preview);
		ImGui::EndTable();
	}
	if (wideButton("Refresh saves"))
		s.refreshRequested = true;
	hint("Load any reached save without changing levels first. New saves include a preview; legacy "
	     "saves remain loadable. Save into the playing level. Unlock a milestone using Update "
	     "before overwriting it.");
	ImGui::SeparatorText("Stored saves / all levels");
	for (const auto &info : s.saves) {
		if (!info.exists)
			continue;
		ImGui::PushID(info.level * 100 + info.slot);
		const std::string label = "L" + std::to_string(info.level + 1) + " / " +
		                          (info.slot ? "Slot " + std::to_string(info.slot) : "Checkpoint") +
		                          (info.protectedSave ? " [locked]" : "") +
		                          (info.supported ? "" : " [invalid]");
		if (ImGui::Selectable(label.c_str(),
		                      info.slot == s.saveSlot && info.level == s.browseLevel))
			selectSave(s, info.level, info.slot);
		if (!info.label.empty())
			ImGui::TextWrapped("%s", info.label.c_str());
		ImGui::PopID();
	}
}

void cheatsPage(settings::Session &s, const char *query) {
	ImGui::SeparatorText("Session cheats");
	hint("Cheats apply immediately, affect this session and are not saved as preferences. Scripted "
	     "deaths may still occur.");
	auto flag = [&](const char *label, uint32_t mask, const char *detail) {
		if (!match(query, label, detail))
			return;
		bool enabled = (s.cheats & mask) != 0;
		if (ImGui::Checkbox(label, &enabled)) {
			s.cheats = enabled ? s.cheats | mask : s.cheats & ~mask;
			s.cheatsRequested = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", detail);
	};
	flag("One-hit enemy kills", kCheatOneHitKill,
	     "Enemies lose their remaining life after a successful hit.");
	flag("Ignore projectile / gun hits", kCheatNoHit,
	     "Ignore the engine's gun-hit test; this does not disable all hazards.");
	flag("Keep life counter", kCheatLifeCounter, "Cancel normal life-counter decrements.");
	auto debugFlag = [&](const char *label, uint8_t mask, const char *detail) {
		if (!match(query, label, detail))
			return;
		bool enabled = (s.debugFlags & mask) != 0;
		if (ImGui::Checkbox(label, &enabled)) {
			s.debugFlags = enabled ? s.debugFlags | mask : s.debugFlags & ~mask;
			s.cheatsRequested = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", detail);
	};
	debugFlag("Unlimited life", PlayerInput::DF_SETLIFE,
	          "Ctrl+I: maintain a very large life counter while enabled.");
	debugFlag("Fast mode", PlayerInput::DF_FASTMODE,
	          "Ctrl+F: original engine fast mode; changes simulation pacing.");
	debugFlag("Collision / dirty rectangles", PlayerInput::DF_DBLOCKS,
	          "Ctrl+B: show the engine's diagnostic rectangles.");
	ImGui::BeginDisabled(!s.autoZoomAvailable);
	debugFlag("Automatic combat zoom (DOS)", PlayerInput::DF_AUTOZOOM,
	          "Ctrl+G: original DOS combat zoom, unavailable for other editions.");
	ImGui::EndDisabled();
	if (match(query, "Restore health clear cheats")) {
		ImGui::BeginDisabled(!s.canSave);
		if (wideButton("Restore health once"))
			s.action = settings::Action::Heal;
		ImGui::EndDisabled();
		if (wideButton("Turn off all cheats / debug")) {
			s.cheats = 0;
			s.debugFlags = 0;
			s.cheatsRequested = true;
		}
	}
}
void levelsPage(settings::Session &s, const char *query) {
	if (!match(query, "Level select difficulty restart start jungle earth titan morphs"))
		return;
	ImGui::SeparatorText("Level selection");
	hint("Start a level with a fresh inventory and score. Existing save files are kept; current "
	     "progress and rewind history are reset.");
	for (int level = 0; level < 7; ++level) {
		ImGui::PushID(level);
		ImGui::BeginDisabled(!s.levelAvailable[level]);
		char label[96];
		snprintf(label, sizeof(label), "%d / %s", level + 1, Menu::_levelNames[level]);
		if (ImGui::Selectable(label, s.targetLevel == level))
			s.targetLevel = level;
		ImGui::EndDisabled();
		ImGui::PopID();
	}
	ImGui::TextUnformatted("Difficulty");
	ImGui::SetNextItemWidth(-1);
	ImGui::Combo("##difficulty", &s.targetSkill, "Easy\0Normal\0Expert\0");
	ImGui::BeginDisabled(!s.levelAvailable[s.targetLevel]);
	if (wideButton("Start selected level..."))
		ImGui::OpenPopup("Start selected level?");
	ImGui::EndDisabled();
	confirmation("Start selected level?",
	             "Start the selected level and difficulty? Unsaved progress, score and rewind "
	             "history will be reset.",
	             "Start", [&] { s.action = settings::Action::StartLevel; });
}
void graphicsPage(settings::Session &s, const char *query) {
	ImGui::SeparatorText("Window and display");
	toggle(s, query, "Fullscreen", s.values.fullscreen,
	       "Apply immediately. Alt+Enter remains available outside the menu.");
	toggle(s, query, "Vertical sync", s.values.vsync,
	       "Ask the renderer to synchronize presentation to the display. Gameplay timing is "
	       "unchanged.");
	if (match(query, "Pixel filter point nearest linear ScaleNx")) {
		int filter = int(s.values.filter);
		const int previous = filter;
		combo(s, "Pixel filter", filter,
		      "Point / sharp pixels\0Linear / smooth pixels\0ScaleNx / edge-aware\0");
		if (filter != previous) {
			s.values.filter = settings::Filter(filter);
			if (filter == 2)
				s.values.scale = std::max(2, std::min(4, s.values.scale));
		}
	}
	slider(s, query, "Window scale", s.values.scale,
	       s.values.filter == settings::Filter::Scale ? 2 : 1,
	       s.values.filter == settings::Filter::Scale ? 4 : 6, "%dx");
	if (match(query, "Widescreen adjacent mirror blur borders")) {
		combo(s, "Side content", s.values.widescreen,
		      "Original playfield\0Adjacent rooms\0Adjacent rooms / blur\0Mirrored edges\0Blurred "
		      "backdrop\0Player-supplied CD-i borders\0");
		hint("Side content does not enlarge the playable area. CD-i borders need your own "
		     "flashp*.bob files.");
	}
	ImGui::SeparatorText("Menu readability");
	if (match(query, "Menu layout compact automatic"))
		combo(s, "Menu layout", s.values.menuLayout,
		      "Automatic / desktop\0Compact / single column\0");
	slider(s, query, "Menu text scale", s.values.uiScale, 75, 175, "%d%%");
	hint("Display changes apply while paused. Game artwork keeps the edition's original "
	     "resolution.");
}
void audioPage(settings::Session &s, const char *query) {
	ImGui::SeparatorText("Mix levels");
	toggle(s, query, "Mute all audio", s.values.muted,
	       "Mute output without discarding music or effect playback state.");
	slider(s, query, "Master volume", s.values.masterVolume, 0, 100, "%d%%");
	slider(s, query, "Music volume", s.values.musicVolume, 0, 100, "%d%%");
	slider(s, query, "Effects volume", s.values.effectsVolume, 0, 100, "%d%%");
	ImGui::Spacing();
	hint("Changes are audible immediately. Audio can continue while gameplay is paused so you can "
	     "adjust the mix.");
	ImGui::SeparatorText("Current device");
	ImGui::Text("Output: %d Hz / stereo", s.outputRate);
	ImGui::Text("PRF fallback: %s", s.audioBackend.c_str());
	hint("Output rate, soundfonts and synthesizer selection are startup settings. Use --outputrate "
	     "and --mididriver when launching.");
	toggle(s, query, "Save confirmation sound", s.values.saveChime,
	       "Play the save-confirmation sample after successful saves.");
}
void enhancementsPage(settings::Session &s, const char *query) {
	ImGui::SeparatorText("Play your way");
	if (match(
	        query,
	        "Conrad shirt t-shirt undershirt jacket original red pink white appearance clothing")) {
		int choice = int(s.values.conradShirt);
		combo(s, "Conrad's T-shirt", choice, "Original / asset colours\0Red / Pink\0White\0");
		s.values.conradShirt = appearance::Shirt(choice);
		hint("Changes only the T-shirt inside Conrad's jacket. The jacket, shoes, skin, hair and "
		     "equipment retain their colours. Original uses your edition's T-shirt artwork.");
	}
	toggle(s, query, "Rewind history", s.values.rewind,
	       "Session-only snapshots; use Ctrl+R to restore. Turning this off clears the history.");
	toggle(s, query, "New items at inventory bottom", s.values.inventoryOriginal,
	       "Changes placement of newly collected objects; existing objects are not reordered.");
	toggle(s, query, "Fade transitions to black", s.values.fadeTransitions,
	       "Affects future transitions; otherwise use the engine's palette blending.");
	toggle(s, query, "Save confirmation sound", s.values.saveChime,
	       "Play the save-confirmation sample after successful saves.");
	ImGui::SeparatorText("Accessibility assists (optional)");
	toggle(s, query, "Sticky Action button", s.values.stickyAction,
	       "Press Action once to hold run/fire, press again to release. Cleared on pause, focus "
	       "loss and disconnect. Also affects firing a drawn weapon.");
	toggle(
	    s, query, "Reduce screen shake", s.values.reduceShake,
	    "Keep the gameplay camera still during scripted shaking. Does not alter cutscene artwork.");
	toggle(s, query, "Short input buffer", s.values.inputBuffer,
	       "Keep jump (Up), Use and Weapon taps for at least 100 ms. Gameplay only; clear on "
	       "pause, disconnect and focus loss.");
	toggle(s, query, "Steady shield-hit sprite", s.values.reduceBlink,
	       "Keep Conrad visible during the shield-hit blink period; invulnerability timing is "
	       "unchanged. This is not a filter for movie/cutscene artwork.");
	slider(s, query, "Gameplay speed", s.values.gameSpeed, 50, 100, "%d%%");
	hint("100% preserves original timing. Slower speed gives more time to react without changing "
	     "movement steps or scripts. Music and cutscenes keep their normal speed.");
	ImGui::SeparatorText("Cutscene presentation");
	hint("These choices take effect on the next applicable scene and need the corresponding "
	     "edition resources.");
	toggle(s, query, "Restore extra MEMO content", s.values.restoreMemo,
	       "Restore the additional memory-sequence content.");
	toggle(s, query, "Text summaries", s.values.textCutscenes,
	       "Use available text summaries in place of polygon scenes.");
	toggle(s, query, "Prefer PC CD SEQ movies", s.values.seqCutscenes,
	       "Use available SEQ replacements on the next applicable scene.");
	toggle(s, query, "Optional ASC scene", s.values.extraAsc,
	       "Play the optional elevator sequence when supported.");
	toggle(s, query, "Optional CAILLOU scene", s.values.extraCaillou,
	       "Play the optional SET sequence at save checkpoints.");
	toggle(s, query, "Optional CARTE scene", s.values.extraCarte,
	       "Play the optional key-card sequence when supported.");
	toggle(s, query, "Optional METRO scene", s.values.extraMetro,
	       "Play the optional metro sequence when supported.");
	toggle(s, query, "Optional SERRURE scene", s.values.extraSerrure,
	       "Play the optional lock sequence when supported.");
}
} // namespace

SettingsOverlay::SettingsOverlay() {
	IMGUI_CHECKVERSION();
	_context = ImGui::CreateContext();
	auto &io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
	ImFontConfig font;
	font.SizePixels = 16;
	io.Fonts->AddFontDefault(&font);
	font.SizePixels = 11;
	io.Fonts->AddFontDefault(&font);
	ImGui::StyleColorsDark();
	auto &style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(18, 16);
	style.FramePadding = ImVec2(9, 6);
	style.ItemSpacing = ImVec2(12, 10);
	style.WindowRounding = 10;
	style.FrameRounding = 4;
	style.GrabRounding = 4;
	style.TabRounding = 4;
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.055f, 0.085f, 0.96f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.035f, 0.055f, 0.085f, 0.40f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.11f, 0.30f, 0.56f, 0.90f);
	style.Colors[ImGuiCol_HeaderActive] = accent;
	style.Colors[ImGuiCol_CheckMark] = accent;
	style.Colors[ImGuiCol_SliderGrab] = accent;
	style.Colors[ImGuiCol_TabSelected] = ImVec4(0.10f, 0.34f, 0.68f, 1);
	style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.25f, 0.44f, 1);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.39f, 0.68f, 1);
}
SettingsOverlay::~SettingsOverlay() {
	detach();
	ImGui::DestroyContext(_context);
}
bool SettingsOverlay::attach(SDL_Window *window, SDL_Renderer *renderer) {
	ImGui::SetCurrentContext(_context);
	if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer))
		return false;
	if (!ImGui_ImplSDLRenderer2_Init(renderer)) {
		ImGui_ImplSDL2_Shutdown();
		return false;
	}
	_renderer = renderer;
	_attached = true;
	return true;
}
void SettingsOverlay::detach() {
	clearPreview();
	if (!_attached)
		return;
	ImGui::SetCurrentContext(_context);
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	_attached = false;
	_renderer = nullptr;
}
void SettingsOverlay::resetRenderer() {
	clearPreview();
	if (!_attached)
		return;
	ImGui::SetCurrentContext(_context);
	ImGui_ImplSDLRenderer2_DestroyDeviceObjects();
}
void SettingsOverlay::clearPreview() {
	for (const auto &entry : _previews)
		SDL_DestroyTexture(entry.texture);
	_previews.clear();
}
void SettingsOverlay::drawPreview(std::shared_ptr<const progress::Preview> preview) {
	if (!preview || !preview->width || !preview->height ||
	    preview->rgba.size() != size_t(preview->width) * preview->height * 4)
		return;
	auto found = std::find_if(_previews.begin(), _previews.end(),
	                          [&](const auto &entry) { return entry.owner == preview; });
	if (found == _previews.end()) {
		auto *texture =
		    SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
			                  preview->width, preview->height);
		if (!texture)
			return;
		if (SDL_UpdateTexture(texture, nullptr, preview->rgba.data(), preview->width * 4) != 0) {
			SDL_DestroyTexture(texture);
			return;
		}
		SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
		_previews.push_back({texture, preview, ImGui::GetFrameCount()});
		found = _previews.end() - 1;
	}
	found->frame = ImGui::GetFrameCount();
	const float width = std::min(ImGui::GetContentRegionAvail().x, _native ? 80.0f : 224.0f);
	ImGui::Image(ImTextureID(intptr_t(found->texture)),
	             ImVec2(width, width * preview->height / preview->width));
}

void SettingsOverlay::open(const settings::Preferences &p) {
	ImGui::SetCurrentContext(_context);
	ImGui::GetIO().ClearInputKeys();
	_captureBinding = -1;
	_capturePad = -1;
	_closeArmed = false;
	_search[0] = 0;
	_opening = p;
	_firstDraw = true;
}
void SettingsOverlay::beginPadCapture(size_t action, bool raw) {
	_capturePad = action < settings::PadBindingCount ? int(action) : -1;
	_captureRaw = raw;
	_captureBinding = -1;
	_closeArmed = false;
}
void SettingsOverlay::processEvent(const SDL_Event &event, settings::Session *s) {
	ImGui::SetCurrentContext(_context);
	if (s && _capturePad >= 0) {
		if (event.type == SDL_KEYDOWN && (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE ||
		                                  event.key.keysym.scancode == SDL_SCANCODE_F10)) {
			_capturePad = -1;
			if (event.key.keysym.scancode == SDL_SCANCODE_F10)
				if (!s->recovery)
					s->action = settings::Action::Resume;
			s->status = "Button capture canceled.";
			return;
		}
		const bool trigger = !_captureRaw && event.type == SDL_CONTROLLERAXISMOTION &&
		                     event.caxis.axis >= SDL_CONTROLLER_AXIS_TRIGGERLEFT &&
		                     event.caxis.value >= 16000;
		if (trigger || (!_captureRaw && event.type == SDL_CONTROLLERBUTTONDOWN) ||
		    (_captureRaw && event.type == SDL_JOYBUTTONDOWN)) {
			const int button = trigger ? 32 + event.caxis.axis - SDL_CONTROLLER_AXIS_TRIGGERLEFT
			                   : _captureRaw ? event.jbutton.button
			                                 : event.cbutton.button;
			if (_capturePad != 4 && button == (_captureRaw ? 9 : SDL_CONTROLLER_BUTTON_START)) {
				_capturePad = -1;
				_closeArmed = false;
				s->status = "Button capture canceled.";
				return;
			}
			// Start/B remain navigation controls outside capture; during capture
			// no button event may accidentally resume the underlying game.
			std::string reason;
			if (settings::validPadBinding(button, s->values, _capturePad, _captureRaw, reason)) {
				auto &bindings = _captureRaw ? s->values.joystickButtons : s->values.padButtons;
				bindings[_capturePad] = button;
				s->applyRequested = true;
				s->status = "Joypad binding changed. Save settings to keep it.";
				s->statusError = false;
				_capturePad = -1;
				_closeArmed = false;
				ImGui::GetIO().ClearInputKeys();
			} else {
				s->status = reason;
				s->statusError = true;
			}
		}
		// Swallow all pad/keyboard events while waiting, including key-up.
		if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
		    (event.type >= SDL_JOYAXISMOTION && event.type <= SDL_CONTROLLERDEVICEREMAPPED))
			return;
	}
	// SDL cannot map every vintage USB joypad. Translate the fallback device's
	// UI controls independently of gameplay remapping, just like mapped pads.
	if (s && (event.type == SDL_JOYHATMOTION || event.type == SDL_JOYAXISMOTION)) {
		auto &io = ImGui::GetIO();
		if (event.type == SDL_JOYHATMOTION) {
			io.AddKeyEvent(ImGuiKey_UpArrow, event.jhat.value & SDL_HAT_UP);
			io.AddKeyEvent(ImGuiKey_DownArrow, event.jhat.value & SDL_HAT_DOWN);
			io.AddKeyEvent(ImGuiKey_LeftArrow, event.jhat.value & SDL_HAT_LEFT);
			io.AddKeyEvent(ImGuiKey_RightArrow, event.jhat.value & SDL_HAT_RIGHT);
		} else if (event.jaxis.axis < 2) {
			const int threshold = s->values.stickDeadzone * 32767 / 100;
			io.AddKeyEvent(event.jaxis.axis ? ImGuiKey_UpArrow : ImGuiKey_LeftArrow,
			               event.jaxis.value < -threshold);
			io.AddKeyEvent(event.jaxis.axis ? ImGuiKey_DownArrow : ImGuiKey_RightArrow,
			               event.jaxis.value > threshold);
		}
		return;
	}
	if (s && (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP)) {
		const bool pressed = event.jbutton.state == SDL_PRESSED;
		const int button = event.jbutton.button;
		if (button == 0 || button == 2 || button == 9)
			ImGui::GetIO().AddKeyEvent(button == 2 ? ImGuiKey_Enter : ImGuiKey_Escape, pressed);
		if (pressed && button == 4)
			_requestedTab = (_tab + PageCount - 1) % PageCount;
		if (pressed && button == 5)
			_requestedTab = (_tab + 1) % PageCount;
		return;
	}
	if (s && event.type == SDL_KEYDOWN && !event.key.repeat) {
		if (event.key.keysym.scancode == SDL_SCANCODE_F10) {
			_captureBinding = -1;
			if (!s->recovery)
				s->action = settings::Action::Resume;
			return;
		}
		if (_captureBinding >= 0) {
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
				_captureBinding = -1;
				s->status = "Key capture canceled.";
				s->statusError = false;
			} else {
				std::string reason;
				if (settings::validBinding(event.key.keysym.scancode, s->values, _captureBinding,
				                           reason)) {
					s->values.keys[_captureBinding] = event.key.keysym.scancode;
					s->applyRequested = true;
					s->status = "Binding changed. Save settings to keep it.";
					s->statusError = false;
					_captureBinding = -1;
				} else {
					s->status = reason;
					s->statusError = true;
				}
			}
			return;
		}
	}
	if (_attached)
		ImGui_ImplSDL2_ProcessEvent(&event);
}
void SettingsOverlay::beginFrame(const settings::Preferences &p) {
	ImGui::SetCurrentContext(_context);
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	auto &io = ImGui::GetIO();
	_native = io.DisplaySize.x < 400 || io.DisplaySize.y < 300;
	_compact = _native || p.menuLayout == 1 || io.DisplaySize.x < 700 || io.DisplaySize.y < 500;
	compactLayout = _compact;
	io.FontDefault = io.Fonts->Fonts[_native ? 1 : 0];
	io.FontGlobalScale = _native ? std::min(1.25f, p.uiScale / 100.0f) : p.uiScale / 100.0f;
	auto &style = ImGui::GetStyle();
	style.WindowPadding = _native ? ImVec2(6, 5) : ImVec2(18, 16);
	style.FramePadding = _native ? ImVec2(4, 3) : ImVec2(9, 6);
	style.ItemSpacing = _native ? ImVec2(5, 5) : ImVec2(12, 10);
	style.WindowRounding = _native ? 0 : 10;
	ImGui::NewFrame();
}
void SettingsOverlay::draw(settings::Session &s) {
	ImGui::SetCurrentContext(_context);
	const auto &io = ImGui::GetIO();
	if (!ImGui::IsKeyDown(ImGuiKey_Escape) && !ImGui::IsKeyDown(ImGuiKey_GamepadStart) &&
	    !ImGui::IsKeyDown(ImGuiKey_GamepadFaceRight))
		_closeArmed = true;
	if (_closeArmed && _captureBinding < 0 && _capturePad < 0 &&
	    !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
	    (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
	     ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false) ||
	     ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)))
		if (!s.recovery)
			s.action = settings::Action::Resume;
	ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize,
	                                              IM_COL32(0, 5, 14, 145));
	const ImVec2 size(_native ? io.DisplaySize.x
	                          : std::min(_compact ? 540.0f : 1080.0f, io.DisplaySize.x - 24),
	                  _native ? io.DisplaySize.y : io.DisplaySize.y - 30);
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Always,
	                        ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(size, ImGuiCond_Always);
	ImGui::Begin("REminiscence2 settings", nullptr,
	             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
	                 ImGuiWindowFlags_NoMove);
	ImGui::TextColored(accent, s.recovery  ? "REminiscence2 / RECOVERY"
	                           : s.startup ? "REminiscence2 / WELCOME"
	                           : _native   ? "REminiscence2 / PAUSED"
	                                       : "REminiscence2 / Quality of Life");
	if (!_native)
		ImGui::TextDisabled("%s / %s / %s", s.edition.c_str(), REMINISCENCE2_VERSION,
		                    s.recovery  ? "Choose a recovery point"
		                    : s.startup ? "Choose Continue or New game"
		                                : "Esc, F10 or Start to resume");
	ImGui::Separator();
	if (s.controllerDisconnected)
		ImGui::TextWrapped(
		    "%s",
		    s.controllerName.empty()
		        ? "Controller disconnected. Reconnect or use the keyboard, then choose Resume."
				: "Controller reconnected. Choose Resume when ready.");
	auto joypad = [&] {
		ImGui::SeparatorText("Joypad / controller");
		ImGui::TextWrapped("Device: %s", s.controllerName.empty() ? "No controller connected"
		                                                          : s.controllerName.c_str());
		if (!s.controllerProfile.empty())
			hint("Save settings stores bindings and stick options for this device GUID. Other "
			     "preferences stay global.");
		if (!compactLayout || ImGui::CollapsingHeader("Stick and device options")) {
			slider(s, _search, "Stick dead zone", s.values.stickDeadzone, 5, 50, "%d%%");
			toggle(s, _search, "Right stick also moves", s.values.rightStickMovement,
			       "Off keeps camera-stick noise from moving Conrad. D-pad and left stick always "
			       "move.");
			hint("Start always opens settings. Bind optional quick save, quick load and rewind "
			     "below; LT/L2 and RT/R2 can also be captured. "
			     "The south face button confirms and the east face button backs out in menus "
			     "regardless of gameplay bindings.");
			if (ImGui::Checkbox("Unmapped joystick (raw buttons)", &_captureRaw))
				_capturePad = -1;
		}
		if (compactLayout)
			hint("Start: settings / cancel capture");
		for (size_t i = 0; i < settings::PadBindingCount; ++i) {
			ImGui::PushID(int(i) + 100);
			auto &bindings = _captureRaw ? s.values.joystickButtons : s.values.padButtons;
			const int b = bindings[i];
			const char *name =
			    !_captureRaw && b >= 0 ? settings::padButtonName(b, s.controllerStyle) : nullptr;
			const std::string label = _capturePad == int(i) ? "Press a button..."
			                          : b < 0               ? "Unassigned"
			                          : name                ? name
			                                                : "Button " + std::to_string(b + 1);
			ImGui::TextUnformatted(settings::padBindingName(i));
			ImGui::SameLine(compactLayout ? 120 : 220);
			if (ImGui::Button(label.c_str(), ImVec2(std::min(compactLayout ? 60.0f : 175.0f,
			                                                 ImGui::GetContentRegionAvail().x),
			                                        0))) {
				beginPadCapture(i, _captureRaw);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear")) {
				bindings[i] = -1;
				s.applyRequested = true;
				_capturePad = -1;
			}
			ImGui::PopID();
		}
		if (ImGui::Button("Reset joypad bindings")) {
			const settings::Preferences defaults;
			s.values.padButtons = defaults.padButtons;
			s.values.joystickButtons = defaults.joystickButtons;
			s.values.stickDeadzone = defaults.stickDeadzone;
			s.values.rightStickMovement = false;
			s.applyRequested = true;
			_capturePad = -1;
		}
	};
	auto controls = [&] {
		if (match(_search, "Keyboard diagram action keys bindings controls remap")) {
			ImGui::SeparatorText("Your keyboard / action guide");
			if (_compact)
				hint(_captureBinding >= 0 ? "Press a key. Esc cancels; F10 resumes."
				                          : "Select a colored key or action to rebind.");
			else
				hint("Colored keys are your primary bindings. Click a colored key or its action "
				     "to rebind. Arrows connect keys to their actions; other keys stay dark.");
			const int selected = keyboard::draw(s.values, _captureBinding, _compact);
			if (selected >= 0)
				_captureBinding = selected;
			hint("Physical key positions / SDL key names. F10 always opens settings. "
			     "Fixed engine shortcuts and legacy aliases are listed below.");
		}
		ImGui::SeparatorText("Keyboard bindings");
		hint("Choose a binding, then press a key. Escape cancels capture. F10 always opens "
		     "settings, even after rebinding Pause.");
		for (size_t i = 0; i < settings::BindingCount; ++i) {
			if (!match(_search, settings::bindingName(i), "keyboard controls remap"))
				continue;
			ImGui::PushID(static_cast<int>(i));
			if (compactLayout)
				ImGui::TextUnformatted(settings::bindingName(i));
			const char *key = _captureBinding == int(i)
			                      ? "Press a key..."
			                      : SDL_GetScancodeName(SDL_Scancode(s.values.keys[i]));
			const char *caption = key && *key ? key : "Unassigned";
			if (ImGui::Button(caption,
			                  ImVec2(compactLayout
			                             ? std::min(130.0f, ImGui::GetContentRegionAvail().x)
										 : std::max(175.0f, ImGui::CalcTextSize(caption).x + 22),
			                         0)))
				_captureBinding = static_cast<int>(i);
			const settings::Preferences defaults;
			if (s.values.keys[i] != defaults.keys[i]) {
				ImGui::SameLine();
				if (ImGui::Button("Reset")) {
					std::string reason;
					if (settings::validBinding(defaults.keys[i], s.values, i, reason)) {
						s.values.keys[i] = defaults.keys[i];
						s.applyRequested = true;
						_captureBinding = -1;
					} else {
						s.status = reason;
						s.statusError = true;
					}
				}
			}
			if (!compactLayout) {
				ImGui::SameLine();
				ImGui::TextUnformatted(settings::bindingName(i));
			}
			ImGui::PopID();
		}
		ImGui::SeparatorText("Menu and engine shortcuts");
		hint("Mouse: point and click. Keyboard: Tab / Shift+Tab to focus, arrows to adjust, Enter "
		     "/ Space to activate. Controller: D-pad or stick, A to confirm, B to close, shoulders "
		     "to switch tabs.");
		hint("Ctrl+S / Ctrl+L save and load. Ctrl+R rewinds. Alt+Enter toggles fullscreen. These "
		     "engine shortcuts remain fixed. Joypad defaults: A=Use, B=Weapon, X=Action, "
		     "Y=Inventory. "
		     "Joypad bindings above apply during gameplay.");
	};

	auto autosaves = [&](bool preview = false, bool focusButton = false) {
		ImGui::SeparatorText(s.startup ? "Welcome back" : "Persistent autosaves");
		if (s.autosaves.empty()) {
			hint("No valid autosaves for this edition yet.");
			return;
		}
		s.autosaveIndex = std::max(0, std::min(s.autosaveIndex, int(s.autosaves.size()) - 1));
		if (!s.startup) {
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderInt("##autosave", &s.autosaveIndex, 0, int(s.autosaves.size()) - 1,
			                 "Generation %d (0 = latest)");
		}
		const auto &save = s.autosaves[s.startup ? 0 : s.autosaveIndex];
		ImGui::Text("Level %d / Room %d / Health %d", save.level + 1, save.room, save.health);
		ImGui::TextWrapped("%s", progress::date(save.timestamp).c_str());
		if (focusButton)
			ImGui::SetKeyboardFocusHere();
		if (wideButton(s.startup ? "Continue latest autosave" : "Load this autosave...")) {
			if (s.startup)
				s.action = settings::Action::ContinueLatest;
			else
				ImGui::OpenPopup("Restore autosave?");
		}
		confirmation(
		    "Restore autosave?",
		    "Restore this autosave, switching levels if needed? Unsaved progress will be replaced.",
		    "Load", [&] { s.action = settings::Action::LoadAutosave; });
		if (preview)
			drawPreview(save.preview);
		hint("Three generations rotate separately from slots 00-99. Continue skips damaged or "
		     "incompatible generations.");
	};
	auto rewind = [&] {
		ImGui::SeparatorText("Rewind timeline");
		if (s.rewindPoints.empty()) {
			hint("Enable Rewind history on Session, then play for five seconds. History is kept in "
			     "memory for this level.");
			return;
		}
		s.rewindIndex = std::max(0, std::min(s.rewindIndex, int(s.rewindPoints.size()) - 1));
		ImGui::SetNextItemWidth(-1);
		ImGui::SliderInt("##rewind", &s.rewindIndex, 0, int(s.rewindPoints.size()) - 1,
		                 "Snapshot %d (0 = latest)");
		const auto &point = s.rewindPoints[s.rewindIndex];
		ImGui::Text("%u sec ago / Room %d / Health %d", point.ageSeconds, point.room, point.health);
		if (wideButton("Restore selected moment..."))
			ImGui::OpenPopup("Restore rewind?");
		confirmation(
		    "Restore rewind?",
		    "Return to this moment? Newer snapshots will be discarded; older history is kept.",
		    "Rewind", [&] { s.action = settings::Action::Rewind; });
		drawPreview(point.preview);
		hint("Ctrl+R restores the latest snapshot directly. Rewind history does not survive an "
		     "exit or level change.");
	};
	auto content = [&](int page) {
		switch (page) {
		case 0:
			sessionPage(s, _search);
			ImGui::SeparatorText("Preferences");
			if (preferenceButtons(s, _opening))
				_captureBinding = -1;
			break;
		case 1:
			savesPage(s, _search, [&](auto p) { drawPreview(p); });
			if (ImGui::CollapsingHeader("Persistent autosaves"))
				autosaves();
			break;
		case 2:
			graphicsPage(s, _search);
			break;
		case 3:
			audioPage(s, _search);
			break;
		case 4:
			enhancementsPage(s, _search);
			break;
		case 5:
			controls();
			break;
		case 6:
			cheatsPage(s, _search);
			break;
		case 7:
			levelsPage(s, _search);
			break;
		case 10:
			rewind();
			break;
		case 9:
			joypad();
			break;
		case 12:
			ImGui::SeparatorText("Assets and system diagnostics");
			for (const auto &line : s.diagnostics)
				ImGui::TextWrapped("%s", line.c_str());
			ImGui::TextWrapped("Audio output: %s / %d Hz",
			                   s.audioAvailable ? "connected"
			                                    : "silent fallback (voice clock advances)",
			                   s.outputRate);
			ImGui::TextWrapped("Controller: %s",
			                   s.controllerName.empty() ? "none" : s.controllerName.c_str());
			if (!s.controllerProfile.empty())
				ImGui::TextWrapped("Device profile: %s", s.controllerProfile.c_str());
			ImGui::Text("Disk writes: last %.2f ms / worst %.2f ms", s.lastSaveMs, s.worstSaveMs);
			hint("Level checks report primary resource presence; loading still validates required "
			     "data. Save timings measure local synchronous writes and are not a Deck/Quest "
			     "benchmark.");
			break;
		case 11:
			ImGui::SeparatorText("Objectives notebook");
			if (!compactLayout || ImGui::CollapsingHeader("Notebook options"))
				toggle(s, _search, "Objectives notebook", s.values.objectivesNotebook,
				       "Optional chapter goals for places you have reached. Hints stay folded "
				       "until requested.");
			hint(compactLayout
			         ? "Reached chapters. Open a hint when you want help."
					 : "Goals follow chapters you have reached. Open a hint when you want help.");
			if (s.values.objectivesNotebook) {
				if (std::any_of(s.journalEntries.begin(), s.journalEntries.end(),
				                [](const auto &entry) { return entry.id == 7; })) {
					ImGui::SeparatorText("Observed progress");
					ImGui::TextWrapped("A memory sequence has been viewed in this timeline. Its "
					                   "journal entry is available.");
				}
				if (s.objectives.empty())
					hint("Your notebook begins when you start playing.");
				for (const auto &objective : s.objectives) {
					if (!match(_search, objective.title.c_str(), objective.goal.c_str()))
						continue;
					ImGui::PushID(int(objective.level));
					ImGui::SeparatorText(objective.current ? "Current chapter"
					                                       : "Previously visited");
					ImGui::TextColored(turquoise, "%s", objective.title.c_str());
					ImGui::TextWrapped("%s", objective.goal.c_str());
					if (ImGui::CollapsingHeader("Show a gentle hint"))
						ImGui::TextWrapped("%s", objective.hint.c_str());
					ImGui::PopID();
				}
			}
			break;
		case 8:
			ImGui::SeparatorText("Conrad's journal");
			if (!compactLayout || ImGui::CollapsingHeader("Journal options")) {
				toggle(s, _search, "Story journal", s.values.storyJournal,
				       "Optional companion passages for events you have reached.");
				toggle(s, _search, "New entry notification", s.values.journalNotifications,
				       "A small notice during gameplay; reading never interrupts play.");
				hint("Original REminiscence2 companion prose. Entries follow your current save, "
				     "including rewind. Future entries remain hidden.");
			}
			if (s.values.storyJournal) {
				if (s.journalEntries.empty())
					ImGui::TextWrapped("Your journal will begin when you start playing.");
				for (auto &entry : s.journalEntries) {
					if (!match(_search, entry.title.c_str(), entry.text.c_str()))
						continue;
					const std::string label =
					    entry.title + (entry.unread ? " [NEW]###entry" : "###entry");
					ImGui::PushID(int(entry.id));
					if (ImGui::CollapsingHeader(label.c_str(),
					                            entry.id == s.journalEntries.front().id
					                                ? ImGuiTreeNodeFlags_DefaultOpen
													: 0)) {
						ImGui::TextWrapped("%s", entry.text.c_str());
						if (entry.unread && s.readJournalEntry) {
							s.readJournalEntry(entry.id);
							entry.unread = false;
						}
					}
					ImGui::PopID();
				}
			}
			break;
		}
	};
	int requested = _requestedTab;
	_requestedTab = -1;
	if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1))
		requested = (_tab + PageCount - 1) % PageCount;
	if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1))
		requested = (_tab + 1) % PageCount;
	static const char *const pages[] = {
	    "Session", "Save / Load", "Graphics", "Audio",  "Enhancements", "Controls",   "Cheats",
	    "Levels",  "Journal",     "Joypad",   "Rewind", "Objectives",   "Diagnostics"};
	const float footer = ImGui::GetFrameHeightWithSpacing() +
	                     (_native ? 0 : ImGui::GetTextLineHeightWithSpacing()) + 5;
	auto drawContent = [&](int page) {
		ImGui::BeginChild("Page content", ImVec2(0, -footer), ImGuiChildFlags_Borders);
		if (!s.status.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      s.statusError ? ImVec4(1, 0.48f, 0.4f, 1) : turquoise);
			ImGui::TextWrapped("%s", s.status.c_str());
			ImGui::PopStyleColor();
			ImGui::Separator();
		}
		if (_search[0]) {
			for (int i = 0; i < PageCount; ++i) {
				ImGui::PushID(i);
				content(i);
				ImGui::PopID();
			}
		} else
			content(page);
		ImGui::EndChild();
	};
	if (s.recovery) {
		ImGui::BeginChild("Recovery choices", ImVec2(0, -footer), ImGuiChildFlags_Borders);
		hint("Conrad fell. Choose where to return; the game stays paused.");
		if (!s.status.empty())
			ImGui::TextWrapped("%s", s.status.c_str());
		const auto checkpoint = std::find_if(s.saves.begin(), s.saves.end(), [&](const auto &v) {
			return v.level == s.level && v.slot == 0 && v.supported;
		});
		bool details = !_compact;
		if (_compact) {
			bool focused = false;
			auto primary = [&](const char *label, bool available, settings::Action action) {
				if (_firstDraw && available && !focused) {
					ImGui::SetKeyboardFocusHere();
					focused = true;
				}
				ImGui::BeginDisabled(!available);
				const bool chosen = wideButton(label);
				if (chosen)
					s.action = action;
				ImGui::EndDisabled();
				return chosen;
			};
			primary("Rewind latest moment", !s.rewindPoints.empty(), settings::Action::Rewind);
			if (primary("Return to checkpoint", checkpoint != s.saves.end(),
			            settings::Action::LoadGame)) {
				s.browseLevel = s.level;
				s.commandSlot = 0;
			}
			primary("Continue latest autosave", !s.autosaves.empty(),
			        settings::Action::ContinueLatest);
			details = ImGui::CollapsingHeader("Previews / older moments");
		}
		if (details && ImGui::BeginTable("Recovery points", _compact ? 1 : 3,
		                                 ImGuiTableFlags_SizingStretchSame)) {
			bool defaultChosen = false;
			auto focus = [&](bool available) {
				if (_firstDraw && available && !defaultChosen) {
					ImGui::SetKeyboardFocusHere();
					defaultChosen = true;
				}
			};
			ImGui::TableNextColumn();
			ImGui::SeparatorText("Rewind");
			focus(!s.rewindPoints.empty());
			ImGui::BeginDisabled(s.rewindPoints.empty());
			if (wideButton("Restore living moment"))
				s.action = settings::Action::Rewind;
			ImGui::EndDisabled();
			if (!s.rewindPoints.empty()) {
				s.rewindIndex = std::clamp(s.rewindIndex, 0, int(s.rewindPoints.size()) - 1);
				ImGui::SetNextItemWidth(-1);
				ImGui::SliderInt("##recoveryrewind", &s.rewindIndex, 0,
				                 int(s.rewindPoints.size()) - 1, "Moment %d (0 = latest)");
				const auto &point = s.rewindPoints[s.rewindIndex];
				ImGui::TextWrapped("%u sec ago / Room %d / Health %d", point.ageSeconds, point.room,
				                   point.health);
				drawPreview(point.preview);
			} else
				hint("No living rewind moments in this session.");
			ImGui::TableNextColumn();
			ImGui::SeparatorText("Checkpoint");
			focus(checkpoint != s.saves.end());
			ImGui::BeginDisabled(checkpoint == s.saves.end());
			if (wideButton("Return to checkpoint")) {
				s.browseLevel = s.level;
				s.commandSlot = 0;
				s.action = settings::Action::LoadGame;
			}
			ImGui::EndDisabled();
			if (checkpoint != s.saves.end()) {
				ImGui::Text("Level %d / Room %d", checkpoint->level + 1, checkpoint->room);
				drawPreview(checkpoint->preview);
				if (!checkpoint->preview)
					hint("Legacy checkpoint / no preview");
			} else
				hint("No checkpoint stored for this level.");
			ImGui::TableNextColumn();
			autosaves(true, _firstDraw && !defaultChosen && !s.autosaves.empty());
			ImGui::EndTable();
		}
		if (ImGui::CollapsingHeader("Browse manual saves / other checkpoints")) {
			savesPage(s, "", [&](auto p) { drawPreview(p); });
		}
		if (wideButton("Return to title..."))
			ImGui::OpenPopup("Recovery title?");
		confirmation("Recovery title?",
		             "Leave this session and return to the title? Stored saves are kept.", "Title",
		             [&] { s.action = settings::Action::Title; });
		ImGui::EndChild();
	} else if (s.startup) {
		ImGui::BeginChild("Welcome", ImVec2(0, -footer), ImGuiChildFlags_Borders);
		if (!s.status.empty())
			ImGui::TextWrapped("%s", s.status.c_str());
		autosaves(true);
		ImGui::EndChild();
	} else if (_compact) {
		if (requested >= 0)
			_tab = requested;
		const float arrow = ImGui::GetFrameHeight();
		if (ImGui::Button("<", ImVec2(arrow, 0)))
			_tab = (_tab + PageCount - 1) % PageCount;
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - arrow -
		                        ImGui::GetStyle().ItemSpacing.x);
		if (ImGui::BeginCombo("##page", pages[_tab])) {
			for (int i = 0; i < PageCount; ++i)
				if (ImGui::Selectable(pages[i], i == _tab))
					_tab = i;
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button(">", ImVec2(arrow, 0)))
			_tab = (_tab + 1) % PageCount;
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##search", "Find a setting...", _search, sizeof(_search));
		drawContent(_tab);
	} else {
		ImGui::SetNextItemWidth(300);
		ImGui::InputTextWithHint("##search", "Find a setting...", _search, sizeof(_search));
		if (ImGui::BeginTabBar("Settings pages", ImGuiTabBarFlags_FittingPolicyScroll)) {
			for (int i = 0; i < PageCount; ++i) {
				if (ImGui::BeginTabItem(pages[i], nullptr,
				                        requested == i ? ImGuiTabItemFlags_SetSelected : 0)) {
					_tab = i;
					drawContent(i);
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
	const float available = ImGui::GetContentRegionAvail().x;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float padding = ImGui::GetStyle().FramePadding.x * 2;
	const float resumeWidth = std::max(160.0f, ImGui::CalcTextSize("Resume game").x + padding);
	const float naturalExit = std::max(190.0f, ImGui::CalcTextSize("EXIT GAME").x + padding);
	const float saveWidth = ImGui::CalcTextSize("Save settings").x + padding;
	const bool compactFooter =
	    _compact || resumeWidth + naturalExit + saveWidth + spacing * 2 > available;
	const float exitWidth = compactFooter ? (available - spacing) / 2 : naturalExit;
	const char *resumeLabel = s.recovery                 ? "Restart level..."
	                          : s.startup                ? "New game"
	                          : compactFooter || _native ? "Resume"
	                                                     : "Resume game";
	if (ImGui::Button(resumeLabel, ImVec2(compactFooter ? exitWidth : resumeWidth, 0))) {
		if (s.recovery)
			ImGui::OpenPopup("Recovery restart?");
		else
			s.action = settings::Action::Resume;
	}
	confirmation("Recovery restart?", "Restart this level from its beginning?", "Restart", [&] {
		s.targetLevel = s.level;
		s.targetSkill = s.skill;
		s.action = settings::Action::StartLevel;
	});
	ImGui::SameLine();
	if (!compactFooter) {
		if (!s.startup) {
			if (ImGui::Button("Save settings"))
				s.saveRequested = true;
			ImGui::SameLine();
		}
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x -
		                     exitWidth);
	}
	ImGui::PushStyleColor(ImGuiCol_Button, turquoise);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.95f, 0.86f, 1));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.65f, 0.60f, 1));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.015f, 0.08f, 0.09f, 1));
	if (ImGui::Button("EXIT GAME", ImVec2(exitWidth, 0)))
		ImGui::OpenPopup("Exit game?");
	ImGui::PopStyleColor(4);
	confirmation("Exit game?",
	             s.startup
	                 ? "Exit REminiscence2? Stored autosaves are kept."
					 : "Exit REminiscence2? Unsaved progress will be lost. Save your game first.",
	             "EXIT GAME", [&] { s.action = settings::Action::Quit; });
	if (!_native) {
		ImGui::TextDisabled(
		    s.recovery ? "Choose a recovery point or restart. Escape keeps recovery open."
			: s.startup
		        ? "Esc / F10 / Start: New game | Continue restores the newest compatible autosave."
				: "Esc / F10 / Start: resume | Settings, saves and cheats have separate controls.");
	}
	ImGui::End();
	_firstDraw = false;
}
bool SettingsOverlay::drawHud(const settings::Preferences &preferences, bool gameplay,
                              bool newJournal, const std::string &contextHint,
                              const std::string &notice, bool noticeError) {
	bool clicked = false;
	_shortcut = {};
	if (!notice.empty()) {
		const auto display = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(4, display.y * 0.25f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(std::min(display.x - 8, 560.0f), 0));
		ImGui::SetNextWindowBgAlpha(0.92f);
		ImGui::Begin("Save and device feedback", nullptr,
		             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
		ImGui::PushStyleColor(ImGuiCol_Text, noticeError ? ImVec4(1, 0.65f, 0.4f, 1) : turquoise);
		ImGui::TextWrapped("%s", notice.c_str());
		ImGui::PopStyleColor();
		ImGui::End();
	}
	if (gameplay && newJournal) {
		ImGui::SetNextWindowPos(ImVec2(4, preferences.showFps ? 36 : 4), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::Begin("Journal notification", nullptr,
		             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
		ImGui::TextColored(turquoise, "Journal: new entry");
		ImGui::End();
	}
	if (preferences.showFps) {
		ImGui::SetNextWindowPos(ImVec2(4, 4), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::Begin("Frame rate", nullptr,
		             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
		ImGui::End();
	}
	if (gameplay && preferences.contextualHints && !contextHint.empty()) {
		const auto display = ImGui::GetIO().DisplaySize;
		const float width = std::min(display.x - 8, _native ? 240.0f : 520.0f);
		ImGui::SetNextWindowPos(ImVec2(4, display.y - (_native ? 28 : 42)), ImGuiCond_Always,
		                        ImVec2(0, 1));
		ImGui::SetNextWindowSize(ImVec2(width, 0));
		ImGui::SetNextWindowBgAlpha(0.85f);
		ImGui::Begin("Control reminder", nullptr,
		             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
		ImGui::PushStyleColor(ImGuiCol_Text, turquoise);
		ImGui::TextWrapped("%s", contextHint.c_str());
		ImGui::PopStyleColor();
		ImGui::End();
	}
	if (gameplay && preferences.showMenuButton) {
		ImGui::SetNextWindowPos(
		    ImVec2(ImGui::GetIO().DisplaySize.x - 4, ImGui::GetIO().DisplaySize.y - 4),
		    ImGuiCond_Always, ImVec2(1, 1));
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
		ImGui::Begin("QoL shortcut", nullptr,
		             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
		clicked = ImGui::Button("Menu [F10]");
		const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
		_shortcut = {lo.x, lo.y, hi.x, hi.y};
		ImGui::End();
		ImGui::PopStyleVar();
	}
	return clicked;
}
bool SettingsOverlay::shortcutContains(float x, float y) const {
	return x >= _shortcut[0] && x < _shortcut[2] && y >= _shortcut[1] && y < _shortcut[3];
}
void SettingsOverlay::render() {
	ImGui::Render();
	const auto &io = ImGui::GetIO();
	SDL_RenderSetScale(_renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), _renderer);
	// Retire unused textures only after queued draw commands have finished.
	for (auto entry = _previews.begin(); entry != _previews.end();) {
		if (entry->frame != ImGui::GetFrameCount()) {
			SDL_DestroyTexture(entry->texture);
			entry = _previews.erase(entry);
		} else
			++entry;
	}
}
