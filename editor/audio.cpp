// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio.h"
#include "mixer.h"
#include "systemstub.h"
#include "file.h"
#include "atomic_file.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <zlib.h>
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
namespace editor {
namespace {
constexpr size_t MaxFile = 64 * 1024 * 1024, MaxSamples = 32 * 1024 * 1024;
std::vector<uint8_t> bytes(const std::filesystem::path &p) {
	std::error_code ec;
	auto size = std::filesystem::file_size(p, ec);
	if (ec || size < 12 || size > MaxFile)
		throw std::runtime_error("Audio missing or exceeds 64 MiB");
	std::vector<uint8_t> data(size);
	std::ifstream f(p, std::ios::binary);
	if (!f.read(reinterpret_cast<char *>(data.data()), data.size()))
		throw std::runtime_error("Cannot read audio");
	return data;
}
void bounds(int channels, int rate, uint64_t frames) {
	if ((channels != 1 && channels != 2) || rate < 8000 || rate > 192000 || !frames ||
	    frames > uint64_t(rate) * 600 || frames * 2 > MaxSamples)
		throw std::runtime_error(
		    "Audio needs mono/stereo, 8..192 kHz, at most 10 minutes and 64 MiB decoded");
}
bool audio(NodeKind k) {
	return k == NodeKind::Music || k == NodeKind::Sound;
}
} // namespace
AudioClip readAudio(const std::filesystem::path &path) {
	const auto data = bytes(path);
	AudioClip result;
	if (!memcmp(data.data(), "OggS", 4)) {
		int error = 0;
		std::unique_ptr<stb_vorbis, decltype(&stb_vorbis_close)> stream(
		    stb_vorbis_open_memory(data.data(), int(data.size()), &error, nullptr),
		    stb_vorbis_close);
		if (!stream)
			throw std::runtime_error("Invalid Ogg Vorbis audio");
		const auto info = stb_vorbis_get_info(stream.get());
		const uint64_t frames = stb_vorbis_stream_length_in_samples(stream.get());
		bounds(info.channels, info.sample_rate, frames);
		result.rate = info.sample_rate;
		result.samples.resize(frames * 2);
		size_t offset = 0;
		while (offset < frames) {
			const int got = stb_vorbis_get_samples_short_interleaved(
			    stream.get(), 2, result.samples.data() + offset * 2,
			    int(std::min<uint64_t>(4096, (frames - offset) * 2)));
			if (!got)
				throw std::runtime_error("Truncated Ogg audio");
			offset += got;
		}
		if (stb_vorbis_get_error(stream.get()) != VORBIS__no_error)
			throw std::runtime_error("Corrupt Ogg audio");
	} else {
		if (memcmp(data.data(), "RIFF", 4) || memcmp(data.data() + 8, "WAVE", 4))
			throw std::runtime_error("Use 16-bit PCM WAV or Ogg Vorbis audio");
		const uint64_t end = uint64_t(READ_LE_UINT32(data.data() + 4)) + 8;
		if (end > data.size() || end < 12)
			throw std::runtime_error("Truncated WAV");
		const uint8_t *pcm = nullptr;
		uint32_t size = 0;
		int channels = 0;
		for (uint64_t p = 12; p < end;) {
			if (end - p < 8)
				throw std::runtime_error("Truncated WAV chunk");
			const auto *h = data.data() + p;
			const uint32_t n = READ_LE_UINT32(h + 4);
			const uint64_t next = p + 8 + n + (n & 1);
			if (next > end)
				throw std::runtime_error("Invalid WAV chunk size");
			if (!memcmp(h, "fmt ", 4)) {
				if (channels || n < 16 || READ_LE_UINT16(h + 8) != 1 ||
				    READ_LE_UINT16(h + 22) != 16)
					throw std::runtime_error("WAV must be 16-bit PCM");
				channels = READ_LE_UINT16(h + 10);
				result.rate = READ_LE_UINT32(h + 12);
				bounds(channels, result.rate, 1);
				if (READ_LE_UINT16(h + 20) != channels * 2 ||
				    READ_LE_UINT32(h + 16) != unsigned(result.rate * channels * 2))
					throw std::runtime_error("Invalid WAV alignment");
			} else if (!memcmp(h, "data", 4)) {
				if (pcm)
					throw std::runtime_error("Duplicate WAV data");
				pcm = h + 8;
				size = n;
			}
			p = next;
		}
		if (!pcm || !channels || size % (channels * 2))
			throw std::runtime_error("Incomplete WAV");
		const uint64_t frames = size / (channels * 2);
		bounds(channels, result.rate, frames);
		result.samples.resize(frames * 2);
		for (size_t f = 0; f < frames; ++f)
			for (int c = 0; c < 2; ++c)
				result.samples[f * 2 + c] =
				    int16_t(READ_LE_UINT16(pcm + (f * channels + (channels == 1 ? 0 : c)) * 2));
	}
	return result;
}
std::string importAudio(const std::filesystem::path &source, const std::filesystem::path &root) {
	readAudio(source);
	const auto data = bytes(source);
	std::ostringstream name;
	name << "audio/" << std::hex << crc32(0, data.data(), uInt(data.size())) << '-' << data.size()
	     << (!memcmp(data.data(), "OggS", 4) ? ".ogg" : ".wav");
	const auto target = assetPath(root, name.str());
	std::filesystem::create_directories(target.parent_path());
	if (std::filesystem::exists(target)) {
		if (bytes(target) != data)
			throw std::runtime_error("Audio name collision; existing file preserved");
	} else {
		std::string reason;
		if (!writeAtomicFile(
		        target.u8string(), false, [&](File &f) { f.write(data.data(), data.size()); },
		        reason))
			throw std::runtime_error("Cannot import audio: " + reason);
	}
	return name.str();
}
void copyModuleAudio(Module &m, const std::filesystem::path &source,
                     const std::filesystem::path &dest) {
	for (auto &n : m.nodes)
		if (!n.audioFile.empty())
			n.audioFile = importAudio(assetPath(source, n.audioFile), dest);
}
std::vector<Issue> audioIssues(const Project &p, const std::filesystem::path &root) {
	std::vector<Issue> issues;
	std::map<std::string, int> durations;
	size_t total = 0, voices = 0;
	for (const auto &m : p.modules)
		for (const auto &n : m.nodes)
			if (audio(n.kind)) {
				try {
					if (++voices > 32)
						throw std::runtime_error("Maximum 32 audio nodes per level");
					if (!durations.count(n.audioFile)) {
						const auto clip = readAudio(assetPath(root, n.audioFile));
						total += clip.samples.size() * 2;
						if (total > 128 * 1024 * 1024)
							throw std::runtime_error("Level audio exceeds 128 MiB decoded");
						durations[n.audioFile] = clip.ticks();
					}
					if (n.duration != durations.at(n.audioFile))
						throw std::runtime_error("Audio changed duration: re-import the clip");
				} catch (const std::exception &e) {
					issues.push_back({true, e.what(), n.at.room, m.id, n.id});
				}
			}
	return issues;
}
SceneAudio::SceneAudio(const Project &p, const std::filesystem::path &root, Mixer &m) : _mixer(m) {
	const auto issues = audioIssues(p, root);
	if (!issues.empty())
		throw std::runtime_error(issues.front().message);
	for (size_t i = 0; i < p.modules.size(); ++i)
		for (size_t j = 0; j < p.modules[i].nodes.size(); ++j) {
			const auto &n = p.modules[i].nodes[j];
			if (!audio(n.kind))
				continue;
			if (!_clips.count(n.audioFile))
				_clips.emplace(n.audioFile, readAudio(assetPath(root, n.audioFile)));
			AudioVoice v;
			v.clip = &_clips.at(n.audioFile);
			v.gain = n.audioVolume;
			v.loop = n.audioLoop;
			v.music = n.kind == NodeKind::Music;
			_voices.emplace(std::make_pair(i, j), v);
		}
	if (!_voices.empty())
		_mixer.setContentHook(mix, this);
}
SceneAudio::~SceneAudio() {
	_mixer.clearContentHook(this);
}
void SceneAudio::reset() {
	LockAudioStack lock(_mixer._stub);
	for (auto &v : _voices) {
		v.second.running = false;
		v.second.generation = 0;
	}
}
void SceneAudio::sync(size_t m, size_t n, bool active, uint64_t generation, uint64_t age) {
	const auto it = _voices.find({m, n});
	if (it == _voices.end())
		return;
	LockAudioStack lock(_mixer._stub);
	auto &v = it->second;
	if (!active) {
		v.running = false;
		v.generation = 0;
		return;
	}
	if (v.generation != generation || !v.generation) {
		const uint64_t frames = v.clip->frames();
		// Age is graph time (30 Hz), also the deterministic seek point on restore.
		const uint64_t frame = age / 30 * v.clip->rate + age % 30 * v.clip->rate / 30;
		v.cursor = (v.loop ? frame % frames : std::min(frame, frames)) << 32;
		v.generation = generation;
		v.running = true;
	}
}
void SceneAudio::mix(void *owner, int16_t *out, int len, int music, int effects) {
	auto &self = *static_cast<SceneAudio *>(owner);
	const auto rate = self._mixer.getSampleRate();
	if (!rate)
		return;
	for (auto &entry : self._voices) {
		auto &v = entry.second;
		if (!v.running)
			continue;
		const auto &clip = *v.clip;
		const uint64_t end = clip.frames() << 32, step = (uint64_t(clip.rate) << 32) / rate;
		const int gain = v.gain * (v.music ? music : effects);
		for (int f = 0; f < len; ++f) {
			if (v.cursor >= end) {
				if (v.loop)
					v.cursor %= end;
				else {
					v.running = false;
					break;
				}
			}
			const uint64_t a = v.cursor >> 32, b = a + 1 < clip.frames() ? a + 1 : (v.loop ? 0 : a);
			const int64_t fraction = (v.cursor & 0xffffffffu) >> 16;
			for (int c = 0; c < 2; ++c) {
				const int sample = int((clip.samples[a * 2 + c] * (65536 - fraction) +
				                        clip.samples[b * 2 + c] * fraction) /
				                       65536);
				out[f * 2 + c] =
				    int16_t(std::clamp(int(out[f * 2 + c]) + sample * gain / 10000, -32768, 32767));
			}
			v.cursor += step;
		}
	}
}
} // namespace editor
