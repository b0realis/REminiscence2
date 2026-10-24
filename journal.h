// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_JOURNAL_H
#define REMINISCENCE2_JOURNAL_H
#include <cstdint>
#include <vector>
#include <string>
namespace journal {
struct Entry {
	unsigned id;
	std::string title, text;
	bool unread;
};
constexpr unsigned EntryCount = 8;
constexpr uint32_t ValidMask = (1u << EntryCount) - 1;
struct State {
	uint32_t unlocked = 0, read = 0;
	void chapter(unsigned level) {
		if (level < 7)
			unlocked |= 1u << level;
	}
	void memory() { unlocked |= 1u << 7; }
	void markRead(unsigned id) {
		if (id < EntryCount)
			read |= unlocked & (1u << id);
	}
	bool unread() const { return (unlocked & ~read) != 0; }
	std::vector<Entry> entries() const;
};
struct Objective {
	unsigned level;
	std::string title, goal, hint;
	bool current;
};
// Derive only from persisted, reached chapters. No separate quest flags can
// become stale after a rewind, legacy import or level jump.
std::vector<Objective> objectives(const State &state, int currentLevel);
} // namespace journal
#endif
