
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <limits>
#include <filesystem>
#include "file.h"
#include "fs.h"
#include "util.h"
#ifdef USE_ZLIB
#include "zlib.h"
#endif
#ifdef USE_RWOPS
#include <SDL_filesystem.h>
#include <SDL_rwops.h>
#endif

struct File_impl {
	bool _ioErr;
	File_impl() : _ioErr(false) {}
	virtual ~File_impl() {}
	virtual bool open(const char *path, const char *mode) = 0;
	virtual void close() = 0;
	virtual uint32_t size() = 0;
	virtual void seek(int32_t off) = 0;
	virtual uint32_t tell() = 0;
	virtual uint32_t read(void *ptr, uint32_t len) = 0;
	virtual uint32_t write(const void *ptr, uint32_t len) = 0;
};

struct StdioFile : File_impl {
	FILE *_fp;
	bool _writable = false;
	StdioFile() : _fp(0) {}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		_writable = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');
		_fp = fopen(path, mode);
		return (_fp != 0);
	}
	void close() {
		if (_fp) {
			fclose(_fp);
			_fp = 0;
		}
	}
	uint32_t size() {
		uint32_t sz = 0;
		if (_fp) {
			const long pos = ftell(_fp);
			struct stat status = {};
			if (fstat(fileno(_fp), &status) == 0 && S_ISREG(status.st_mode)) {
				const auto length = _writable ? MAX<int64_t>(status.st_size, pos) : status.st_size;
				if (length >= 0 && length <= UINT32_MAX) {
					return static_cast<uint32_t>(length);
				}
				_ioErr = true;
			} else if (pos >= 0 && fseek(_fp, 0, SEEK_END) == 0) {
				const long length = ftell(_fp);
				if (length >= 0 && uint64_t(length) <= UINT32_MAX) {
					sz = length;
				} else {
					_ioErr = true;
				}
				if (fseek(_fp, pos, SEEK_SET) != 0) {
					_ioErr = true;
				}
			} else {
				_ioErr = true;
			}
		}
		return sz;
	}
	void seek(int32_t off) {
		if (_fp) {
			if (off < 0 || fseek(_fp, off, SEEK_SET) != 0) {
				_ioErr = true;
			}
		}
	}
	uint32_t tell() {
		if (_fp) {
			return ftell(_fp);
		}
		return 0;
	}
	uint32_t read(void *ptr, uint32_t len) {
		if (_fp) {
			uint32_t r = fread(ptr, 1, len, _fp);
			if (r != len) {
				_ioErr = true;
			}
			return r;
		}
		return 0;
	}
	uint32_t write(const void *ptr, uint32_t len) {
		if (_fp) {
			uint32_t r = fwrite(ptr, 1, len, _fp);
			if (r != len) {
				_ioErr = true;
			}
			return r;
		}
		return 0;
	}
};

#ifdef USE_ZLIB
struct GzipFile : File_impl {
	gzFile _fp;
	GzipFile() : _fp(0) {}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		_fp = gzopen(path, mode);
		return (_fp != 0);
	}
	void close() {
		if (_fp) {
			gzclose(_fp);
			_fp = 0;
		}
	}
	uint32_t size() {
		uint32_t sz = 0;
		if (_fp) {
			int pos = gztell(_fp);
			gzseek(_fp, 0, SEEK_END);
			sz = gztell(_fp);
			gzseek(_fp, pos, SEEK_SET);
		}
		return sz;
	}
	void seek(int32_t off) {
		if (_fp) {
			gzseek(_fp, off, SEEK_SET);
		}
	}
	uint32_t tell() {
		if (_fp) {
			return gztell(_fp);
		}
		return 0;
	}
	uint32_t read(void *ptr, uint32_t len) {
		if (_fp) {
			uint32_t r = gzread(_fp, ptr, len);
			if (r != len) {
				_ioErr = true;
			}
			return r;
		}
		return 0;
	}
	uint32_t write(const void *ptr, uint32_t len) {
		if (_fp) {
			uint32_t r = gzwrite(_fp, ptr, len);
			if (r != len) {
				_ioErr = true;
			}
			return r;
		}
		return 0;
	}
};
#endif

