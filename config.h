// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_CONFIG_H
#define REMINISCENCE2_CONFIG_H

#include "intern.h"

void setDefaultOptions(Options &options);
bool loadConfiguration(const char *filename, Options &options);

#endif
