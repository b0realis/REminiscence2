// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "systemstub.h"
#include <SDL.h>
namespace editor {
// The editor owns video and input; the game owns the mixer callback. Closing
// the audio device joins its thread before a scene releases its sample data.
struct PreviewSystem final : SystemStub {
	Color palette[256]{};
	SDL_AudioDeviceID audio = 0;
	bool audible = true;
	AudioCallback callback = nullptr;
	void *user = nullptr;
	PreviewSystem() { _pi = {}; }
	~PreviewSystem() { stopAudio(); }
	void init(const char *, int, int, bool, int, const ScalerParameters *) override {}
	void destroy() override { stopAudio(); }
	bool hasWidescreen() const override { return false; }
	void setScreenSize(int, int) override {}
	void setPalette(const uint8_t *p, int n) override {
		for (int i = 0; i < n; ++i) {
			palette[i] = {p[0], p[1], p[2]};
			p += 3;
		}
	}
	void getPalette(uint8_t *p, int n) override {
		for (int i = 0; i < n; ++i) {
			*p++ = palette[i].r;
			*p++ = palette[i].g;
			*p++ = palette[i].b;
		}
	}
	void setPaletteEntry(int i, const Color *c) override { palette[i] = *c; }
	void getPaletteEntry(int i, Color *c) override { *c = palette[i]; }
	void setOverscanColor(int) override {}
	void copyRect(int, int, int, int, const uint8_t *, int) override {}
	void copyRectRgb24(int, int, int, int, const uint8_t *) override {}
	void zoomRect(int, int, int, int) override {}
	void copyWidescreenLeft(int, int, const uint8_t *, bool = true) override {}
	void copyWidescreenRight(int, int, const uint8_t *, bool = true) override {}
	void copyWidescreenMirror(int, int, const uint8_t *) override {}
	void copyWidescreenBlur(int, int, const uint8_t *) override {}
	void copyWidescreenCDi(int, int, const uint8_t *, const uint8_t *) override {}
	void clearWidescreen() override {}
	void enableWidescreen(bool) override {}
	void fadeScreen() override {}
	void updateScreen(int) override {}
	void processEvents() override {}
	void sleep(int ms) override { SDL_Delay(ms); }
	uint32_t getTimeStamp() override { return SDL_GetTicks(); }
	uint32_t getOutputSampleRate() override { return 22050; }
	static void fillAudio(void *opaque, Uint8 *stream, int bytes) {
		auto &s = *static_cast<PreviewSystem *>(opaque);
		// The engine callback counts stereo frames, not individual int16 samples.
		s.callback(s.user, reinterpret_cast<int16_t *>(stream), bytes / (sizeof(int16_t) * 2));
	}
	void startAudio(AudioCallback cb, void *param) override {
		stopAudio();
		if (!audible)
			return;
		callback = cb;
		user = param;
		SDL_AudioSpec spec{};
		spec.freq = 22050;
		spec.format = AUDIO_S16SYS;
		spec.channels = 2;
		spec.samples = 1024;
		spec.userdata = this;
		spec.callback = fillAudio;
		audio = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
		if (audio)
			SDL_PauseAudioDevice(audio, 0);
	}
	void stopAudio() override {
		if (audio) {
			SDL_CloseAudioDevice(audio);
			audio = 0;
		}
		callback = nullptr;
		user = nullptr;
	}
	void lockAudio() override {
		if (audio)
			SDL_LockAudioDevice(audio);
	}
	void unlockAudio() override {
		if (audio)
			SDL_UnlockAudioDevice(audio);
	}
};
} // namespace editor
