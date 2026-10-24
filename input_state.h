// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_INPUT_STATE_H
#define REMINISCENCE2_INPUT_STATE_H
#include "systemstub.h"
#include <SDL.h>
#include <algorithm>
#include <cstdlib>

// Keep physical sources separate. Releasing a second key, returning a stick
// to centre, or unplugging a pad must not release another held source.
class InputState {
	std::array<uint32_t, SDL_NUM_SCANCODES> _keys{};
	std::array<bool, SDL_CONTROLLER_BUTTON_MAX> _buttons{};
	std::array<bool, 32> _rawButtons{};
	std::array<bool, 2> _triggers{};
	std::array<uint32_t, 3> _bufferStart{};
	uint32_t _physicalPublished = 0, _bufferActive = 0;
	std::array<int, 4> _axes{};
	std::array<int, 4> _directions{};
	uint8_t _hat = 0;
	uint32_t _published = 0;
	bool _sticky = false, _physicalAction = false;
	enum : uint32_t {
		Use = 1 << 4,
		Weapon = 1 << 5,
		Action = 1 << 6,
		Inventory = 1 << 7,
		Pause = 1 << 8,
		Save = 1 << 9,
		Load = 1 << 10,
		Rewind = 1 << 11
	};
	static uint32_t action(size_t i) {
		static const uint32_t bits[] = {Action, Use, Weapon, Inventory, Pause, Save, Load, Rewind};
		return bits[i];
	}

public:
	static uint32_t keyAction(SDL_Keycode key) {
		switch (key) {
		case SDLK_LEFT:
			return PlayerInput::DIR_LEFT;
		case SDLK_RIGHT:
			return PlayerInput::DIR_RIGHT;
		case SDLK_UP:
			return PlayerInput::DIR_UP;
		case SDLK_DOWN:
			return PlayerInput::DIR_DOWN;
		case SDLK_RETURN:
			return Use;
		case SDLK_SPACE:
			return Weapon;
		case SDLK_LSHIFT:
		case SDLK_RSHIFT:
			return Action;
		case SDLK_BACKSPACE:
		case SDLK_TAB:
			return Inventory;
		case SDLK_ESCAPE:
			return Pause;
		default:
			return 0;
		}
	}
	void key(int code, SDL_Keycode mapped, bool down) {
		if (code > 0 && code < SDL_NUM_SCANCODES)
			_keys[code] = down ? keyAction(mapped) : 0;
	}
	void button(int code, bool down, bool raw) {
		if (raw) {
			if (code >= 0 && code < int(_rawButtons.size()))
				_rawButtons[code] = down;
		} else if (code >= 0 && code < int(_buttons.size()))
			_buttons[code] = down;
	}
	void axis(int index, int value) {
		if (index >= 0 && index < 4)
			_axes[index] = value;
		else if (index == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
		         index == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
			auto &pressed = _triggers[index - SDL_CONTROLLER_AXIS_TRIGGERLEFT];
			if (value >= 16000)
				pressed = true;
			else if (value <= 10000)
				pressed = false;
		}
	}
	void hat(uint8_t value) { _hat = value; }
	void publish(PlayerInput &pi, const settings::Preferences &p, bool gameplay = false,
	             uint32_t now = 0) {
		uint32_t held = 0;
		for (auto key : _keys)
			held |= key;
		for (size_t i = 0; i < settings::PadBindingCount; ++i) {
			const int b = p.padButtons[i], r = p.joystickButtons[i];
			if ((b >= 0 && b < int(_buttons.size()) && _buttons[b]) ||
			    ((b == 32 || b == 33) && _triggers[b - 32]) ||
			    (r >= 0 && r < int(_rawButtons.size()) && _rawButtons[r]))
				held |= action(i);
		}
		// Start remains a reliable route to settings, including after remapping.
		if (_buttons[SDL_CONTROLLER_BUTTON_START] || _rawButtons[9])
			held |= Pause;
		if (_buttons[SDL_CONTROLLER_BUTTON_BACK] &&
		    std::find(p.padButtons.begin(), p.padButtons.end(), SDL_CONTROLLER_BUTTON_BACK) ==
		        p.padButtons.end())
			held |= Pause;
		const int commit = p.stickDeadzone * 32767 / 100;
		const int release = std::max(0, p.stickDeadzone - 3) * 32767 / 100;
		for (int i = 0; i < 4; ++i) {
			const int v = _axes[i];
			if (v > commit)
				_directions[i] = 1;
			else if (v < -commit)
				_directions[i] = -1;
			else if (std::abs(v) <= release)
				_directions[i] = 0;
			if (i >= 2 && !p.rightStickMovement)
				continue;
			if (i & 1)
				held |= _directions[i] < 0   ? PlayerInput::DIR_UP
				        : _directions[i] > 0 ? PlayerInput::DIR_DOWN
				                             : 0;
			else
				held |= _directions[i] < 0   ? PlayerInput::DIR_LEFT
				        : _directions[i] > 0 ? PlayerInput::DIR_RIGHT
				                             : 0;
		}
		if ((_hat & SDL_HAT_UP) || _buttons[SDL_CONTROLLER_BUTTON_DPAD_UP])
			held |= PlayerInput::DIR_UP;
		if ((_hat & SDL_HAT_DOWN) || _buttons[SDL_CONTROLLER_BUTTON_DPAD_DOWN])
			held |= PlayerInput::DIR_DOWN;
		if ((_hat & SDL_HAT_LEFT) || _buttons[SDL_CONTROLLER_BUTTON_DPAD_LEFT])
			held |= PlayerInput::DIR_LEFT;
		if ((_hat & SDL_HAT_RIGHT) || _buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT])
			held |= PlayerInput::DIR_RIGHT;
		const uint32_t physicalPressed = held & ~_physicalPublished;
		_physicalPublished = held;
		static const uint32_t buffered[] = {PlayerInput::DIR_UP, Use, Weapon};
		if (p.inputBuffer && gameplay) {
			for (unsigned i = 0; i < 3; ++i) {
				if (physicalPressed & buffered[i]) {
					_bufferStart[i] = now;
					_bufferActive |= 1u << i;
				}
				if ((_bufferActive & (1u << i)) && uint32_t(now - _bufferStart[i]) < 100)
					held |= buffered[i];
				else
					_bufferActive &= ~(1u << i);
			}
		} else
			_bufferActive = 0;
		const bool physicalAction = held & Action;
		if (p.stickyAction && gameplay) {
			if (physicalAction && !_physicalAction)
				_sticky = !_sticky;
			held = (held & ~Action) | (_sticky ? Action : 0);
		} else
			_sticky = false;
		_physicalAction = physicalAction;
		pi.dirMask = held & 15;
		const uint32_t changed = held ^ _published, pressed = changed & held;
		// The engine consumes action flags in menus. Only physical transitions
		// may assert them again; unrelated axis noise must not retrigger an item.
		if (changed & Use)
			pi.enter = held & Use;
		if (changed & Weapon)
			pi.space = held & Weapon;
		if (changed & Action)
			pi.shift = held & Action;
		if (changed & Inventory)
			pi.backspace = held & Inventory;
		if (changed & Pause)
			pi.escape = held & Pause;
		if (pressed & Save)
			pi.save = true;
		if (pressed & Load)
			pi.load = true;
		if (pressed & Rewind)
			pi.rewind = true;
		_published = held;
	}
	void clearPad(PlayerInput &pi, const settings::Preferences &p) {
		_buttons.fill(false);
		_rawButtons.fill(false);
		_triggers.fill(false);
		_bufferActive = 0;
		_axes.fill(0);
		_directions.fill(0);
		_hat = 0;
		_sticky = _physicalAction = false;
		publish(pi, p);
	}
	void clear(PlayerInput &pi) {
		*this = InputState{};
		pi.dirMask = 0;
		pi.enter = pi.space = pi.shift = pi.backspace = pi.escape = false;
		pi.save = pi.load = pi.rewind = false;
	}
};
#endif
