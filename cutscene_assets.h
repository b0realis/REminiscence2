// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_CUTSCENE_ASSETS_H
#define REMINISCENCE2_CUTSCENE_ASSETS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Shared by the engine loader and the cutscene browser. CMP contains a polygon
// block followed by a command block; each can be raw or Bytekiller-compressed.
bool decodeCutsceneCmp(const uint8_t *data, size_t size, std::vector<uint8_t> &polygons,
                       std::vector<uint8_t> &commands, std::string &reason);

// DOS/Amiga offsets are relative to the command body. Mac offsets are absolute.
// Part zero in DOS/Amiga starts at the body, regardless of its table value.
bool cutscenePartOffsets(const uint8_t *data, size_t size, bool macintosh,
                         std::vector<size_t> &offsets, std::string &reason);

#endif
