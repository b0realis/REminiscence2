// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/runtime.h"
#include "config.h"
#include "game.h"
#include "systemstub.h"
#include <SDL.h>
#include <iostream>
#include <thread>

Options g_options;
const char *g_caption = "Authored scene input regression";
static Uint32 windowId;
static void key(SDL_Scancode scan, bool pressed) {
	SDL_Event e{};
	e.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
	e.key.keysym.scancode = scan;
	e.key.keysym.sym = scan == SDL_SCANCODE_F10 ? SDLK_F10 : SDLK_ESCAPE;
	e.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
	e.key.windowID = windowId;
	SDL_PushEvent(&e);
}
int main(int argc, char **argv) {
	if (argc != 3)
		return 2;
	try {
		setDefaultOptions(g_options);
		const auto assets = assets::discover(argv[2]);
		if (assets.games.empty())
			throw std::runtime_error("Missing test assets");
		std::unique_ptr<SystemStub> stub(SystemStub_SDL_create());
		settings::Preferences prefs;
		prefs.pauseOnFocusLoss = false;
		prefs.fullscreen = false;
		prefs.scale = 3;
		prefs.widescreen = kWidescreenNone;
		stub->configurePreferences(prefs);
		auto scaler = ScalerParameters::defaults();
		stub->init(g_caption, 256, 224, false, kWidescreenNone, &scaler);
		// This test owns the only SDL window. ImGui rejects events for a missing
		// window ID, unlike the engine's global F10 shortcut.
		windowId = SDL_GetWindowID(SDL_GetWindowFromID(1));
		if (!windowId)
			throw std::runtime_error("Missing test window");
		{
			editor::Scene scene(editor::makeProject(), argv[1], assets.games.front(), stub.get());
			for (const auto scan : {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F10}) {
				scene.present();
				stub->updateScreen(0);
				const auto before = scene.saveSession();
				stub->setGameplayActive(true);
				key(scan, true);
				stub->processEvents();
				if (!stub->_pi.escape)
					throw std::runtime_error("Esc/F10 did not request QoL");
				std::thread close([&] {
					SDL_Delay(150);
					key(scan, false);
					SDL_Delay(100);
					key(scan, true);
				});
				const bool opened = scene.handleMenuInput(nullptr, argv[1]);
				close.join();
				if (!opened || stub->_pi.quit || stub->_pi.escape || scene.saveSession() != before)
					throw std::runtime_error(
					    "QoL exited, leaked keys or advanced authored gameplay");
				key(scan, false);
				stub->processEvents();
				stub->_pi = {};
				scene.tick();
				scene.present();
				stub->updateScreen(0);
			}
		}
		stub->destroy();
		std::cout << "SDL Esc and F10 open/resume authored QoL; frozen state and teardown passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
