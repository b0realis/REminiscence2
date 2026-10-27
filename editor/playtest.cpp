// SPDX-License-Identifier: GPL-3.0-or-later
#include "playtest.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
namespace editor {
static std::array<int8_t, 7424> startingCollision(const Project &p) {
	auto ct = compileCollision(p);
	auto block = [&](const Anchor &a, int w, int h) {
		const int room = roomIndex(p, a.room);
		for (int y = (a.y - h) / 36; y <= (a.y - 1) / 36; ++y)
			for (int x = a.x / 16; x < (a.x + w) / 16; ++x)
				ct[256 + room * 112 + y * 16 + x] = 1;
	};
	for (const auto &m : p.modules) {
		if (m.kind == Kind::Door && !m.initiallyOpen)
			block(m.a, m.width, m.height);
		for (const auto &n : m.nodes)
			if (n.kind == NodeKind::Door && !n.initiallyActive)
				block(n.at, n.width, n.height);
	}
	return ct;
}
static bool clear(const Project &p, const std::array<int8_t, 7424> &ct, const Anchor &at,
                  bool floor) {
	const int r = roomIndex(p, at.room);
	if (r < 0 || at.x < 0 || at.x > 240 || at.y < 0 || at.y > 214)
		return false;
	// Native actor positions sit two pixels above a 72-pixel floor boundary.
	const int row = std::clamp((at.y + 2) / 36, 0, 6);
	const int left = at.x / 16, right = (at.x + 15) / 16;
	for (int y = std::max(0, row - 2); y < row; ++y)
		for (int x = left; x <= right; ++x)
			if (ct[256 + r * 112 + y * 16 + x] & 1)
				return false;
	return !floor || (ct[256 + r * 112 + row * 16 + (at.x + 8) / 16] & 1);
}
bool clearPlaytestStart(const Project &p, const Anchor &at, bool requireFloor) {
	return clear(p, startingCollision(p), at, requireFloor);
}
void validatePlaytestStart(const Project &p, const PlaytestStart &start) {
	if (!clearPlaytestStart(p, start.at))
		throw std::runtime_error(
		    "Test position is outside the room or blocked; move the TEST marker");
}
PlaytestStart roomPlaytestStart(const Project &p, const std::string &room, const Anchor *near) {
	if (roomIndex(p, room) < 0)
		throw std::runtime_error("Choose a room to playtest");
	const auto ct = startingCollision(p);
	const Anchor preferred = near ? *near : p.spawn.room == room ? p.spawn : Anchor{room, 32, 214};
	if (preferred.room == room && clear(p, ct, preferred, true))
		return {preferred, p.spawnLeft};
	int best = std::numeric_limits<int>::max();
	Anchor result{room, 32, 214};
	for (int y : {70, 142, 214})
		for (int x = 8; x <= 232; x += 8) {
			Anchor candidate{room, x, y};
			const int distance = std::abs(x - preferred.x) + std::abs(y - preferred.y);
			if (distance < best && clear(p, ct, candidate, true)) {
				result = candidate;
				best = distance;
			}
		}
	if (best == std::numeric_limits<int>::max())
		throw std::runtime_error(
		    "No clear standing position in this room; place a TEST marker or edit its collision");
	return {result, p.spawnLeft};
}
} // namespace editor
