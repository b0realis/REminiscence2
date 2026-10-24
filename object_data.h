// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_OBJECT_DATA_H
#define REMINISCENCE2_OBJECT_DATA_H
#include "intern.h"
#include <cstddef>
#include <vector>
struct DecodedObjects {
	std::vector<std::vector<Object>> nodes;
	std::vector<unsigned> map;
};
bool decodeObjects(const uint8_t *data, size_t size, unsigned count, size_t tableOffset,
                   bool bigEndian, DecodedObjects &output);
#endif
