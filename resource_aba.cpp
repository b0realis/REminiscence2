#include "resource_aba.h"
#include "unpack.h"
#include "util.h"
#include <algorithm>
#include <vector>

ResourceAba::ResourceAba(FileSystem *fs) : _fs(fs), _filesCount(0), _entries(nullptr), _entriesCount(0) {}
ResourceAba::~ResourceAba() { free(_entries); }
static int compareAbaEntry(const void *a, const void *b) {
	return strcasecmp(static_cast<const ResourceAbaEntry *>(a)->name,
	                  static_cast<const ResourceAbaEntry *>(b)->name);
}
void ResourceAba::readEntries(const char *aba) {
	if (_filesCount >= ARRAYSIZE(_f) || !_f[_filesCount].open(aba, "rb", _fs)) return;
	File &f = _f[_filesCount];
	const uint32_t length = f.size();
	const unsigned count = f.readUint16BE(), recordSize = f.readUint16BE();
	std::vector<ResourceAbaEntry> pending;
	bool valid = !f.ioErr() && recordSize == 30 && length >= 4 && count <= (length - 4) / 30;
	uint64_t nextOffset = 4 + uint64_t(count) * 30;
	for (unsigned i = 0; valid && i < count; ++i) {
		ResourceAbaEntry e = {};
		f.read(e.name, sizeof(e.name));
		e.offset = f.readUint32BE(); e.compressedSize = f.readUint32BE(); e.size = f.readUint32BE();
		e.fileIndex = _filesCount;
		const uint32_t tag = f.readUint32BE();
		// Original resources are small. Cap expansion before allocation, and
		// require a bounded, terminated name before sorting or logging it.
		valid = !f.ioErr() && memchr(e.name, 0, sizeof(e.name)) && e.name[0] && tag == TAG &&
		        e.offset >= nextOffset && uint64_t(e.offset) + e.compressedSize <= length &&
		        e.size <= 64 * 1024 * 1024 && e.compressedSize <= 64 * 1024 * 1024 &&
		        (e.compressedSize == e.size || e.compressedSize >= 12);
		nextOffset = uint64_t(e.offset) + e.compressedSize;
		pending.push_back(e);
	}
	if (!valid) { warning("Invalid or truncated ABA archive '%s'", aba); f.close(); return; }
	if (!pending.empty()) {
		void *grown =
		    realloc(_entries, (_entriesCount + pending.size()) * sizeof(ResourceAbaEntry));
		if (!grown) {
			warning("Cannot allocate ABA index");
			f.close();
			return;
		}
		_entries = static_cast<ResourceAbaEntry *>(grown);
		memcpy(_entries + _entriesCount, pending.data(), pending.size() * sizeof(ResourceAbaEntry));
	}
	_entriesCount += pending.size();
	if (_entriesCount > 1) std::stable_sort(_entries, _entries + _entriesCount,
	    [](const ResourceAbaEntry &a, const ResourceAbaEntry &b) { return strcasecmp(a.name, b.name) < 0; });
	++_filesCount;
}
const ResourceAbaEntry *ResourceAba::findEntry(const char *name) const {
	if (!name || strlen(name) >= sizeof(ResourceAbaEntry::name) || !_entriesCount) return nullptr;
	ResourceAbaEntry key = {};
	memcpy(key.name, name, strlen(name));
	return static_cast<const ResourceAbaEntry *>(bsearch(&key, _entries, _entriesCount, sizeof(key), compareAbaEntry));
}
uint8_t *ResourceAba::loadEntry(const char *name, uint32_t *size) {
	if (size) *size = 0;
	const auto *e = findEntry(name);
	if (!e) return nullptr;
	std::vector<uint8_t> source(std::max(1u, e->compressedSize));
	File &f = _f[e->fileIndex];
	f.seek(e->offset);
	if (f.read(source.data(), e->compressedSize) != e->compressedSize || f.ioErr()) return nullptr;
	auto *dst = static_cast<uint8_t *>(malloc(std::max(1u, e->size)));
	if (!dst) return nullptr;
	if (e->size == e->compressedSize) memcpy(dst, source.data(), e->size);
	else if (!bytekiller_unpack(dst, e->size, source.data(), e->compressedSize)) {
		warning("Invalid ABA compressed entry '%s'", name); free(dst); return nullptr;
	}
	if (size) *size = e->size;
	return dst;
}