#ifdef USE_RWOPS
struct AssetFile: File_impl {
	SDL_RWops *_rw;
	AssetFile() : _rw(0) {}
	bool prefixedOpen(const char *prefix, const char *name) {
		char path[MAXPATHLEN];
		snprintf(path, sizeof(path), "%s%s", prefix, name);
		_rw = SDL_RWFromFile(path, "rb");
		if (!_rw) {
			// try uppercase
			char fixedPath[MAXPATHLEN];
			{
				int i = 0;
				for (; path[i] && i < MAXPATHLEN - 1; ++i) {
					fixedPath[i] = path[i];
					if (i < strlen(prefix)) {
						continue;
					}
					if (fixedPath[i] >= 'a' && fixedPath[i] <= 'z') {
						fixedPath[i] += 'A' - 'a';
					}
				}
				fixedPath[i] = 0;
			}
			_rw = SDL_RWFromFile(fixedPath, "rb");
		}
		return _rw != 0;
	}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		return prefixedOpen("", path) || prefixedOpen("/sdcard/flashback/", path);
	}
	void close() {
		if (_rw) {
			SDL_RWclose(_rw);
			_rw = 0;
		}
	}
	uint32_t size() {
		if (_rw) {
			return SDL_RWsize(_rw);
		}
		return 0;
	}
	void seek(int32_t off) {
		if (_rw) {
			SDL_RWseek(_rw, off, RW_SEEK_SET);
		}
	}
	uint32_t read(void *ptr, uint32_t len) {
		if (_rw) {
			const int count = SDL_RWread(_rw, ptr, 1, len);
			if (count != len) {
				_ioErr = true;
			}
		}
		return 0;
	}
	uint32_t write(const void *ptr, uint32_t len) {
		_ioErr = true;
		return 0;
	}
};
#endif

struct MemoryBufferFile: File_impl {
	uint8_t *_ptr;
	uint32_t _capacity, _offset, _len;
	MemoryBufferFile(int initialCapacity) {
		_capacity = MAX(initialCapacity, 1);
		_ptr = (uint8_t *)malloc(_capacity);
		if (!_ptr) {
			_capacity = 0;
			_ioErr = true;
		}
		_offset = _len = 0;
	}
	~MemoryBufferFile() {
		free(_ptr);
	}
	bool open(const char *path, const char *mode) {
		return false;
	}
	void close() {
	}
	uint32_t size() {
		return _len;
	}
	uint32_t tell() {
		return _offset;
	}
	void seek(int offs) {
		if (offs < 0 || uint32_t(offs) > _len) {
			_ioErr = true;
			return;
		}
		_offset = offs;
	}
	uint32_t read(void *ptr, uint32_t len) {
		uint32_t count = MIN(len, _len - _offset);
		if (count != len) {
			_ioErr = true;
		}
		if (count != 0) {
			memcpy(ptr, _ptr + _offset, count);
			_offset += count;
		}
		return count;
	}
	uint32_t write(const void *ptr, uint32_t count) {
		if (count > uint32_t(INT32_MAX) - _offset) {
			_ioErr = true;
			return 0;
		}
		const uint32_t required = _offset + count;
		if (required > _capacity) {
			const uint32_t capacity = MAX<uint32_t>(
			    required, MIN<uint64_t>(uint64_t(MAX<uint32_t>(_capacity, 1)) * 2, INT32_MAX));
			void *replacement = realloc(_ptr, capacity);
			if (!replacement) {
				_ioErr = true;
				return 0;
			}
			_ptr = static_cast<uint8_t *>(replacement);
			_capacity = capacity;
		}
		if (count != 0) {
			memcpy(_ptr + _offset, ptr, count);
			_offset += count;
		}
		_len = MAX(_len, _offset);
		return count;
	}
};

File::File()
	: _impl(0) {
}

File::~File() {
	if (_impl) {
		_impl->close();
		delete _impl;
	}
}

