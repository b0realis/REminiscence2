// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_DECODE_SEQ_H
#define REMINISCENCE2_DECODE_SEQ_H
#include <cstddef>
#include <cstdint>
// Decode a 256x128 movie frame into the centre of a 256x224 game layer.
// Failure never publishes the partially decoded frame to the renderer.
bool decodeSeqFrame(const uint8_t *source, size_t size, uint8_t *destination, size_t capacity);
#endif
