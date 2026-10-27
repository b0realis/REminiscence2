// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/campaign_progress.h"
#include "third_party/editor/json.hpp"
static void campaignProgressChecks(const std::filesystem::path &root) {
	using namespace editor;
	using json = nlohmann::json;
	auto p = makeProject();
	auto a = makeGraph(p, 5, p.spawn), b = makeGraph(p, 5, p.spawn);
	a.nodes.push_back(makeNode(a, NodeKind::Item, p.spawn));
	a.nodes[0].campaignKey = "battery";
	b.nodes.push_back(makeNode(b, NodeKind::ItemCondition, p.spawn));
	b.nodes[0].campaignKey = "battery";
	b.nodes.push_back(makeNode(b, NodeKind::CampaignSet, p.spawn));
	b.nodes[1].campaignKey = "power";
	b.nodes.push_back(makeNode(b, NodeKind::CampaignTest, p.spawn));
	b.nodes[2].campaignKey = "power";
	b.wires = {{b.nodes[0].id, b.nodes[1].id, Signal::Choice1, Input::Activate}};
	CampaignValues values;
	auto sa = initialGraphState(a), sb = initialGraphState(b);
	GraphMachine pickup(a, sa, 0, &values), receiver(b, sb, 0, &values);
	pickup.send(0, Input::Activate);
	receiver.send(0, Input::Activate);
	check(values["flag:power"] && !values["item:battery"],
	      "Cross-module item consumption/flag signal failed");
	receiver.send(2, Input::Activate);
	check(sb.nodes[2].active, "Campaign flag test missed true branch");
	receiver.send(1, Input::Reset);
	receiver.send(2, Input::Activate);
	check(!sb.nodes[2].active && !sb.nodes[1].active, "Campaign flag reset/test failed");
	p.modules = {a, b};
	check(deserialize(serialize(p)).modules[0].nodes[0].campaignKey == "battery",
	      "Campaign key roundtrip failed");
	auto folder = root / "progress";
	std::filesystem::create_directories(folder);
	saveProject(p, folder / "level.json");
	Campaign c;
	auto level = newStage(c, StageKind::Level);
	level.level = "level.json";
	c.entry = level.id;
	c.stages.push_back(level);
	auto end = newStage(c, StageKind::End);
	c.stages.push_back(end);
	c.stages[0].routes = {{"complete", "Continue", end.id}};
	CampaignProgress saved;
	saved.identity = campaignIdentity(c, folder);
	saved.stage = c.entry;
	saved.values = values;
	saved.levels[c.entry] = "snapshot";
	saveCampaignProgress(saved, folder / "save.json");
	check(serializeProgress(loadCampaignProgress(folder / "save.json", c, saved.identity, 0)) ==
	          serializeProgress(saved),
	      "Campaign save roundtrip");
	auto invalid = saved;
	invalid.health = 0;
	rejects([&] { saveCampaignProgress(invalid, folder / "save.json"); },
	        "Invalid save overwrote good progress");
	check(serializeProgress(loadCampaignProgress(folder / "save.json", c, saved.identity, 0)) ==
	          serializeProgress(saved),
	      "Rejected write lost previous save");
	const auto encoded = serializeProgress(saved);
	const auto document = json::parse(encoded);
	for (auto it = document.begin(); it != document.end(); ++it) {
		auto bad = document;
		bad.erase(it.key());
		rejects([&] { deserializeProgress(bad.dump(), c, saved.identity, 0); },
		        "Missing save field accepted");
	}
	for (const auto &key : {"", "item:", "item:../bad", "flag:space name"}) {
		auto bad = document;
		bad["values"][key] = true;
		rejects([&] { deserializeProgress(bad.dump(), c, saved.identity, 0); },
		        "Invalid shared save key accepted");
	}
	for (const auto &key : {"schema", "health", "visits", "edition"}) {
		auto bad = json::parse(encoded);
		bad[key] = 1.5;
		rejects([&] { deserializeProgress(bad.dump(), c, saved.identity, 0); },
		        "Lossy save integer accepted");
	}
	for (auto text :
	     {encoded.substr(0, encoded.size() / 2), std::string("[]"), std::string("null")})
		rejects([&] { deserializeProgress(text, c, saved.identity, 0); },
		        "Truncated/wrong save shape accepted");
	auto bad = json::parse(encoded);
	bad["stage"] = "absent";
	rejects([&] { deserializeProgress(bad.dump(), c, saved.identity, 0); },
	        "Unknown saved stage accepted");
	bad = json::parse(encoded);
	bad["values"]["item:battery"] = 1;
	rejects([&] { deserializeProgress(bad.dump(), c, saved.identity, 0); },
	        "Nonboolean campaign item accepted");
	rejects([&] { deserializeProgress(encoded, c, "changed", 0); },
	        "Edited campaign accepted stale save");
	rejects([&] { deserializeProgress(encoded, c, saved.identity, 1); },
	        "Cross-edition campaign save accepted");
}
static void campaignSnapshotChecks(const std::filesystem::path &root, const assets::Source &source,
                                   SystemStub &stub) {
	using namespace editor;
	using json = nlohmann::json;
	stub._pi = {};
	auto p = makeProject();
	auto m = makeGraph(p, 5, p.spawn);
	for (auto kind : {NodeKind::Start, NodeKind::Timer, NodeKind::Dialogue, NodeKind::CampaignSet,
	                  NodeKind::Item, NodeKind::Door})
		m.nodes.push_back(makeNode(m, kind, p.spawn));
	m.nodes[1].duration = 20;
	m.nodes[2].text = "Wait here. Your progress will survive this conversation.";
	m.nodes[3].campaignKey = "talked";
	m.nodes[4].campaignKey = "battery";
	m.nodes[5].at.x = 192;
	m.wires = {{m.nodes[0].id, m.nodes[1].id, Signal::Completed, Input::Activate},
	           {m.nodes[1].id, m.nodes[2].id, Signal::Completed, Input::Activate},
	           {m.nodes[2].id, m.nodes[3].id, Signal::Completed, Input::Activate},
	           {m.nodes[2].id, m.nodes[5].id, Signal::Completed, Input::Open}};
	p.modules = {m};
	Scene scene(p, root, source, &stub);
	for (int t = 0; t < 8; ++t)
		scene.tick();
	const auto timer = scene.saveSession();
	for (int t = 0; t < 16; ++t)
		scene.tick();
	check(scene.dialogueOpen(), "Timed dialogue did not start");
	scene.restoreSession(timer);
	check(!scene.dialogueOpen() && scene.states()[0].graph.nodes[1].active,
	      "Timer snapshot failed");
	for (int t = 0; t < 16; ++t)
		scene.tick();
	const auto during = scene.saveSession();
	Scene restored(p, root, source, &stub);
	restored.restoreSession(during);
	check(restored.dialogueOpen() && restored.saveSession() == during,
	      "Dialogue snapshot lost runtime state");
	for (int t = 0; t < 12; ++t) {
		stub._pi.enter = t % 2 == 0;
		restored.tick();
	}
	stub._pi = {};
	check(!restored.dialogueOpen() && restored.states()[0].graph.nodes[5].active,
	      "Restored dialogue did not deliver its completion signal");
	const auto good = restored.saveSession();
	const auto required = json::parse(good);
	for (auto it = required.begin(); it != required.end(); ++it) {
		if (it.key() == "localValues")
			continue; // added compatibly after schema 1
		auto bad = required;
		bad.erase(it.key());
		rejects([&] { restored.restoreSession(bad.dump()); }, "Missing snapshot field accepted");
		check(restored.saveSession() == good, "Missing-field rejection changed the running scene");
	}
	for (int mutation = 0; mutation < 10; ++mutation) {
		auto bad = json::parse(good);
		if (mutation == 0)
			bad["modules"][0]["nodes"][0]["changedAt"] = UINT64_MAX;
		if (mutation == 1)
			bad["modules"][0]["nodes"][0]["count"] = -1;
		if (mutation == 2)
			bad["modules"][0]["nodes"][0]["progress"] = {999};
		if (mutation == 3)
			bad["messages"] = {{999, 0, 1}};
		if (mutation == 4)
			bad["game"] = "0000";
		if (mutation == 5)
			bad["page"] = 999;
		if (mutation == 7)
			bad["cutscene"] = 65534;
		if (mutation == 8)
			bad["text"] = 65534;
		if (mutation == 9)
			bad["icon"] = 255;
		if (mutation == 6)
			bad["requests"] = {{0, 999, 0, "bad"}};
		rejects([&] { restored.restoreSession(bad.dump()); }, "Malformed snapshot accepted");
		check(restored.saveSession() == good, "Rejected snapshot changed live world");
	}
	// Capture a legacy NPC/computer notice (node=-1), not just a Dialogue node.
	auto notice = p;
	notice.modules[0] = makeGraph(notice, 5, notice.spawn);
	for (auto kind : {NodeKind::Start, NodeKind::Computer})
		notice.modules[0].nodes.push_back(makeNode(notice.modules[0], kind, notice.spawn));
	notice.modules[0].wires = {{notice.modules[0].nodes[0].id, notice.modules[0].nodes[1].id,
	                            Signal::Completed, Input::Progress}};
	Scene computer(notice, root, source, &stub);
	stub._pi.enter = true;
	computer.tick();
	stub._pi = {};
	check(computer.dialogueOpen(), "Computer notice fixture missing");
	const auto text = computer.saveSession();
	computer.restoreSession(text);
	check(computer.dialogueOpen(), "Legacy computer notice was not restored");
	// Queued native actor messages survive and remain in delivery order.
	restored.game().pge_sendMessage(0, 1, 7);
	restored.game().pge_sendMessage(2, 1, 9);
	const auto queued = restored.saveSession();
	restored.restoreSession(queued);
	check(restored.game()._pge_messagesTable[1] && restored.saveSession() == queued,
	      "Native message queue was lost/reordered on restore");
	CampaignRun run;
	run.campaign.stages = {Stage{}};
	run.campaign.stages[0].id = "level";
	run.campaign.entry = "level";
	run.progress.stage = "level";
	run.progress.identity = "test";
	run.progress.edition = source.edition;
	run.path = root / "campaign-session.json";
	run.enter(restored);
	std::string reason;
	check(run.save(restored, reason), "Atomic scene campaign save failed");
	restored.game()._pgeLive[0].life = 1;
	check(run.load(restored, reason) && restored.game()._pgeLive[0].life == 5,
	      "Campaign load did not restore player");
	auto cp = makeProject();
	auto checkpointModule = makeGraph(cp, 5, cp.spawn);
	for (auto kind : {NodeKind::Start, NodeKind::Checkpoint})
		checkpointModule.nodes.push_back(makeNode(checkpointModule, kind, cp.spawn));
	checkpointModule.wires = {{checkpointModule.nodes[0].id, checkpointModule.nodes[1].id,
	                           Signal::Completed, Input::Activate}};
	cp.modules = {checkpointModule};
	Scene checkpoint(cp, root, source, &stub);
	run.progress.levels.clear();
	run.enter(checkpoint);
	checkpoint.tick();
	check(checkpoint.checkpointRequested(), "Checkpoint did not request a campaign save");
	run.poll(checkpoint);
	check(!checkpoint.checkpointRequested() &&
	          run.progress.levels.at("level") == checkpoint.saveSession(),
	      "Checkpoint waited for the periodic autosave instead of storing progress");
}

