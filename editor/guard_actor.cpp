// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.h"
#include "game.h"
#include <algorithm>
#include <cstdlib>
namespace editor {
namespace {
enum Phase { Patrol, Aim, Fire, Recover, Hurt, Dead };
}
bool Scene::findGuard(int actor, size_t &module, size_t &node) const {
	if (_borrowed)
		return false;
	for (size_t m = 0; m < _states.size(); ++m)
		for (size_t n = 0; n < _states[m].actors.size(); ++n)
			if (_states[m].actors[n] == actor && _project.modules[m].nodes[n].actorStyle == 1) {
				module = m;
				node = n;
				return true;
			}
	return false;
}
bool Scene::drawsNativeActor(const LivePGE &actor) const {
	size_t m, n;
	return !findGuard(actor.index, m, n);
}
int Scene::acceptsHit(const LivePGE &actor, int message) const {
	size_t m, n;
	if (!findGuard(actor.index, m, n))
		return -1;
	return _states[m].graph.nodes[n].enabled && actor.life > 0 && message >= 1 && message <= 4
	           ? 0xFFFF
			   : 0;
}
void Scene::tickGuard(size_t module, size_t node, LivePGE &actor) {
	auto &g = *_game;
	auto &s = _states[module].graph.nodes[node];
	const auto &n = _project.modules[module].nodes[node];
	// The native collision ray supplies these messages. Never infer a hit from
	// a cosmetic laser, keypress or sprite overlap, and consume a shot only once.
	bool hit = false;
	for (auto *m = g._pge_messagesTable[actor.index]; m; m = m->next_entry)
		hit |= m->src_pge == 2 && m->msg_num >= 1 && m->msg_num <= 4;
	g.pge_clearMessages(actor.index);
	if (!s.enabled)
		return;
	if (hit && actor.life > 0) {
		actor.life = (g._cheats & kCheatOneHitKill) ? 0 : actor.life - 1;
		g._score += 100;
		s.guardPhase = actor.life > 0 ? Hurt : Dead;
		s.guardTicks = actor.life > 0 ? 18 : 24;
	}
	if (actor.life <= 0 && s.guardPhase != Dead) {
		s.guardPhase = Dead;
		s.guardTicks = 24;
	}
	if (actor.room_location != g._currentRoom)
		return;
	s.guardAnim = (s.guardAnim + 1) % 120;
	if (s.guardPhase == Dead) {
		if (s.guardTicks > 0)
			--s.guardTicks;
		return;
	}
	if (s.held && s.guardPhase != Hurt) {
		s.guardPhase = Patrol;
		s.guardTicks = 0;
		return;
	}
	// Pacification is immediate even when a shot was already being aimed.
	if (s.friendly && s.guardPhase != Hurt) {
		s.guardPhase = Patrol;
		s.guardTicks = 0;
	}
	auto &player = g._pgeLive[0];
	const int dx = player.pos_x - actor.pos_x;
	const int row = (actor.pos_y + 2) / 36;
	auto solid = [&](int x, int y) {
		return x < 0 || x >= 256 || y < 0 || y >= 7 || actor.room_location >= 64 ||
		       g._res._ctData[256 + actor.room_location * 112 + y * 16 + x / 16] != 0;
	};
	bool sight = !s.friendly && player.life > 0 && player.room_location == actor.room_location &&
	             std::abs(player.pos_y - actor.pos_y) <= 8 && std::abs(dx) <= 160;
	for (int x = std::min<int>(actor.pos_x, player.pos_x) + 8;
	     sight && x <= std::max<int>(actor.pos_x, player.pos_x) + 8; x += 4)
		if (solid(x, row - 1))
			sight = false;
	if (s.guardPhase == Aim && (!sight || ((actor.flags & 1) != (dx < 0 ? 1 : 0)))) {
		s.guardPhase = Patrol;
		s.guardTicks = 0;
	}
	if (s.guardTicks > 0) {
		--s.guardTicks;
		if (s.guardTicks)
			return;
		if (s.guardPhase == Aim && sight) {
			s.guardPhase = Fire;
			s.guardTicks = 4;
			// Original player rules determine posture vulnerability and damage;
			// pge_sendMessage also preserves invulnerability/No Hit behavior.
			const int message = ((actor.flags ^ player.flags) & 1) ? 3 : 1;
			if (g.col_detectHitCallbackHelper(&player, message))
				g.pge_sendMessage(actor.index, player.index, message);
			g.playSound(0, 0);
			return;
		}
		if (s.guardPhase == Fire || s.guardPhase == Hurt) {
			s.guardPhase = Recover;
			s.guardTicks = 60;
			return;
		}
		s.guardPhase = Patrol;
	}
	if (sight) {
		actor.flags = (actor.flags & ~1) | (dx < 0 ? 1 : 0);
		s.guardPhase = Aim;
		s.guardTicks = 30; // one second of visible warning before every attack
		return;
	}
	if (!s.patrol)
		return;
	const int x = actor.pos_x + ((actor.flags & 1) ? -n.speed : n.speed);
	if (x < n.patrolLeft || x > n.patrolRight || !solid(x + 8, row) || solid(x + 8, row - 1))
		actor.flags ^= 1;
	else
		actor.pos_x = x;
}
void Scene::drawGuards(Image &out) {
	if (_borrowed)
		return;
	for (size_t m = 0; m < _states.size(); ++m)
		for (size_t j = 0; j < _states[m].actors.size(); ++j) {
			const auto &n = _project.modules[m].nodes[j];
			if (n.actorStyle != 1 || _states[m].actors[j] < 0)
				continue;
			const auto &s = _states[m].graph.nodes[j];
			const auto &a = _game->_pgeLive[_states[m].actors[j]];
			if (!s.enabled || a.room_location != _game->_currentRoom)
				continue;
			const int frame = s.guardPhase == Dead   ? (s.guardTicks > 12 ? 6 : 7)
			                  : s.guardPhase == Hurt ? 5
			                  : s.guardPhase == Fire ? 4
			                  : s.guardPhase == Aim  ? 3
			                  : s.guardPhase == Patrol && s.patrol && !s.held
			                      ? 1 + (s.guardAnim / 7) % 2
			                      : 0;
			drawGuard(out, n, frame, a.flags & 1, a.pos_x, a.pos_y + 2, _images);
		}
}
} // namespace editor
