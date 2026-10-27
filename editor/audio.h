// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.h"
#include <map>
#include <memory>
struct Mixer;
namespace editor {
struct AudioClip {
	int rate = 0;
	std::vector<int16_t> samples; // interleaved stereo, decoded away from audio callback
	uint64_t frames() const { return samples.size() / 2; }
	int ticks() const { return int((frames() * 30 + rate - 1) / rate); }
};
AudioClip readAudio(const std::filesystem::path &);
std::string importAudio(const std::filesystem::path &, const std::filesystem::path &);
void copyModuleAudio(Module &, const std::filesystem::path &, const std::filesystem::path &);
std::vector<Issue> audioIssues(const Project &, const std::filesystem::path &);
struct AudioVoice {
	const AudioClip *clip = nullptr;
	uint64_t cursor = 0, generation = 0;
	int gain = 70;
	bool loop = false, music = false, running = false;
};
// Fixed project voice list: no allocation, file I/O or graph mutation in callback.
// All main-thread voice updates hold the mixer's audio lock.
class SceneAudio {
public:
	SceneAudio(const Project &, const std::filesystem::path &, Mixer &);
	~SceneAudio();
	void sync(size_t module, size_t node, bool active, uint64_t generation, uint64_t age);
	void reset();

private:
	Mixer &_mixer;
	std::map<std::string, AudioClip> _clips;
	std::map<std::pair<size_t, size_t>, AudioVoice> _voices;
	static void mix(void *, int16_t *, int, int, int);
};
} // namespace editor
