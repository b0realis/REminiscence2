// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_USER_NOTICE_H
#define REMINISCENCE2_USER_NOTICE_H
#include <cstdint>
#include <string>
// One bounded HUD notice. Errors stay visible through routine autosave success;
// unsigned tick subtraction remains correct across SDL's 32-bit clock wrap.
struct UserNotice {
	std::string text;
	bool error = false;
	uint32_t started = 0;
	bool visible(uint32_t now) const {
		return !text.empty() && uint32_t(now - started) < (error ? 8000u : 3500u);
	}
	void push(const std::string &message, bool failed, uint32_t now) {
		if (!failed && error && visible(now) && message.compare(0, 8, "Autosave") == 0)
			return;
		text = message.substr(0, 512);
		error = failed;
		started = now;
	}
};
#endif
