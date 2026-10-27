// SPDX-License-Identifier: GPL-3.0-or-later
#include "content.h"
#include <algorithm>
#include <set>
#include <tuple>
#include <stdexcept>
namespace editor {
bool validCampaignKey(const std::string &key) {
	return key.size() <= 64 && std::all_of(key.begin(), key.end(), [](unsigned char c) {
		       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			          c == '_' || c == '-' || c == '.';
	       });
}
const char *nodeName(NodeKind k) {
	const char *names[] = {"Button",
	                       "Door",
	                       "Dialogue NPC",
	                       "Item / pickup",
	                       "Mission computer",
	                       "Area trigger",
	                       "Relay",
	                       "Animated prop",
	                       "Dialogue",
	                       "Play animation",
	                       "Pickup animation",
	                       "Timer",
	                       "State",
	                       "Actor (crawler)",
	                       "Health pickup",
	                       "Damage player",
	                       "Counter",
	                       "Has item?",
	                       "Checkpoint",
	                       "Teleport player",
	                       "Level exit",
	                       "On start",
	                       "Set campaign flag",
	                       "Test campaign flag",
	                       "Music",
	                       "Sound"};
	return int(k) >= 0 && int(k) < int(NodeKind::Count) ? names[int(k)] : "Unknown";
}
const char *signalName(Signal s) {
	const char *names[] = {"Used",    "Completed", "Opened",          "Closed",           "Entered",
	                       "Started", "Stopped",   "Choice 1",        "Choice 2",         "Exited",
	                       "Hurt",    "Died",      "Patrol endpoint", "Native rule fired"};
	return int(s) >= 0 && int(s) < 14 ? names[int(s)] : "Unknown";
}
const char *inputName(Input s) {
	const char *names[] = {"Activate", "Open",         "Close",        "Toggle", "Progress",
	                       "Reset",    "Enable",       "Disable",      "Stop",   "Friendly",
	                       "Hostile",  "Start patrol", "Hold position"};
	return int(s) >= 0 && int(s) < 13 ? names[int(s)] : "Unknown";
}
std::vector<Signal> nodeSignals(NodeKind k) {
	switch (k) {
	case NodeKind::Actor:
		return {Signal::Used, Signal::Hurt, Signal::Died, Signal::Reached, Signal::StateChanged};
	case NodeKind::State:
		return {Signal::Entered, Signal::Exited, Signal::Completed};
	case NodeKind::Dialogue:
		return {Signal::Started, Signal::Completed, Signal::Stopped, Signal::Choice1,
		        Signal::Choice2};
	case NodeKind::Music:
	case NodeKind::Sound:
	case NodeKind::Animation:
	case NodeKind::PickupAnimation:
	case NodeKind::Timer:
		return {Signal::Started, Signal::Completed, Signal::Stopped};
	case NodeKind::Door:
		return {Signal::Opened, Signal::Closed};
	case NodeKind::Npc:
	case NodeKind::Computer:
		return {Signal::Used, Signal::Completed};
	case NodeKind::HealthPickup:
	case NodeKind::Damage:
	case NodeKind::Counter:
	case NodeKind::Checkpoint:
	case NodeKind::Teleport:
	case NodeKind::LevelExit:
	case NodeKind::Start:
	case NodeKind::CampaignSet:
	case NodeKind::Item:
		return {Signal::Completed};
	case NodeKind::CampaignTest:
	case NodeKind::ItemCondition:
		return {Signal::Choice1, Signal::Choice2};
	case NodeKind::Zone:
		return {Signal::Entered};
	case NodeKind::Prop:
		return {};
	default:
		return {Signal::Used};
	}
}
std::vector<Input> nodeInputs(NodeKind k) {
	std::vector<Input> result{Input::Enable, Input::Disable};
	if (k == NodeKind::CampaignSet || k == NodeKind::CampaignTest)
		result.insert(result.begin(), {Input::Activate, Input::Reset});
	else if (k == NodeKind::Counter)
		result.insert(result.begin(), {Input::Progress, Input::Reset});
	else if (k >= NodeKind::HealthPickup && k <= NodeKind::LevelExit)
		result.insert(result.begin(), {Input::Activate, Input::Reset});
	else if (k == NodeKind::Actor)
		result.insert(result.begin(), {Input::Activate, Input::Friendly, Input::Hostile,
		                               Input::Patrol, Input::Hold});
	else if (k == NodeKind::State)
		result.insert(result.begin(),
		              {Input::Activate, Input::Progress, Input::Stop, Input::Reset});
	else if (k == NodeKind::Door)
		result.insert(result.begin(), {Input::Open, Input::Close, Input::Toggle});
	else if (k == NodeKind::Computer)
		result.insert(result.begin(), {Input::Activate, Input::Progress, Input::Reset});
	else if (k == NodeKind::Music || k == NodeKind::Sound || k == NodeKind::Dialogue ||
	         k == NodeKind::Animation || k == NodeKind::PickupAnimation || k == NodeKind::Timer)
		result.insert(result.begin(), {Input::Activate, Input::Stop, Input::Reset});
	else if (k == NodeKind::Button || k == NodeKind::Relay || k == NodeKind::Item)
		result.insert(result.begin(), Input::Activate);
	return result;
}
int nodeIndex(const Module &m, const std::string &id) {
	for (size_t i = 0; i < m.nodes.size(); ++i)
		if (m.nodes[i].id == id)
			return int(i);
	return -1;
}
Node makeNode(const Module &m, NodeKind kind, const Anchor &at) {
	if (m.nodes.size() >= 32)
		throw std::runtime_error("Maximum 32 nodes per module");
	Node n;
	for (int id = 1;; ++id) {
		n.id = "node-" + std::to_string(id);
		const bool referenced =
		    std::any_of(m.nodes.begin(), m.nodes.end(),
			            [&](const auto &other) { return other.requiredItem == n.id; }) ||
		    std::any_of(m.wires.begin(), m.wires.end(),
			            [&](const auto &w) { return w.from == n.id || w.to == n.id; });
		if (nodeIndex(m, n.id) < 0 && !referenced)
			break;
	}
	n.name = nodeName(kind);
	n.audioLoop = kind == NodeKind::Music;
	n.kind = kind;
	n.at = at;
	n.at.x = std::clamp(at.x / 16 * 16, 0, 240);
	n.at.y = std::clamp((at.y + 36) / 72 * 72, 72, 216);
	n.graphX = 24 + int(m.nodes.size() % 3) * 240;
	n.graphY = 24 + int(m.nodes.size() / 3) * 220;
	if (kind == NodeKind::Door) {
		n.height = 72;
		n.visuals[1].loop = false;
	}
	if (kind == NodeKind::Npc) {
		n.height = 48;
		n.text = "Please find the item.";
		n.doneText = "Thank you. The way is open.";
	}
	if (kind == NodeKind::Computer) {
		n.text = "Mission accepted. Complete the connected objectives.";
		n.doneText = "Mission complete. Access granted.";
	}
	if (kind == NodeKind::Zone) {
		n.width = 32;
		n.height = 72;
		n.at.x = std::min(n.at.x, 224);
		n.once = true;
	}
	if (kind == NodeKind::CampaignSet || kind == NodeKind::CampaignTest) {
		n.campaignKey = "objective";
		n.initiallyActive = kind == NodeKind::CampaignSet;
	}
	if (kind == NodeKind::LevelExit)
		n.text = "complete";
	if (kind == NodeKind::Checkpoint || kind == NodeKind::HealthPickup)
		n.once = true;
	if (kind == NodeKind::Dialogue)
		n.text = "Welcome. What will you do next?";
	if (kind == NodeKind::Animation || kind == NodeKind::PickupAnimation) {
		n.visuals[1].loop = false;
		n.visuals[1].frames = 6;
	}
	return n;
}
const char *recipeName(int i) {
	const char *names[] = {"Button + door",
	                       "Keycard + locked door",
	                       "NPC retrieval quest",
	                       "Mission computer + door",
	                       "Area trigger + door",
	                       "Empty module",
	                       "Branching dialogue + door",
	                       "Pickup effect + door",
	                       "State machine airlock"};
	return i >= 0 && i < 9 ? names[i] : "Unknown";
}
Module makeGraph(Project &p, int recipe, const Anchor &at) {
	if (recipe < 0 || recipe >= 9)
		throw std::runtime_error("Unknown module recipe");
	auto m = makeModule(p, Kind::Graph, at);
	m.name = recipeName(recipe);
	auto add = [&](NodeKind k, int x) {
		auto n = makeNode(m, k, {at.room, x, m.a.y});
		m.nodes.push_back(n);
		return n.id;
	};
	auto wire = [&](const std::string &a, Signal s, const std::string &b, Input in) {
		m.wires.push_back({a, b, s, in});
	};
	const int x = std::clamp(m.a.x, 96, 224);
	m.a.x = x;
	if (recipe == 5)
		return m;
	const auto door = add(NodeKind::Door, x);
	if (recipe == 8) {
		auto locked = add(NodeKind::State, x);
		m.nodes.back().name = "Locked";
		m.nodes.back().initiallyActive = true;
		auto open = add(NodeKind::State, x);
		m.nodes.back().name = "Authorized";
		auto button = add(NodeKind::Button, x - 64);
		auto timer = add(NodeKind::Timer, x);
		m.nodes.back().name = "Close delay";
		m.nodes.back().duration = 90;
		wire(button, Signal::Used, locked, Input::Progress);
		wire(locked, Signal::Completed, open, Input::Activate);
		wire(open, Signal::Entered, door, Input::Open);
		wire(open, Signal::Entered, timer, Input::Activate);
		wire(timer, Signal::Completed, locked, Input::Activate);
		wire(locked, Signal::Entered, door, Input::Close);
		wire(locked, Signal::Entered, timer, Input::Stop);
	} else if (recipe == 0) {
		auto b = add(NodeKind::Button, x - 48);
		wire(b, Signal::Used, door, Input::Toggle);
	} else if (recipe == 1 || recipe == 2) {
		auto item = add(NodeKind::Item, x - 96);
		auto npc = add(NodeKind::Npc, x - 48);
		m.nodes.back().requiredItem = item;
		if (recipe == 1) {
			m.nodes.back().name = "Keycard reader";
			m.nodes.back().height = 24;
			m.nodes.back().text = "A keycard is required.";
			m.nodes.back().doneText = "Access granted.";
		}
		wire(npc, Signal::Completed, door, Input::Open);
	} else if (recipe == 3) {
		auto computer = add(NodeKind::Computer, x - 48);
		auto objective = add(NodeKind::Button, x - 96);
		m.nodes.back().name = "Objective switch";
		wire(objective, Signal::Used, computer, Input::Progress);
		wire(computer, Signal::Completed, door, Input::Open);
	} else if (recipe == 6) {
		auto button = add(NodeKind::Button, x - 96);
		auto dialogue = add(NodeKind::Dialogue, x - 48);
		m.nodes.back().speaker = "Access terminal";
		m.nodes.back().text = "Release the door?";
		m.nodes.back().choices = {"Open door", "Leave closed"};
		wire(button, Signal::Used, dialogue, Input::Activate);
		wire(dialogue, Signal::Choice1, door, Input::Open);
	} else if (recipe == 7) {
		auto item = add(NodeKind::Item, x - 96);
		auto effect = add(NodeKind::PickupAnimation, x - 96);
		wire(item, Signal::Completed, effect, Input::Activate);
		wire(effect, Signal::Completed, door, Input::Open);
	} else if (recipe == 4) {
		auto zone = add(NodeKind::Zone, x - 64);
		wire(zone, Signal::Entered, door, Input::Open);
	}
	// Arrange source -> condition -> receiver, independent of room positions.
	m.nodes[0].graphX = m.nodes.size() > 2 ? 504 : 264;
	m.nodes[1].graphX = 24;
	if (m.nodes.size() > 2)
		m.nodes[2].graphX = 264;
	return m;
}
void removeNode(Module &m, size_t i) {
	if (i >= m.nodes.size())
		return;
	auto id = m.nodes[i].id;
	m.nodes.erase(m.nodes.begin() + i);
	m.wires.erase(std::remove_if(m.wires.begin(), m.wires.end(),
	                             [&](const auto &w) { return w.from == id || w.to == id; }),
	              m.wires.end());
	// Retain an unresolved item requirement instead of silently unlocking an NPC.
	// Validation explains how to repair it before Play or Save.
}
void forEachArt(Module &m, const std::function<void(Art &)> &f) {
	f(m.sprite);
	for (auto &a : m.assets)
		f(a);
	for (auto &n : m.nodes)
		for (auto &a : n.visuals)
			f(a.art);
}
void validateGraph(const Project &p, const Module &m, std::vector<Issue> &issues) {
	std::string node, room = m.a.room;
	auto error = [&](const std::string &s) {
		issues.push_back({true, m.name + ": " + s, room, m.id, node});
	};
	if (m.assets.size() > 64)
		error("maximum 64 module assets");
	for (const auto &a : m.assets)
		if (!safeAssetName(a.file) || a.file.empty() || a.x < 0 || a.y < 0 || a.x > 16384 ||
		    a.y > 16384 || a.w < 1 || a.h < 1 || a.w > 16384 || a.h > 16384)
			error("invalid module asset");
	if (m.kind != Kind::Graph) {
		if (!m.nodes.empty() || !m.wires.empty())
			error("node data needs a graph module");
		return;
	}
	if (m.nodes.size() > 32 || m.wires.size() > 128) {
		error("maximum 32 nodes and 128 wires per module");
		return;
	}
	std::set<std::string> ids, initialGroups;
	for (const auto &n : m.nodes) {
		node = n.id;
		room = n.at.room;
		if (n.id.empty() || n.id.size() > 80 ||
		    !std::all_of(n.id.begin(), n.id.end(),
		                 [](unsigned char c) {
			                 return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
							        (c >= '0' && c <= '9') || c == '-' || c == '_';
		                 }) ||
		    !ids.insert(n.id).second)
			error("duplicate or invalid node ID");
		if (int(n.kind) < 0 || int(n.kind) >= int(NodeKind::Count) || int(n.layer) < 0 ||
		    int(n.layer) > 2)
			error("invalid node kind or layer");
		if (n.comments.size() > 2047 || n.name.size() > 160 || n.text.size() > 1024 ||
		    n.doneText.size() > 1024 || n.goal < 1 || n.goal > 32)
			error("invalid node text or goal");
		if (n.speaker.size() > 28 || n.choices[0].size() > 26 || n.choices[1].size() > 26 ||
		    n.choices[0].empty() != n.choices[1].empty() || n.duration < 1 || n.duration > 18000)
			error("invalid speaker, choices (supply both or neither), or timer duration");
		if (n.kind >= NodeKind::Dialogue && n.kind <= NodeKind::Timer && n.initiallyActive)
			error("action nodes must start through Activate");
		if (n.kind == NodeKind::Actor &&
		    (n.patrolLeft < 0 || n.patrolRight > 240 || n.patrolRight - n.patrolLeft < 16 ||
		     n.speed < 1 || n.speed > 4 || n.health < 1 || n.health > 100 ||
		     (n.patrol && (n.at.x < n.patrolLeft || n.at.x > n.patrolRight))))
			error("actor needs patrol bounds 0..240 (at least 16px), spawn within bounds, speed "
			      "1..4 and health 1..100");
		if (n.kind == NodeKind::Checkpoint && p.native.enabled)
			error(
			    "Checkpoint nodes are for custom scenes; native levels use the native save system");
		if (n.kind == NodeKind::LevelExit && (n.text.empty() || n.text.size() > 80))
			error("Level exit needs a route name of 1..80 characters");
		if ((n.kind == NodeKind::HealthPickup || n.kind == NodeKind::Damage) &&
		    (n.health < 1 || n.health > 100))
			error("Health amount must be 1..100");
		if (n.nativeActor < -1 || n.nativeRule < -1 || n.nativeRule > 65535 || n.message < 0 ||
		    n.message > 255 || n.messageFrom < 0 || n.messageFrom > 255)
			error("Invalid native actor event/message binding");
		if (!p.native.enabled && n.nativeActor != -1)
			error("Native actor bindings need a native campaign");
		if (n.kind == NodeKind::State) {
			if (n.stateGroup.empty() || n.stateGroup.size() > 80)
				error("state machine group needs 1..80 characters");
			if (n.initiallyActive && (!n.enabled || !initialGroups.insert(n.stateGroup).second))
				error("each state machine allows one enabled initial state");
		}
		if (n.kind == NodeKind::Dialogue && n.text.empty())
			error("dialogue text cannot be empty");
		if (n.graphX < 0 || n.graphX > 8192 || n.graphY < 0 || n.graphY > 8192)
			error("graph layout is outside 0..8192");
		if (roomIndex(p, n.at.room) < 0 || n.at.x < 0 || n.at.x > 248 || n.at.y < 0 ||
		    n.at.y > 216 || n.width < 1 || n.width > 256 || n.height < 1 || n.height > 216 ||
		    int64_t(n.at.x) + n.width > 256)
			error("node is outside its room");
		if (n.kind == NodeKind::Door && (n.at.x % 16 || n.at.y < 72 || n.at.y % 72 ||
		                                 n.width % 16 || n.height % 72 || n.height > n.at.y))
			error("door needs 16px columns and 72px floor/height alignment");
		for (const auto &a : n.visuals) {
			if (a.frames < 1 || a.frames > 128 || a.fps < 1 || a.fps > 30 || a.art.x < 0 ||
			    a.art.y < 0 || a.art.x > 16384 || a.art.y > 16384 || a.art.w < 1 || a.art.h < 1 ||
			    a.art.w > 16384 || a.art.h > 16384)
				error("invalid animation geometry");
			if (!safeAssetName(a.art.file))
				error("invalid node image path");
		}
		if (!safeAssetName(n.audioFile) || n.audioVolume < 0 || n.audioVolume > 100)
			error("Invalid audio path or volume (0..100)");
		if (n.kind == NodeKind::Music || n.kind == NodeKind::Sound) {
			if (n.initiallyActive)
				error("Audio starts through Activate / On start");
			if (n.audioFile.empty())
				error("Import a WAV or Ogg clip into the audio node");
		}
		if (!validCampaignKey(n.campaignKey) ||
		    ((n.kind == NodeKind::CampaignSet || n.kind == NodeKind::CampaignTest) &&
		     n.campaignKey.empty()))
			error("Campaign key needs 1..64 letters, digits, dot, underscore or hyphen");
		if (!n.requiredItem.empty()) {
			int item = nodeIndex(m, n.requiredItem);
			if ((n.kind != NodeKind::Npc && n.kind != NodeKind::ItemCondition) || item < 0 ||
			    m.nodes[item].kind != NodeKind::Item)
				error("NPC requirement must name an item in this module");
		}
		if (n.kind == NodeKind::Computer) {
			std::set<std::string> sources;
			for (const auto &w : m.wires)
				if (w.to == n.id && w.input == Input::Progress)
					sources.insert(w.from);
			if (int(sources.size()) < n.goal)
				issues.push_back({false,
				                  m.name + ": mission goal exceeds its distinct objective sources",
				                  room, m.id, node});
		}
	}
	std::set<std::tuple<std::string, int, std::string, int>> wires;
	for (const auto &w : m.wires) {
		node = w.from;
		room = m.a.room;
		int a = nodeIndex(m, w.from), b = nodeIndex(m, w.to);
		if (a < 0 || b < 0) {
			error("wire refers to a missing node");
			continue;
		}
		auto signals = nodeSignals(m.nodes[a].kind);
		auto inputs = nodeInputs(m.nodes[b].kind);
		if (std::find(signals.begin(), signals.end(), w.signal) == signals.end() ||
		    std::find(inputs.begin(), inputs.end(), w.input) == inputs.end())
			error("wire uses an incompatible port");
		if (!wires.emplace(w.from, int(w.signal), w.to, int(w.input)).second)
			error("duplicate wire");
	}
}
} // namespace editor

