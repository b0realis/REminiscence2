// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_logic.h"
#include "game.h"
#include <stdexcept>

namespace editor {
void validateNativeLive(const Game &g, const NativeCampaign &before, const NativeCampaign &after) {
	if (before.actors.size() != after.actors.size())
		throw std::runtime_error("Actor slots added: restart to allocate the complete puzzle");
	for (size_t i = 0; i < after.actors.size(); ++i) {
		const auto &a = before.actors[i], &b = after.actors[i];
		const auto &live = g._pgeLive[i];
		const auto &init = g._res._pgeInit[i];
		const auto who = "Actor " + std::to_string(i) + ": ";
		if (a.id != b.id || a.source != b.source || a.objectKind != b.objectKind ||
		    a.type != b.type || a.room != b.room || a.x != b.x || a.y != b.y ||
		    a.facingLeft != b.facingLeft || a.removed != b.removed)
			throw std::runtime_error(
			    who + "placement, initial state or removal changed; restart required");
		if (a.life != b.life && (live.life != a.life || a.life <= 0 || b.life <= 0))
			throw std::runtime_error(
			    who +
			    "health changed during play or crosses death; restart to change initial health");
		if (a.life != b.life && int16_t(init.life) != a.life)
			throw std::runtime_error(who + "initial health no longer matches the applied document");
		for (int d = 0; d < 4; ++d)
			if (a.data[d] != b.data[d] && init.data[d] != a.data[d])
				throw std::runtime_error(who + "dependency changed during play; restart required");
		if (a.behavior < 0 || a.behavior >= int(before.programs.size()) || b.behavior < 0 ||
		    b.behavior >= int(after.programs.size()))
			throw std::runtime_error(who + "missing rule table; restart required");
		if (init.obj_node_number != a.behavior)
			throw std::runtime_error(who + "running behavior differs from applied document");
		const auto &oldRows = before.programs[a.behavior].rules;
		const auto &newRows = after.programs[b.behavior].rules;
		if (nativeProgramSource(before, a.behavior) != nativeProgramSource(after, b.behavior) ||
		    newRows.size() < oldRows.size())
			throw std::runtime_error(
			    who + "behavior resource identity or row layout changed; restart required");
		// The interpreter retains row indices, not Object pointers. Appended rows
		// are safe; changing an existing row's animation state is not. Private
		// behavior copies retain the same resource identity and row numbering.
		for (size_t row = 0; row < oldRows.size(); ++row)
			if (newRows[row].type != oldRows[row].type)
				throw std::runtime_error(who + "existing rule state changed; restart required");
		if (live.first_obj_number >= newRows.size())
			throw std::runtime_error(who + "running rule cursor is outside the replacement table");
	}
}
void applyNativeLiveActors(Game &g, const NativeCampaign &before, const NativeCampaign &after) {
	// Preflight has completed. Touch only authored fields; preserve counters,
	// animation cursors, inventory/room chains, messages and activity flags.
	for (size_t i = 0; i < after.actors.size(); ++i) {
		const auto &a = before.actors[i], &b = after.actors[i];
		auto &init = g._res._pgeInit[i];
		init.obj_node_number = b.behavior;
		for (int d = 0; d < 4; ++d)
			if (a.data[d] != b.data[d])
				init.data[d] = b.data[d];
		if (a.life != b.life) {
			init.life = b.life;
			g._pgeLive[i].life = b.life;
		}
	}
}
} // namespace editor
