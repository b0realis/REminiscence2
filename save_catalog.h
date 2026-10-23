// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_SAVE_CATALOG_H
#define REMINISCENCE2_SAVE_CATALOG_H
#include "settings.h"
struct File;
namespace saves {
std::vector<settings::SaveSlot> scan(const char *directory, int level);
// Validate serialized counts, links and truncation before the legacy decoder
// can modify the running game. Leaves a valid file at its payload (byte 38).
bool validate(File &file, unsigned entities, int &version, std::string &reason);
} // namespace saves
#endif