namespace editor {
const char *nodeCategory(NodeKind k) {
	if (k == NodeKind::Music || k == NodeKind::Sound)
		return "Audio / music";
	if (k == NodeKind::Item || k == NodeKind::HealthPickup || k == NodeKind::ItemCondition)
		return "Pickups / inventory";
	if (k == NodeKind::Actor || k == NodeKind::Damage || k == NodeKind::Teleport ||
	    k == NodeKind::Checkpoint || k == NodeKind::LevelExit)
		return "Gameplay";
	if (k == NodeKind::Dialogue || k == NodeKind::Npc || k == NodeKind::Computer)
		return "Story / objectives";
	if (k == NodeKind::Prop || k == NodeKind::Animation || k == NodeKind::PickupAnimation)
		return "Presentation";
	return "Logic / triggers";
}
const char *nodeHelp(NodeKind k) {
	switch (k) {
	case NodeKind::Button:
		return "Press Enter near the button to emit Used. Activate simulates a press. One shot "
		       "prevents repeated uses.";
	case NodeKind::Door:
		return "Open/Close/Toggle change the collision gate and its visual state. A close is "
		       "refused while an actor overlaps. Disable blocks inputs but retains collision. "
		       "Native doors use Actor message bindings.";
	case NodeKind::Npc:
		return "Enter talks to this NPC. Used fires on interaction. Completed fires when the "
		       "required module-local item has been collected, optionally consuming it. "
		       "Before/after text describes the quest.";
	case NodeKind::Item:
		return "A real module-local quest pickup: walking nearby collects it, hides its sprite and "
		       "emits Completed once. Use Has item? or an NPC requirement to test/consume it. It "
		       "is separate from the original campaign inventory.";
	case NodeKind::Computer:
		return "Enter or Activate accepts the mission. Progress counts distinct source nodes, "
		       "including objectives completed before acceptance. Completed fires at the goal. "
		       "Reset clears acceptance and progress.";
	case NodeKind::Zone:
		return "Emits Entered when Conrad crosses into the authored rectangle. One shot limits it "
		       "to the first entry. Connect to damage, teleport, checkpoint or any action.";
	case NodeKind::Relay:
		return "Activate emits Used. Fan an event out to several receivers, or use Enable/Disable "
		       "as a gate. One shot limits repeated activation.";
	case NodeKind::Prop:
		return "Decorative sprite/animation, with no collision or automatic events. Enable/Disable "
		       "controls visibility.";
	case NodeKind::Dialogue:
		return "Activate opens a paginated dialogue. Started fires immediately; Completed after "
		       "dismissal. Two optional choices also emit Choice 1/2. Stop/Disable cancels without "
		       "completion; Reset rearms One shot.";
	case NodeKind::Music:
	case NodeKind::Sound:
		return "Import 16-bit PCM WAV or Ogg Vorbis. Activate starts playback; Stop/Disable "
		       "cancels; Reset rearms One shot. Non-looping clips emit Completed at their "
		       "imported duration; loops run until stopped. Music and Sound use their respective "
		       "volume buses. Multiple nodes mix; wire Stop when changing music. No room falloff.";
	case NodeKind::Animation:
		return "Activate plays the active sprite strip. A non-looping clip emits Completed on its "
		       "final frame. Looping clips require Stop. Reset rearms One shot. This does not "
		       "change actor AI.";
	case NodeKind::PickupAnimation:
		return "An effect visible only while running. Connect a pickup's Completed to Activate. It "
		       "does not grant an item or force Conrad into a pickup pose.";
	case NodeKind::Timer:
		return "Activate starts a 30 Hz countdown. Completed fires at the deadline. Repeated "
		       "Activate while running is ignored; Stop cancels; Reset rearms One shot.";
	case NodeKind::State:
		return "States with the same machine group are exclusive within this module. Activate "
		       "exits the old state before entering this one. Progress emits Completed once while "
		       "active. Stop exits; Reset also clears completion.";
	case NodeKind::Actor:
		return "Custom scenes use crawler AI/art. Native projects bind an existing actor: Activate "
		       "sends a native message. Used/Hurt/Died/Patrol endpoint/Native rule fired expose "
		       "events. Friendly prevents attack AI; Patrol/Hold controls movement.";
	case NodeKind::HealthPickup:
		return "Walking nearby restores Amount health up to five, only while alive and injured. It "
		       "hides and emits Completed once. Activate grants it remotely; Reset restores the "
		       "pickup.";
	case NodeKind::Damage:
		return "Activate subtracts Amount from Conrad's health, never below zero, then emits "
		       "Completed. Wire an Area trigger or timer here. One shot limits it until Reset.";
	case NodeKind::Counter:
		return "Each Progress adds one, including repeated signals from the same source. Completed "
		       "fires once at Goal; Reset clears the count. Use Mission computer when distinct "
		       "objectives are required.";
	case NodeKind::ItemCondition:
		return "Activate tests the selected module-local item: Choice 1 means held; Choice 2 means "
		       "missing. Optionally consume on success. Enable/Disable gates the test.";
	case NodeKind::Checkpoint:
		return "Activate remembers Conrad's current room and position for death restart in a "
		       "custom scene, then emits Completed. Restart resets the puzzle world. Session-only; "
		       "native campaigns keep their original save system.";
	case NodeKind::Teleport:
		return "Activate moves Conrad to this node's room/X/Feet Y and emits Completed. Place the "
		       "destination on a safe floor; connect a separate Area trigger at the departure "
		       "point. Preserves health and puzzle state.";
	case NodeKind::LevelExit:
		return "Activate ends this level through the named Exit route (default complete). Campaign "
		       "edges with that route choose the next stage. Standalone play ends. Use different "
		       "routes for secret exits and forks.";
	case NodeKind::CampaignSet:
		return "Activate writes the chosen true/false value to a named campaign flag, then emits "
		       "Completed. Reset clears it. The key is shared by all modules and levels, saved "
		       "with campaign progress; standalone previews keep it for this session.";
	case NodeKind::CampaignTest:
		return "Activate tests a named campaign flag. Choice 1 means true; Choice 2 means false or "
		       "absent. Connect these outputs to different actions. No flag is changed by testing.";
	case NodeKind::Start:
		return "Emits Completed once when this module begins. Connect it to the first dialogue, "
		       "timer or initial setup action. No Activate input is needed.";
	default:
		return "Unknown node type.";
	}
}
} // namespace editor
