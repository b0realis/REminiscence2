// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_DECODE_SGD_H
#define REMINISCENCE2_DECODE_SGD_H
#include <cstddef>
#include <cstdint>
#include <vector>
bool decodeAmigaRle(const uint8_t *source, size_t size, std::vector<uint8_t> &output,
                    size_t capacity);
// Decode a bounded room foreground transactionally into a 256x224 indexed layer.
bool decodeSgdForeground(uint8_t *frame, const uint8_t *commands, size_t commandSize,
                         const uint8_t *tiles, size_t tileSize, bool amiga, int level, int room);
#endif
