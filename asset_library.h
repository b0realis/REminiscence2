// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_ASSET_LIBRARY_H
#define REMINISCENCE2_ASSET_LIBRARY_H
#include "intern.h"
#include <string>
#include <vector>

namespace assets {
struct Source {
	std::string id, label, root;
	int edition = -1; // -1 is an external soundtrack folder.
	std::vector<std::string> excludedRoots;
};
struct Library {
	std::vector<Source> games, music;
	std::vector<std::string> diagnostics;
};
const char *editionId(int edition);
// Discover each edition independently. No file index spans two detected games.
Library discover(const std::string &root);
int find(const std::vector<Source> &sources, const std::string &id);
} // namespace assets
#endif
