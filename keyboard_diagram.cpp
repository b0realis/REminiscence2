// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard_diagram.h"
#include <SDL.h>
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace keyboard {
namespace {
constexpr float KeyboardWidth = 23.2f;
const ImU32 colors[] = {
    IM_COL32(70, 160, 255, 255),  IM_COL32(85, 195, 255, 255),  IM_COL32(85, 215, 175, 255),
    IM_COL32(145, 210, 100, 255), IM_COL32(255, 190, 75, 255),  IM_COL32(185, 150, 255, 255),
    IM_COL32(255, 125, 135, 255), IM_COL32(240, 140, 210, 255), IM_COL32(45, 210, 195, 255)};
static_assert(sizeof(colors) / sizeof(*colors) == settings::BindingCount);
const char *const actions[] = {"Move left",      "Move right",       "Up / climb",
                               "Down / duck",    "Act / run / fire", "Use / confirm",
                               "Draw / holster", "Inventory",        "Pause / QoL"};

std::string keyName(int code) {
	const char *name = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
	return name && *name ? name : "Unknown";
}
void textFit(ImDrawList *draw, ImVec2 lo, ImVec2 hi, const std::string &text, ImU32 color) {
	const auto measured = ImGui::CalcTextSize(text.c_str());
	const float font = ImGui::GetFontSize();
	const float scale = std::min({1.0f, (hi.x - lo.x - 6) / std::max(1.0f, measured.x),
	                              (hi.y - lo.y - 4) / std::max(1.0f, measured.y)});
	const ImVec2 at(lo.x + (hi.x - lo.x - measured.x * scale) / 2,
	                lo.y + (hi.y - lo.y - measured.y * scale) / 2);
	draw->AddText(ImGui::GetFont(), font * std::max(0.1f, scale), at, color, text.c_str());
}
void arrow(ImDrawList *draw, ImVec2 from, ImVec2 to, ImU32 color, float thickness) {
	draw->AddLine(from, to, color, thickness);
	const float dx = to.x - from.x, dy = to.y - from.y;
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length < 1)
		return;
	const float ux = dx / length, uy = dy / length;
	const float size = 5 + thickness;
	draw->AddTriangleFilled(
	    to, ImVec2(to.x - ux * size - uy * size * .5f, to.y - uy * size + ux * size * .5f),
	    ImVec2(to.x - ux * size + uy * size * .5f, to.y - uy * size - ux * size * .5f), color);
}
bool callout(int action, const std::string &key, ImVec2 at, ImVec2 size, int capturing) {
	ImGui::SetCursorScreenPos(at);
	ImGui::PushID(action);
	const std::string caption = key + '\n' + actions[action];
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.055f, .09f, .14f, 1));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.12f, .20f, .30f, 1));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
	const bool clicked = ImGui::Button(caption.c_str(), size);
	ImGui::PopStyleColor(3);
	auto *draw = ImGui::GetWindowDrawList();
	draw->AddRect(at, ImVec2(at.x + size.x, at.y + size.y), colors[action], 4, 0,
	              capturing == action || ImGui::IsItemFocused() ? 2.5f : 1);
	textFit(draw, at, ImVec2(at.x + size.x, at.y + size.y),
	        capturing == action ? std::string("Press a key...\n") + actions[action] : caption,
	        IM_COL32(233, 241, 251, 255));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s: %s\nChoose a new key. Escape cancels; F10 resumes.",
		                  settings::bindingName(action), key.c_str());
	ImGui::PopID();
	return clicked;
}
} // namespace

