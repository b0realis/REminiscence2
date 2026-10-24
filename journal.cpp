// SPDX-License-Identifier: GPL-3.0-or-later
// Original companion prose, rather than reproduced dialogue or manual pages.
#include "journal.h"
namespace journal {
std::vector<Entry> State::entries() const {
	static const char *const titles[] = {
	    "Among the leaves",      "A city of strangers", "Under the lights", "Back on Earth",
	    "Behind the club doors", "An unfamiliar world", "Further inside",   "Fragments of memory"};
	static const char *const passages[] = {
	    "The jungle offers no explanation. Conrad can trust the ground beneath his feet, the "
	    "weight of his equipment, and the next ledge. His past will have to wait until he survives "
	    "the present.",
	    "New Washington replaces the wilderness with streets, doors, and unfamiliar faces. "
	    "Reaching a city has not made the questions disappear. Here, listening may be as useful as "
	    "a weapon.",
	    "The Death Tower turns survival into a spectacle. Beyond the lights, every platform and "
	    "opening still asks the same question: can Conrad make the next move count?",
	    "Earth should feel familiar. Coming back, however, does not make everything clear. Conrad "
	    "has travelled far enough to recognise that appearances alone are a poor guide to whom he "
	    "can trust.",
	    "The Paradise Club is another threshold. Familiar surroundings give way to guarded spaces, "
	    "and the journey becomes less about finding a destination than understanding what lies "
	    "behind it.",
	    "The landscape is alien now. The distance from home is more than a journey between places: "
	    "every passage leads into a world whose rules Conrad must learn while moving through it.",
	    "There is less room for uncertainty this deep inside. The route has narrowed, but the "
	    "habits that brought Conrad here still matter: watch, listen, and leave himself a way "
	    "through.",
	    "Memory returns in fragments rather than certainty. Conrad has seen another piece of his "
	    "past. He must carry what he has learned into the present, without mistaking an incomplete "
	    "picture for the whole truth."};
	std::vector<Entry> result;
	for (unsigned i = 0; i < EntryCount; ++i)
		if (unlocked & (1u << i))
			result.push_back({i, titles[i], passages[i], !(read & (1u << i))});
	return result;
}
std::vector<Objective> objectives(const State &state, int currentLevel) {
	static const char *const goals[] = {
	    "Explore the jungle, recover useful equipment and find a route out.",
	    "Explore New Washington. Listen to people and follow the tasks they give Conrad.",
	    "Work through the Death Tower, one safe platform and encounter at a time.",
	    "Explore Earth and investigate the route ahead. Watch for guards and usable doors.",
	    "Investigate the spaces beyond the club. Keep track of passages and equipment.",
	    "Learn the alien terrain and search for a route deeper inside.",
	    "Follow the final route carefully and use what Conrad has learned."};
	static const char *const hints[] = {
	    "Watch how far Conrad can jump before committing. Check the inventory for usable items; "
		"Action near an object can collect or operate it.",
	    "Read conversations fully. Jobs and equipment matter more than rushing; check your "
		"inventory when a task mentions an object.",
	    "Observe enemy timing from cover. Holster the weapon when a move needs free hands, and "
		"keep a recovery point before a difficult stretch.",
	    "Check both directions before crossing an exposed screen. Use the weapon deliberately, "
		"then return to movement controls.",
	    "Revisit a closed passage after finding an item. Select the relevant object in the "
		"inventory before trying Use.",
	    "Pause to study unfamiliar platforms. Try short, controlled moves and use rewind if you "
		"have enabled it.",
	    "Keep a named, protected milestone before the next difficult encounter. Restore it from "
		"the recovery screen if needed."};
	std::vector<Objective> result;
	for (const auto &entry : state.entries())
		if (entry.id < 7)
			result.push_back({entry.id, entry.title, goals[entry.id], hints[entry.id],
			                  int(entry.id) == currentLevel});
	return result;
}
} // namespace journal
