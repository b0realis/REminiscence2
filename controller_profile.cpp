// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller_profile.h"
#include <algorithm>
#include <filesystem>
namespace controller {
std::string profilePath(const std::string &directory, const std::string &guid, bool raw) {
	if (directory.empty() || guid.size() != 32 ||
	    !std::all_of(guid.begin(), guid.end(),
	                 [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
		return {};
	return (std::filesystem::u8path(directory) /
	        ("controller-" + guid + (raw ? "-raw.cfg" : "-mapped.cfg")))
	    .u8string();
}
void copyBindings(settings::Preferences &to, const settings::Preferences &from) {
	to.padButtons = from.padButtons;
	to.joystickButtons = from.joystickButtons;
	to.stickDeadzone = from.stickDeadzone;
	to.rightStickMovement = from.rightStickMovement;
}
bool load(const std::string &path, settings::Preferences &p, std::string &reason) {
	reason.clear();
	if (path.empty())
		return true;
	std::error_code ec;
	if (!std::filesystem::exists(std::filesystem::u8path(path), ec) && !ec)
		return true;
	settings::Preferences loaded = p;
	if (!settings::load(path, loaded, reason) || !reason.empty()) {
		if (reason.empty())
			reason = "Cannot read this controller profile; global bindings were kept.";
		return false;
	}
	// Missing files inherit global bindings, not built-in defaults.
	if (std::filesystem::is_regular_file(std::filesystem::u8path(path), ec))
		copyBindings(p, loaded);
	return true;
}
bool save(const std::string &path, const settings::Preferences &p, std::string &reason) {
	if (path.empty())
		return true;
	settings::Preferences profile;
	copyBindings(profile, p);
	return settings::save(path, profile, reason);
}
} // namespace controller
