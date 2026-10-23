// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_HEADLESS_SYSTEM_H
#define REMINISCENCE2_HEADLESS_SYSTEM_H

#include "systemstub.h"

// Deterministic platform boundary for real resource/simulation tests. It never
// opens a window or sound device; the engine still renders its indexed layers.
struct HeadlessSystem : SystemStub {
	Color palette[256] = {};
	uint32_t clock = 0;
	WidescreenMode mode = kWidescreenNone;
	int leftCopies = 0, rightCopies = 0;
	AudioCallback audioCallback = nullptr;
	void *audioUser = nullptr;

	HeadlessSystem() { _pi = {}; }
	void init(const char *, int, int, bool, int wide, const ScalerParameters *) override {
		mode = static_cast<WidescreenMode>(wide);
	}
	void destroy() override {}
	bool applyPreferences(const settings::Preferences &p, std::string &) override {
		mode = static_cast<WidescreenMode>(p.widescreen);
		return true;
	}
	bool hasWidescreen() const override { return mode != kWidescreenNone; }
	void setScreenSize(int, int) override {}
	void setPalette(const uint8_t *p, int count) override {
		for (int i = 0; i < count; ++i) {
			palette[i] = {p[0], p[1], p[2]};
			p += 3;
		}
	}
	void getPalette(uint8_t *p, int count) override {
		for (int i = 0; i < count; ++i) {
			*p++ = palette[i].r;
			*p++ = palette[i].g;
			*p++ = palette[i].b;
		}
	}
	void setPaletteEntry(int i, const Color *color) override { palette[i] = *color; }
	void getPaletteEntry(int i, Color *color) override { *color = palette[i]; }
	void setOverscanColor(int) override {}
	void copyRect(int, int, int, int, const uint8_t *, int) override {}
	void copyRectRgb24(int, int, int, int, const uint8_t *) override {}
	void zoomRect(int, int, int, int) override {}
	void copyWidescreenLeft(int, int, const uint8_t *, bool = true) override { ++leftCopies; }
	void copyWidescreenRight(int, int, const uint8_t *, bool = true) override { ++rightCopies; }
	void copyWidescreenMirror(int, int, const uint8_t *) override {}
	void copyWidescreenBlur(int, int, const uint8_t *) override {}
	void copyWidescreenCDi(int, int, const uint8_t *, const uint8_t *) override {}
	void clearWidescreen() override {}
	void enableWidescreen(bool) override {}
	void fadeScreen() override {}
	void updateScreen(int) override {}
	void processEvents() override {}
	void sleep(int millis) override { clock += millis > 0 ? millis : 0; }
	uint32_t getTimeStamp() override { return clock; }
	void startAudio(AudioCallback cb, void *user) override {
		audioCallback = cb;
		audioUser = user;
	}
	void stopAudio() override { audioCallback = nullptr; }
	uint32_t getOutputSampleRate() override { return 22050; }
	void lockAudio() override {}
	void unlockAudio() override {}
};

#endif
