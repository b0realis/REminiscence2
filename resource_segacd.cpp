// SPDX-License-Identifier: GPL-3.0-or-later
#include "resource.h"
#include "file.h"
#include "util.h"
#include <algorithm>
#include <array>
#include <zlib.h>

void Resource::selectObjectBank(bool objects) {
	if (!isSegaCD() || objects == _usingObjectBank)
		return;
	std::swap(_mbk, _objectMbk);
	std::swap(_mbkSize, _objectMbkSize);
	_usingObjectBank = objects;
	clearBankData();
}
void Resource::loadSegaCDGlobals() {
	File f;
	std::array<uint8_t, 96400> program{};
	// These are data ranges in the US disc's program, not executable code.
	// Validate the edition before interpreting fixed offsets. Other pressings
	// need a separately verified layout, never a best-guess memory read.
	if (!f.open("SUB.BIN", "rb", _fs) || f.size() != program.size() ||
	    f.read(program.data(), program.size()) != program.size() ||
	    crc32(0, program.data(), program.size()) != 0x80cee8d8U)
		error("Unsupported Sega CD program revision (expected the US disc)");
	free(_fnt);
	_fnt = static_cast<uint8_t *>(calloc(224, 32));
	free(_icn);
	_icnLen = 79 * 128;
	_icn = static_cast<uint8_t *>(calloc(1, _icnLen));
	if (!_fnt || !_icn)
		error("Unable to allocate Sega CD interface graphics");
	memcpy(_fnt, program.data() + 0x1144, 95 * 32);
	memcpy(_icn, program.data() + 0xfa96, 31 * 128);
	// Engine-owned inventory navigation glyphs; the CD program draws its
	// navigation separately and has no DOS-style icons 76 through 78.
	for (int n = 76; n <= 78; ++n)
		for (int y = 0; y < 16; ++y)
			for (int x = 0; x < 16; ++x) {
				const bool on =
				    n == 76 ? (x == 0 || y == 0 || x == 15 || y == 15)
					        : (abs(x - 7) <= (n == 77 ? y - 4 : 11 - y) && y >= 4 && y <= 11);
				const int pixel = (x / 8 * 2 + y / 8) * 64 + y % 8 * 8 + x % 8;
				if (on)
					_icn[n * 128 + pixel / 2] |= 1 << (pixel % 2 ? 0 : 4);
			}
}
void Resource::loadSegaCDLevel(int level) {
	static const char *logic[] = {"level1",   "level2",   "dt",      "level3_1",
	                              "level3_2", "level4_1", "level4_2"};
	static const char *graphics[] = {"level1", "level2", "dt",    "level3",
	                                 "level3", "level4", "level4"};
	static const char *objects[] = {"level1",   "level2",   "dt",      "level3_1",
	                                "level3_1", "level4_1", "level4_1"};
	static const char *sprites[] = {"level1",   "level2_1", "dt",    "level3_1",
	                                "level3_2", "level4",   "level4"};
	static const char *banks[] = {"spc1", "spc2", "spcdt", "spc3", "spc3", "spc4", "spc4"};
	if (level < 0 || level >= 7)
		error("Invalid Sega CD level");
	load(banks[level], OT_MBK);
	_objectMbk = _mbk;
	_objectMbkSize = _mbkSize;
	_mbk = nullptr;
	_mbkSize = 0;
	load(graphics[level], OT_MBK);
	load(graphics[level], OT_CT);
	load(graphics[level], OT_PAL);
	load(objects[level], OT_RP);
	free(_spc);
	_spc = nullptr;
	load(objects[level], OT_SPC);
	load(level == 1 ? "level2_1" : graphics[level], OT_LEV);
	_levNum = 1;
	memset(_segaCDRoomBank, 0, sizeof(_segaCDRoomBank));
	if (level == 1) {
		// Repeated LEV end offsets denote absent rooms. Derive ownership from
		// both directories so a save can enter any room without first passing
		// the original game's four bank-switch boundary rooms.
		for (unsigned bank = 1; bank <= 2; ++bank) {
			File f;
			std::array<uint8_t, 256> directory{};
			if (!f.open(bank == 1 ? "LEVEL2_1.LEV" : "LEVEL2_2.LEV", "rb", _fs) ||
			    f.read(directory.data(), directory.size()) != directory.size())
				error("Missing Sega CD city room directory");
			unsigned previous = directory.size();
			for (unsigned room = 0; room < 64; ++room) {
				const unsigned end = READ_BE_UINT32(directory.data() + room * 4);
				if (end < previous || end > f.size())
					error("Invalid Sega CD city room extent");
				if (end > previous) {
					if (_segaCDRoomBank[room])
						error("Ambiguous Sega CD city room bank");
					_segaCDRoomBank[room] = bank;
				}
				previous = end;
			}
		}
	}
	load(logic[level], OT_PGE);
	load(logic[level], OT_OBJ);
	load(logic[level], OT_ANI);
	if (level < 3)
		load(logic[level], OT_TBN);
	else {
		// The last four inventory-name tables are embedded in SUB.BIN.
		static const unsigned offsets[] = {0x139be, 0x13a3a, 0x13b0a, 0x13bc4};
		File f;
		std::array<uint8_t, 512> data{};
		if (!f.open("SUB.BIN", "rb", _fs))
			error("Missing Sega CD inventory names");
		f.seek(offsets[level - 3]);
		if (f.read(data.data(), data.size()) != data.size())
			error("Truncated Sega CD inventory names");
		const unsigned table = READ_BE_UINT16(data.data());
		if (!table || table % 2 || table > 128)
			error("Invalid Sega CD inventory name table");
		const unsigned last = READ_BE_UINT16(data.data() + table - 2);
		if (last < table || last >= data.size())
			error("Invalid Sega CD inventory name offset");
		const auto *end =
		    static_cast<const uint8_t *>(memchr(data.data() + last, 0, data.size() - last));
		if (!end)
			error("Unterminated Sega CD inventory name");
		const unsigned size = end - data.data() + 1;
		_tbn = static_cast<uint8_t *>(malloc(size));
		if (!_tbn)
			error("Unable to allocate Sega CD inventory names");
		memcpy(_tbn, data.data(), size);
	}
	free(_spr1);
	_spr1 = nullptr;
	load(sprites[level], OT_SPR);
	load_SPR_OFF(sprites[level], _spr1, "TAB");
	load(logic[level], OT_SPL);
	if (level == 0)
		load("level1", OT_SGD);
}
