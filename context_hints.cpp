// SPDX-License-Identifier: GPL-3.0-or-later
#include "context_hints.h"
#include "settings.h"
#include <SDL_keyboard.h>
namespace hints {
Kind State::update(uint64_t tick, bool enabled, bool interaction, bool inventory, bool lowHealth) {
	if (tick < previous) {
		until = next = 0;
		active = Kind::None;
	}
	previous = tick;
	if (!enabled) {
		active = Kind::None;
		return active;
	}
	if (active != Kind::None && tick < until)
		return active;
	active = Kind::None;
	if (tick < next)
		return active;
	auto offer = [&](Kind kind, bool condition) {
		const auto bit = 1u << unsigned(kind);
		if (active == Kind::None && condition && !(seen & bit)) {
			active = kind;
			seen |= bit;
		}
	};
	offer(Kind::LowHealth, lowHealth);
	offer(Kind::Interact, interaction);
	offer(Kind::Inventory, inventory);
	offer(Kind::Movement, tick >= 30);
	if (active != Kind::None) {
		until = tick + 240;
		next = until + 300;
	}
	return active;
}
std::string text(Kind kind, const settings::Preferences &p, bool rawJoystick, int controllerStyle) {
	auto binding = [&](settings::Binding key, int pad) {
		const char *name = SDL_GetScancodeName(SDL_Scancode(p.keys[size_t(key)]));
		std::string label = name && *name ? name : "Unassigned";
		if (pad >= 0) {
			const int button = rawJoystick ? p.joystickButtons[pad] : p.padButtons[pad];
			if (button >= 0)
				label += rawJoystick ? " / joystick button " + std::to_string(button + 1)
				                     : std::string(" / pad ") +
				                           settings::padButtonName(button, controllerStyle);
		}
		return label;
	};
	switch (kind) {
	case Kind::Movement:
		return p.stickyAction ? "Tap " + binding(settings::Binding::Action, 0) +
		                            " to toggle run/fire. Tap again to release."
		                      : "Hold " + binding(settings::Binding::Action, 0) +
		                            " with a direction to run. Release to walk.";
	case Kind::Interact:
		return "Nearby object: try " + binding(settings::Binding::Use, 1) + ". Some actions use " +
		       binding(settings::Binding::Action, 0) + " + " +
		       binding(settings::Binding::Down, -1) + ".";
	case Kind::Inventory:
		return "Open inventory: " + binding(settings::Binding::Inventory, 3) +
		       ". Move to an item; close inventory to equip it.";
	case Kind::LowHealth:
		return "Low shield. Open " + binding(settings::Binding::Pause, 4) + " for Save / Load" +
		       (p.rewind ? " or Rewind." : ".");
	default:
		return {};
	}
}
} // namespace hints
