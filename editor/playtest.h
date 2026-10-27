// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.h"
namespace editor {
// Session-only launch position; never changes the authored campaign start.
struct PlaytestStart {
	Anchor at;
	bool facingLeft = false;
};
bool clearPlaytestStart(const Project &, const Anchor &, bool requireFloor = false);
PlaytestStart roomPlaytestStart(const Project &, const std::string &room,
                                const Anchor *near = nullptr);
void validatePlaytestStart(const Project &, const PlaytestStart &);
} // namespace editor
