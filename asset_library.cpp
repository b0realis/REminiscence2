// SPDX-License-Identifier: GPL-3.0-or-later
#include "asset_library.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace assets {
namespace {
namespace fs = std::filesystem;
std::string lower(std::string s) {
	for (char &c : s)
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
	return s;
}
bool within(const fs::path &child, const fs::path &parent) {
	const auto rel = child.lexically_relative(parent);
	return !rel.empty() && *rel.begin() != "..";
}
int marker(const std::string &name) {
	if (name == "demo_uk.aba" || name == "glob_fr.aba" || name == "intro.seq" ||
	    name == "menu1ssi.map" || name == "level1.map" || name == "level1.bnq")
		return kResourceTypeDOS;
	if (name == "present.lev")
		return kResourceTypeSega;
	if (name == "level1.lev" || name == "demo.lev")
		return kResourceTypeAmiga;
	if (name == "flashback.bin" || name == "flashback.rsrc")
		return kResourceTypeMac;
	if (name == "global.paq")
		return kResourceTypePC98;
	return -1;
}
bool musicFile(const std::string &name) {
	const auto dot = name.rfind('.');
	const auto ext = dot == std::string::npos ? "" : name.substr(dot);
	return ext == ".ogg" || ext == ".wav" || ext == ".mod" || ext == ".prf" || name == "intro" ||
	       name == "options" || name == "logo";
}
const char *label(int edition) {
	switch (edition) {
	case kResourceTypeAmiga:
		return "Amiga";
	case kResourceTypeDOS:
		return "DOS / GOG";
	case kResourceTypeMac:
		return "Macintosh";
	case kResourceTypePC98:
		return "PC-98";
	case kResourceTypeSega:
		return "Mega Drive (experimental)";
	default:
		return "Soundtrack";
	}
}
} // namespace
const char *editionId(int edition) {
	switch (edition) {
	case kResourceTypeAmiga:
		return "amiga";
	case kResourceTypeDOS:
		return "dos";
	case kResourceTypeMac:
		return "mac";
	case kResourceTypePC98:
		return "pc98";
	case kResourceTypeSega:
		return "sega";
	default:
		return "music";
	}
}
Library discover(const std::string &directory) {
	Library result;
	std::error_code ec;
	const auto root = fs::absolute(fs::u8path(directory), ec).lexically_normal();
	if (ec)
		return result;
	std::map<fs::path, std::set<int>> anchors;
	std::set<fs::path> musicDirs, packDirs, packedDirs;
	fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec),
	    end;
	while (!ec && it != end) {
		const auto name = lower(it->path().filename().u8string());
		if (name.empty() || name[0] == '.' || it.depth() >= 64 ||
		    name.find_first_of("\r\n") != std::string::npos) {
			it.disable_recursion_pending();
		} else if (it->is_regular_file(ec) && !ec) {
			const int type = marker(name);
			if (type >= 0) {
				char signature[3] = {};
				std::ifstream input(it->path(), std::ios::binary);
				input.read(signature, sizeof(signature));
				if (std::string(signature, sizeof(signature)) == "RNC") {
					packedDirs.insert(it->path().parent_path());
					result.diagnostics.push_back("Packed WHDLoad data needs extraction: " +
					                             it->path().u8string());
				} else
					anchors[it->path().parent_path()].insert(type);
			}
			if (musicFile(name)) {
				char signature[3] = {};
				std::ifstream input(it->path(), std::ios::binary);
				input.read(signature, sizeof(signature));
				// Raw WHDLoad modules cannot play until unpacked. Do not offer
				// their sibling music directory as an apparently usable source.
				if (input.gcount() == sizeof(signature) &&
				    std::string(signature, sizeof(signature)) != "RNC")
					musicDirs.insert(it->path().parent_path());
			}
			if (it->path().has_extension() && lower(it->path().extension().u8string()) == ".ogg")
				packDirs.insert(it->path().parent_path());
		}
		ec.clear();
		it.increment(ec);
	}
	// assets/music/<pack>/ is an explicit soundtrack boundary, including
	// MOD-only packs and nested track directories. It also works when a single
	// game's resources are placed directly in assets/.
	std::set<fs::path> reservedPacks;
	const auto musicRoot = root / "music";
	for (const auto &dir : musicDirs) {
		if (dir != musicRoot && within(dir, musicRoot)) {
			const auto relative = dir.lexically_relative(musicRoot);
			reservedPacks.insert(musicRoot / *relative.begin());
		}
	}
	packDirs.insert(reservedPacks.begin(), reservedPacks.end());
	// DOS tile backgrounds and extracted Mega Drive data also use LEVEL1.LEV;
	// their stronger markers disambiguate it from an Amiga edition.
	for (auto &[anchor, types] : anchors)
		if (types.count(kResourceTypeDOS) || types.count(kResourceTypeSega))
			types.erase(kResourceTypeAmiga);
	for (const auto &[anchor, types] : anchors) {
		if (types.size() != 1) {
			result.diagnostics.push_back("Mixed editions in " + anchor.u8string() +
			                             ": put each edition in its own subfolder.");
			continue;
		}
		// Include sibling fonts/music in extracted Amiga layouts, but never rise
		// to a parent containing another game's marker directory.
		auto selected = anchor;
		while (selected != root) {
			const auto parent = selected.parent_path();
			if (!within(parent, root))
				break;
			bool another = false;
			for (const auto &[other, unused] : anchors)
				another |= other != anchor && within(other, parent);
			for (const auto &pack : packDirs) {
				// A game's own music/ sibling can contain converted CD tracks.
				// Treating every Ogg directory as an independent pack strands the
				// marker in DATA/ and hides sibling movies, SUB.BIN and fonts.
				const bool ownMusic = pack.parent_path() == parent &&
				                      lower(pack.filename().u8string()) == "music" &&
				                      !reservedPacks.count(pack);
				another |= within(pack, parent) && !within(pack, selected) && !ownMusic;
			}
			for (const auto &packed : packedDirs)
				another |= within(packed, parent) && !within(packed, selected);
			if (another)
				break;
			selected = parent;
		}
		const int type = *types.begin();
		const bool segaCD = type == kResourceTypeSega && fs::exists(selected / "SC1.MCD", ec);
		const auto relative = selected.lexically_relative(root).generic_u8string();
		Source source{std::string(editionId(type)) + ":" + relative,
		              std::string(segaCD ? "Sega CD (US)" : label(type)) + " / " + relative,
		              selected.u8string(),
		              type,
		              {}};
		result.games.push_back(std::move(source));
	}
	for (auto &game : result.games) {
		for (const auto &pack : reservedPacks)
			if (within(pack, fs::u8path(game.root)))
				game.excludedRoots.push_back(pack.u8string());
		for (const auto &other : result.games)
			if (other.root != game.root && within(fs::u8path(other.root), fs::u8path(game.root)))
				game.excludedRoots.push_back(other.root);
		for (const auto &packed : packedDirs)
			if (packed != fs::u8path(game.root) && within(packed, fs::u8path(game.root)))
				game.excludedRoots.push_back(packed.u8string());
		// An ambiguous child is excluded too; it must not contaminate a valid parent.
		for (const auto &[anchor, types] : anchors)
			if (types.size() > 1 && within(anchor, fs::u8path(game.root)))
				game.excludedRoots.push_back(anchor.u8string());
		if ((game.edition == kResourceTypeAmiga || game.edition == kResourceTypeDOS ||
		     game.edition == kResourceTypeSega) &&
		    std::any_of(musicDirs.begin(), musicDirs.end(), [&](const auto &p) {
			    return within(p, fs::u8path(game.root)) &&
				       std::none_of(
			               game.excludedRoots.begin(), game.excludedRoots.end(),
			               [&](const auto &excluded) { return within(p, fs::u8path(excluded)); });
		    }))
			result.music.push_back(game);
	}
	std::set<fs::path> externalDirs = reservedPacks;
	for (const auto &dir : musicDirs) {
		if (std::any_of(reservedPacks.begin(), reservedPacks.end(),
		                [&](const auto &p) { return within(dir, p); }))
			continue;
		if (std::any_of(result.games.begin(), result.games.end(),
		                [&](const auto &game) { return within(dir, fs::u8path(game.root)); }))
			continue;
		externalDirs.insert(dir);
	}
	for (const auto &dir : externalDirs) {
		const auto relative = dir.lexically_relative(root).generic_u8string();
		result.music.push_back(
		    {"music:" + relative, "Soundtrack / " + relative, dir.u8string(), -1, {}});
	}
	return result;
}
int find(const std::vector<Source> &sources, const std::string &id) {
	for (size_t i = 0; i < sources.size(); ++i)
		if (sources[i].id == id)
			return int(i);
	int found = -1;
	for (size_t i = 0; i < sources.size(); ++i)
		if (id == editionId(sources[i].edition)) {
			if (found != -1)
				return -1; // Edition shorthand must be unambiguous.
			found = int(i);
		}
	return found;
}
} // namespace assets
