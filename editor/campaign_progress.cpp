// SPDX-License-Identifier: GPL-3.0-or-later
#include "campaign_progress.h"
#include "runtime.h"
#include "game.h"
#include "file.h"
#include "atomic_file.h"
#include "systemstub.h"
#include "third_party/editor/json.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace editor {
using json = nlohmann::json;
static constexpr size_t Limit = 32 * 1024 * 1024;
static void require(bool ok) {
	if (!ok)
		throw std::runtime_error("Invalid campaign save; progress was kept");
}
static int integer(const json &v, int lo, int hi) {
	require(v.is_number_integer() && v >= lo && v <= hi);
	return v.get<int>();
}
std::string campaignIdentity(const Campaign &c, const std::filesystem::path &root) {
	uint64_t hash = 14695981039346656037ull;
	auto feed = [&](const std::string &s) {
		for (unsigned char b : s) {
			hash ^= b;
			hash *= 1099511628211ull;
		}
	};
	feed(serializeCampaign(c));
	for (const auto &s : c.stages)
		if (s.kind == StageKind::Level)
			feed(serialize(loadProject(assetPath(root, s.level))));
	std::ostringstream out;
	out << std::hex << hash;
	return out.str();
}
std::string serializeProgress(const CampaignProgress &p) {
	require(!p.identity.empty() && p.identity.size() <= 80 && !p.stage.empty() &&
	        p.stage.size() <= 80 && p.exit.size() <= 80);
	require(p.edition >= 0 && p.edition <= 4 && p.health >= 1 && p.health <= 32767 &&
	        p.visits >= 0 && p.visits <= 1000000);
	require(p.values.size() <= 4096 && p.levels.size() <= 128 && p.inventory.size() <= 256);
	for (const auto &[key, value] : p.values)
		require(key.size() > 5 && validCampaignKey(key.substr(5)) &&
		        (key.rfind("item:", 0) == 0 || key.rfind("flag:", 0) == 0));
	for (const auto &[key, value] : p.levels)
		require(!key.empty() && key.size() <= 80 && value.size() <= 2 * 1024 * 1024);
	for (const auto &a : p.inventory)
		require(a[0] >= 0 && a[0] <= 65535 && a[1] >= 0 && a[1] <= 65535 && a[2] >= 0 &&
		        a[2] <= 255 && a[3] >= 0 && a[3] <= 255 && a[4] >= INT16_MIN && a[4] <= INT16_MAX);
	json j = {{"schema", 1},        {"identity", p.identity},  {"edition", p.edition},
	          {"stage", p.stage},   {"exit", p.exit},          {"finished", p.finished},
	          {"health", p.health}, {"visits", p.visits},      {"values", p.values},
	          {"levels", p.levels}, {"inventory", p.inventory}};
	auto text = j.dump();
	require(text.size() <= Limit);
	return text;
}
CampaignProgress deserializeProgress(const std::string &text, const Campaign &c,
                                     const std::string &identity, int edition) {
	require(text.size() <= Limit);
	const auto j = json::parse(text, [](int depth, json::parse_event_t, json &) {
		require(depth <= 12);
		return true;
	});
	require(j.at("schema").is_number_integer() && j.at("schema") == 1);
	if (j.at("identity") != identity || integer(j.at("edition"), 0, 8) != edition)
		throw std::runtime_error(
		    "Campaign or asset edition changed. Start a new campaign; the previous save was kept.");
	CampaignProgress p;
	p.identity = identity;
	p.edition = edition;
	p.stage = j.at("stage").get<std::string>();
	p.exit = j.at("exit").get<std::string>();
	p.finished = j.at("finished").get<bool>();
	p.health = integer(j.at("health"), 1, 32767);
	p.visits = integer(j.at("visits"), 0, 1000000);
	const int stage = stageIndex(c, p.stage);
	require(stage >= 0 && p.exit.size() <= 80);
	require(p.exit.empty() || !campaignRoutes(c.stages[stage], p.exit).empty());
	require(!p.finished || c.stages[stage].kind == StageKind::End);
	require(j.at("values").is_object() && j.at("values").size() <= 4096);
	for (auto it = j.at("values").begin(); it != j.at("values").end(); ++it) {
		require(it.key().size() > 5 && validCampaignKey(it.key().substr(5)) &&
		        (it.key().rfind("flag:", 0) == 0 || it.key().rfind("item:", 0) == 0));
		p.values[it.key()] = it.value().get<bool>();
	}
	require(j.at("levels").is_object() && j.at("levels").size() <= 128);
	for (auto it = j.at("levels").begin(); it != j.at("levels").end(); ++it) {
		const int n = stageIndex(c, it.key());
		require(n >= 0 && c.stages[n].kind == StageKind::Level && it.value().is_string());
		auto value = it.value().get<std::string>();
		require(value.size() <= 2 * 1024 * 1024);
		p.levels[it.key()] = std::move(value);
	}
	require(j.at("inventory").is_array() && j.at("inventory").size() <= 256);
	for (const auto &a : j.at("inventory")) {
		require(a.is_array() && a.size() == 5);
		p.inventory.push_back({integer(a[0], 0, 65535), integer(a[1], 0, 65535),
		                       integer(a[2], 0, 255), integer(a[3], 0, 255),
		                       integer(a[4], INT16_MIN, INT16_MAX)});
	}
	return p;
}
CampaignProgress loadCampaignProgress(const std::filesystem::path &path, const Campaign &c,
                                      const std::string &identity, int edition) {
	require(std::filesystem::file_size(path) <= Limit);
	std::ifstream file(path, std::ios::binary);
	std::string text(std::istreambuf_iterator<char>(file), {});
	if (file.bad() || !file)
		throw std::runtime_error("Cannot read campaign save");
	return deserializeProgress(text, c, identity, edition);
}
void saveCampaignProgress(const CampaignProgress &p, const std::filesystem::path &path) {
	const auto text = serializeProgress(p);
	std::filesystem::create_directories(std::filesystem::absolute(path).parent_path());
	std::string reason;
	if (!writeAtomicFile(
	        path.u8string(), false, [&](File &f) { f.write(text.data(), text.size()); }, reason))
		throw std::runtime_error("Cannot save campaign: " + reason);
}
static bool matches(const InitPGE &a, const std::array<int, 5> &item) {
	return a.type == item[0] && a.obj_node_number == item[1] && a.object_type == item[2] &&
	       a.icon_num == item[3];
}
void CampaignRun::enter(Scene &scene) {
	scene.bindCampaign(this);
	const auto found = progress.levels.find(progress.stage);
	if (found != progress.levels.end() &&
	    (resumeLevel || campaign.stages.at(stageIndex(campaign, progress.stage)).remember))
		scene.restoreSession(found->second, !resumeLevel);
	auto &g = scene.game();
	if (!resumeLevel) {
		if (campaign.carryHealth)
			g._pgeLive[0].life = progress.health;
		if (campaign.carryInventory && scene.project().native.enabled) {
			// Import only matching actors already provided by this level. Dependencies
			// stay local; ambiguous/missing templates remain in the campaign inventory.
			for (const auto &item : progress.inventory) {
				int match = -1;
				for (int i = 1; i < g._res._pgeNum; ++i)
					if (!scene.project().native.actors[i].removed &&
					    matches(g._res._pgeInit[i], item)) {
						if (match != -1) {
							match = -2;
							break;
						}
						match = i;
					}
				if (match < 0)
					continue;
				auto &a = g._pgeLive[match];
				// pge_updateInventory relocates the inventory chain. Also remove it
				// from its old room so the same item cannot be collected twice.
				const int old = a.room_location;
				g.pge_updateInventory(&g._pgeLive[0], &a);
				a.room_location = 0xfe;
				a.life = item[4];
				g.pge_addToCurrentRoomList(&a, old);
			}
		}
	}
	resumeLevel = false;
	lastSave = g._stub->getTimeStamp();
	if (scene.canSave()) {
		capture(scene);
		store();
	}
}
void CampaignRun::capture(Scene &scene) {
	if (!scene.canSave())
		return;
	auto &g = scene.game();
	progress.levels[progress.stage] = scene.saveSession();
	progress.health = g._pgeLive[0].life;
	if (campaign.carryInventory && scene.project().native.enabled) {
		// Replace the inventory represented in this level; retain unavailable
		// templates for a later compatible stage. Consumption therefore persists.
		auto &bank = progress.inventory;
		bank.erase(std::remove_if(bank.begin(), bank.end(),
		                          [&](const auto &item) {
			                          int matchesHere = 0;
			                          for (int i = 1; i < g._res._pgeNum; ++i)
				                          if (!scene.project().native.actors[i].removed &&
										      matches(g._res._pgeInit[i], item))
					                          ++matchesHere;
			                          return matchesHere == 1;
		                          }),
		           bank.end());
		int id = g._pgeLive[0].current_inventory_PGE, count = 0;
		while (id != 255) {
			require(id > 0 && id < g._res._pgeNum && ++count <= g._res._pgeNum);
			const auto &a = g._res._pgeInit[id];
			std::array<int, 5> item{a.type, a.obj_node_number, a.object_type, a.icon_num,
			                        g._pgeLive[id].life};
			bank.push_back(item);
			id = g._pgeLive[id].next_inventory_PGE;
		}
		require(bank.size() <= 256);
	}
}
void CampaignRun::store() {
	saveCampaignProgress(progress, path);
}
bool CampaignRun::save(Scene &scene, std::string &reason) {
	try {
		if (!scene.canSave())
			throw std::runtime_error("Save after returning to living gameplay");
		capture(scene);
		store();
		scene.acknowledgeCheckpoint();
		lastSave = scene.game()._stub->getTimeStamp();
		reason = "Campaign saved";
		return true;
	} catch (const std::exception &e) {
		reason = e.what();
		return false;
	}
}
bool CampaignRun::load(Scene &scene, std::string &reason) {
	try {
		auto next = loadCampaignProgress(path, campaign, progress.identity, progress.edition);
		if (next.stage != progress.stage || !next.exit.empty() || !next.levels.count(next.stage))
			throw std::runtime_error("Resume this save from the campaign menu");
		scene.restoreSession(next.levels.at(next.stage)); // validate before swapping shared state
		progress = std::move(next);
		scene.bindCampaign(this);
		lastSave = scene.game()._stub->getTimeStamp();
		reason = "Campaign restored";
		return true;
	} catch (const std::exception &e) {
		reason = e.what();
		return false;
	}
}
void CampaignRun::poll(Scene &scene) {
	auto &g = scene.game();
	auto &input = g._stub->_pi;
	std::string reason;
	if (input.load) {
		input.load = false;
		load(scene, reason);
		scene.message(reason);
		g._stub->notify(reason, false);
	} else if (input.save) {
		input.save = false;
		save(scene, reason);
		scene.message(reason);
		g._stub->notify(reason, false);
	} else if ((scene.checkpointRequested() || g._stub->getTimeStamp() - lastSave >= 10000) &&
	           scene.canSave()) {
		save(scene, reason);
		scene.acknowledgeCheckpoint();
		lastSave = g._stub->getTimeStamp();
		if (reason != "Campaign saved")
			scene.message(reason);
	}
}
} // namespace editor
