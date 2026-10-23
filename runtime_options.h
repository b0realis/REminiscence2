// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_RUNTIME_OPTIONS_H
#define REMINISCENCE2_RUNTIME_OPTIONS_H

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Device settings are passed to the platform boundary, independently of game
// compatibility switches and simulation timing. The device may negotiate a
// different rate; consumers must use getOutputSampleRate() after opening it.
struct RuntimeOptions {
	int outputSampleRate = 22050;
	bool maximized = false;
};

inline bool parseUnsignedOption(const char *text, uint32_t minimum, uint32_t maximum,
                                uint32_t &result) {
	if (!text || !*text || *text == '-' || *text == '+' || *text == ' ' || *text == '\t') {
		return false;
	}
	char *end = nullptr;
	errno = 0;
	const int base = std::strncmp(text, "0x", 2) == 0 || std::strncmp(text, "0X", 2) == 0 ? 16 : 10;
	const unsigned long long value = std::strtoull(text, &end, base);
	if (errno || end == text || *end || value < minimum || value > maximum) {
		return false;
	}
	result = static_cast<uint32_t>(value);
	return true;
}

#endif