int bindingForKey(const settings::Preferences &preferences, int scancode) {
	for (size_t i = 0; i < settings::BindingCount; ++i)
		if (preferences.keys[i] == scancode)
			return static_cast<int>(i);
	return -1;
}
static std::vector<Key> buildLayout(const settings::Preferences &preferences) {
	std::vector<Key> keys;
	auto add = [&](int code, float x, float y, float width = 1, float height = 1,
	               const char *legend = nullptr) {
		keys.push_back({code, x, y, width, height, legend ? legend : keyName(code)});
	};
	auto row = [&](float x, float y, std::initializer_list<int> codes) {
		for (int code : codes) {
			add(code, x, y);
			x += 1;
		}
	};
	add(SDL_SCANCODE_ESCAPE, 0, 0, 1, .85f, "Esc");
	for (int i = 0; i < 12; ++i)
		add(SDL_SCANCODE_F1 + i, 2 + i + (i / 4) * .5f, 0, 1, .85f);
	row(15.6f, 0, {SDL_SCANCODE_PRINTSCREEN, SDL_SCANCODE_SCROLLLOCK, SDL_SCANCODE_PAUSE});
	row(0, 1.3f,
	    {SDL_SCANCODE_GRAVE, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
	     SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9,
	     SDL_SCANCODE_0, SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS});
	add(SDL_SCANCODE_BACKSPACE, 13, 1.3f, 2, 1, "Backspace");
	add(SDL_SCANCODE_TAB, 0, 2.3f, 1.5f);
	row(1.5f, 2.3f,
	    {SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T,
	     SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P,
	     SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET});
	add(SDL_SCANCODE_BACKSLASH, 13.5f, 2.3f, 1.5f);
	add(SDL_SCANCODE_CAPSLOCK, 0, 3.3f, 1.75f, 1, "Caps");
	row(1.75f, 3.3f,
	    {SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G,
	     SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON,
	     SDL_SCANCODE_APOSTROPHE});
	add(SDL_SCANCODE_RETURN, 12.75f, 3.3f, 2.25f, 1, "Enter");
	add(SDL_SCANCODE_LSHIFT, 0, 4.3f, 1.25f, 1, "LShift");
	add(SDL_SCANCODE_NONUSBACKSLASH, 1.25f, 4.3f, 1, 1, "ISO");
	row(2.25f, 4.3f,
	    {SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B,
	     SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD,
	     SDL_SCANCODE_SLASH});
	add(SDL_SCANCODE_RSHIFT, 12.25f, 4.3f, 2.75f, 1, "RShift");
	add(SDL_SCANCODE_LCTRL, 0, 5.3f, 1.25f, 1, "Ctrl");
	add(SDL_SCANCODE_LGUI, 1.25f, 5.3f, 1.25f, 1, "Super");
	add(SDL_SCANCODE_LALT, 2.5f, 5.3f, 1.25f, 1, "Alt");
	add(SDL_SCANCODE_SPACE, 3.75f, 5.3f, 6.25f, 1, "Space");
	add(SDL_SCANCODE_RALT, 10, 5.3f, 1.25f, 1, "AltGr");
	add(SDL_SCANCODE_RGUI, 11.25f, 5.3f, 1.25f, 1, "Super");
	add(SDL_SCANCODE_APPLICATION, 12.5f, 5.3f, 1.25f, 1, "Menu");
	add(SDL_SCANCODE_RCTRL, 13.75f, 5.3f, 1.25f, 1, "Ctrl");
	row(15.6f, 1.3f, {SDL_SCANCODE_INSERT, SDL_SCANCODE_HOME, SDL_SCANCODE_PAGEUP});
	row(15.6f, 2.3f, {SDL_SCANCODE_DELETE, SDL_SCANCODE_END, SDL_SCANCODE_PAGEDOWN});
	add(SDL_SCANCODE_UP, 16.6f, 4.3f, 1, 1, "Up");
	row(15.6f, 5.3f, {SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_RIGHT});
	row(19.2f, 1.3f,
	    {SDL_SCANCODE_NUMLOCKCLEAR, SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_KP_MULTIPLY,
	     SDL_SCANCODE_KP_MINUS});
	row(19.2f, 2.3f, {SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9});
	add(SDL_SCANCODE_KP_PLUS, 22.2f, 2.3f, 1, 2, "+");
	row(19.2f, 3.3f, {SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6});
	row(19.2f, 4.3f, {SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3});
	add(SDL_SCANCODE_KP_ENTER, 22.2f, 4.3f, 1, 2, "Enter");
	add(SDL_SCANCODE_KP_0, 19.2f, 5.3f, 2, 1, "0");
	add(SDL_SCANCODE_KP_PERIOD, 21.2f, 5.3f, 1, 1, ".");
	int extra = 0;
	for (int code : preferences.keys) {
		if (std::none_of(keys.begin(), keys.end(),
		                 [&](const Key &k) { return k.scancode == code; })) {
			add(code, (extra % 7) * 3.3f, 6.8f + (extra / 7), 3.2f);
			++extra;
		}
	}
	for (auto &key : keys) {
		if (key.scancode >= SDL_SCANCODE_KP_1 && key.scancode <= SDL_SCANCODE_KP_9)
			key.legend = std::to_string(key.scancode - SDL_SCANCODE_KP_1 + 1);
		switch (key.scancode) {
		case SDL_SCANCODE_PRINTSCREEN:
			key.legend = "PrtSc";
			break;
		case SDL_SCANCODE_SCROLLLOCK:
			key.legend = "ScrLk";
			break;
		case SDL_SCANCODE_PAUSE:
			key.legend = "Break";
			break;
		case SDL_SCANCODE_PAGEUP:
			key.legend = "PgUp";
			break;
		case SDL_SCANCODE_PAGEDOWN:
			key.legend = "PgDn";
			break;
		case SDL_SCANCODE_NUMLOCKCLEAR:
			key.legend = "Num";
			break;
		case SDL_SCANCODE_KP_DIVIDE:
			key.legend = "/";
			break;
		case SDL_SCANCODE_KP_MULTIPLY:
			key.legend = "*";
			break;
		case SDL_SCANCODE_KP_MINUS:
			key.legend = "-";
			break;
		default:
			break;
		}
	}
	return keys;
}
const std::vector<Key> &layout(const settings::Preferences &preferences) {
	thread_local std::array<int, settings::BindingCount> bindings{};
	thread_local std::vector<Key> cached;
	if (cached.empty() || bindings != preferences.keys) {
		cached = buildLayout(preferences);
		bindings = preferences.keys;
	}
	return cached;
}


