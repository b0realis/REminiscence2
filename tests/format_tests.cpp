// SPDX-License-Identifier: GPL-3.0-or-later
#include "byte_reader.h"
#include "graphics.h"
#include "decode_sega.h"
#include "edition.h"
#include "credits.h"
#include "unpack.h"
#include "decode_mac.h"
#include "appearance.h"
#include "resource_mac.h"
#include "mixer.h"
#include "headless_system.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

Options g_options;
const char *g_caption = "format tests";
static int failures = 0;
static void check(bool value, const char *message) {
	if (!value) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}
static void be16(std::vector<uint8_t> &b, size_t at, uint16_t n) {
	b.at(at) = n >> 8;
	b.at(at + 1) = n;
}
static void be32(std::vector<uint8_t> &b, size_t at, uint32_t n) {
	be16(b, at, n >> 16);
	be16(b, at + 2, n);
}
static std::vector<uint8_t> image(uint16_t signature, int width, int height,
                                  std::initializer_list<uint8_t> encoded) {
	const size_t header = signature == 0xC211 ? 20 : 16;
	std::vector<uint8_t> b(header);
	be16(b, 0, signature);
	be16(b, 2, 1);
	be32(b, 8, 12);
	be16(b, 12, width);
	be16(b, 14, height);
	b.insert(b.end(), encoded);
	return b;
}
static void checkEveryTruncation(const std::vector<uint8_t> &b) {
	MacImage decoded;
	for (size_t n = 0; n < b.size(); ++n) {
		check(!decodeMacImage(b.data(), n, 0, decoded), "truncated image rejected");
	}
}
static std::vector<uint8_t> resourceFork() {
	// Two resource types intentionally share a name; type lookup must disambiguate.
	std::vector<uint8_t> b(400);
	be32(b, 0, 256);
	be32(b, 4, 280);
	be32(b, 8, 16);
	be32(b, 12, 120);
	be32(b, 256, 1);
	b[260] = 42;
	be32(b, 264, 1);
	b[268] = 99;
	const size_t m = 280;
	be16(b, m + 24, 28);
	be16(b, m + 26, 80);
	be16(b, m + 28, 1);
	std::copy_n("PPSS", 4, b.begin() + m + 30);
	be16(b, m + 34, 0);
	be16(b, m + 36, 18);
	std::copy_n("STRS", 4, b.begin() + m + 38);
	be16(b, m + 42, 0);
	be16(b, m + 44, 30);
	be16(b, m + 46, 128);
	be16(b, m + 48, 0);
	be32(b, m + 50, 0);
	be16(b, m + 58, 129);
	be16(b, m + 60, 0);
	be32(b, m + 62, 8);
	b[m + 80] = 4;
	std::copy_n("same", 4, b.begin() + m + 81);
	return b;
}
static void save(const std::filesystem::path &path, const std::vector<uint8_t> &bytes) {
	std::ofstream f(path, std::ios::binary);
	f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}
