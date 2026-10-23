// SPDX-License-Identifier: GPL-3.0-or-later
// Resource fork format researched in REminiscence by Gregory Montoir.
#include "resource_mac.h"
#include "byte_reader.h"
#include "fs.h"
#include "util.h"
#include <algorithm>
#include <cstring>

const char *ResourceMac::FILENAME1 = "Flashback.bin";
const char *ResourceMac::FILENAME2 = "Flashback.rsrc";
ResourceMac::ResourceMac(const char *filePath, FileSystem *fs)
    : _dataOffset(0), _map{}, _types(nullptr), _entries(nullptr), _sndIndex(-1) {
	if (fs) {
		_f.open(filePath, "rb", fs);
	}
}
ResourceMac::~ResourceMac() = default;

void ResourceMac::load() {
	if (_f.size() < 26) {
		return;
	}
	_f.seek(0);
	const uint32_t signature = _f.readUint32BE();
	if (signature == 0x00051607) { // AppleDouble
		_f.seek(24);
		const unsigned count = _f.readUint16BE();
		if (count > (_f.size() - 26) / 12) {
			return;
		}
		for (unsigned i = 0; i < count; ++i) {
			const uint32_t id = _f.readUint32BE();
			const uint32_t offset = _f.readUint32BE();
			const uint32_t length = _f.readUint32BE();
			if (id == 2) {
				loadResourceFork(offset, length);
				return;
			}
		}
	} else {
		if (signature >= 16 && signature <= _f.size()) {
			loadResourceFork(0, _f.size());
			if (isOpen()) {
				return;
			}
		}
		if (_f.size() < 128) {
			return;
		} // MacBinary: forks are padded separately to 128 bytes.
		_f.seek(83);
		const uint32_t dataSize = _f.readUint32BE();
		const uint32_t resourceSize = _f.readUint32BE();
		const uint64_t offset = 128 + ((uint64_t(dataSize) + 127) & ~uint64_t(127));
		if (offset <= _f.size()) {
			loadResourceFork(static_cast<uint32_t>(offset), resourceSize);
		}
	}
}

void ResourceMac::loadResourceFork(uint32_t offset, uint32_t size) {
	_types = nullptr;
	_entries = nullptr;
	_map = {};
	_sndIndex = -1;
	_typeStorage.clear();
	_entryStorage.clear();
	_entryPointers.clear();
	_decodedSizes.clear();
	const uint32_t fileSize = _f.size();
	if (offset > fileSize || size > fileSize - offset || size < 16 || fileSize > 0x7FFFFFFF ||
	    size > 256 * 1024 * 1024) {
		return;
	}
	std::vector<uint8_t> bytes(size);
	_f.seek(offset);
	if (_f.read(bytes.data(), size) != size) {
		return;
	}
	ByteReader input(bytes.data(), bytes.size());
	const uint32_t data = input.be32(), map = input.be32();
	const uint32_t dataSize = input.be32(), mapSize = input.be32();
	auto within = [](size_t start, size_t length, size_t limit) {
		return start <= limit && length <= limit - start;
	};
	if (!within(data, dataSize, size) || !within(map, mapSize, size) || mapSize < 30) {
		return;
	}
	ByteReader directory(bytes.data() + map, mapSize);
	directory.seek(24);
	const uint16_t typesOffset = directory.be16(), namesOffset = directory.be16();
	if (!directory.seek(typesOffset)) {
		return;
	}
	const unsigned typeCount = unsigned(directory.be16()) + 1;
	if (!directory.good() || typeCount > 65535 || typeCount > directory.remaining() / 8 ||
	    namesOffset > mapSize) {
		return;
	}
	std::vector<ResourceMacType> types(typeCount);
	for (auto &type : types) {
		directory.read(type.id, 4);
		const unsigned count = unsigned(directory.be16()) + 1;
		if (count > 65535) {
			return;
		}
		type.count = count;
		type.startOffset = directory.be16();
	}
	std::vector<std::vector<ResourceMacEntry>> entries(typeCount);
	size_t total = 0;
	for (size_t i = 0; i < types.size(); ++i) {
		const auto &type = types[i];
		total += type.count;
		if (total > 65535 || !directory.seek(size_t(typesOffset) + type.startOffset) ||
		    type.count > directory.remaining() / 12) {
			return;
		}
		auto &list = entries[i];
		list.resize(type.count);
		for (auto &entry : list) {
			entry.id = directory.be16();
			entry.nameOffset = directory.be16();
			entry.dataOffset = directory.be32() & 0xFFFFFF;
			directory.skip(4);
			if (!within(entry.dataOffset, 4, dataSize)) {
				return;
			}
			ByteReader payload(bytes.data() + data, dataSize);
			payload.seek(entry.dataOffset);
			entry.dataSize = payload.be32();
			if (!payload.good() || entry.dataSize > payload.remaining()) {
				return;
			}
		}
		for (auto &entry : list) {
			entry.name[0] = 0;
			if (entry.nameOffset == 0xFFFF) {
				continue;
			}
			if (!directory.seek(size_t(namesOffset) + entry.nameOffset)) {
				return;
			}
			const unsigned length = directory.u8();
			if (!directory.read(entry.name, length)) {
				return;
			}
			entry.name[length] = 0;
		}
	}
	// Publish only after every offset, name and data extent has been validated.
	_dataOffset = offset + data;
	_map = {typesOffset, namesOffset, static_cast<uint16_t>(typeCount)};
	_typeStorage = std::move(types);
	_entryStorage = std::move(entries);
	for (size_t i = 0; i < _typeStorage.size(); ++i) {
		_entryPointers.push_back(_entryStorage[i].data());
		if (std::memcmp(_typeStorage[i].id, "snd ", 4) == 0) {
			_sndIndex = i;
		}
	}
	_types = _typeStorage.data();
	_entries = _entryPointers.data();
}

