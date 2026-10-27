// SPDX-License-Identifier: GPL-3.0-or-later
#include "content.h"
#include "audio.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
namespace editor {
static void anchors(Module &m, const std::function<void(Anchor &, int, int)> &visit) {
	if (m.kind == Kind::Graph && !m.nodes.empty()) {
		for (auto &n : m.nodes)
			visit(n.at, n.width, n.kind == NodeKind::Door ? n.height : 0);
	} else {
		visit(m.a, m.width, m.kind == Kind::Door ? m.height : 0);
		if (m.kind == Kind::Door)
			visit(m.b, 8, 0);
	}
}
void translateModules(Project &p, const std::vector<std::string> &ids, int dx, int dy) {
	std::set<std::string> selection(ids.begin(), ids.end());
	int minX = -32768, maxX = 32767, minY = -32768, maxY = 32767;
	bool door = false;
	for (auto &m : p.modules)
		if (selection.count(m.id)) {
			door |= m.kind == Kind::Door ||
			        std::any_of(m.nodes.begin(), m.nodes.end(),
			                    [](auto &n) { return n.kind == NodeKind::Door; });
			anchors(m, [&](auto &a, int w, int height) {
				minX = std::max(minX, -a.x);
				maxX = std::min(maxX, std::min(248, 256 - w) - a.x);
				minY = std::max(minY, height - a.y);
				maxY = std::min(maxY, 216 - a.y);
			});
		}
	if (minX > maxX || minY > maxY)
		throw std::runtime_error("Fix selected module positions before moving the group");
	dx = std::clamp(dx, minX, maxX);
	dy = std::clamp(dy, minY, maxY);
	if (door) {
		dx = dx / 16 * 16;
		dy = dy / 72 * 72;
	}
	for (auto &m : p.modules)
		if (selection.count(m.id)) {
			anchors(m, [&](auto &a, int, int) {
				a.x += dx;
				a.y += dy;
			});
			if (m.kind == Kind::Graph && !m.nodes.empty())
				m.a = m.nodes.front().at;
		}
}
ModuleClipboard copyModules(const Project &p, const std::vector<std::string> &ids,
                            const std::filesystem::path &root, const std::string &room) {
	ModuleClipboard clipboard;
	clipboard.sourceRoot = root;
	clipboard.originRoom = room;
	for (const auto &m : p.modules)
		if (std::find(ids.begin(), ids.end(), m.id) != ids.end())
			clipboard.modules.push_back(m);
	return clipboard;
}
std::vector<std::string> pasteModules(Project &p, const ModuleClipboard &clipboard,
                                      const std::filesystem::path &root, const std::string &room) {
	if (p.native.enabled)
		throw std::runtime_error(
		    "Native script actors keep their original IDs; paste modules in a custom project");
	if (clipboard.modules.empty())
		return {};
	if (p.modules.size() + clipboard.modules.size() > MaxModules)
		throw std::runtime_error("Maximum 64 modules");
	if (roomIndex(p, room) < 0)
		throw std::runtime_error("Paste needs an existing destination room");
	auto next = p;
	std::map<std::string, std::string> ids, images;
	for (const auto &m : clipboard.modules)
		ids[m.id] = newId(next, "module");
	std::vector<std::string> pasted;
	for (auto m : clipboard.modules) {
		m.id = ids.at(m.id);
		pasted.push_back(m.id);
		m.name = (m.name + " copy").substr(0, 160);
		auto rebase = [&](Anchor &a) {
			if (a.room == clipboard.originRoom)
				a.room = room;
		};
		rebase(m.a);
		rebase(m.b);
		for (auto &n : m.nodes)
			rebase(n.at);
		std::vector<std::string> outputs;
		for (const auto &id : m.outputs)
			if (ids.count(id))
				outputs.push_back(ids.at(id));
		m.outputs = std::move(outputs);
		forEachArt(m, [&](Art &a) {
			if (!a.file.empty() && std::filesystem::absolute(root) !=
			                           std::filesystem::absolute(clipboard.sourceRoot)) {
				if (!images.count(a.file))
					images[a.file] = importImage(assetPath(clipboard.sourceRoot, a.file), root);
				a.file = images.at(a.file);
			}
		});
		copyModuleAudio(m, clipboard.sourceRoot, root);
		next.modules.push_back(std::move(m));
	}
	translateModules(next, pasted, 16, 0);
	for (const auto &issue : validate(next))
		if (issue.error)
			throw std::runtime_error(issue.message);
	p = std::move(next);
	return pasted;
}
} // namespace editor
