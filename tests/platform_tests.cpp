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
static int checkRate(int rate) {
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
	// Background and focus are independent reasons to pause. A focus gain
	// must not resume a backgrounded app before the foreground event.
	event.type = SDL_APP_WILLENTERBACKGROUND;
	SDL_PushEvent(&event);
	event.type = SDL_WINDOWEVENT;
	event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
	SDL_PushEvent(&event);
	const auto pauseStart = SDL_GetTicks();
	std::thread foreground([] {
		SDL_Delay(80);
		SDL_Event e{};
		e.type = SDL_APP_DIDENTERFOREGROUND;
		SDL_PushEvent(&e);
	});
	platform->processEvents();
	foreground.join();
	if (SDL_GetTicks() - pauseStart < 60)
		return 38;
	// Virtual devices exercise hotplug/instance filtering without hardware.
	const int virtualIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
	if (virtualIndex >= 0) {
		platform->processEvents();
		auto *device = SDL_JoystickOpen(virtualIndex);
		SDL_Event button = {};
		button.type = SDL_JOYBUTTONDOWN;
		button.jbutton.button = 1;
		button.jbutton.state = SDL_PRESSED;
		button.jbutton.which = SDL_JoystickInstanceID(device);
		SDL_PushEvent(&button);
		platform->processEvents();
		if (!platform->_pi.shift)
			return 20;
		button.type = SDL_JOYBUTTONUP;
		button.jbutton.state = SDL_RELEASED;
		button.jbutton.which += 10000;
		SDL_PushEvent(&button);
		platform->processEvents();
		if (!platform->_pi.shift)
			return 21;
		SDL_JoystickClose(device);
		SDL_JoystickDetachVirtual(virtualIndex);
		platform->processEvents();
		if (platform->_pi.shift || platform->_pi.dirMask)
			return 22;
		const int reconnect = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
		if (reconnect < 0)
			return 23;
		platform->processEvents();
		SDL_JoystickDetachVirtual(reconnect);
		platform->processEvents();
	}
	// Mapped pads take a different SDL path from the fallback raw joystick.
	const int mappedIndex = SDL_JoystickAttachVirtual(
	    SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 1);
	if (mappedIndex >= 0) {
		platform->processEvents();
		auto *pad = SDL_JoystickOpen(mappedIndex);
		if (!SDL_IsGameController(mappedIndex))
			return 24;
		settings::Preferences mapping;
		platform->readPreferences(mapping);
		mapping.padButtons[0] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
		std::string reason;
		if (!platform->applyPreferences(mapping, reason))
			return 25;
		SDL_JoystickSetVirtualButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_PRESSED);
		platform->processEvents();
		if (!platform->_pi.shift)
			return 26;
		SDL_Event held = {};
		held.type = SDL_KEYDOWN;
		held.key.keysym.scancode = SDL_SCANCODE_RIGHT;
		held.key.keysym.sym = SDLK_RIGHT;
		SDL_PushEvent(&held);
		platform->processEvents();
		SDL_Event neutral = {};
		neutral.type = SDL_CONTROLLERAXISMOTION;
		neutral.caxis.which = SDL_JoystickInstanceID(pad);
		neutral.caxis.axis = SDL_CONTROLLER_AXIS_LEFTX;
		SDL_PushEvent(&neutral);
		platform->processEvents();
		if (!(platform->_pi.dirMask & PlayerInput::DIR_RIGHT))
			return 27;
		SDL_JoystickClose(pad);
		SDL_JoystickDetachVirtual(mappedIndex);
		platform->processEvents();
		if (platform->_pi.shift || !(platform->_pi.dirMask & PlayerInput::DIR_RIGHT))
			return 28;
		held.type = SDL_KEYUP;
		SDL_PushEvent(&held);
		platform->processEvents();
	}
	SDL_Event reset = {};
	reset.type = SDL_RENDER_DEVICE_RESET;
	SDL_PushEvent(&reset);
	platform->processEvents();
	platform->updateScreen(0);
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
	SDL_PushEvent(&reset);
	platform->processEvents();
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
	for (auto action : {settings::Action::ContinueLatest, settings::Action::LoadAutosave,
	                    settings::Action::Rewind}) {
		session.action = action;
		bool called = false;
		session.command = [&](settings::Action received, int, std::string &) {
			called = received == action;
			return true;
		};
		platform->showSettings(session);
		if (!called || session.action != settings::Action::Resume)
			return 37;
	}
	// A newly seen pad inherits the last globally saved controls, including
	// settings saved after SDL was initialized.
	session.values.padButtons[0] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
	session.saveRequested = true;
	session.persist = [](const settings::Preferences &, std::string &) { return true; };
	session.action = settings::Action::LoadGame;
	session.command = [](settings::Action, int, std::string &) { return true; };
	platform->showSettings(session);
	const int unseen = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
	if (unseen >= 0) {
		platform->processEvents();
		settings::Preferences inherited;
		platform->readPreferences(inherited);
		if (inherited.padButtons[0] != SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
			std::cerr << "Saved global controller defaults were lost on hotplug\n";
			return 45;
		}
		SDL_JoystickDetachVirtual(unseen);
		platform->processEvents();
	}
	// Disconnect during gameplay must request a pause without another player
	// event, and reconnect must not resume the simulation implicitly.
	platform->setGameplayActive(true);
	const int unplug = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
	if (unplug >= 0) {
		platform->processEvents();
		SDL_JoystickDetachVirtual(unplug);
		platform->processEvents();
		if (!platform->consumePauseRequest() || platform->consumePauseRequest() ||
		    platform->_pi.dirMask)
			return 40;
	}
	platform->setGameplayActive(false);
	// Device removal keeps the voice clock alive on the main thread. Reopen
	// at the existing mixer rate, rather than changing pitch mid-track.
	SDL_AudioDeviceID playing = 0;
	for (SDL_AudioDeviceID id = 1; id < 64; ++id)
		if (SDL_GetAudioDeviceStatus(id) == SDL_AUDIO_PLAYING) {
			playing = id;
			break;
		}
	if (!playing)
		return 41;
	SDL_Event lost{};
	lost.type = SDL_AUDIODEVICEREMOVED;
	lost.adevice.which = playing;
	SDL_PushEvent(&lost);
	platform->processEvents();
	const auto beforeSilent = frames.load();
	platform->sleep(80);
	if (platform->audioOutputAvailable() || frames <= beforeSilent)
		return 42;
	int pausedFrames = 0;
	uint32_t resumeStamp = 0;
	SDL_Event background{};
	background.type = SDL_APP_WILLENTERBACKGROUND;
	SDL_PushEvent(&background);
	std::thread silentResume([&] {
		SDL_Delay(90);
		pausedFrames = frames.load();
		resumeStamp = SDL_GetTicks();
		SDL_Event e{};
		e.type = SDL_APP_DIDENTERFOREGROUND;
		SDL_PushEvent(&e);
	});
	platform->processEvents();
	silentResume.join();
	platform->sleep(20);
	// SDL_Delay is a minimum wait. A busy runner can oversleep arbitrarily;
	// compare with actual foreground time, while still excluding the pause.
	const uint64_t foregroundMs = SDL_GetTicks() - resumeStamp;
	if (uint64_t(frames - pausedFrames) > (foregroundMs + 5) * rate / 1000)
		return 44;
	lost.type = SDL_AUDIODEVICEADDED;
	lost.adevice.which = 0;
	SDL_PushEvent(&lost);
	platform->processEvents();
	if (!platform->audioOutputAvailable() || platform->getOutputSampleRate() != unsigned(rate))
		return 43;
	platform->stopAudio();
	platform->stopAudio();
	platform->destroy();
	return 0;
}
int main() {
	for (int rate : {8000, 44100, 48000}) {
		const int result = checkRate(rate);
		if (result) {
			std::cerr << "Platform check " << result << " failed at " << rate
			          << " Hz: " << SDL_GetError() << '\n';
			return result;
		}
	}
	std::cout << "Platform checks passed: audio, focus reset, live display, rebinding, paused "
	             "overlay and shutdown.\n";
}