int main(int argc, char **argv) {
	if (argc != 2) {
		return 1;
	}
	const auto root = std::filesystem::path(argv[1]);
	std::filesystem::create_directories(root);
	const uint8_t bytes[] = {0x12, 0x34};
	ByteReader reader(bytes, 2);
	check(reader.be16() == 0x1234, "bounded endian read");
	check(!reader.skip(SIZE_MAX) && reader.remaining() == 0,
	      "overflowing skip fails without moving cursor");

	MacImage decoded;
	auto literal = image(0xC103, 3, 1, {0, 4, 5, 6});
	check(decodeMacImage(literal.data(), literal.size(), 0, decoded) &&
	          decoded.pixels == std::vector<uint8_t>({4, 5, 6}),
	      "C103 literal pixels");
	checkEveryTruncation(literal);
	auto reference = image(0xC103, 4, 1, {2, 7, 0, 0});
	check(decodeMacImage(reference.data(), reference.size(), 0, decoded) &&
	          decoded.pixels == std::vector<uint8_t>(4, 7),
	      "C103 overlapping reference");
	reference[16] = 1;
	check(!decodeMacImage(reference.data(), reference.size(), 0, decoded),
	      "C103 reference before beginning rejected");
	auto repeated = image(0xC211, 4, 1, {2, 0x62, 7, 8, 1, 0x41});
	check(decodeMacImage(repeated.data(), repeated.size(), 0, decoded) &&
	          decoded.pixels == std::vector<uint8_t>({7, 8, 7, 8}),
	      "C211 repeat count is exact");
	checkEveryTruncation(repeated);
	auto overflow = image(0xC211, 2, 1, {0x43, 7, 0x41});
	check(!decodeMacImage(overflow.data(), overflow.size(), 0, decoded),
	      "C211 row overflow rejected");
	auto unbalanced = image(0xC211, 1, 1, {1, 0x41});
	check(!decodeMacImage(unbalanced.data(), unbalanced.size(), 0, decoded),
	      "C211 unmatched loop rejected");
	auto emptyLoop = image(0xC211, 1, 1, {0, 255, 255, 0, 255, 255, 1, 1, 0x41});
	check(!decodeMacImage(emptyLoop.data(), emptyLoop.size(), 0, decoded),
	      "C211 excessive empty loops bounded");

	MacImage sprite;
	sprite.width = 3;
	sprite.height = 1;
	sprite.pixels = {0, 4, 5};
	std::vector<uint8_t> target = {0xAA, 0x80, 1, 2, 0xAA};
	DecodeBuffer dst = {};
	dst.ptr = target.data() + 1;
	dst.w = 3;
	dst.h = 1;
	dst.pitch = 3;
	dst.xflip = true;
	dst.transparent = true;
	dst.respectMask = true;
	blitMacImage(sprite, dst);
	check(target == std::vector<uint8_t>({0xAA, 0x80, 4, 2, 0xAA}),
	      "flip, foreground mask, transparency and canaries");
	dst.x = -1;
	dst.respectMask = false;
	blitMacImage(sprite, dst);
	check(target.front() == 0xAA && target.back() == 0xAA && target[1] == 4,
	      "negative sprite clipping");

	MacImage shirt;
	shirt.width = 4;
	shirt.height = 1;
	shirt.pixels = {0x4C, 0x4D, 0x44, 0};
	std::vector<uint8_t> clothes(4, 0);
	DecodeBuffer player{};
	player.ptr = clothes.data();
	player.w = player.pitch = 4;
	player.h = 1;
	player.transparent = player.respectMask = true;
	player.conrad = true;
	blitMacImage(shirt, player);
	check(clothes ==
	          std::vector<uint8_t>({appearance::ShirtShadow, appearance::ShirtLight, 0x44, 0}),
	      "Mac player shirt remaps without changing skin or transparency");
	player.conrad = false;
	blitMacImage(shirt, player);
	check(clothes[0] == 0x4C && clothes[1] == 0x4D, "Mac NPC shirt stays original");
	player.conrad = true;
	player.xflip = true;
	std::fill(clothes.begin(), clothes.end(), 0x80);
	blitMacImage(shirt, player);
	check(clothes == std::vector<uint8_t>(4, 0x80),
	      "Mac shirt respects foreground mask when mirrored");

	const auto fork = resourceFork();
	save(root / "raw.rsrc", fork);
	ResourceMac raw("raw.rsrc", nullptr);
	// FileSystem lookup is unnecessary for synthetic files opened by directory.
	raw._f.open("raw.rsrc", "rb", root.string().c_str());
	raw.load();
	check(raw.isOpen(), "raw resource fork opens");
	raw._f.seek(1000);
	check(raw._f.size() == fork.size(), "file size is independent of a read cursor beyond EOF");
	const auto a = raw.findEntry("same", "PPSS"), b = raw.findEntry("same", "STRS");
	check(a && b && a->id == 128 && b->id == 129, "typed resources with duplicate names");
	check(!raw.getEntry("PPSS", -1) && !raw.getEntry("PPSS", 1),
	      "resource indexes checked on both sides");
	auto bad = fork;
	be32(bad, 256, UINT32_MAX);
	save(root / "bad.rsrc", bad);
	ResourceMac broken("bad.rsrc", nullptr);
	broken._f.open("bad.rsrc", "rb", root.string().c_str());
	broken.load();
	check(!broken.isOpen(), "resource payload outside fork rejected");
	// A MacBinary header must not be mistaken for a raw fork by its filename bytes.
	std::vector<uint8_t> binary(256 + fork.size());
	binary[1] = 8;
	std::copy_n("Flashbak", 8, binary.begin() + 2);
	be32(binary, 83, 3);
	be32(binary, 87, fork.size());
	std::copy(fork.begin(), fork.end(), binary.begin() + 256);
	save(root / "wrapped.bin", binary);
	ResourceMac wrapped("wrapped.bin", nullptr);
	wrapped._f.open("wrapped.bin", "rb", root.string().c_str());
	wrapped.load();
	check(wrapped.isOpen() && wrapped.findEntry("same", "STRS"),
	      "MacBinary uses resource fork length and padding");

	std::vector<uint8_t> canvas(18 * 18, 0xAA);
	Graphics graphics = {};
	graphics.setLayer(canvas.data(), 18);
	graphics.setClippingRect(1, 1, 16, 16);
	const Point concave[] = {{-2, -2}, {12, -2}, {12, 4}, {4, 4}, {4, 12}, {-2, 12}};
	graphics.drawPolygon(9, false, concave, 6);
	check(canvas[3 * 18 + 9] == 9 && canvas[9 * 18 + 3] == 9 && canvas[9 * 18 + 9] == 0xAA,
	      "concave polygon preserves its notch");
	check(canvas[0] == 0xAA && canvas[17 * 18 + 17] == 0xAA,
	      "concave clipping preserves border canaries");
	graphics._areaPoints[0] = 1;
	graphics._areaPoints[1] = 1;
	graphics._areaPoints[2] = 1;
	graphics._areaPoints[3] = -1;
	canvas[2 * 18 + 2] = 3;
	graphics.fillArea(0xD8, true);
	check(canvas[2 * 18 + 2] == 0xDB,
	      "alpha fill preserves low color bits and applies the full palette bank");
	check(editionTraits(kResourceTypeMac).resolutionScale == 2 &&
	          editionTraits(kResourceTypeSega).bigEndian &&
	          !editionTraits(kResourceTypeSega).shapeProtection,
	      "independent edition traits");
	std::vector<uint8_t> tiles(128);
	for (int tile = 0; tile < 4; ++tile) {
		std::fill_n(tiles.begin() + tile * 32, 32, uint8_t((tile + 1) * 17));
	}
	std::vector<uint8_t> pixels(258, 0xAA);
	check(decodeSegaTiles(tiles.data(), tiles.size(), 16, 16, pixels.data() + 1, 256),
	      "Sega four-tile sprite decodes");
	check(pixels[1] == 1 && pixels[9] == 3 && pixels[129] == 2 && pixels[137] == 4,
	      "Sega column-major tiles reach all four destinations");
	check(pixels.front() == 0xAA && pixels.back() == 0xAA, "Sega decode retains output canaries");
	check(!decodeSegaTiles(tiles.data(), 127, 16, 16, pixels.data() + 1, 256),
	      "truncated Sega tiles rejected");
	const uint8_t character[] = {0, 0, 0, 7, 0xF1, 255, 0xF1, 127, 0xF2, 255, 0xF2, 127};
	pixels.assign(1538, 0xAA);
	check(decodeSegaCharacter(character, sizeof(character), pixels.data() + 1, 1536),
	      "Sega RLE character decodes");
	check(pixels[1] == 1 && pixels[768] == 1 && pixels[769] == 2 && pixels[1536] == 2,
	      "Sega character parts have independent tile order");
	check(pixels.front() == 0xAA && pixels.back() == 0xAA, "Sega character canaries retained");
	for (size_t n = 0; n < sizeof(character); ++n) {
		check(!decodeSegaCharacter(character, n, pixels.data() + 1, 1536),
		      "truncated Sega character rejected");
	}
	const uint8_t credits[] = {1, 2, 3, 'F', 'L', 'A', 'S', 'H', 0, 0xFE, 60, 0xFF};
	check(validateCredits(credits, sizeof(credits)) &&
	          !validateCredits(credits, sizeof(credits) - 1),
	      "external credits require an explicit terminator");
	std::vector<uint8_t> longCredits(501, 'X');
	longCredits.back() = 0xFF;
	check(!validateCredits(longCredits.data(), longCredits.size()),
	      "external credits cannot overflow a display line");
	uint8_t emptyPacked[12] = {};
	uint8_t unpacked[34] = {};
	check(bytekiller_unpack(unpacked + 1, 32, emptyPacked, 12),
	      "zero-length Bytekiller output terminates");
	std::vector<uint8_t> packed(12);
	const uint32_t literalBits = (1u << 13) | (1u << 6) | (1u << 12);
	be32(packed, 0, literalBits);
	be32(packed, 4, literalBits);
	be32(packed, 8, 1);
	check(bytekiller_unpack(unpacked + 1, 32, packed.data(), packed.size()) && unpacked[1] == 'A',
	      "bounded reverse literal decoding");
	for (int n = 0; n < 12; ++n) {
		check(!bytekiller_unpack(unpacked + 1, 32, packed.data(), n),
		      "truncated Bytekiller trailer rejected");
	}
	const uint32_t badReference = (1u << 10) | (1u << 1) | (1u << 9);
	be32(packed, 0, badReference);
	be32(packed, 4, badReference);
	be32(packed, 8, 2);
	check(!bytekiller_unpack(unpacked + 1, 32, packed.data(), packed.size()),
	      "reverse reference into uninitialized output rejected");
	check(unpacked[0] == 0 && unpacked[33] == 0, "reverse decoder canaries retained");
	HeadlessSystem system;
	Mixer mixer(nullptr, &system);
	mixer.init();
	const uint8_t sampleA[] = {1, 2, 3}, sampleB[] = {4, 5, 6};
	mixer.play(sampleA, 3, 8000, 64);
	mixer.play(sampleB, 3, 8000, 64);
	mixer._channels[0].active = false;
	mixer._channels[1].chunkPos = 4096;
	mixer.play(sampleB, 3, 8000, 32);
	check(!mixer._channels[0].active && mixer._channels[1].chunkPos == 0 &&
	          mixer._channels[1].volume == 32,
	      "existing effect restarts beyond a free channel");
	int ownerA = 0, ownerB = 0;
	mixer.setPremixHook([](void *, int16_t *, int) { return true; }, &ownerA);
	mixer.clearPremixHook(&ownerB);
	check(mixer._premixHook != nullptr, "music player cannot remove another player's hook");
	mixer.clearPremixHook(&ownerA);
	check(!mixer._premixHook, "music player can remove its own hook");
	mixer.stopAll();
	mixer.play(nullptr, 0, 8000, 64);
	check(!mixer._channels[0].active, "empty effects rejected");
	mixer.free();

	File stream;
	stream.openMemoryBuffer(0);
	stream.writeByte(5);
	stream.writeByte(6);
	stream.seek(0);
	stream.writeByte(7);
	check(stream.size() == 2, "memory overwrite preserves trailing bytes");
	stream.seek(99);
	check(stream.tell() == 1 && stream.ioErr(), "invalid memory seek does not escape buffer");
	std::cout << (failures ? "Format checks failed\n" : "Format checks passed\n");
	return failures ? 1 : 0;
}
