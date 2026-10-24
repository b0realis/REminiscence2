// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_RUNTIME_PATHS_H
#define REMINISCENCE2_RUNTIME_PATHS_H
#include <string>
struct RuntimePaths { std::string data, tunes, saves, config; };
// Resolves defaults without creating directories or requiring SDL video/audio.
RuntimePaths runtimePaths(bool portable);
#endif
