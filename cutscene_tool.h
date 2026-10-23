// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_CUTSCENE_TOOL_H
#define REMINISCENCE2_CUTSCENE_TOOL_H

#include <cstdio>
#include <string>
#include <vector>

struct Resource;
struct Game;

namespace cinematics {
struct Entry {
	std::string name;
	std::string title;
	int resourceIndex;
	unsigned parts;
};
struct Selection {
	std::string name;
	unsigned part = 0;
};

bool parseSelection(const char *value, Selection &selection);
std::vector<Entry> catalog(Resource &resource);
void printCatalog(FILE *stream, const std::vector<Entry> &entries, const Resource &resource);
// The caller initializes the graphics platform; this runs no title/game menus.
bool play(Game &game, const Selection &selection);
} // namespace cinematics

#endif