static void campaignRestartChecks(const std::filesystem::path &root, const assets::Source &source,
                                  SystemStub &stub) {
	using namespace editor;
	stub._pi = {};
	auto p = makeProject();
	auto m = makeGraph(p, 5, p.spawn);
	for (auto kind : {NodeKind::Start, NodeKind::CampaignTest, NodeKind::Door, NodeKind::Button,
	                  NodeKind::CampaignSet, NodeKind::HealthPickup})
		m.nodes.push_back(makeNode(m, kind, p.spawn));
	m.nodes[1].campaignKey = m.nodes[4].campaignKey = "opened";
	m.nodes[2].at.x = 192;
	m.nodes[5].at.x = 224;
	m.wires = {{m.nodes[0].id, m.nodes[1].id, Signal::Completed, Input::Activate},
	           {m.nodes[1].id, m.nodes[2].id, Signal::Choice1, Input::Open},
	           {m.nodes[3].id, m.nodes[4].id, Signal::Used, Input::Activate},
	           {m.nodes[4].id, m.nodes[1].id, Signal::Completed, Input::Activate},
	           {m.nodes[3].id, m.nodes[5].id, Signal::Used, Input::Activate}};
	p.modules = {m};
	Scene scene(p, root, source, &stub);
	scene.tick();
	scene.game()._pgeLive[0].life = 10;
	stub._pi.enter = true;
	scene.tick();
	stub._pi = {};
	check(scene.game()._pgeLive[0].life == 10, "Health pickup reduced above-cap health");
	check(scene.states()[0].graph.nodes[2].active, "Standalone flag fixture did not open door");
	scene.restart();
	scene.tick();
	check(!scene.states()[0].graph.nodes[2].active,
	      "Standalone restart retained obsolete shared flag");
}
