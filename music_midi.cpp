// SPDX-License-Identifier: GPL-3.0-or-later
#include "music_midi.h"
#include "byte_reader.h"
#include <cstring>
#include <limits>

namespace music {
namespace {
bool vlq(ByteReader &in, uint32_t &value) {
	value = 0;
	for (int n = 0; n < 4; ++n) {
		const uint8_t byte = in.u8();
		if (!in.good()) {
			return false;
		}
		value = (value << 7) | (byte & 127);
		if (!(byte & 128)) {
			return true;
		}
	}
	return false;
}
bool track(ByteReader &in, Track &out) {
	uint8_t running = 0;
	uint64_t tick = 0;
	while (in.remaining()) {
		uint32_t delay = 0;
		if (!vlq(in, delay) || tick > UINT64_MAX - delay) {
			return false;
		}
		tick += delay;
		uint8_t status = in.u8(), first = 0, second = 0;
		const bool reused = status < 128;
		if (reused) {
			first = status;
			status = running;
		}
		if (status >= 0x80 && status <= 0xEF) {
			running = status;
			if (!reused) {
				first = in.u8();
			}
			const unsigned kind = status & 0xF0;
			if (kind != 0xC0 && kind != 0xD0) {
				second = in.u8();
			}
			if (!in.good() || first > 127 || second > 127 || out.events.size() >= 1000000) {
				return false;
			}
			out.events.push_back({tick, status, first, second});
		} else if (status == 0xFF && !reused) {
			running = 0;
			const unsigned type = in.u8();
			uint32_t length = 0;
			if (!vlq(in, length) || !in.skip(length)) {
				return false;
			}
			if (type == 0x2F) {
				out.endTick = tick;
				return length == 0;
			}
		} else if ((status == 0xF0 || status == 0xF7) && !reused) {
			running = 0;
			uint32_t length = 0;
			if (!vlq(in, length) || !in.skip(length)) {
				return false;
			}
		} else {
			return false;
		}
	}
	return false; // A PRF loop requires a definite, validated end-of-track.
}
std::string filename(ByteReader &in, size_t length, size_t maximum = SIZE_MAX) {
	std::vector<char> bytes(length);
	if (!in.read(bytes.data(), length)) {
		return {};
	}
	const auto end = static_cast<const char *>(std::memchr(bytes.data(), 0, length));
	std::string name(bytes.data(), end ? size_t(end - bytes.data()) : length);
	if (name.size() > maximum) {
		name.resize(maximum);
	}
	while (!name.empty() && name.back() == ' ') {
		name.pop_back();
	}
	// Assets are flat logical names resolved by FileSystem, never external paths.
	for (const unsigned char c : name) {
		if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') &&
		    c != '.' && c != '_' && c != '-') {
			return {};
		}
	}
	if (name.find("..") != std::string::npos) {
		return {};
	}
	return name;
}
} // namespace

bool parseMidi(const uint8_t *data, size_t size, Song &result, std::string &error) {
	result = {};
	error.clear();
	auto fail = [&]() {
		error = "Invalid or truncated Standard MIDI file";
		return false;
	};
	if (!data || size > 16 * 1024 * 1024) {
		return fail();
	}
	ByteReader input(data, size);
	if (input.be32() != 0x4D546864) {
		return fail();
	}
	const uint32_t headerSize = input.be32();
	if (headerSize < 6 || headerSize > input.remaining()) {
		return fail();
	}
	const unsigned format = input.be16(), count = input.be16(), division = input.be16();
	if (format > 1 || count == 0 || count > 16 || (format == 0 && count != 1) || !division ||
	    (division & 0x8000) || !input.skip(headerSize - 6)) {
		return fail();
	}
	Song parsed;
	parsed.division = division;
	for (unsigned i = 0; i < count; ++i) {
		if (input.be32() != 0x4D54726B) {
			return fail();
		}
		const uint32_t length = input.be32();
		if (!input.good() || length > input.remaining()) {
			return fail();
		}
		ByteReader chunk(input.current(), length);
		Track decoded;
		if (!track(chunk, decoded)) {
			return fail();
		}
		parsed.tracks.push_back(std::move(decoded));
		input.skip(length);
	}
	result = std::move(parsed);
	return true;
}

bool parsePrf(const uint8_t *data, size_t size, Prf &result, std::string &error) {
	result = {};
	error.clear();
	if (!data || size != 0x2F1) {
		error = "PRF must contain its complete 753-byte record";
		return false;
	}
	ByteReader input(data, size);
	Prf parsed;
	for (auto &name : parsed.instruments) {
		name = filename(input, 30, 8);
	}
	auto words = [&](auto &array) {
		for (auto &value : array) {
			value = static_cast<int16_t>(input.le16());
		}
	};
	words(parsed.adlibNotes);
	words(parsed.adlibVelocity);
	parsed.timerTicks = input.le32();
	parsed.timerMod = input.le16();
	parsed.midi = filename(input, 20);
	parsed.lookupAdlib = input.le16() != 0;
	words(parsed.adlibPrograms);
	words(parsed.mt32Programs);
	words(parsed.mt32Velocity);
	words(parsed.mt32Notes);
	for (auto &v : parsed.channels) {
		v = input.u8();
	}
	for (auto &v : parsed.programs) {
		v = input.u8();
	}
	for (auto &v : parsed.noLoop) {
		v = input.u8();
	}
	parsed.duration = input.le32();
	parsed.lookupChannels = input.u8() != 0;
	if (!input.good() || parsed.midi.empty() || !parsed.timerTicks ||
	    parsed.timerTicks > 2242 * 60 || !parsed.timerMod) {
		error = "Invalid PRF clock or MIDI filename";
		return false;
	}
	// Unused track slots may contain sentinel bytes. Playback validates the
	// mappings it actually uses; any nonzero loop flag means play once.
	result = std::move(parsed);
	return true;
}

bool parseInstrument(const uint8_t *data, size_t size, AdlibInstrument &result) {
	if (!data || size != 80) {
		return false;
	}
	ByteReader input(data, size);
	AdlibInstrument decoded;
	decoded.mode = input.u8();
	decoded.channel = input.u8();
	for (auto &op : decoded.operators) {
		for (auto &parameter : op) {
			parameter = input.le16();
		}
	}
	input.seek(74);
	decoded.modWave = input.u8();
	input.skip(1);
	decoded.carrierWave = input.u8();
	input.skip(1);
	if (input.le16() != 1 || !input.good() || decoded.mode > 1 ||
	    (decoded.mode && (decoded.channel < 6 || decoded.channel > 10)) || decoded.modWave > 3 ||
	    decoded.carrierWave > 3) {
		return false;
	}
	result = decoded;
	return true;
}
} // namespace music