const ResourceMacEntry *ResourceMac::findEntry(const char *name, const char *type) const {
	if (!isOpen()) {
		return nullptr;
	}
	for (size_t i = 0; i < _typeStorage.size(); ++i) {
		if (type && std::memcmp(type, _typeStorage[i].id, 4)) {
			continue;
		}
		for (const auto &entry : _entryStorage[i]) {
			if (std::strcmp(name, entry.name) == 0) {
				return &entry;
			}
		}
	}
	return nullptr;
}

const ResourceMacEntry *ResourceMac::getEntry(const char *type, int index) const {
	if (!isOpen() || index < 0) {
		return nullptr;
	}
	for (size_t i = 0; i < _typeStorage.size(); ++i) {
		if (std::memcmp(type, _typeStorage[i].id, 4) == 0) {
			return size_t(index) < _entryStorage[i].size() ? &_entryStorage[i][index] : nullptr;
		}
	}
	return nullptr;
}

std::string ResourceMac::versionDescription() {
	const auto *entry = getEntry("vers", 0);
	if (!entry || entry->dataSize < 7 || entry->dataSize > 4096) {
		return {};
	}
	const uint32_t oldPosition = _f.tell();
	_f.seek(_dataOffset + entry->dataOffset + 4);
	std::vector<uint8_t> bytes(entry->dataSize);
	const bool complete = _f.read(bytes.data(), bytes.size()) == bytes.size();
	_f.seek(oldPosition);
	if (!complete) {
		return {};
	}
	ByteReader input(bytes.data(), bytes.size());
	input.skip(6);
	auto pascal = [&]() {
		const unsigned length = input.u8();
		std::string text(length, ' ');
		if (!input.read(text.data(), length)) {
			return std::string();
		}
		for (char &c : text) {
			if (static_cast<unsigned char>(c) < 32 || c == 127) {
				c = ' ';
			}
		}
		return text;
	};
	std::string shortVersion = pascal();
	if (!input.good()) {
		return {};
	}
	if (input.remaining()) {
		const std::string description = pascal();
		if (!input.good()) {
			return {};
		}
		if (!description.empty()) {
			shortVersion += " (" + description + ")";
		}
	}
	return shortVersion;
}
