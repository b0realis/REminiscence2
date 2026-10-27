// SPDX-License-Identifier: GPL-3.0-or-later
#include "campaign.h"
#include "audio.h"
#include "atomic_file.h"
#include "file.h"
#include "fs.h"
#include "resource.h"
#include "cutscene_tool.h"
#include "third_party/editor/json.hpp"
#include <fstream>
#include <set>
#include <algorithm>
#include <stdexcept>
namespace editor {
using json = nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StoryElement, kind, text, file, x, y, width, height,
                                                size, color, effect, delay, duration, rate, moveX, moveY,
                                                moveDuration)
void to_json(json &j, const Route &r) {
	j = {{"key", r.key},
	     {"label", r.label},
	     {"to", r.to},
	     {"condition", r.condition},
	     {"requires", r.requiredValue}};
}
void from_json(const json &j, Route &r) {
	r.key = j.value("key", std::string("complete"));
	r.label = j.value("label", std::string("Continue"));
	r.to = j.value("to", std::string{});
	r.condition = j.value("condition", std::string{});
	r.requiredValue = j.value("requires", true);
}
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Stage, id, name, comments, kind, x, y, level, movie,
                                                background, elements, routes, advance, remember, fadeOut)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Campaign, schema, name, comments, entry, stages,
                                                carryInventory, carryHealth)
