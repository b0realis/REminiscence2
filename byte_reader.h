// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_BYTE_READER_H
#define REMINISCENCE2_BYTE_READER_H
#include <cstddef>
#include <cstdint>
#include <cstring>

// A borrowed, bounded stream. A failed operation consumes no bytes and makes
// the error sticky. Subtraction checks avoid overflowing offset + length.
class ByteReader {
	const uint8_t *_bytes;
	size_t _size, _position = 0;
	bool _good;

public:
	ByteReader(const uint8_t *bytes, size_t size)
	    : _bytes(bytes), _size(size), _good(bytes || !size) {}
	bool good() const { return _good; }
	size_t position() const { return _position; }
	size_t remaining() const { return _size - _position; }
	const uint8_t *current() const { return _bytes ? _bytes + _position : nullptr; }
	bool seek(size_t offset) {
		if (!_good || offset > _size) {
			_good = false;
			return false;
		}
		_position = offset;
		return true;
	}
	bool skip(size_t length) {
		if (!_good || length > remaining()) {
			_good = false;
			return false;
		}
		_position += length;
		return true;
	}
	bool read(void *destination, size_t length) {
		if (!_good || length > remaining() || (length && !destination)) {
			_good = false;
			return false;
		}
		if (length) {
			std::memcpy(destination, current(), length);
		}
		_position += length;
		return true;
	}
	uint8_t u8() {
		uint8_t value = 0;
		read(&value, 1);
		return value;
	}
	uint16_t be16() {
		uint8_t b[2] = {};
		read(b, 2);
		return (uint16_t(b[0]) << 8) | b[1];
	}
	uint16_t le16() {
		uint8_t b[2] = {};
		read(b, 2);
		return (uint16_t(b[1]) << 8) | b[0];
	}
	uint32_t be32() {
		uint8_t b[4] = {};
		read(b, 4);
		return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
	}
	uint32_t le32() {
		uint8_t b[4] = {};
		read(b, 4);
		return (uint32_t(b[3]) << 24) | (uint32_t(b[2]) << 16) | (uint32_t(b[1]) << 8) | b[0];
	}
};
#endif
