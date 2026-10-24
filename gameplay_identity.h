// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_GAMEPLAY_IDENTITY_H
#define REMINISCENCE2_GAMEPLAY_IDENTITY_H
#include <cstdint>
struct Resource;
uint32_t gameplayFingerprint(const Resource &res);
#endif
