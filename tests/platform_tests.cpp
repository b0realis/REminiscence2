// SPDX-License-Identifier: GPL-3.0-or-later
#include "systemstub.h"
#include <SDL.h>
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
Options g_options;
const char *g_caption = "platform tests";
static void audio(void *context, int16_t *samples, int frames) {
	static_cast<std::atomic<int> *>(context)->fetch_add(frames);
	for (int i = 0; i < frames * 2; ++i) {
		samples[i] = 0;
	}
}
int main() {
	for (int rate : {8000, 44100, 48000}) {
		auto platform = std::unique_ptr<SystemStub>(SystemStub_SDL_create());
		RuntimeOptions options;
		options.outputSampleRate = rate;
		options.maximized = true;
		platform->configure(options);
		const auto scaler = ScalerParameters::defaults();
		platform->init(g_caption, 256, 224, false, kWidescreenNone, &scaler);
		std::atomic<int> frames{0};
		platform->startAudio(audio, &frames);
		for (int i = 0; i < 50 && frames == 0; ++i) {
			SDL_Delay(5);
		}
		if (platform->getOutputSampleRate() != unsigned(rate) || frames == 0) {
			return 1;
		}
		platform->_pi.dirMask = PlayerInput::DIR_RIGHT;
		platform->_pi.shift = true;
		SDL_Event event = {};
		event.type = SDL_WINDOWEVENT;
		event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
		SDL_PushEvent(&event);
		event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
		SDL_PushEvent(&event);
		platform->processEvents();
		if (platform->_pi.dirMask || platform->_pi.shift) {
			return 2;
		}
		// Virtual devices exercise hotplug/instance filtering without hardware.
		const int virtualIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
		if (virtualIndex >= 0) {
			platform->processEvents();
			auto *device = SDL_JoystickOpen(virtualIndex);
			SDL_Event button = {};
			button.type = SDL_JOYBUTTONDOWN; button.jbutton.button = 1; button.jbutton.state = SDL_PRESSED;
			button.jbutton.which = SDL_JoystickInstanceID(device);
			SDL_PushEvent(&button); platform->processEvents();
			if (!platform->_pi.shift) return 20;
			button.type = SDL_JOYBUTTONUP; button.jbutton.state = SDL_RELEASED;
			button.jbutton.which += 10000;
			SDL_PushEvent(&button); platform->processEvents();
			if (!platform->_pi.shift) return 21;
			SDL_JoystickClose(device);
			SDL_JoystickDetachVirtual(virtualIndex); platform->processEvents();
			if (platform->_pi.shift || platform->_pi.dirMask) return 22;
			const int reconnect = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
			if (reconnect < 0) return 23;
			platform->processEvents(); SDL_JoystickDetachVirtual(reconnect); platform->processEvents();
		}
		SDL_Event reset = {}; reset.type = SDL_RENDER_DEVICE_RESET;
		SDL_PushEvent(&reset); platform->processEvents(); platform->updateScreen(0);
		settings::Preferences preferences;
		platform->readPreferences(preferences);
		preferences.fullscreen = false;
		preferences.filter = settings::Filter::Point;
		preferences.scale = 2;
		preferences.widescreen = kWidescreenMirrorRoom;
		preferences.keys[0] = SDL_SCANCODE_A;
		std::string reason;
		if (!platform->applyPreferences(preferences, reason)) {
			std::cerr << reason << '\n';
			return 3;
		}
		settings::Preferences current;
		platform->readPreferences(current);
		if (current.fullscreen || current.scale != 2 || current.filter != settings::Filter::Point ||
		    !platform->hasWidescreen() || current.keys[0] != SDL_SCANCODE_A)
			return 4;
		SDL_Event key = {};
		key.type = SDL_KEYDOWN;
		key.key.keysym.scancode = SDL_SCANCODE_A;
		key.key.keysym.sym = SDLK_a;
		SDL_PushEvent(&key);
		platform->processEvents();
		if (!(platform->_pi.dirMask & PlayerInput::DIR_LEFT))
			return 5;
		key.type = SDL_KEYUP;
		SDL_PushEvent(&key);
		platform->processEvents();
		if (platform->_pi.dirMask)
			return 6;
		key.type = SDL_KEYDOWN;
		key.key.keysym.scancode = SDL_SCANCODE_LEFT;
		key.key.keysym.sym = SDLK_LEFT;
		SDL_PushEvent(&key);
		platform->processEvents();
		if (platform->_pi.dirMask)
			return 7;
		preferences.scale = 1;
		preferences.widescreen = kWidescreenNone;
		preferences.menuLayout = 1;
		preferences.showMenuButton = true;
		if (!platform->applyPreferences(preferences, reason))
			return 9;
		platform->setGameplayActive(true);
		platform->updateScreen(0);
		reset.type = SDL_RENDER_DEVICE_RESET;
		SDL_PushEvent(&reset); platform->processEvents();
		platform->updateScreen(0);
		SDL_Event click = {};
		click.type = SDL_MOUSEBUTTONUP;
		click.button.button = SDL_BUTTON_LEFT;
		click.button.x = 215;
		click.button.y = 208;
		SDL_PushEvent(&click);
		platform->processEvents();
		if (!platform->_pi.escape)
			return 10;
		platform->_pi.escape = false;
		platform->setGameplayActive(false);
		settings::Session session;
		session.values = preferences;
		session.edition = "Synthetic";
		session.language = "English";
		bool cheatApplied = false, healed = false;
		session.cheatsRequested = true;
		session.applyCheats = [&](uint32_t, uint8_t) { cheatApplied = true; };
		session.action = settings::Action::Heal;
		session.command = [&](settings::Action action, int, std::string &) {
			healed = action == settings::Action::Heal;
			return true;
		};
		std::thread closer([] {
			SDL_Delay(120);
			SDL_Event close = {};
			close.type = SDL_KEYDOWN;
			close.key.keysym.scancode = SDL_SCANCODE_F10;
			close.key.keysym.sym = SDLK_F10;
			SDL_PushEvent(&close);
		});
		platform->showSettings(session);
		closer.join();
		if (session.action != settings::Action::Resume || platform->_pi.save || platform->_pi.load)
			return 8;
		if (!cheatApplied || !healed)
			return 11;
		SDL_Window *window = nullptr;
		for (uint32_t id = 1; id < 100 && !window; ++id)
			window = SDL_GetWindowFromID(id);
		int width = 0, height = 0;
		if (window)
			SDL_GetWindowSize(window, &width, &height);
		if (width != 256 || height != 224)
			return 12;
		session.saveSlot = 7;
		session.commandSlot = 0;
		session.action = settings::Action::LoadGame;
		bool checkpointLoaded = false;
		session.command = [&](settings::Action action, int slot, std::string &) {
			checkpointLoaded = action == settings::Action::LoadGame && slot == 0;
			return true;
		};
		platform->showSettings(session);
		if (!checkpointLoaded || session.saveSlot != 7 || session.commandSlot != -1 ||
		    session.action != settings::Action::Resume)
			return 13;
		platform->stopAudio();
		platform->stopAudio();
		platform->destroy();
	}
	std::cout << "Platform checks passed: audio, focus reset, live display, rebinding, paused "
	             "overlay and shutdown.\n";
}
