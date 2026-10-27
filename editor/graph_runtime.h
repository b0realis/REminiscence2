// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.h"
#include <deque>
#include <set>
#include <map>
namespace editor {
struct NodeState {
	bool enabled = true, active = false, completed = false, collected = false, inside = false;
	bool friendly = false, patrol = false, held = false;
	int count = 0;
	std::set<int> progress; // distinct contributing nodes, never repeated button presses
	uint64_t changedAt = 0, generation = 0; // invalidates canceled dialogue callbacks
};
enum class TraceKind { Signal, Delivery, Changed, Cycle };
struct GraphTrace {
	uint64_t sequence = 0, tick = 0;
	TraceKind kind = TraceKind::Signal;
	int node = -1, wire = -1;
	Signal signal = Signal::Used;
	Input input = Input::Activate;
	bool enabled = false, active = false, completed = false;
};
struct GraphState {
	std::vector<NodeState> nodes;
	std::map<std::string, bool> localValues; // isolated graph simulator fallback
	bool started = false;
	uint64_t sequence = 0, revision = 0;
	std::deque<GraphTrace> trace; // bounded to 256 events; transient, never project data
	std::deque<size_t> pending;
	std::set<size_t> delivered;
};
GraphState initialGraphState(const Module &);
// One transaction delivers each wire at most once. Cyclic author graphs are
// bounded without recursion, while each independent interaction starts fresh.
class GraphMachine {
public:
	GraphMachine(const Module &, GraphState &, uint64_t tick = 0,
	             std::map<std::string, bool> *shared = nullptr);
	std::function<bool(const Node &)> canClose;
	bool automatic = true; // false retains deliveries for the graph debugger
	void start();
	bool step();                   // exactly one queued wire; same evaluator as normal gameplay
	void signal(int node, Signal); // explicit external event (world/simulator adapter)
	std::string message, notice;
	bool changedDoor = false;
	void use(int node); // local player interaction; dialogue and mission text
	void send(int node, Input, int source = -1);
	void enter(int node);
	void advance(); // timed actions; no player proximity checks
	void finishDialogue(int node, uint64_t generation, int choice = 0);
	std::vector<std::pair<int, uint64_t>> dialogues;
	std::vector<int> actorCommands, worldCommands;
	static bool action(NodeKind);

private:
	const Module &_module;
	GraphState &_state;
	uint64_t _tick;
	std::map<std::string, bool> &_values;
	bool hasItem(const Node &) const;
	void consumeItem(const Node &);
	void trace(TraceKind, int node, int wire = -1, Signal = Signal::Used, Input = Input::Activate);
	void emit(int node, Signal);
	void deliver(int node, Input, int source);
	void complete(int node);
	void drain();
};
} // namespace editor