int draw(const settings::Preferences &preferences, int capturing, bool compact) {
	ImGui::PushID("Keyboard diagram");
	const auto origin = ImGui::GetCursorScreenPos();
	const float width = ImGui::GetContentRegionAvail().x;
	const float font = ImGui::GetFontSize(), gap = 8;
	int selected = -1;
	if (compact || width < 680 || font > 20) {
		// A complete keyboard shrunk to 256 pixels has unreadable key legends.
		// Keep its colored caps and arrows, using one accessible action per row.
		for (size_t i = 0; i < settings::BindingCount; ++i) {
			const auto at = ImGui::GetCursorScreenPos();
			const float height = font * 2 + 12;
			ImGui::Dummy(ImVec2(width, height));
			const auto next = ImGui::GetCursorScreenPos();
			auto *draw = ImGui::GetWindowDrawList();
			const ImVec2 capEnd(at.x + 65, at.y + height);
			ImGui::SetCursorScreenPos(at);
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::InvisibleButton("Key cap", ImVec2(65, height)))
				selected = static_cast<int>(i);
			ImGui::PopID();
			draw->AddRectFilled(at, capEnd, colors[i], 4);
			textFit(draw, at, capEnd, keyName(preferences.keys[i]), IM_COL32(8, 18, 30, 255));
			arrow(draw, ImVec2(capEnd.x + 3, at.y + height / 2),
			      ImVec2(capEnd.x + 20, at.y + height / 2), colors[i], 1.5f);
			if (callout(static_cast<int>(i), keyName(preferences.keys[i]), ImVec2(at.x + 90, at.y),
			            ImVec2(std::max(40.0f, width - 90), height), capturing))
				selected = static_cast<int>(i);
			ImGui::SetCursorScreenPos(next);
		}
		ImGui::PopID();
		return selected;
	}
	const auto &keys = layout(preferences);
	const float unit = (width - 16) / KeyboardWidth;
	float rows = 6.3f;
	for (const auto &key : keys)
		rows = std::max(rows, key.y + key.height);
	const float labelHeight = font * 2 + 14, routing = 25;
	const float keyTop = origin.y + labelHeight + routing;
	const float keyBottom = keyTop + rows * unit;
	const float height = keyBottom - origin.y + routing + labelHeight;
	// Reserve the canvas without claiming clicks intended for the callout
	// buttons drawn inside it. Only the actual key caps use rectangle hit tests.
	ImGui::Dummy(ImVec2(width, height));
	const bool hoveredCanvas = ImGui::IsItemHovered();
	const bool clickedCanvas = hoveredCanvas && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	const auto next = ImGui::GetCursorScreenPos();
	auto *draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(origin.x, keyTop - 5), ImVec2(origin.x + width, keyBottom + 5),
	                    IM_COL32(10, 17, 28, 255), 6);
	std::array<const Key *, settings::BindingCount> bound{};
	std::vector<int> top, bottom;
	for (size_t i = 0; i < bound.size(); ++i) {
		// Keep the presentation safe even if a caller supplies a transient model
		// with duplicate keys. The actual rebind path rejects such a model.
		auto key = std::find_if(keys.begin(), keys.end(),
		                        [&](const Key &k) { return k.scancode == preferences.keys[i]; });
		if (key != keys.end()) {
			bound[i] = &*key;
			(key->y < 3 ? top : bottom).push_back(static_cast<int>(i));
		}
	}
	auto leaders = [&](std::vector<int> group, bool upper) {
		std::sort(group.begin(), group.end(), [&](int a, int b) {
			const float x = bound[a]->x + bound[a]->width / 2;
			const float y = bound[b]->x + bound[b]->width / 2;
			return x == y ? a < b : x < y;
		});
		const float boxWidth = group.empty() ? width : (width + gap) / group.size() - gap;
		for (size_t rank = 0; rank < group.size(); ++rank) {
			const int action = group[rank];
			const auto &key = *bound[action];
			const ImVec2 box(origin.x + rank * (boxWidth + gap),
			                 upper ? origin.y : keyBottom + routing);
			const ImVec2 anchor(origin.x + 8 +
			                        (key.x + key.width * (.35f + .1f * (action % 3))) * unit,
			                    keyTop + (key.y + (upper ? 0 : key.height)) * unit);
			const ImVec2 end(box.x + boxWidth / 2, box.y + (upper ? labelHeight : 0));
			const ImU32 color = (colors[action] & 0x00FFFFFFU) | (155U << 24);
			arrow(draw, anchor, end, color, capturing == action ? 2.5f : 1.25f);
			if (callout(action, keyName(preferences.keys[action]), box,
			            ImVec2(boxWidth, labelHeight), capturing))
				selected = action;
		}
	};
	leaders(top, true);
	leaders(bottom, false);
	int keyCount = 0;
	const auto *pressed = SDL_GetKeyboardState(&keyCount);
	for (const auto &key : keys) {
		const int binding = bindingForKey(preferences, key.scancode);
		const bool active = binding >= 0;
		const ImVec2 lo(origin.x + 8 + key.x * unit, keyTop + key.y * unit);
		const ImVec2 hi(lo.x + (key.width - .09f) * unit, lo.y + (key.height - .09f) * unit);
		const bool hovered = hoveredCanvas && ImGui::IsMouseHoveringRect(lo, hi);
		const bool down = key.scancode >= 0 && key.scancode < keyCount && pressed[key.scancode];
		draw->AddRectFilled(lo, hi, active ? colors[binding] : IM_COL32(25, 36, 51, 255), 3);
		draw->AddRect(lo, hi,
		              down || hovered || (active && capturing == binding)
		                  ? IM_COL32(245, 250, 255, 255)
						  : IM_COL32(47, 63, 82, 255),
		              3, 0, down ? 2.5f : 1);
		textFit(draw, lo, hi, key.legend,
		        active ? IM_COL32(8, 18, 30, 255) : IM_COL32(131, 147, 166, 255));
		if (hovered) {
			ImGui::SetTooltip("%s\n%s", keyName(key.scancode).c_str(),
			                  active ? settings::bindingName(binding)
			                         : "No primary action assigned");
			if (active && clickedCanvas)
				selected = binding;
		}
	}
	ImGui::SetCursorScreenPos(next);
	ImGui::PopID();
	return selected;
}
} // namespace keyboard
