// SPDX-License-Identifier: GPL-3.0-or-later
#include "atomic_file.h"
#include "file.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
static int closeDescriptor(int fd) { return _close(fd); }
static int duplicateDescriptor(int fd) { return _dup(fd); }
#else
#include <unistd.h>
static int closeDescriptor(int fd) { return close(fd); }
static int duplicateDescriptor(int fd) { return dup(fd); }
#endif

bool writeAtomicFile(const std::string &path, bool compressed,
                     const std::function<void(File &)> &writer, std::string &reason) {
	namespace fs = std::filesystem;
	static std::atomic<unsigned> serial{0};
	const auto destination = fs::u8path(path);
	auto temporary = destination;
	temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
	           + '-' + std::to_string(++serial);
	struct Cleanup {
		fs::path path;
		int descriptor = -1;
		~Cleanup() { if (descriptor >= 0) closeDescriptor(descriptor); std::error_code ec; fs::remove(path, ec); }
	} cleanup{temporary};
#ifdef _WIN32
	const int descriptor = _wopen(temporary.c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
	const int descriptor = open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
#endif
	if (descriptor < 0) { reason = "Cannot create a temporary save in the destination directory."; return false; }
	// Retain a descriptor so compression can finish before syncing the complete
	// stream (including its trailer). gzdopen/fdopen own the original descriptor.
	cleanup.descriptor = duplicateDescriptor(descriptor);
	File file;
	if (cleanup.descriptor < 0 || !file.openDescriptor(descriptor, compressed)) {
		closeDescriptor(descriptor); reason = "Cannot open the temporary save stream."; return false;
	}
	writer(file);
	bool good = file.close();
#ifdef _WIN32
	good = _commit(cleanup.descriptor) == 0 && good;
	good = closeDescriptor(cleanup.descriptor) == 0 && good;
	cleanup.descriptor = -1;
	if (good) good = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	good = fsync(cleanup.descriptor) == 0 && good;
	std::error_code ec;
	if (good) { fs::rename(temporary, destination, ec); good = !ec; }
	if (good) {
		const auto parent = destination.has_parent_path() ? destination.parent_path() : fs::path(".");
		const int directory = open(parent.c_str(), O_RDONLY | O_DIRECTORY);
		if (directory >= 0) { fsync(directory); closeDescriptor(directory); }
	}
#endif
	if (!good) { reason = "Save failed while writing or replacing the slot; the previous save was kept."; return false; }
	reason.clear();
	return true;
}
