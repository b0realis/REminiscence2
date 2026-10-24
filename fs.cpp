/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */
#include "fs.h"
#include "util.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
// Asset names are ASCII on every supported edition; do not depend on host locale.
std::string folded(std::string name) {
	for (char &c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	return name;
}
}
struct FileSystem_impl {
	std::unordered_map<std::string, std::string> files;
	void setRootDirectory(const char *directory, const std::vector<std::string> &excluded) {
		namespace fs = std::filesystem;
		const auto root = fs::u8path(directory);
		std::error_code ec;
		fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
		if (ec) { debug(DBG_FILE, "Cannot scan '%s': %s", directory, ec.message().c_str()); return; }
		struct Entry { std::string relative, path, name; int depth; };
		std::vector<Entry> entries;
		while (it != end) {
			const auto &entry = *it;
			const auto name = entry.path().filename().u8string();
			// Directory symlinks/junctions are never followed. Limit stack depth for
			// hostile directory trees, and preserve the historical hidden-file rule.
			if (name.empty() || name[0] == '.' || it.depth() >= 64 ||
			    std::find(excluded.begin(), excluded.end(), entry.path().u8string()) !=
			        excluded.end()) {
				it.disable_recursion_pending();
			} else if (entry.is_regular_file(ec) && !ec) {
				entries.push_back({entry.path().lexically_relative(root).generic_u8string(),
				                   entry.path().u8string(), name, it.depth()});
			}
			ec.clear();
			it.increment(ec);
			if (ec) debug(DBG_FILE, "Skipped inaccessible asset directory: %s", ec.message().c_str());
		}
		// Prefer the shallowest file, then bytewise relative path order. Duplicate
		// basenames therefore resolve identically on Windows, Linux and macOS.
		std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
			return a.depth != b.depth ? a.depth < b.depth : a.relative < b.relative;
		});
		for (const auto &e : entries) {
			if (!files.emplace(folded(e.name), e.path).second)
				debug(DBG_FILE, "Duplicate asset basename ignored: %s", e.path.c_str());
		}
		debug(DBG_FILE, "Indexed %zu assets", files.size());
	}
};
FileSystem::FileSystem(const char *dataPath, const std::vector<std::string> &excludedRoots)
    : _impl(new FileSystem_impl) {
	_impl->setRootDirectory(dataPath, excludedRoots);
}
FileSystem::~FileSystem() { delete _impl; }
char *FileSystem::findPath(const char *filename) const {
	const auto found = _impl->files.find(folded(filename));
	if (found == _impl->files.end()) return nullptr;
	const auto &path = found->second;
	char *copy = static_cast<char *>(malloc(path.size() + 1));
	if (copy) memcpy(copy, path.c_str(), path.size() + 1);
	return copy;
}
bool FileSystem::exists(const char *filename) const { return _impl->files.count(folded(filename)) != 0; }
