// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_CONTROLLER_PROFILE_H
#define REMINISCENCE2_CONTROLLER_PROFILE_H
#include "settings.h"
namespace controller {
// SDL GUID + mapped/raw kind is a stable identity across reconnects. Instance
// IDs are deliberately excluded: SDL changes them each time a device opens.
std::string profilePath(const std::string &directory, const std::string &guid, bool raw);
void copyBindings(settings::Preferences &destination, const settings::Preferences &source);
bool load(const std::string &path, settings::Preferences &preferences, std::string &reason);
bool save(const std::string &path, const settings::Preferences &preferences, std::string &reason);
} // namespace controller
#endif
