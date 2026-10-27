// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/audio.h"
static void audioChecks(const std::filesystem::path &root) {
	using namespace editor;
	const auto dir = root / "audio-input";
	std::filesystem::create_directories(dir);
	const auto source = dir / "tone.wav";
	std::vector<uint8_t> wav(44 + 1600, 0);
	auto put16 = [&](int at, unsigned v) {
		wav[at] = v;
		wav[at + 1] = v >> 8;
	};
	auto put32 = [&](int at, unsigned v) {
		put16(at, v);
		put16(at + 2, v >> 16);
	};
	memcpy(wav.data(), "RIFF", 4);
	put32(4, wav.size() - 8);
	memcpy(wav.data() + 8, "WAVEfmt ", 8);
	put32(16, 16);
	put16(20, 1);
	put16(22, 1);
	put32(24, 8000);
	put32(28, 16000);
	put16(32, 2);
	put16(34, 16);
	memcpy(wav.data() + 36, "data", 4);
	put32(40, 1600);
	for (int i = 0; i < 800; ++i)
		put16(44 + i * 2, 1000 + i);
	auto write = [&](const auto &path, const auto &data) {
		std::ofstream f(path, std::ios::binary);
		f.write(reinterpret_cast<const char *>(data.data()), data.size());
	};
	write(source, wav);
	const auto clip = readAudio(source);
	check(clip.rate == 8000 && clip.frames() == 800 && clip.ticks() == 3 &&
	          clip.samples[2] == 1001 && clip.samples[3] == 1001,
	      "WAV mono decode/rate/duration");
	const auto imported = importAudio(source, root);
	check(importAudio(source, root) == imported, "Audio import duplicated file");
	auto invalid = wav;
	invalid.resize(40);
	write(dir / "truncated.wav", invalid);
	rejects([&] { readAudio(dir / "truncated.wav"); }, "Truncated WAV accepted");
	invalid = wav;
	invalid[22] = 0;
	write(dir / "channels.wav", invalid);
	rejects([&] { readAudio(dir / "channels.wav"); }, "Zero audio channels accepted");
	invalid = wav;
	invalid[34] = 8;
	write(dir / "bits.wav", invalid);
	rejects([&] { readAudio(dir / "bits.wav"); }, "Wrong PCM format accepted");
	auto p = makeProject();
	auto m = makeGraph(p, 5, p.spawn);
	auto n = makeNode(m, NodeKind::Music, p.spawn);
	n.audioFile = imported;
	n.duration = 3;
	n.audioVolume = 100;
	n.audioLoop = true;
	m.nodes.push_back(n);
	n = makeNode(m, NodeKind::Sound, p.spawn);
	n.audioFile = imported;
	n.duration = 3;
	n.audioVolume = 100;
	m.nodes.push_back(n);
	p.modules.push_back(m);
	check(audioIssues(p, root).empty(), "Valid audio project rejected");
	p.modules[0].nodes[0].duration = 4;
	check(!audioIssues(p, root).empty(), "Changed audio duration accepted");
	p.modules[0].nodes[0].duration = 3;
	const auto exported = root / "audio-prefab" / "test.r2module.json";
	savePrefab(p.modules[0], root, exported);
	auto target = makeProject();
	const auto dest = root / "audio-import";
	const auto round = loadPrefab(target, {target.spawn.room, m.a.x, m.a.y}, exported, dest);
	check(readAudio(assetPath(dest, round.nodes[0].audioFile)).frames() == 800,
	      "Prefab lost audio");
	auto snapshot = initialGraphState(m);
	GraphMachine start(m, snapshot, 0);
	start.send(1, Input::Activate);
	GraphMachine early(m, snapshot, 2);
	early.advance();
	check(snapshot.nodes[1].active, "Sound finished early");
	GraphMachine end(m, snapshot, 3);
	end.advance();
	check(snapshot.nodes[1].completed && !snapshot.nodes[1].active, "Sound completion timing");
	end.send(0, Input::Activate);
	GraphMachine later(m, snapshot, 100);
	later.advance();
	check(snapshot.nodes[0].active, "Loop completed unexpectedly");
	later.send(0, Input::Disable);
	check(!snapshot.nodes[0].active, "Disable did not stop music");
	HeadlessSystem stub;
	Mixer mix(nullptr, &stub);
	mix.init();
	{
		SceneAudio audio(p, root, mix);
		audio.sync(0, 0, true, 1, 0);
		audio.sync(0, 1, true, 1, 0);
		mix.setVolumes(100, 25, 75, false);
		std::array<int16_t, 16> buffer{};
		mix.mix(buffer.data(), 8);
		check(buffer[0] == 1000 && buffer[1] == 1000,
		      "Music/effects buses did not mix independently");
		audio.reset();
		audio.sync(0, 0, true, 2, 0);
		audio.sync(0, 1, true, 2, 0);
		mix.setVolumes(50, 25, 75, false);
		mix.mix(buffer.data(), 8);
		check(buffer[0] == 500, "Master gain bypassed");
		mix.setVolumes(100, 100, 100, true);
		mix.mix(buffer.data(), 8);
		check(buffer[0] == 0, "Mute bypassed");
		audio.reset();
		audio.sync(0, 0, true, 3, 2);
		mix.setVolumes(100, 100, 100, false);
		mix.mix(buffer.data(), 8);
		check(buffer[0] == 1533, "Resumed clip did not seek to graph time");
		audio.sync(0, 0, false, 3, 0);
		mix.mix(buffer.data(), 8);
		check(buffer[0] == 0, "Stop left audible samples");
	}
	check(mix._contentHook == nullptr, "Destroyed scene left dangling audio callback");
	mix.free();
}
