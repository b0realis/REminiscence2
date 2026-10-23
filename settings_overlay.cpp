// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_overlay.h"
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
		if (info.slot == slot)
			return &info;
	return nullptr;
}
void savesPage(settings::Session &s, const char *query) {
	if (!match(query, "Save load slot checkpoint progress refresh"))
		return;
	ImGui::SeparatorText("Save / Load");
	ImGui::Text("Level %d / slot %02d", s.level + 1, s.saveSlot);
	ImGui::SetNextItemWidth(-1);
	ImGui::SliderInt("##slot", &s.saveSlot, 1, 99, "Manual slot %02d");
	const auto *selected = saveInfo(s, s.saveSlot);
	const bool exists = selected ? selected->exists : (s.hasSave && s.hasSave(s.saveSlot));
	const bool supported = selected ? selected->supported : exists;
	if (selected && selected->exists) {
		ImGui::Text("Room %d%s", selected->room, selected->supported ? "" : " / incompatible");
		ImGui::TextUnformatted(selected->modified.c_str());
	} else
		hint("Empty slot");
	ImGui::BeginDisabled(!s.canSave);
	if (wideButton("Save game")) {
		if (exists)
			ImGui::OpenPopup("Overwrite save?");
		else
			s.action = settings::Action::SaveGame;
	}
	ImGui::EndDisabled();
	confirmation("Overwrite save?", "Replace the selected manual save with your current progress?",
	             "Save", [&] { s.action = settings::Action::SaveGame; });
	ImGui::BeginDisabled(!supported);
	if (wideButton("Load game..."))
		ImGui::OpenPopup("Load save?");
	ImGui::EndDisabled();
	confirmation("Load save?", "Load the selected save? Current unsaved progress will be replaced.",
	             "Load", [&] { s.action = settings::Action::LoadGame; });
	const auto *checkpoint = saveInfo(s, 0);
	ImGui::BeginDisabled(!checkpoint || !checkpoint->supported);
	if (wideButton("Load checkpoint..."))
		ImGui::OpenPopup("Load checkpoint?");
	ImGui::EndDisabled();
	confirmation("Load checkpoint?",
	             "Load the in-game checkpoint (slot 00)? Unsaved progress will be replaced.",
	             "Load", [&] {
		             s.commandSlot = 0;
		             s.action = settings::Action::LoadGame;
	             });
	if (wideButton("Refresh saves"))
		s.refreshRequested = true;
	hint("Saves belong to this level and asset edition. Use Levels to change levels first. "
	     "Settings and rewind history are separate.");
	ImGui::SeparatorText("Stored manual slots");
	bool found = false;
	for (const auto &info : s.saves) {
		if (!info.slot || !info.exists)
			continue;
		found = true;
		ImGui::PushID(info.slot);
		char label[80];
		snprintf(label, sizeof(label), "%02d / Room %d%s", info.slot, info.room,
		         info.supported ? "" : " / incompatible");
		if (ImGui::Selectable(label, info.slot == s.saveSlot))
			s.saveSlot = info.slot;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", info.modified.c_str());
		ImGui::PopID();
	}
	if (!found)
		hint("No manual saves for this level yet.");
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
	if (!_attached)
		return;
	ImGui::SetCurrentContext(_context);
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	_attached = false;
	_renderer = nullptr;
}
void SettingsOverlay::open(const settings::Preferences &p) {
	ImGui::SetCurrentContext(_context);
	ImGui::GetIO().ClearInputKeys();
	_captureBinding = -1;
	_closeArmed = false;
	_search[0] = 0;
	_opening = p;
}
void SettingsOverlay::processEvent(const SDL_Event &event, settings::Session *s) {
	ImGui::SetCurrentContext(_context);
	if (s && event.type == SDL_KEYDOWN && !event.key.repeat) {
		if (event.key.keysym.scancode == SDL_SCANCODE_F10) {
			_captureBinding = -1;
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
	if (_closeArmed && _captureBinding < 0 &&
	    !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
	    (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
	     ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false) ||
	     ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)))
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
	ImGui::TextColored(accent,
	                   _native ? "REminiscence2 / PAUSED" : "REminiscence2 / Quality of Life");
	if (!_native)
		ImGui::TextDisabled("%s / %s / Esc, F10 or Start to resume", s.edition.c_str(),
		                    REMINISCENCE2_VERSION);
	ImGui::Separator();
	auto controls = [&] {
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
		     "engine shortcuts remain fixed. Controller gameplay bindings remain A=Use, B=Weapon, "
		     "X=Action, Y=Inventory.");
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
			savesPage(s, _search);
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
		}
	};
	int requested = _requestedTab;
	_requestedTab = -1;
	if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1))
		requested = (_tab + PageCount - 1) % PageCount;
	if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1))
		requested = (_tab + 1) % PageCount;
	static const char *const pages[] = {"Session",      "Save / Load", "Graphics", "Audio",
	                                    "Enhancements", "Controls",    "Cheats",   "Levels"};
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
	if (_compact) {
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
	const float exitWidth = _compact ? (available - ImGui::GetStyle().ItemSpacing.x) / 2 : 190;
	if (ImGui::Button(_native ? "Resume" : "Resume game", ImVec2(_compact ? exitWidth : 160, 0)))
		s.action = settings::Action::Resume;
	ImGui::SameLine();
	if (!_compact) {
		if (ImGui::Button("Save settings"))
			s.saveRequested = true;
		ImGui::SameLine();
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
	             "Exit REminiscence2? Unsaved progress will be lost. Save your game first.",
	             "EXIT GAME", [&] { s.action = settings::Action::Quit; });
	if (!_native) {
		ImGui::TextDisabled(
		    "Esc / F10 / Start: resume | Settings, saves and cheats have separate controls.");
	}
	ImGui::End();
}
bool SettingsOverlay::drawHud(const settings::Preferences &preferences, bool gameplay) {
	bool clicked = false;
	_shortcut = {};
	if (preferences.showFps) {
		ImGui::SetNextWindowPos(ImVec2(4, 4), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::Begin("Frame rate", nullptr,
		             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
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
}
