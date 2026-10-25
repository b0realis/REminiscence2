// SPDX-License-Identifier: GPL-3.0-or-later
#include "display_modes.h"
#include <algorithm>
#include <cstdlib>
#include <tuple>

namespace display {
int canvasWidth(int width, int height, int widescreen) {
	return widescreen == kWidescreenCDi    ? width + 2 * kWidescreenBorderCDiW
	       : widescreen != kWidescreenNone ? height * 16 / 9
	                                       : width;
}
WindowState capture(SDL_Window *window) {
	WindowState state;
	state.flags = SDL_GetWindowFlags(window);
	SDL_GetWindowDisplayMode(window, &state.mode);
	SDL_GetWindowPosition(window, &state.x, &state.y);
	SDL_GetWindowSize(window, &state.width, &state.height);
	return state;
}
bool restore(SDL_Window *window, const WindowState &state) {
	bool ok = SDL_SetWindowFullscreen(window, 0) == 0;
	ok = SDL_SetWindowDisplayMode(window, &state.mode) == 0 && ok;
	SDL_RestoreWindow(window);
	SDL_SetWindowPosition(window, state.x, state.y);
	SDL_SetWindowSize(window, state.width, state.height);
	ok = SDL_SetWindowFullscreen(window, state.flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0 && ok;
	if (state.flags & SDL_WINDOW_MAXIMIZED)
		SDL_MaximizeWindow(window);
	return ok;
}
bool apply(SDL_Window *window, const settings::Preferences &p, int automaticWidth,
           int automaticHeight, std::string &reason) {
#ifdef __ANDROID__
	// The Android/Quest host owns the application surface and panel size.
	if (!p.fullscreen || p.exclusiveFullscreen || p.windowWidth || p.fullscreenWidth) {
		reason = "Android/Quest manages the app panel's window mode and resolution.";
		return false;
	}
	if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
		reason = std::string("Could not enter the app's fullscreen surface: ") + SDL_GetError();
		return false;
	}
	return true;
#else
	const int monitor = SDL_GetWindowDisplayIndex(window);
	SDL_DisplayMode selected{};
	const bool exclusive = p.fullscreen && p.exclusiveFullscreen;
	if (exclusive) {
		SDL_DisplayMode desktop{};
		if (SDL_GetDesktopDisplayMode(monitor, &desktop) != 0) {
			reason = "This display does not report an exclusive fullscreen mode.";
			return false;
		}
		const int width = p.fullscreenWidth ? p.fullscreenWidth : desktop.w;
		const int height = p.fullscreenHeight ? p.fullscreenHeight : desktop.h;
		for (int i = 0; i < SDL_GetNumDisplayModes(monitor); ++i) {
			SDL_DisplayMode mode{};
			if (SDL_GetDisplayMode(monitor, i, &mode) == 0 && mode.w == width && mode.h == height &&
			    (!selected.w || std::abs(mode.refresh_rate - desktop.refresh_rate) <
			                        std::abs(selected.refresh_rate - desktop.refresh_rate)))
				selected = mode;
		}
		if (!selected.w) {
			reason = "That fullscreen resolution is not reported by the current display.";
			return false;
		}
	}
	const auto previous = capture(window);
	const Uint32 flags = !p.fullscreen ? 0
	                     : exclusive   ? SDL_WINDOW_FULLSCREEN
	                                   : SDL_WINDOW_FULLSCREEN_DESKTOP;
	// Leave the old fullscreen mode before changing an exclusive resolution.
	const bool leave =
	    (previous.flags & SDL_WINDOW_FULLSCREEN) &&
	    ((previous.flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != flags ||
		 (exclusive && (previous.mode.w != selected.w || previous.mode.h != selected.h)));
	if ((leave && SDL_SetWindowFullscreen(window, 0) != 0) ||
	    SDL_SetWindowDisplayMode(window, exclusive ? &selected : nullptr) != 0 ||
	    SDL_SetWindowFullscreen(window, flags) != 0) {
		reason = std::string("Display change was rejected: ") + SDL_GetError();
		if (!restore(window, previous))
			reason += " The previous mode could not be fully restored.";
		return false;
	}
	// Some compatibility backends return success but silently use borderless
	// for an exclusive request. Report that limit instead of saving a false mode.
	if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != flags) {
		reason = "This window backend did not accept the requested fullscreen mode.";
		if (!restore(window, previous))
			reason += " The previous mode could not be fully restored.";
		return false;
	}
	if (!p.fullscreen) {
		SDL_RestoreWindow(window);
		SDL_SetWindowSize(window, p.windowWidth ? p.windowWidth : automaticWidth,
		                  p.windowHeight ? p.windowHeight : automaticHeight);
	}
	return true;
#endif
}
void choices(SDL_Window *window, int width, int height, settings::Session &s) {
	s.windowResolutions.clear();
	s.fullscreenResolutions.clear();
	const int monitor = SDL_GetWindowDisplayIndex(window);
	SDL_DisplayMode desktop{};
	SDL_GetDesktopDisplayMode(monitor, &desktop);
	s.displayWidth = desktop.w;
	s.displayHeight = desktop.h;
#ifdef __ANDROID__
	s.windowModesAvailable = false;
#else
	s.windowModesAvailable = true;
#endif
	auto add = [](std::vector<settings::Resolution> &list, int w, int h) {
		if (w < 160 || h < 120 || w > 16384 || h > 16384)
			return;
		if (std::none_of(list.begin(), list.end(),
		                 [&](const auto &r) { return r.width == w && r.height == h; }))
			list.push_back({w, h});
	};
	for (int i = 0; i < SDL_GetNumDisplayModes(monitor); ++i) {
		SDL_DisplayMode mode{};
		if (SDL_GetDisplayMode(monitor, i, &mode) == 0) {
			add(s.fullscreenResolutions, mode.w, mode.h);
			add(s.windowResolutions, mode.w, mode.h);
		}
	}
	for (int factor = 1; factor <= 6; ++factor)
		add(s.windowResolutions, width * factor, height * factor);
	for (const auto r : {settings::Resolution{640, 480},
	                     {800, 600},
	                     {1024, 768},
	                     {1280, 720},
	                     {1280, 800},
	                     {1600, 900},
	                     {1920, 1080},
	                     {2560, 1440},
	                     {3840, 2160}})
		add(s.windowResolutions, r.width, r.height);
	add(s.windowResolutions, s.values.windowWidth, s.values.windowHeight);
	for (auto *list : {&s.windowResolutions, &s.fullscreenResolutions})
		std::sort(list->begin(), list->end(), [](const auto &a, const auto &b) {
			return std::tie(a.width, a.height) < std::tie(b.width, b.height);
		});
}
} // namespace display
