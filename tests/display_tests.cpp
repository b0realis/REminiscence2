// SPDX-License-Identifier: GPL-3.0-or-later
#include "display_modes.h"
#include "systemstub.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

Options g_options;
const char *g_caption = "Display regression tests";
static void check(bool ok, const char *message) {
	if (!ok)
		throw std::runtime_error(message);
}
static SDL_Window *gameWindow() {
	for (Uint32 id = 1; id < 1000; ++id)
		if (auto *window = SDL_GetWindowFromID(id))
			return window;
	return nullptr;
}
int main() {
	try {
		for (int nativeWidth : {256, 512}) {
			const int nativeHeight = nativeWidth * 224 / 256;
			auto platform = std::unique_ptr<SystemStub>(SystemStub_SDL_create());
			settings::Preferences p;
			p.fullscreen = false;
			p.filter = settings::Filter::Point;
			p.scale = 3;
			p.integerScale = true;
			p.windowWidth = 1280;
			p.windowHeight = 720;
			platform->configurePreferences(p);
			auto scaler = ScalerParameters::defaults();
			strcpy(scaler.name, "point");
			scaler.factor = p.scale;
			platform->init(g_caption, nativeWidth, nativeHeight, false, kWidescreenNone, &scaler);
			auto *window = gameWindow();
			check(window, "Game window was not created");
			auto *renderer = SDL_GetRenderer(window);
			std::string reason;
			auto apply = [&] {
				if (!platform->applyPreferences(p, reason))
					throw std::runtime_error(reason);
				platform->updateScreen(0);
			};
			auto windowSize = [&](int width, int height) {
				int actualWidth = 0, actualHeight = 0;
				SDL_GetWindowSize(window, &actualWidth, &actualHeight);
				check(actualWidth == width && actualHeight == height,
				      "Window resolution changed unexpectedly");
			};
			auto integerViewport = [&] {
				int width = 0, height = 0;
				SDL_GetRendererOutputSize(renderer, &width, &height);
				float x = 0, y = 0;
				SDL_RenderGetScale(renderer, &x, &y);
				const int expected = std::min(width / nativeWidth, height / nativeHeight);
				check(SDL_RenderGetIntegerScale(renderer) && x == expected && y == expected,
				      "Integer viewport is not the largest whole native-pixel multiple");
			};
			platform->updateScreen(0);
			windowSize(1280, 720);
			integerViewport();
			p.scale = 4;
			apply();
			windowSize(1280, 720);
			integerViewport(); // The selected CPU/window factor does not restrict fitting.
			p.integerScale = false;
			apply();
			check(!SDL_RenderGetIntegerScale(renderer), "Fractional fit stayed integer-only");
			p.integerScale = true;
			p.windowWidth = 160;
			p.windowHeight = 120;
			apply();
			check(!SDL_RenderGetIntegerScale(renderer),
			      "Tiny window crops instead of fitting down");
			p.windowWidth = 1280;
			p.windowHeight = 720;
			apply();
			integerViewport();

			settings::Session choices;
			choices.values = p;
			display::choices(window, nativeWidth, nativeHeight, choices);
			check(!choices.windowResolutions.empty() && !choices.fullscreenResolutions.empty(),
			      "Display resolution choices are missing");
			p.fullscreen = true;
			p.exclusiveFullscreen = false;
			apply();
			check((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ==
			          SDL_WINDOW_FULLSCREEN_DESKTOP,
			      "Borderless mode did not fill the desktop");
			p.exclusiveFullscreen = true;
			p.fullscreenWidth = choices.fullscreenResolutions.front().width;
			p.fullscreenHeight = choices.fullscreenResolutions.front().height;
			const bool exclusiveSupported = platform->applyPreferences(p, reason);
			if (exclusiveSupported) {
				check((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ==
				          SDL_WINDOW_FULLSCREEN,
				      "Exclusive fullscreen silently uses desktop/borderless flags");
				windowSize(p.fullscreenWidth, p.fullscreenHeight);
			} else {
				check(!reason.empty() &&
				          (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ==
				              SDL_WINDOW_FULLSCREEN_DESKTOP,
				      "Unsupported exclusive backend did not preserve borderless mode");
				platform->readPreferences(p);
				check(!p.exclusiveFullscreen, "Rejected exclusive mode was stored");
			}
			settings::Preferences before;
			platform->readPreferences(before);
			const auto oldWindow = display::capture(window);
			p.exclusiveFullscreen = true;
			p.fullscreenWidth = 1234;
			p.fullscreenHeight = 777;
			p.filter = settings::Filter::Xbrz; // Staged textures must also be discarded.
			check(!platform->applyPreferences(p, reason) && !reason.empty(),
			      "Unsupported fullscreen resolution was silently substituted");
			settings::Preferences after;
			platform->readPreferences(after);
			check(before == after && SDL_GetWindowFlags(window) == oldWindow.flags,
			      "Rejected display change changed applied preferences or window mode");
			p = before;
			p.fullscreen = false;
			apply();
			windowSize(1280, 720);
			check(!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN),
			      "Windowed mode stayed fullscreen");
			auto shortcut = [&](SDL_Scancode code, SDL_Keycode key, SDL_Keymod mod) {
				SDL_Event event{};
				event.type = SDL_KEYDOWN;
				event.key.keysym.scancode = code;
				event.key.keysym.sym = key;
				event.key.keysym.mod = mod;
				SDL_PushEvent(&event);
				event.type = SDL_KEYUP;
				SDL_PushEvent(&event);
				platform->processEvents();
			};
			shortcut(SDL_SCANCODE_RETURN, SDLK_RETURN, KMOD_ALT);
			check((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ==
			          (exclusiveSupported ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP),
			      "Alt+Enter forgot the last fullscreen kind");
			shortcut(SDL_SCANCODE_RETURN, SDLK_RETURN, KMOD_ALT);
			check(gameWindow() == window && !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN),
			      "Alt+Enter failed to return to the same window");
			windowSize(1280, 720);
			// Fixed window size survives filter changes, renderer resets, and native
			// title/game canvas changes (the Amiga title is wider than gameplay).
			for (auto filter :
			     {settings::Filter::Xbrz, settings::Filter::Linear, settings::Filter::Point}) {
				p.filter = filter;
				apply();
				windowSize(1280, 720);
			}
			platform->setScreenSize(320, 224);
			window = gameWindow();
			renderer = SDL_GetRenderer(window);
			windowSize(1280, 720);
			platform->setScreenSize(nativeWidth, nativeHeight);
			window = gameWindow();
			renderer = SDL_GetRenderer(window);
			windowSize(1280, 720);
			platform->updateScreen(0);
			integerViewport();
			// Point sampling must remain nearest after a renderer reset, even when
			// the preceding filter used linear sampling. Fractional fitting exposes it.
			p.integerScale = false;
			apply();
			const uint8_t colors[] = {0, 0, 0, 255, 255, 255};
			platform->setPalette(colors, 2);
			std::vector<uint8_t> pattern(nativeWidth * nativeHeight);
			for (int y = 0; y < nativeHeight; ++y)
				for (int x = 0; x < nativeWidth; ++x)
					pattern[y * nativeWidth + x] = (x + y) % 2;
			platform->copyRect(0, 0, nativeWidth, nativeHeight, pattern.data(), nativeWidth);
			SDL_Event reset{};
			reset.type = SDL_RENDER_DEVICE_RESET;
			SDL_PushEvent(&reset);
			platform->processEvents();
			platform->updateScreen(0);
			int width = 0, height = 0;
			SDL_GetRendererOutputSize(renderer, &width, &height);
			std::vector<uint32_t> pixels(width * height);
			check(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(),
			                           width * 4) == 0,
			      "Could not read point-sampled frame");
			int white = 0;
			for (auto pixel : pixels) {
				check((pixel & 0xFFFFFF) == 0 || (pixel & 0xFFFFFF) == 0xFFFFFF,
				      "Point sampling blended pixels after reset");
				white += (pixel & 0xFFFFFF) == 0xFFFFFF;
			}
			check(white > 10000, "Rendered game frame is missing");
			platform->destroy();
		}
		// A saved exclusive size can become unavailable after changing monitors.
		auto platform = std::unique_ptr<SystemStub>(SystemStub_SDL_create());
		settings::Preferences obsolete;
		obsolete.fullscreen = obsolete.exclusiveFullscreen = true;
		obsolete.fullscreenWidth = 1234;
		obsolete.fullscreenHeight = 777;
		platform->configurePreferences(obsolete);
		const auto scaler = ScalerParameters::defaults();
		platform->init(g_caption, 256, 224, true, kWidescreenNone, &scaler);
		settings::Preferences actual;
		platform->readPreferences(actual);
		check(actual.fullscreen && !actual.exclusiveFullscreen && !actual.fullscreenWidth &&
		          !actual.fullscreenHeight,
		      "Obsolete startup resolution did not fall back to borderless desktop");
		platform->updateScreen(0);
		platform->destroy();
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	std::cout << "Display modes, resolution rollback, integer fit and pixel sampling passed\n";
}
