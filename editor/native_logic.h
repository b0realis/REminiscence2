// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.h"
#include <map>
#include <set>
struct Game;
namespace editor {
void captureNativeLogic(Game &, NativeCampaign &);
void validateNativeLogic(const NativeCampaign &, const NativeCampaign &original);
void installNativeLogic(Game &, const NativeCampaign &);
bool sameNativeLogic(const NativeCampaign &, const NativeCampaign &);
const char *nativeOpcodeName(int);
// Each reference names its storage location. data >= 0 refers to InitPGE.data;
// otherwise rule/operand identifies a literal operand. Dynamic collision and
// inventory targets are reported separately, never guessed from integer values.
enum class NativePortKind { Actor, Room };
struct NativePort {
	NativePortKind kind = NativePortKind::Actor;
	int actor = 0, data = -1, rule = -1, operand = -1, target = -1;
	std::string label;
	bool editable = false;
};
int nativeActorSource(const NativeActor &);
int nativeProgramSource(const NativeCampaign &, int behavior);
std::vector<NativePort> nativePorts(const NativeCampaign &, int actor);
std::set<int> nativeSharedActors(const NativeCampaign &);
std::vector<int> nativePuzzleClosure(const NativeCampaign &, int puzzle);
std::string nativeActorRole(const NativeCampaign &, int actor);
std::vector<std::string> nativeDynamicLinks(const NativeCampaign &, int actor);
// Edits are transactional. These functions leave p untouched on any failure.
void isolateNativeBehavior(Project &, int actor, const NativeCampaign &original);
void reconnectNativePort(Project &, const NativePort &, int target, const NativeCampaign &original);
int copyNativePuzzle(Project &, int puzzle, const std::map<int, int> &rooms, int dx, int dy,
                     const NativeCampaign &original);
void moveNativePuzzle(Project &, int puzzle, const std::map<int, int> &rooms, int dx, int dy,
                      const NativeCampaign &original);
void installNativeActors(Game &, const NativeCampaign &);
std::vector<int> nativeDependencies(const NativeCampaign &, int actor);
} // namespace editor
