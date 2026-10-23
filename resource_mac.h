
#ifndef RESOURCE_MAC_H__
#define RESOURCE_MAC_H__

#include <stdint.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "file.h"

struct ResourceMacMap {
	uint16_t typesOffset;
	uint16_t namesOffset;
	uint16_t typesCount;
};

struct ResourceMacType {
	unsigned char id[4];
	uint16_t count;
	uint16_t startOffset;
};

enum { kResourceMacEntryNameLength = 256 };

struct ResourceMacEntry {
	uint16_t id;
	uint16_t nameOffset;
	uint32_t dataOffset;
	uint32_t dataSize;
	char name[kResourceMacEntryNameLength];
};

struct ResourceMac {

	static const char *FILENAME1;
	static const char *FILENAME2;

	File _f;

	uint32_t _dataOffset;
	ResourceMacMap _map;
	ResourceMacType *_types;
	ResourceMacEntry **_entries;
	int _sndIndex;
	// Storage owns the arrays exposed to the legacy resource adapter.
	std::vector<ResourceMacType> _typeStorage;
	std::vector<std::vector<ResourceMacEntry>> _entryStorage;
	std::vector<ResourceMacEntry *> _entryPointers;
	std::unordered_map<const uint8_t *, size_t> _decodedSizes;

	ResourceMac(const char *filePath, FileSystem *);
	~ResourceMac();

	bool isOpen() const { return _entries != 0; }
	void load();
	std::string versionDescription();
	void loadResourceFork(uint32_t offset, uint32_t size);
	const ResourceMacEntry *findEntry(const char *name, const char *type = nullptr) const;
	const ResourceMacEntry *getEntry(const char *type, int index) const;
	void rememberData(const uint8_t *ptr, size_t size) { _decodedSizes[ptr] = size; }
	size_t decodedSize(const uint8_t *ptr) const {
		const auto found = _decodedSizes.find(ptr);
		return found == _decodedSizes.end() ? 0 : found->second;
	}
};

#endif
