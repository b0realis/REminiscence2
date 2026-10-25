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
	float opacity(uint32_t now) const {
		if (!visible(now))
			return 0;
		const uint32_t remaining = (error ? 8000u : 3500u) - uint32_t(now - started);
		return remaining >= 750u ? 1.0f : float(remaining) / 750.0f;
	}
	void push(const std::string &message, bool failed, uint32_t now) {
		if (!failed && error && visible(now) && message.compare(0, 8, "Autosave") == 0)
			return;
		text = message.substr(0, 512);
		error = failed;
		started = now;
	}
};

// The game publishes unread entry bits every frame. Only a newly unread entry
// starts a toast: leaving an entry unread must never extend its lifetime, and
// another chapter must still notify while an older entry remains unread.
struct JournalNotice {
	uint32_t unread = 0;
	UserNotice toast;
	void update(uint32_t entries, uint32_t now) {
		if (entries & ~unread)
			toast.push("Journal: new entry", false, now);
		unread = entries;
		if (!entries)
			toast.text.clear();
	}
};
#endif
