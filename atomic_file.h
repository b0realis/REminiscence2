// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_ATOMIC_FILE_H
#define REMINISCENCE2_ATOMIC_FILE_H
#include <functional>
#include <string>
struct File;
// The writer owns no paths. Failed writes never replace the destination.
bool writeAtomicFile(const std::string &path, bool compressed,
                     const std::function<void(File &)> &writer, std::string &reason);
#endif
