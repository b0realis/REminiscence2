// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_MENU_INPUT_H
#define REMINISCENCE2_MENU_INPUT_H

#include <cstdint>
#include <deque>
#include <algorithm>
#include <cmath>

// Menus consume presses, while gameplay consumes held controls. Keep an ordered
// queue so a complete press/release between two rendered frames is not lost.
class MenuInput {
public:
	enum Type {
		None,
		Up = 1,
		Down = 2,
		Left = 4,
		Right = 8,
		Confirm = 16,
		Cancel = 32,
		Erase = 64,
		Hover = 128,
		Click = 256
	};
	struct Event {
		Type type = None;
		int x = 0, y = 0;
		bool pointer() const { return type == Hover || type == Click; }
		bool inside(int left, int top, int width, int height) const {
			return pointer() && x >= left && y >= top && x < left + width && y < top + height;
		}
	};
	void push(Type type, int x = 0, int y = 0) {
		// Coalesce motion only: it must not bury a click, key or wheel step.
		if (type == Hover && !_events.empty() && _events.back().type == Hover)
			_events.back() = {type, x, y};
		else if (_events.size() < 64)
			_events.push_back({type, x, y});
	}
	void buttons(uint32_t held, uint32_t now) {
		const uint32_t pressed = held & ~_held;
		for (const auto type : {Up, Down, Left, Right, Confirm, Cancel, Erase})
			if (pressed & type)
				push(type);
		if ((held & 15) != (_held & 15)) {
			_stamp = now;
			_delay = 400;
		}
		_held = held;
	}
	void wheel(float delta) {
		if (!std::isfinite(delta))
			return;
		if ((delta > 0 && _wheel < 0) || (delta < 0 && _wheel > 0))
			_wheel = 0;
		// Trackpads report fractional notches. Accumulate them instead of either
		// losing small movements or advancing a row for every tiny event.
		_wheel += std::clamp(delta, -8.f, 8.f);
		while (_wheel >= 1) {
			push(Up);
			--_wheel;
		}
		while (_wheel <= -1) {
			push(Down);
			++_wheel;
		}
	}
	void repeat(uint32_t now) {
		if ((_held & 15) && uint32_t(now - _stamp) >= _delay) {
			for (const auto type : {Up, Down, Left, Right})
				if (_held & type)
					push(type);
			// Never catch up after a stalled frame or suspended application.
			_stamp = now;
			_delay = 120;
		}
	}
	Event next() {
		if (_events.empty())
			return {};
		const auto event = _events.front();
		_events.pop_front();
		return event;
	}

private:
	std::deque<Event> _events;
	uint32_t _held = 0, _stamp = 0, _delay = 400;
	float _wheel = 0;
};

#endif
