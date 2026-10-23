// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_EDITION_H
#define REMINISCENCE2_EDITION_H
#include "intern.h"
struct EditionTraits {
	bool extendedIntroduction;
	bool bigEndian;
	int resolutionScale;
	bool shapeProtection;
};
// Immutable per-edition facts. Callers ask about their own ResourceType;
// creating another engine/test instance cannot change a global edition.
constexpr EditionTraits editionTraits(ResourceType type) {
	switch (type) {
	case kResourceTypeAmiga:
		return {false, true, 1, true};
	case kResourceTypeDOS:
		return {true, false, 1, true};
	case kResourceTypeMac:
		return {true, true, 2, false};
	case kResourceTypePC98:
		return {true, false, 1, false};
	case kResourceTypeSega:
		return {false, true, 1, false};
	}
	return {false, false, 1, false};
}
#endif
