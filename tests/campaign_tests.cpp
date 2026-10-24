// SPDX-License-Identifier: GPL-3.0-or-later
#include "atomic_file.h"
#include "file.h"
#include "fs.h"
#include "mixer.h"
#include "save_catalog.h"
#include "resource.h"
#include "screenshot.h"
#include "decode_sgd.h"
#include "headless_system.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
Options g_options;
const char *g_caption = "Campaign regression tests";
static int failures = 0;
static void check(bool v, const char *message) {
	if (!v) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}
static std::vector<uint8_t> saved(unsigned entities = 2) {
	File f;
	f.openMemoryBuffer(16384);
	f.writeUint32BE(0x46425356);
	f.writeUint16BE(4);
	char name[32] = "level=1 room=0";
	f.write(name, 32);
	f.writeByte(1);
	f.writeUint32BE(0);
	f.writeUint32BE(UINT32_MAX);
	f.writeUint32BE(UINT32_MAX);
	for (unsigned n = 0; n < entities; ++n) {
		uint8_t p[30] = {};
		p[9] = 3;
		p[13] = p[14] = p[15] = 255;
		p[19] = n;
		std::fill_n(p + 22, 4, 255);
		p[29] = n;
		f.write(p, 30);
	}
	std::vector<uint8_t> grid(0x1C00);
	f.write(grid.data(), grid.size());
	f.writeUint16BE(0);
	f.writeUint32BE(1);
	f.writeByte(0);
	f.writeByte(0);
	f.writeUint16BE(entities);
	f.writeUint32BE(123);
	f.writeUint32BE(0);
	f.writeUint32BE(0);
	std::vector<uint8_t> bytes(f.size());
	f.seek(0);
	f.read(bytes.data(), bytes.size());
	return bytes;
}
static bool valid(const std::vector<uint8_t> &bytes) {
	File f;
	f.openMemoryBuffer(bytes.size());
	f.write(bytes.data(), bytes.size());
	int version;
	std::string reason;
	return saves::validate(f, 2, version, reason, 0, 0, 123);
}
static bool premix(void *, int16_t *, int) {
	return true;
}
static void decoderCampaign() {
	std::vector<uint8_t> cpc(56 + 24 + 8, 0);
	auto put32 = [&](size_t at, uint32_t value) {
		for (int i = 0; i < 4; ++i)
			cpc[at + i] = uint8_t(value >> ((3 - i) * 8));
	};
	memcpy(cpc.data(), "SNDS", 4);
	put32(4, 56);
	memcpy(cpc.data() + 16, "SHDR", 4);
	put32(44, 44100);
	put32(48, 2);
	memcpy(cpc.data() + 52, "SDX2", 4);
	memcpy(cpc.data() + 56, "SNDS", 4);
	put32(60, 32);
	memcpy(cpc.data() + 72, "SSMP", 4);
	put32(76, 8);
	for (unsigned i = 80; i < cpc.size(); ++i)
		cpc[i] = i - 79;
	std::vector<int16_t> pcm;
	unsigned rate = 0;
	check(decodeCpc(cpc.data(), cpc.size(), pcm, rate) && rate == 44100 && pcm.size() == 8 &&
	          pcm[0] == 2 && pcm[1] == 8,
	      "bounded CPC/SDX2 fixture");
	auto zeroChunk = cpc;
	std::fill_n(zeroChunk.begin() + 4, 4, 0);
	check(!decodeCpc(zeroChunk.data(), zeroChunk.size(), pcm, rate),
	      "CPC zero-length chunk cannot loop");
	for (size_t n = 0; n < cpc.size(); ++n)
		check(!decodeCpc(cpc.data(), n, pcm, rate), "CPC truncation rejected");
	const std::vector<uint8_t> tiles = {255, 255, 255,  252,  0,    14,   1,    0,    0,    2,
	                                    255, 255, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
	const std::vector<uint8_t> commands = {0, 1, 0, 0, 0, 255, 0, 223};
	std::vector<uint8_t> guarded(256 * 224 + 2, 0x77);
	check(decodeSgdForeground(guarded.data() + 1, commands.data(), commands.size(), tiles.data(),
	                          tiles.size(), false, 0, 0) &&
	          guarded[256 * 224] == 1 && guarded.back() == 0x77 && guarded.front() == 0x77,
	      "packed foreground clips both pixels at bottom-right edge");
	const auto frame = guarded;
	auto repeat = commands;
	repeat[2] = repeat[3] = 255;
	check(!decodeSgdForeground(guarded.data() + 1, repeat.data(), repeat.size(), tiles.data(),
	                           tiles.size(), true, 0, 0) &&
	          guarded == frame,
	      "foreground repeat cannot read an uninitialized tile");
	for (size_t n = 0; n < tiles.size(); ++n)
		check(!decodeSgdForeground(guarded.data() + 1, commands.data(), commands.size(),
		                           tiles.data(), n, true, 0, 0) &&
		          guarded == frame,
		      "foreground truncation is transactional");
	std::vector<uint8_t> rleOut{9};
	const uint8_t run[] = {0, 2, 0x80, 42};
	check(!decodeAmigaRle(run, sizeof(run), rleOut, 128) && rleOut == std::vector<uint8_t>{9},
	      "RLE output expansion bounded");
	uint32_t seed = 0x417ab31u;
	auto random = [&] {
		seed ^= seed << 13;
		seed ^= seed >> 17;
		seed ^= seed << 5;
		return seed;
	};
	for (unsigned n = 0; n < 20000; ++n) {
		auto mutation = cpc;
		mutation[random() % mutation.size()] ^= uint8_t(1u << (random() % 8));
		std::vector<int16_t> decoded{123};
		unsigned hz = 99;
		if (!decodeCpc(mutation.data(), mutation.size(), decoded, hz))
			check(decoded == std::vector<int16_t>{123} && hz == 99,
			      "CPC mutation keeps previous output");
		auto changed = tiles;
		changed[random() % changed.size()] ^= uint8_t(1u << (random() % 8));
		const auto before = guarded;
		const bool valid = decodeSgdForeground(guarded.data() + 1, commands.data(), commands.size(),
		                                       changed.data(), changed.size(), n % 2, 0, 0);
		check(guarded.front() == 0x77 && guarded.back() == 0x77 && (valid || guarded == before),
		      "foreground mutation retains canaries and transactional output");
	}
	std::cout << "40,000 deterministic CPC/foreground mutations completed\n";
}
struct FixtureArchive : ResourceArchive {
	bool hasEntry(const char *) const override { return true; }
	uint8_t *loadEntry(const char *, uint32_t *size) override {
		*size = 38;
		auto *data = static_cast<uint8_t *>(calloc(1, *size));
		data[3] = 6;
		data[4] = data[5] = 255;
		for (unsigned n = 6; n < 38; ++n)
			data[n] = n;
		return data;
	}
};
int main(int argc, char **argv) {
	if (argc < 2)
		return 2;
	namespace fs = std::filesystem;
	const auto root = fs::u8path(argv[1]);
	fs::create_directories(root);
	if (argc == 3) {
		File f;
		f.open("Options.Cpc", "wb", argv[1]);
		f.write("SNDS", 4);
		f.writeUint32BE(0);
		f.writeUint32BE(0);
		f.writeUint32BE(0);
		f.write("JUNK", 4);
		f.close();
		FileSystem files(argv[1]);
		HeadlessSystem stub;
		Mixer m(&files, &stub);
		m.init();
		check(!m._cpc.playTrack(2), "CPC zero-length chunk rejected");
		m.free();
		return failures ? 1 : 0;
	}
	FixtureArchive archive;
	{
		FileSystem files(argv[1]);
		Resource resource(&files, kResourceTypeDOS, LANG_EN);
		resource._archive = &archive;
		resource.load("ARCHIVE", Resource::OT_BNQ);
		check(resource._bnqSize == 38, "archived BNQ retains its extent");
		if (resource._bnqSize == 38) {
			resource._mbk = resource._bnq;
			check(resource.getBankDataSize(0) == 32 && resource.loadBankData(0)[0] == 6,
			      "archived BNQ can decode through the bank adapter");
			resource._mbk = nullptr;
		}
		resource.load("ARCHIVE", Resource::OT_LEV);
		check(resource._levSize == 38, "archived LEV retains its extent");
	}
	decoderCampaign();
	const auto original = saved();
	check(valid(original), "valid synthetic state");
	auto bad = original;
	std::fill_n(bad.begin() + 51 + 26, 4, 255);
	check(!valid(bad), "reject null actor initialization link");
	bad = original;
	bad[51 + 30 + 19] = 0;
	check(!valid(bad), "reject duplicate actor index");
	bad = original;
	bad[51 + 13] = 1;
	bad[51 + 30 + 13] = 0;
	check(!valid(bad), "reject cyclic inventory list");
	bad = original;
	bad.push_back(0);
	check(!valid(bad), "reject trailing state bytes");
	std::string reason;
	check(writeAtomicFile((root / "size.gz").u8string(), true,
	                      [](File &f) { f.write("hello world", 11); }, reason),
	      "write compressed fixture");
	File f;
	check(f.open("size.gz", "zrb", argv[1]), "open compressed fixture");
	check(f.size() == 11 && f.tell() == 0,
	      "gzip size reports uncompressed extent and preserves position");
	FileSystem files(argv[1]);
	HeadlessSystem stub;
	Mixer mixer(&files, &stub);
	mixer.init();
	int other = 0;
	mixer.setPremixHook(premix, &other);
	mixer._ogg.stopTrack();
	check(mixer._premixHookData == &other && mixer._premixHook,
	      "OGG stop preserves another player's callback");
	mixer._sfx.play(68);
	mixer.setPremixHook(premix, &other);
	mixer._sfx.stop();
	check(mixer._premixHookData == &other && mixer._premixHook,
	      "action music stop preserves another player's callback");
	mixer._mod.play(-1, 0);
	check(!mixer._mod._playing, "negative MOD selection rejected");
	mixer._cpc._pcm = {100, 200, 300, 400};
	mixer._cpc._sourceRate = 44100;
	std::vector<int16_t> whole(200), part(200);
	mixer._cpc.mix(whole.data(), 100);
	mixer._cpc._cursor = mixer._cpc._phase = 0;
	mixer._cpc.mix(part.data(), 37);
	mixer._cpc.mix(part.data() + 74, 63);
	check(whole == part, "CPC resampling independent of callback partition");
	mixer._sfx.play(68);
	mixer.setPremixHook(premix, &other);
	mixer._sfx.play(70);
	check(mixer._premixHookData == &mixer._sfx,
	      "action music restarts after another backend replaced it");
	mixer.free();
	// Force checksum/trailer validation even when the full expected payload was readable.
	const auto compressedPath = root / "corrupt.state";
	check(writeAtomicFile(
	          compressedPath.u8string(), true,
	          [&](File &out) { out.write(original.data(), original.size()); }, reason),
	      "write checksummed save fixture");
	std::ifstream compressedIn(compressedPath, std::ios::binary);
	std::vector<char> compressed((std::istreambuf_iterator<char>(compressedIn)), {});
	compressedIn.close();
	compressed[compressed.size() - 8] ^= 1;
	std::ofstream corrupted(compressedPath, std::ios::binary);
	corrupted.write(compressed.data(), compressed.size());
	corrupted.close();
	File corrupt;
	corrupt.open("corrupt.state", "zrb", argv[1]);
	int version;
	check(!saves::validate(corrupt, 2, version, reason, 0, 0, 123),
	      "gzip checksum damage rejected before restore");
	std::cout << failures << " campaign failures\n";
	return failures ? 1 : 0;
}
