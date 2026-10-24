// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_CONTEXT_HINTS_H
#define REMINISCENCE2_CONTEXT_HINTS_H
#include <cstdint>
#include <string>
namespace settings {
struct Preferences;
}
namespace hints {
enum class Kind { None, Movement, Interact, Inventory, LowHealth };
// Active gameplay ticks, rather than wall time, keep pauses and slow frames from
// consuming a hint. Each reminder appears at most once until reset.
struct State {
	uint32_t seen = 0;
	uint64_t until = 0, next = 0, previous = 0;
	Kind active = Kind::None;
	Kind update(uint64_t tick, bool enabled, bool interaction, bool inventory, bool lowHealth);
};
std::string text(Kind kind, const settings::Preferences &preferences, bool rawJoystick = false,
                 int controllerStyle = 0);
} // namespace hints
#endif
