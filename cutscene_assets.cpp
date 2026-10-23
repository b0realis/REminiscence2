// SPDX-License-Identifier: GPL-3.0-or-later
#include "cutscene_assets.h"
#include "byte_reader.h"
#include "unpack.h"

bool decodeCutsceneCmp(const uint8_t *data, size_t size, std::vector<uint8_t> &polygons,
                       std::vector<uint8_t> &commands, std::string &reason) {
	constexpr size_t limit = 16 * 1024 * 1024;
	polygons.clear();
	commands.clear();
	reason.clear();
	ByteReader reader(data, size);
	std::vector<uint8_t> blocks[2];
	for (auto &block : blocks) {
		const uint32_t header = reader.be32();
		const bool raw = (header & 0x80000000U) != 0;
		// Unsigned negation also handles INT32_MIN without signed overflow.
		const uint32_t packed = raw ? 0U - header : header;
		if (!reader.good() || !packed || packed > limit || (packed & 1) ||
		    packed > reader.remaining() || (!raw && packed < 12)) {
			reason = "truncated or invalid CMP block";
			return false;
		}
		const uint8_t *source = reader.current();
		const uint32_t decoded = raw ? packed : READ_BE_UINT32(source + packed - 4);
		if (!decoded || decoded > limit) {
			reason = "invalid CMP unpacked size (limit: 16 MiB per block)";
			return false;
		}
		block.resize(decoded);
		if (raw) {
			reader.read(block.data(), packed);
		} else {
			if (!bytekiller_unpack(block.data(), decoded, source, packed)) {
				reason = "invalid CMP Bytekiller stream or checksum";
				return false;
			}
			reader.skip(packed);
		}
	}
	// Some editions pad the file; block lengths, rather than EOF, delimit data.
	polygons = std::move(blocks[0]);
	commands = std::move(blocks[1]);
	return true;
}

bool cutscenePartOffsets(const uint8_t *data, size_t size, bool macintosh,
                         std::vector<size_t> &offsets, std::string &reason) {
	offsets.clear();
	reason.clear();
	ByteReader reader(data, size);
	const size_t count = reader.be16();
	const size_t body = (count + 1) * 2;
	if (!reader.good() || body >= size || (macintosh && !count)) {
		reason = "truncated cutscene part table";
		return false;
	}
	std::vector<size_t> result;
	for (size_t part = 0; part < (count ? count : 1); ++part) {
		const size_t relative = count ? reader.be16() : 0;
		const size_t offset = macintosh ? relative : body + (part ? relative : 0);
		if (!reader.good() || offset < body || offset >= size) {
			reason = "cutscene part points outside the command body";
			return false;
		}
		result.push_back(offset);
	}
	offsets = std::move(result);
	return true;
}
