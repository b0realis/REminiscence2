// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_SAVE_CATALOG_H
#define REMINISCENCE2_SAVE_CATALOG_H
#include "settings.h"
struct File;
struct Resource;
namespace saves {
std::string name(int edition, int level, int slot);
std::vector<settings::SaveSlot> scan(const char *directory, int level, int edition = -1);
std::vector<settings::SaveSlot> scanAll(const char *directory, int edition);
// Validate serialized counts, links and truncation before the legacy decoder
// can modify the running game. Leaves a valid file at its payload (byte 38).
bool validate(File &file, unsigned entities, int &version, std::string &reason, int edition = -1,
              int level = -1, uint32_t fingerprint = 0, const Resource *resource = nullptr);
} // namespace saves
#endif
