// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_MUSIC_OPTIONS_H
#define REMINISCENCE2_MUSIC_OPTIONS_H
#include <string>
namespace music {
enum class Backend { None, Adlib, FluidSynth, Mt32 };
struct Options {
	Backend backend = Backend::Adlib;
	std::string soundFont;
	std::string romDirectory = ".";
	bool correctWaveformRegister = false;
};
bool backendAvailable(Backend backend);
} // namespace music
#endif
