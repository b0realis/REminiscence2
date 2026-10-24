
#include "resource_paq.h"
#include "unpack.h"
#include "util.h"
#include <vector>
#include <algorithm>

ResourcePaq::ResourcePaq(FileSystem *fs) : _fs(fs), _filesCount(0) {}
ResourcePaq::~ResourcePaq() {}

bool ResourcePaq::open() {
	for (int i = 0; i < _namesCount; ++i) {
		if (i >= ARRAYSIZE(_f)) return false;
		char name[16];
		snprintf(name, sizeof(name), "%s.PAQ", _names[i]);
		if (!_f[i].open(name, "rb", _fs)) {
			warning("Fail to open '%s'", name);
		} else {
			++_filesCount;
		}
	}
	return _filesCount == _namesCount;
}

static uint32_t djb2(const char *s) {
	uint32_t hash = 5381;
	for (int i = 0; s[i]; ++i) {
		char c = s[i];
		if (c >= 'a' && c <= 'z') {
			c += 'A' - 'a';
		}
		hash = (hash * 33) ^ ((uint8_t)c);
	}
	return hash;
}

static int comparePaqHashEntry(const void *a, const void *b) {
	const uint32_t hash1 = *(const uint32_t *)a;
	const uint32_t hash2 = ((const ResourcePaqEntry *)b)->hash;
	if (hash1 == hash2) {
		return 0;
	}
	return (hash1 < hash2) ? -1 : 1;
}

const ResourcePaqEntry *ResourcePaq::findEntry(const char *name) const {
	const uint32_t hash = djb2(name);
	debug(DBG_PAQ, "0x%08x", hash);
	return (const ResourcePaqEntry *)bsearch(&hash, _entries, _entriesCount, sizeof(ResourcePaqEntry), comparePaqHashEntry);
}

uint8_t *ResourcePaq::loadEntry(const char *name, uint32_t *size) {
	if (size) *size = 0;
	const auto *e = findEntry(name);
	if (!e || e->paq >= ARRAYSIZE(_f)) return nullptr;
	File &f = _f[e->paq];
	const uint32_t length = f.size();
	if (length < (unsigned(e->num) + 1) * 12) return nullptr;
	f.seek(e->num * 12);
	const uint32_t offset = f.readUint32LE(), compressed = f.readUint32LE(), unpacked = f.readUint32LE();
	if (f.ioErr() || !compressed || !unpacked || offset > INT32_MAX ||
	    uint64_t(offset) + compressed > length || unpacked > 64 * 1024 * 1024 ||
	    compressed > 64 * 1024 * 1024) {
		warning("Invalid PAQ record '%s'", name); return nullptr;
	}
	std::vector<uint8_t> input(compressed), output(unpacked);
	f.seek(offset);
	if (f.read(input.data(), compressed) != compressed || f.ioErr()) return nullptr;
	if (compressed == unpacked) output = std::move(input);
	else {
		if (!pc98_unpack(output.data(), unpacked, input.data(), compressed)) return nullptr;
		const char *ext = strrchr(name, '.');
		if (ext && strcasecmp(ext + 1, "PGE") == 0) {
			if (output.size() < 19) return nullptr;
			const unsigned count = READ_LE_UINT16(output.data() + 10);
			const unsigned begin = READ_LE_UINT16(output.data() + 17);
			if (count <= 0x5f || begin < 19 || begin >= output.size()) return nullptr;
			std::vector<uint8_t> decoded(count);
			if (!pc98_unpack(decoded.data(), count, output.data() + begin, output.size() - begin)) return nullptr;
			output.assign(decoded.begin() + 0x5f, decoded.end());
		} else if (strcasecmp(name, "GLOBAL.ICN") == 0) {
			bool complete = false;
			for (unsigned layer = 0; layer < 8; ++layer) {
				if (output.size() < 16) return nullptr;
				const unsigned count = READ_LE_UINT16(output.data() + 7);
				const unsigned begin = READ_LE_UINT16(output.data() + 14);
				if (begin != 0x112) {
					if (output.size() <= 0x7d0) return nullptr;
					output.erase(output.begin(), output.begin() + 0x7d0);
					complete = true; break;
				}
				if (!count || count > 16384 || begin >= output.size()) return nullptr;
				std::vector<uint8_t> decoded(count);
				if (!pc98_unpack(decoded.data(), count, output.data() + begin, output.size() - begin)) return nullptr;
				output = std::move(decoded);
			}
			if (!complete) return nullptr;
		}
	}
	auto *result = static_cast<uint8_t *>(malloc(output.size()));
	if (!result) return nullptr;
	memcpy(result, output.data(), output.size());
	if (size) *size = output.size();
	return result;
}
