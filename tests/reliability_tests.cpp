// SPDX-License-Identifier: GPL-3.0-or-later
#include "atomic_file.h"
#include "file.h"
#include "fs.h"
#include "resource_aba.h"
#include "resource_paq.h"
#include "unpack.h"
#include "config.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
Options g_options;
const char *g_caption = "Reliability tests";
namespace fs = std::filesystem;
static int failures;
static void check(bool value, const char *what) { if (!value) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }
static std::string contents(const fs::path &path) { std::ifstream f(path, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
static std::vector<uint8_t> literals(const std::vector<uint8_t> &plain) {
	std::vector<uint8_t> bytes(1, 0); size_t bit = 8; uint8_t key = 0;
	for (uint8_t b : plain) {
		const uint8_t encoded = b ^ key;
		for (int i = 8; i >= 0; --i) {
			if (bit / 8 == bytes.size()) bytes.push_back(0);
			if (i < 8) bytes[bit / 8] |= ((encoded >> i) & 1) << (7 - bit % 8);
			++bit;
		}
		key = uint8_t((encoded << 1) | (encoded >> 7));
	}
	return bytes;
}
int main(int argc, char **argv) {
	if (argc != 2) return 1;
	const auto root = fs::u8path(argv[1]) / fs::u8path("assets é 雪"); fs::remove_all(root); fs::create_directories(root / "nested");
	std::ofstream(root / "Mixed.Dat") << "root";
	std::ofstream(root / "nested/mixed.dat") << "nested";
	std::error_code ec; fs::create_directory_symlink(root, root / "nested/loop", ec);
	FileSystem index(root.u8string().c_str());
	check(index.exists("MIXED.DAT") && !index.exists("missing"), "case-folded and missing file lookup");
	char *found = index.findPath("mixed.dat");
	check(found && contents(fs::u8path(found)) == "root", "root wins duplicate basenames"); free(found);
	FileSystem missing((root / "missing").u8string().c_str()); check(!missing.exists("x"), "missing directory is safe");
	const auto save = (root / "slot.state").u8string(); std::string reason;
	std::ofstream(fs::u8path(save)) << "previous";
	check(!writeAtomicFile(save, true, [](File &f) { f.writeByte(1); f.seek(-1); }, reason), "failed writer rejected");
	check(contents(fs::u8path(save)) == "previous", "failed writer preserves previous slot");
	check(writeAtomicFile(save, true, [](File &f) { f.writeUint32BE(0x12345678); }, reason), "compressed replacement succeeds");
	File read; check(read.open("slot.state", "zrb", root.u8string().c_str()), "open compressed replacement");
	check(read.readUint32BE() == 0x12345678 && !read.ioErr(), "compressed save round trip"); read.close();
	fs::create_directory(root / "blocked");
	check(!writeAtomicFile((root / "blocked").u8string(), false, [](File &f) { f.writeByte(1); }, reason), "failed rename reported");
	for (auto &e : fs::directory_iterator(root)) check(e.path().filename().u8string().find(".tmp-") == std::string::npos, "temporary files cleaned");
#ifndef _WIN32
	if (fs::exists("/dev/full")) { File full; if (full.open("full", "wb", "/dev")) { full.writeByte(1); check(!full.close(), "buffered close failure propagated"); } }
#endif
	std::vector<uint8_t> plain(128); for (size_t i = 0; i < plain.size(); ++i) plain[i] = i * 17;
	const auto packed = literals(plain); std::vector<uint8_t> output(plain.size());
	check(pc98_unpack(output.data(), output.size(), packed.data(), packed.size()) && output == plain, "PC98 rolling-key literals");
	for (size_t n = 0; n < packed.size(); ++n) check(!pc98_unpack(output.data(), output.size(), packed.data(), n), "PC98 input truncation rejected");
	const uint8_t badReference[] = {0, 0x80, 0, 0};
	check(!pc98_unpack(output.data(), 4, badReference, sizeof(badReference)), "PC98 reference before output rejected");
	// Valid ABA and truncated tables: use the production parser without game data.
	{
		File f; f.open("fixture.aba", "wb", root.u8string().c_str());
		f.writeUint16BE(1); f.writeUint16BE(30); char name[14] = "TEST.DAT"; f.write(name, 14);
		f.writeUint32BE(34); f.writeUint32BE(4); f.writeUint32BE(4); f.writeUint32BE(ResourceAba::TAG); f.write("test", 4); f.close();
	}
	FileSystem archives(root.u8string().c_str()); ResourceAba aba(&archives); aba.readEntries("fixture.aba");
	uint32_t size = 0; auto *data = aba.loadEntry("test.dat", &size);
	check(data && size == 4 && memcmp(data, "test", 4) == 0, "ABA round trip"); free(data);
	check(!aba.hasEntry(std::string(500, 'x').c_str()), "ABA overlong lookup bounded");
	const auto fixture = contents(root / "fixture.aba");
	for (size_t n = 0; n < fixture.size(); ++n) {
		std::ofstream f(root / "fixture.aba", std::ios::binary); f.write(fixture.data(), n); f.close();
		ResourceAba truncated(&archives); truncated.readEntries("fixture.aba");
		check(!truncated.hasEntry("TEST.DAT"), "ABA truncation rejected transactionally");
	}
	// Exercise short executable wrappers and out-of-file PAQ records through
	// the real hashed lookup table. These files contain no original game data.
	auto paqFixture = [&](const char *name, const std::vector<uint8_t> &plain, bool invalidRange) {
		ResourcePaq lookup(nullptr); const auto *entry = lookup.findEntry(name);
		check(entry != nullptr, "PAQ synthetic entry exists in table");
		if (!entry) return;
		const auto encoded = literals(plain);
		const auto filename = std::string(ResourcePaq::_names[entry->paq]) + ".PAQ";
		File f; f.open(filename.c_str(), "wb", root.u8string().c_str());
		f.seek(entry->num * 12);
		const uint32_t begin = (unsigned(entry->num) + 1) * 12;
		f.writeUint32LE(invalidRange ? 0x7fffffff : begin);
		f.writeUint32LE(encoded.size()); f.writeUint32LE(plain.size());
		f.write(encoded.data(), encoded.size()); f.close();
		FileSystem fixtureIndex(root.u8string().c_str()); ResourcePaq fixturePaq(&fixtureIndex); fixturePaq.open();
		uint32_t length = 999; auto *loaded = fixturePaq.loadEntry(name, &length);
		check(!loaded && length == 0, "PAQ malformed wrapper/range rejected"); free(loaded);
	};
	paqFixture("LEVEL1.PGE", std::vector<uint8_t>(18, 0), false);
	paqFixture("GLOBAL.ICN", std::vector<uint8_t>(15, 0), false);
	paqFixture("GLOBAL.ICN", std::vector<uint8_t>(32, 0), true);
	std::cout << failures << " reliability failures\n"; return failures ? 1 : 0;
}
