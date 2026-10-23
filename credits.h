// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_CREDITS_H
#define REMINISCENCE2_CREDITS_H
#include <cstddef>
#include <cstdint>
// Validate the compact credits control stream before the legacy renderer sees
// it. Each text segment must fit its fixed display buffer and end explicitly.
inline bool validateCredits(const uint8_t *bytes, size_t size) {
	if (!bytes || !size || size > 65536) {
		return false;
	}
	size_t line = 0;
	for (size_t i = 0; i < size; ++i) {
		const uint8_t code = bytes[i];
		if (code == 0xFF) {
			return true;
		}
		if (code == 0) {
			line = 0;
		} else if (code == 0xFE) {
			if (++i >= size) {
				return false;
			}
		} else if (code == 1) {
			if (size - i <= 2) {
				return false;
			}
			i += 2;
		} else if (++line > 498) {
			return false;
		}
	}
	return false;
}
#endif
