// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_KEYBOARD_DIAGRAM_H
#define REMINISCENCE2_KEYBOARD_DIAGRAM_H

#include "settings.h"

namespace keyboard {
// Physical positions are in standard key-width units. Unusual bound keys get
// their own row rather than disappearing from the illustrated guide.
struct Key {
	int scancode;
	float x, y, width, height;
	std::string legend;
};
int bindingForKey(const settings::Preferences &preferences, int scancode);
const std::vector<Key> &layout(const settings::Preferences &preferences);

// Returns the action selected for rebinding, or -1. This only presents the live
// preference model; the host retains validation, persistence and input capture.
int draw(const settings::Preferences &preferences, int capturing, bool compact);
} // namespace keyboard
#endif
