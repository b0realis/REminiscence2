// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_CLI_TEXT_H
#define REMINISCENCE2_CLI_TEXT_H

#include <cstdio>

namespace cli {
void printStartup();
void printUsage(FILE *stream, const char *program);
void printHelp(const char *program);
void printVersionDetails();
} // namespace cli

#endif