bool File::open(const char *filename, const char *mode, FileSystem *fs) {
	if (_impl) {
		_impl->close();
		delete _impl;
		_impl = 0;
	}
	assert(mode[0] != 'z');
	_impl = new StdioFile;
	char *path = fs->findPath(filename);
	if (path) {
		debug(DBG_FILE, "Open file name '%s' mode '%s' path '%s'", filename, mode, path);
		bool ret = _impl->open(path, mode);
		free(path);
		return ret;
	}
#ifdef USE_RWOPS
	if (mode[0] == 'r') {
		_impl = new AssetFile;
		return _impl->open(filename, mode);
	} else if (mode[0] == 'w') {
		bool ret = false;
		char *prefPath = SDL_GetPrefPath("org.cyxdown", "fb");
		if (prefPath) {
			char path[MAXPATHLEN];
			snprintf(path, sizeof(path), "%s/%s", prefPath, filename);
			_impl = new StdioFile;
			ret = _impl->open(path, mode);
			SDL_free(prefPath);
		}
		return ret;
	}
#endif
	return false;
}

bool File::open(const char *filename, const char *mode, const char *directory) {
	if (_impl) {
		_impl->close();
		delete _impl;
		_impl = 0;
	}
#ifdef USE_ZLIB
	if (mode[0] == 'z') {
		_impl = new GzipFile;
		++mode;
	}
#endif
	if (!_impl) {
		_impl = new StdioFile;
	}
	const auto path = (std::filesystem::path(directory) / filename).string();
	debug(DBG_FILE, "Open file name '%s' mode '%s' path '%s'", filename, mode, path.c_str());
	return _impl->open(path.c_str(), mode);
}

void File::openMemoryBuffer(int initialCapacity) {
	if (_impl) {
		_impl->close();
		delete _impl;
		_impl = 0;
	}
	_impl = new MemoryBufferFile(initialCapacity);
}

void File::close() {
	if (_impl) {
		_impl->close();
	}
}

bool File::ioErr() const {
	return !_impl || _impl->_ioErr;
}

uint32_t File::size() {
	return _impl ? _impl->size() : 0;
}

void File::seek(int32_t off) {
	if (_impl) {
		_impl->seek(off);
	}
}

uint32_t File::tell() {
	return _impl ? _impl->tell() : 0;
}

uint32_t File::read(void *ptr, uint32_t len) {
	return _impl ? _impl->read(ptr, len) : 0;
}

uint8_t File::readByte() {
	uint8_t b = 0;
	read(&b, 1);
	return b;
}

uint16_t File::readUint16LE() {
	uint8_t lo = readByte();
	uint8_t hi = readByte();
	return (hi << 8) | lo;
}

uint32_t File::readUint32LE() {
	uint16_t lo = readUint16LE();
	uint16_t hi = readUint16LE();
	return (hi << 16) | lo;
}

uint16_t File::readUint16BE() {
	uint8_t hi = readByte();
	uint8_t lo = readByte();
	return (hi << 8) | lo;
}

uint32_t File::readUint32BE() {
	uint16_t hi = readUint16BE();
	uint16_t lo = readUint16BE();
	return (hi << 16) | lo;
}

uint32_t File::write(const void *ptr, uint32_t len) {
	return _impl ? _impl->write(ptr, len) : 0;
}

void File::writeByte(uint8_t b) {
	write(&b, 1);
}

void File::writeUint16LE(uint16_t n) {
	writeByte(n & 0xFF);
	writeByte(n >> 8);
}

void File::writeUint32LE(uint32_t n) {
	writeUint16LE(n & 0xFFFF);
	writeUint16LE(n >> 16);
}

void File::writeUint16BE(uint16_t n) {
	writeByte(n >> 8);
	writeByte(n & 0xFF);
}

void File::writeUint32BE(uint32_t n) {
	writeUint16BE(n >> 16);
	writeUint16BE(n & 0xFFFF);
}

void dumpFile(const char *filename, const uint8_t *p, int size) {
	char path[MAXPATHLEN];
	snprintf(path, sizeof(path), "DUMP/%s", filename);
	FILE *fp = fopen(filename, "wb");
	if (fp) {
		const int count = fwrite(p, 1, size, fp);
		if (count != size) {
			warning("Failed to write %d bytes (expected %d)", count, size);
		}
		fclose(fp);
	}
}
