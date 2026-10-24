// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime_paths.h"
#include <SDL.h>
#include <filesystem>
#include <cstdlib>

RuntimePaths runtimePaths(bool portable) {
	namespace fs = std::filesystem;
	char *base = SDL_GetBasePath();
	const fs::path executable = base ? fs::u8path(base) : fs::path(".");
	SDL_free(base);
	const char *home = std::getenv("HOME");
	fs::path user;
#ifdef _WIN32
	const char *appdata = std::getenv("APPDATA");
	user = appdata ? fs::u8path(appdata) / "REminiscence2" : executable / "user";
#elif defined(__APPLE__)
	user = home ? fs::u8path(home) / "Library/Application Support/REminiscence2" : executable / "user";
#else
	const char *xdg = std::getenv("XDG_DATA_HOME");
	user = xdg && *xdg ? fs::u8path(xdg) / "REminiscence2" :
	       home ? fs::u8path(home) / ".local/share/REminiscence2" : executable / "user";
#endif
	std::error_code ec;
	fs::path data = user / "assets", tunes = user / "TUNES";
	if (fs::is_directory(executable / "assets", ec)) data = executable / "assets";
	else if (fs::is_directory(executable / "DATA", ec)) data = executable / "DATA";
	else if (fs::is_directory("DATA", ec)) data = "DATA";
	if (fs::is_directory(executable / "TUNES", ec)) tunes = executable / "TUNES";
	else if (fs::is_directory("TUNES", ec)) tunes = "TUNES";
	if (portable) { data = executable / "assets"; tunes = executable / "TUNES"; user = executable; }
	fs::path config = "fb.cfg";
	if (!fs::is_regular_file(config, ec)) config = executable / "fb.cfg";
	return {data.u8string(), tunes.u8string(), (user / "SAVE").u8string(), config.u8string()};
}