static bool validMovie(const std::string &name) {
	cinematics::Selection selected;
	return cinematics::parseSelection(name.c_str(), selected);
}
std::vector<std::string> listCampaignMovies(const assets::Source &source) {
	FileSystem files(source.root.c_str(), source.excludedRoots);
	Resource resource(&files, ResourceType(source.edition), LANG_EN);
	resource.init();
	std::vector<std::string> names;
	for (const auto &entry : cinematics::catalog(resource))
		for (unsigned part = 0; part < entry.parts && names.size() < 512; ++part)
			names.push_back(entry.name + (part ? ":" + std::to_string(part) : ""));
	return names;
}
const char *stageName(StageKind k) {
	const char *names[] = {"Level", "Movie", "Story screen", "Route choice", "End"};
	return int(k) >= 0 && int(k) < 5 ? names[int(k)] : "Unknown";
}
int stageIndex(const Campaign &c, const std::string &id) {
	for (size_t i = 0; i < c.stages.size(); ++i)
		if (c.stages[i].id == id)
			return int(i);
	return -1;
}
Stage newStage(const Campaign &c, StageKind kind) {
	if (c.stages.size() >= 128)
		throw std::runtime_error("Maximum 128 campaign stages");
	Stage s;
	s.kind = kind;
	s.name = stageName(kind);
	for (int i = 1;; ++i) {
		s.id = "stage-" + std::to_string(i);
		if (stageIndex(c, s.id) < 0)
			break;
	}
	s.x = 24 + int(c.stages.size() % 4) * 240;
	s.y = 24 + int(c.stages.size() / 4) * 150;
	if (kind == StageKind::Story || kind == StageKind::Choice || kind == StageKind::End)
		s.elements.emplace_back();
	return s;
}
static bool colorValid(const std::array<int, 4> &color) {
	return std::all_of(color.begin(), color.end(), [](int v) { return v >= 0 && v <= 255; });
}
std::vector<std::string> validateCampaign(const Campaign &c, const std::filesystem::path &root,
                                          bool files) {
	std::vector<std::string> errors;
	auto error = [&](std::string s) { errors.push_back(std::move(s)); };
	if (c.schema != 2 || c.name.empty() || c.name.size() > 160 || c.comments.size() > 2047 ||
	    c.stages.empty() || c.stages.size() > 128)
		error("Campaign needs schema 2, a name and 1..128 stages");
	if (stageIndex(c, c.entry) < 0)
		error("Select an existing entry stage");
	std::set<std::string> ids, sharedKeys;
	for (const auto &s : c.stages) {
		const std::string prefix = s.name + ": ";
		if (s.id.empty() || s.id.size() > 80 || !ids.insert(s.id).second || s.name.empty() ||
		    s.name.size() > 160 || s.comments.size() > 2047 || int(s.kind) < 0 || int(s.kind) > 4 ||
		    s.x < 0 || s.x > 8192 || s.y < 0 || s.y > 8192)
			error(prefix + "invalid identity, kind, comments or graph position");
		if (!colorValid(s.background) || s.advance < 0 || s.advance > 600000 ||
		    s.fadeOut < 0 || s.fadeOut > s.advance ||
		    (s.fadeOut && (s.kind == StageKind::Choice || s.routes.size() > 1)) ||
		    s.elements.size() > 16 || s.routes.size() > 8)
			error(prefix + "invalid screen settings or too many elements/routes");
		if (s.kind == StageKind::Level && (s.level.empty() || !safeAssetName(s.level)))
			error(prefix + "choose a level inside the campaign folder");
		if (s.kind == StageKind::Movie &&
		    (s.movie.empty() || s.movie.size() > 80 || !validMovie(s.movie)))
			error(prefix + "invalid movie catalog selection");
		if (s.kind == StageKind::End && !s.routes.empty())
			error(prefix + "End stages have no routes");
		if (s.kind != StageKind::End && s.routes.empty())
			error(prefix + "connect an exit (or use an End stage)");
		std::set<std::pair<std::string, std::string>> routes;
		for (const auto &r : s.routes) {
			if (!validCampaignKey(r.condition) || r.key.empty() || r.key.size() > 80 ||
			    r.label.empty() || r.label.size() > 80 || stageIndex(c, r.to) < 0 ||
			    !routes.emplace(r.key, r.to).second)
				error(prefix + "invalid, duplicate or dangling route");
			if (s.kind != StageKind::Level && s.kind != StageKind::Choice && r.key != "complete")
				error(prefix + "story and movie routes use complete");
		}
		for (const auto &e : s.elements) {
			if (int(e.kind) < 0 || int(e.kind) > 1 || int(e.effect) < 0 || int(e.effect) > 2 ||
			    e.x < 0 || e.y < 0 || e.width < 1 || e.height < 1 || int64_t(e.x) + e.width > 640 ||
			    int64_t(e.y) + e.height > 360 || e.size < 8 || e.size > 64 || e.delay < 0 ||
			    e.delay > 600000 || e.duration < 1 || e.duration > 600000 || e.rate < 1 ||
			    e.rate > 240 || e.moveX < -1280 || e.moveX > 1280 || e.moveY < -720 ||
			    e.moveY > 720 || e.moveDuration < 1 || e.moveDuration > 600000 ||
			    !colorValid(e.color) || e.text.size() > 4096 ||
			    !safeAssetName(e.file))
				error(prefix + "invalid story element (640 x 360 canvas)");
			if (e.kind == StoryKind::Photo && e.file.empty())
				error(prefix + "photo needs an image");
		}
		if (files)
			try {
				if (s.kind == StageKind::Level) {
					const auto path = assetPath(root, s.level);
					auto p = loadProject(path);
					Images images;
					images.root = path.parent_path();
					validateImages(p, images);
					std::set<std::string> exits;
					if (p.native.enabled)
						exits.insert("complete");
					for (const auto &m : p.modules)
						for (const auto &n : m.nodes) {
							if (n.kind == NodeKind::LevelExit)
								exits.insert(n.text);
							if (!n.campaignKey.empty())
								sharedKeys.insert((n.kind == NodeKind::CampaignSet ||
								                           n.kind == NodeKind::CampaignTest
								                       ? "flag:"
													   : "item:") +
								                  n.campaignKey);
						}
					if (exits.empty())
						error(prefix + "custom level has no Level exit node");
					for (const auto &exit : exits)
						if (campaignRoutes(s, exit).empty())
							error(prefix + "unconnected level exit: " + exit);
				}
				for (const auto &e : s.elements)
					if (e.kind == StoryKind::Photo)
						readImage(assetPath(root, e.file));
			} catch (const std::exception &e) {
				error(prefix + e.what());
			}
	}
	if (sharedKeys.size() > 4096)
		error("Campaign exceeds 4096 shared item/flag keys");
	// Reachability is a warning in the UI, not an error: disconnected drafts are useful.
	return errors;
}
std::string serializeCampaign(const Campaign &c) {
	return json(c).dump(2) + '\n';
}
Campaign deserializeCampaign(const std::string &text) {
	if (text.size() > 2 * 1024 * 1024)
		throw std::runtime_error("Campaign exceeds 2 MiB");
	auto j = json::parse(text, [](int depth, json::parse_event_t, json &) {
		if (depth > 24)
			throw std::runtime_error("Campaign nesting limit");
		return true;
	});
	// Reject lossy integer conversions and malformed fixed arrays before decoding.
	std::function<void(const json &)> inspect = [&](const json &v) {
		if (v.is_number_float() || (v.is_number_unsigned() && v.get<uint64_t>() > INT32_MAX) ||
		    (v.is_number_integer() && !v.is_number_unsigned() &&
		     (v.get<int64_t>() < INT32_MIN || v.get<int64_t>() > INT32_MAX)))
			throw std::runtime_error("Campaign numbers must be 32-bit integers");
		if (v.is_object())
			for (auto it = v.begin(); it != v.end(); ++it) {
				if ((it.key() == "color" || it.key() == "background") &&
				    (!it->is_array() || it->size() != 4))
					throw std::runtime_error("Color needs four channels");
				inspect(it.value());
			}
		else if (v.is_array())
			for (const auto &x : v)
				inspect(x);
	};
	inspect(j);
	auto c = j.get<Campaign>();
	if (c.schema == 1)
		c.schema = 2;
	// Save/load permits incomplete drafts; strict validation runs before Play.
	if (c.schema != 2 || c.stages.size() > 128)
		throw std::runtime_error("Unsupported campaign schema or stage limit");
	for (const auto &s : c.stages)
		if (s.elements.size() > 16 || s.routes.size() > 8)
			throw std::runtime_error("Campaign element/route limit");
	for (const auto &s : c.stages) {
		if (s.id.empty() || s.id.size() > 80 || s.name.size() > 160 || s.comments.size() > 2047 ||
		    int(s.kind) < 0 || int(s.kind) > 4 || s.x < 0 || s.x > 8192 || s.y < 0 || s.y > 8192 ||
		    !colorValid(s.background) || s.advance < 0 || s.advance > 600000 ||
		    s.fadeOut < 0 || s.fadeOut > s.advance ||
		    (s.fadeOut && (s.kind == StageKind::Choice || s.routes.size() > 1)) ||
		    !safeAssetName(s.level) || s.movie.size() > 80)
			throw std::runtime_error("Invalid campaign stage fields");
		for (const auto &e : s.elements)
			if (int(e.kind) < 0 || int(e.kind) > 1 || int(e.effect) < 0 || int(e.effect) > 2 ||
			    e.x < 0 || e.y < 0 || e.width < 1 || e.height < 1 || int64_t(e.x) + e.width > 640 ||
			    int64_t(e.y) + e.height > 360 || e.size < 8 || e.size > 64 || e.delay < 0 ||
			    e.delay > 600000 || e.duration < 1 || e.duration > 600000 || e.rate < 1 ||
			    e.rate > 240 || e.moveX < -1280 || e.moveX > 1280 || e.moveY < -720 ||
			    e.moveY > 720 || e.moveDuration < 1 || e.moveDuration > 600000 ||
			    !colorValid(e.color) || e.text.size() > 4096 ||
			    !safeAssetName(e.file))
				throw std::runtime_error("Invalid story element fields");
		for (const auto &r : s.routes)
			if (!validCampaignKey(r.condition) || r.key.size() > 80 || r.label.size() > 80 ||
			    r.to.size() > 80)
				throw std::runtime_error("Invalid campaign route fields");
	}
	if (c.name.size() > 160 || c.comments.size() > 2047 || c.entry.size() > 80)
		throw std::runtime_error("Invalid campaign metadata");
	return c;
}
Campaign loadCampaign(const std::filesystem::path &path) {
	if (std::filesystem::file_size(path) > 2 * 1024 * 1024)
		throw std::runtime_error("Campaign exceeds 2 MiB");
	std::ifstream file(path);
	return deserializeCampaign(std::string(std::istreambuf_iterator<char>(file), {}));
}
void saveCampaign(const Campaign &c, const std::filesystem::path &path) {
	const auto text = serializeCampaign(c);
	deserializeCampaign(text);
	std::filesystem::create_directories(std::filesystem::absolute(path).parent_path());
	std::string reason;
	if (!writeAtomicFile(
	        path.u8string(), false, [&](File &f) { f.write(text.data(), text.size()); }, reason))
		throw std::runtime_error("Cannot save campaign: " + reason);
}
std::vector<Route> campaignRoutes(const Stage &s, const std::string &exit) {
	std::vector<Route> out;
	for (const auto &r : s.routes)
		if (s.kind == StageKind::Choice || r.key == exit)
			out.push_back(r);
	return out;
}
std::string importCampaignLevel(const std::filesystem::path &source,
                                const std::filesystem::path &root) {
	auto p = loadProject(source);
	Images check;
	check.root = source.parent_path();
	validateImages(p, check);
	std::filesystem::path folder;
	for (int i = 1;; ++i) {
		folder = root / "levels" / ("level-" + std::to_string(i));
		if (!std::filesystem::exists(folder))
			break;
	}
	std::filesystem::create_directories(folder);
	auto copy = [&](Art &a) {
		if (!a.file.empty())
			a.file = importImage(assetPath(source.parent_path(), a.file), folder);
	};
	for (auto &r : p.rooms)
		for (auto &a : r.art)
			copy(a);
	for (auto &m : p.modules) {
		forEachArt(m, copy);
		copyModuleAudio(m, source.parent_path(), folder);
	}
	saveProject(p, folder / "project.r2project.json");
	return std::filesystem::relative(folder / "project.r2project.json", root).generic_u8string();
}
} // namespace editor
