// SPDX-License-Identifier: GPL-3.0-or-later
#include "graph_runtime.h"
#include <stdexcept>
namespace editor {
GraphState initialGraphState(const Module &m) {
	GraphState state;
	state.revision = moduleRevision(m);
	for (const auto &n : m.nodes) {
		NodeState s;
		s.enabled = n.enabled;
		s.friendly = n.friendly;
		s.patrol = n.patrol;
		s.active =
		    n.kind != NodeKind::State && n.kind != NodeKind::CampaignSet && n.initiallyActive;
		state.nodes.push_back(s);
	}
	return state;
}
GraphMachine::GraphMachine(const Module &m, GraphState &s, uint64_t tick,
                           std::map<std::string, bool> *shared)
    : _module(m), _state(s), _tick(tick), _values(shared ? *shared : s.localValues) {
	if (m.nodes.size() != s.nodes.size())
		throw std::runtime_error("Graph state does not match module");
}
void GraphMachine::trace(TraceKind kind, int node, int wire, Signal signal, Input input) {
	const auto &s = _state.nodes.at(node);
	_state.trace.push_back({++_state.sequence, _tick, kind, node, wire, signal, input, s.enabled,
	                        s.active, s.completed});
	if (_state.trace.size() > 256)
		_state.trace.pop_front();
}
void GraphMachine::emit(int node, Signal signal) {
	trace(TraceKind::Signal, node, -1, signal);
	for (size_t i = 0; i < _module.wires.size(); ++i) {
		const auto &w = _module.wires[i];
		if (w.from == _module.nodes[node].id && w.signal == signal) {
			if (_state.delivered.insert(i).second)
				_state.pending.push_back(i);
			else
				trace(TraceKind::Cycle, node, int(i), signal);
		}
	}
}
bool GraphMachine::step() {
	if (_state.pending.empty())
		return false;
	const auto index = _state.pending.front();
	_state.pending.pop_front();
	const auto &w = _module.wires.at(index);
	const int node = nodeIndex(_module, w.to);
	trace(TraceKind::Delivery, node, int(index), w.signal, w.input);
	deliver(node, w.input, nodeIndex(_module, w.from));
	trace(TraceKind::Changed, node, int(index), w.signal, w.input);
	if (_state.pending.empty())
		_state.delivered.clear();
	return true;
}
void GraphMachine::drain() {
	if (automatic)
		while (step()) {
		}
	if (_state.pending.empty())
		_state.delivered.clear();
}
void GraphMachine::signal(int node, Signal signal) {
	if (node < 0 || node >= int(_state.nodes.size()) || !_state.nodes[node].enabled)
		return;
	if (_module.nodes[node].kind == NodeKind::Actor && signal == Signal::Died)
		_state.nodes[node].completed = true;
	emit(node, signal);
	drain();
}
void GraphMachine::start() {
	if (_state.started)
		return;
	_state.started = true;
	for (int i = 0; i < int(_module.nodes.size()); ++i) {
		const auto &n = _module.nodes[i];
		if (n.kind == NodeKind::Start && _state.nodes[i].enabled) {
			complete(i);
		}
		if (n.kind == NodeKind::State && n.initiallyActive)
			deliver(i, Input::Activate, -1);
	}
	drain();
}
void GraphMachine::complete(int i) {
	auto &s = _state.nodes[i];
	if (s.completed)
		return;
	s.completed = true;
	s.active = true;
	s.changedAt = _tick;
	emit(i, Signal::Completed);
}
bool GraphMachine::action(NodeKind k) {
	return k == NodeKind::Music || k == NodeKind::Sound || k == NodeKind::Dialogue ||
	       k == NodeKind::Animation || k == NodeKind::PickupAnimation || k == NodeKind::Timer;
}
bool GraphMachine::hasItem(const Node &n) const {
	const int item = nodeIndex(_module, n.requiredItem);
	const auto key = !n.campaignKey.empty() ? n.campaignKey
	                 : item >= 0            ? _module.nodes[item].campaignKey
	                                        : std::string{};
	if (!key.empty()) {
		const auto it = _values.find("item:" + key);
		return it != _values.end() && it->second;
	}
	return item >= 0 && _state.nodes[item].collected;
}
void GraphMachine::consumeItem(const Node &n) {
	const int item = nodeIndex(_module, n.requiredItem);
	const auto key = !n.campaignKey.empty() ? n.campaignKey
	                 : item >= 0            ? _module.nodes[item].campaignKey
	                                        : std::string{};
	if (!key.empty())
		_values["item:" + key] = false;
	if (item >= 0)
		_state.nodes[item].collected = false;
}
void GraphMachine::deliver(int i, Input input, int source) {
	if (i < 0 || i >= int(_state.nodes.size()))
		return;
	const auto &n = _module.nodes[i];
	auto &s = _state.nodes[i];
	if (input == Input::Enable || input == Input::Disable) {
		if (input == Input::Disable && (action(n.kind) || n.kind == NodeKind::State) && s.active) {
			s.active = false;
			++s.generation;
			emit(i, n.kind == NodeKind::State ? Signal::Exited : Signal::Stopped);
		}
		s.enabled = input == Input::Enable;
		return;
	}
	if (!s.enabled)
		return;
	if (n.kind == NodeKind::CampaignSet) {
		if (input == Input::Activate || input == Input::Reset) {
			_values["flag:" + n.campaignKey] = input == Input::Activate && n.initiallyActive;
			s.completed = false;
			complete(i);
			s.active = _values["flag:" + n.campaignKey];
		}
	} else if (n.kind == NodeKind::CampaignTest) {
		if (input == Input::Activate) {
			const auto it = _values.find("flag:" + n.campaignKey);
			s.active = it != _values.end() && it->second;
			emit(i, s.active ? Signal::Choice1 : Signal::Choice2);
		}
	} else if (n.kind == NodeKind::Counter) {
		if (input == Input::Reset) {
			s.count = 0;
			s.active = s.completed = false;
		}
		if (input == Input::Progress && !s.completed && ++s.count >= n.goal)
			complete(i);
	} else if (n.kind == NodeKind::ItemCondition) {
		if (input == Input::Reset)
			s.active = s.completed = false;
		if (input == Input::Activate) {
			const bool has = hasItem(n);
			s.active = has;
			if (has && n.consumeItem)
				consumeItem(n);
			emit(i, has ? Signal::Choice1 : Signal::Choice2);
		}
	} else if (n.kind >= NodeKind::HealthPickup && n.kind <= NodeKind::LevelExit) {
		if (input == Input::Reset)
			s.active = s.completed = s.collected = false;
		if (input == Input::Activate &&
		    !(s.completed &&
		      (n.once || n.kind == NodeKind::HealthPickup || n.kind == NodeKind::LevelExit))) {
			worldCommands.push_back(i);
			s.collected = n.kind == NodeKind::HealthPickup;
			s.completed = s.active = true;
			s.changedAt = _tick;
			emit(i, Signal::Completed);
		}
	} else if (n.kind == NodeKind::Actor) {
		if (input == Input::Activate)
			actorCommands.push_back(i);
		if (input == Input::Friendly || input == Input::Hostile)
			s.friendly = input == Input::Friendly;
		if (input == Input::Patrol || input == Input::Hold) {
			s.patrol = input == Input::Patrol;
			s.held = input == Input::Hold;
		}
		s.changedAt = _tick;
	} else if (n.kind == NodeKind::State) {
		if (input == Input::Activate && !s.active) {
			// Exit every sibling before entering. Delivery is queued, so observers
			// never see two active states in the same machine, even with cycles.
			for (int j = 0; j < int(_module.nodes.size()); ++j) {
				const auto &other = _module.nodes[j];
				auto &old = _state.nodes[j];
				if (other.kind == NodeKind::State && other.stateGroup == n.stateGroup &&
				    old.active) {
					old.active = false;
					old.changedAt = _tick;
					emit(j, Signal::Exited);
				}
			}
			s.active = true;
			s.completed = false;
			s.changedAt = _tick;
			++s.generation;
			emit(i, Signal::Entered);
		} else if (input == Input::Stop || input == Input::Reset) {
			const bool wasActive = s.active;
			s.active = false;
			s.changedAt = _tick;
			if (input == Input::Reset)
				s.completed = false;
			if (wasActive)
				emit(i, Signal::Exited);
		} else if (input == Input::Progress && s.active && !s.completed) {
			s.completed = true;
			emit(i, Signal::Completed);
		}
	} else if (action(n.kind)) {
		if (input == Input::Stop || input == Input::Reset) {
			const bool running = s.active;
			s.active = false;
			++s.generation;
			if (input == Input::Reset)
				s.completed = false;
			if (running)
				emit(i, Signal::Stopped);
		} else if (input == Input::Activate && !s.active && !(n.once && s.completed)) {
			s.active = true;
			s.completed = false;
			s.changedAt = _tick;
			++s.generation;
			if (n.kind == NodeKind::Dialogue)
				dialogues.emplace_back(i, s.generation);
			emit(i, Signal::Started);
		}
	} else if (n.kind == NodeKind::Door) {
		if (input != Input::Open && input != Input::Close && input != Input::Toggle)
			return;
		const bool next = input == Input::Toggle ? !s.active : input == Input::Open;
		if (next == s.active || (!next && canClose && !canClose(n)))
			return;
		changedDoor = true;
		s.active = next;
		s.changedAt = _tick;
		emit(i, next ? Signal::Opened : Signal::Closed);
	} else if (n.kind == NodeKind::Computer) {
		if (input == Input::Reset) {
			s.active = s.completed = false;
			s.progress.clear();
			s.changedAt = _tick;
			return;
		}
		if (input == Input::Activate && !s.active) {
			s.active = true;
			s.changedAt = _tick;
			emit(i, Signal::Used);
		}
		if (input == Input::Progress && source >= 0 && source < int(_state.nodes.size()))
			s.progress.insert(source);
		if (s.active && int(s.progress.size()) >= n.goal && !s.completed) {
			complete(i);
			if (!message.empty() && !n.doneText.empty())
				message += "\n\n";
			message += n.doneText;
		}
	} else if (n.kind == NodeKind::Item && input == Input::Activate) {
		if (!s.completed) {
			s.collected = true;
			if (!n.campaignKey.empty())
				_values["item:" + n.campaignKey] = true;
			complete(i);
			notice = "Collected: " + n.name;
		}
	} else if ((n.kind == NodeKind::Button || n.kind == NodeKind::Relay) &&
	           input == Input::Activate) {
		if (n.once && s.completed)
			return;
		s.active = s.completed = true;
		s.changedAt = _tick;
		emit(i, Signal::Used);
	}
}
void GraphMachine::send(int i, Input input, int source) {
	message.clear();
	notice.clear();
	deliver(i, input, source);
	if (i >= 0 && i < int(_state.nodes.size()))
		trace(TraceKind::Changed, i, -1, Signal::Used, input);
	drain();
}
void GraphMachine::use(int i) {
	message.clear();
	notice.clear();
	if (i < 0 || i >= int(_state.nodes.size()) || !_state.nodes[i].enabled)
		return;
	const auto &n = _module.nodes[i];
	auto &s = _state.nodes[i];
	if (n.kind == NodeKind::Actor) {
		emit(i, Signal::Used);
	} else if (n.kind == NodeKind::Npc) {
		message = s.completed ? n.doneText : n.text;
		if (!s.completed) {
			emit(i, Signal::Used);
			if ((n.requiredItem.empty() && n.campaignKey.empty()) || hasItem(n)) {
				if (n.consumeItem)
					consumeItem(n);
				complete(i);
				message = n.doneText;
			}
		}
	} else if (n.kind == NodeKind::Computer) {
		message = s.completed ? n.doneText : n.text;
		deliver(i, Input::Activate, -1);
	} else
		deliver(i, Input::Activate, -1);
	drain();
}
void GraphMachine::enter(int i) {
	message.clear();
	notice.clear();
	if (i < 0 || i >= int(_state.nodes.size()))
		return;
	const auto &n = _module.nodes[i];
	auto &s = _state.nodes[i];
	if (n.kind != NodeKind::Zone || !s.enabled || (n.once && s.completed))
		return;
	s.active = s.completed = true;
	s.changedAt = _tick;
	emit(i, Signal::Entered);
	drain();
}
// Snapshot due actions before delivery: a completion may restart another action.
// Its new generation must not complete using this tick's stale deadline.
void GraphMachine::advance() {
	start();
	std::vector<std::pair<int, uint64_t>> due;
	for (size_t i = 0; i < _module.nodes.size(); ++i) {
		const auto &n = _module.nodes[i];
		const auto &s = _state.nodes[i];
		if (!s.enabled || !s.active || !action(n.kind) || n.kind == NodeKind::Dialogue)
			continue;
		const auto &clip = n.visuals[1].art.file.empty() && !n.visuals[0].art.file.empty()
		                       ? n.visuals[0]
		                       : n.visuals[1];
		const bool audio = n.kind == NodeKind::Music || n.kind == NodeKind::Sound;
		if (audio ? n.audioLoop : (n.kind != NodeKind::Timer && clip.loop))
			continue;
		const uint64_t duration = (n.kind == NodeKind::Timer || audio)
		                              ? n.duration
		                              : (clip.frames * 30 + clip.fps - 1) / clip.fps;
		if (_tick - s.changedAt >= duration)
			due.emplace_back(int(i), s.generation);
	}
	for (const auto &[i, generation] : due) {
		auto &s = _state.nodes[i];
		if (!s.active || s.generation != generation)
			continue;
		s.active = false;
		s.completed = true;
		emit(i, Signal::Completed);
		drain();
	}
}
void GraphMachine::finishDialogue(int i, uint64_t generation, int choice) {
	if (i < 0 || i >= int(_state.nodes.size()) || _module.nodes[i].kind != NodeKind::Dialogue)
		return;
	auto &s = _state.nodes[i];
	if (!s.enabled || !s.active || s.generation != generation)
		return;
	s.active = false;
	s.completed = true;
	if (!_module.nodes[i].choices[0].empty())
		emit(i, choice == 1 ? Signal::Choice2 : Signal::Choice1);
	emit(i, Signal::Completed);
	drain();
}
} // namespace editor
