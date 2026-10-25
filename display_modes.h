// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_DISPLAY_MODES_H
#define REMINISCENCE2_DISPLAY_MODES_H
#include "settings.h"
#include <SDL.h>

namespace display {
struct WindowState {
	Uint32 flags = 0;
	SDL_DisplayMode mode{};
	int x = 0, y = 0, width = 0, height = 0;
};
WindowState capture(SDL_Window *window);
bool restore(SDL_Window *window, const WindowState &state);
// Failure restores the actual prior window, including manual sizing/position.
bool apply(SDL_Window *window, const settings::Preferences &preferences, int automaticWidth,
           int automaticHeight, std::string &reason);
void choices(SDL_Window *window, int canvasWidth, int canvasHeight, settings::Session &session);
int canvasWidth(int width, int height, int widescreen);
} // namespace display
#endif
