// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include "progress_store.h"
#include "context_hints.h"
#include "file.h"
#include "settings.h"
#include "controller_profile.h"
#include "save_catalog.h"
#include "user_notice.h"
#include <SDL_scancode.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <zlib.h>
Options g_options;
const char *g_caption = "progress tests";
static unsigned failures = 0;
static void check(bool good, const char *message) {
	if (!good) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}
static std::vector<uint8_t> bytes(const std::filesystem::path &p) {
	std::ifstream f(p, std::ios::binary);
	return {std::istreambuf_iterator<char>(f), {}};
}
static void write(const std::filesystem::path &p, const std::vector<uint8_t> &v) {
	std::ofstream f(p, std::ios::binary);
	f.write(reinterpret_cast<const char *>(v.data()), v.size());
}
static void checksum(std::vector<uint8_t> &v) {
	const uint32_t crc = crc32(0, v.data(), v.size() - 4);
	for (unsigned i = 0; i < 4; ++i)
		v[v.size() - 4 + i] = uint8_t(crc >> ((3 - i) * 8));
}
static progress::Snapshot fixture() {
	progress::Snapshot s;
	s.entities = 1;
	s.room = 4;
	s.health = 3;
	s.fingerprint = 123;
	s.playTicks = 300;
	File f;
	f.openMemoryBuffer(16384);
	f.writeUint32BE(0x46425356);
	f.writeUint16BE(4);
	char description[32] = "level=1 room=4";
	f.write(description, 32);
	f.writeByte(1);
	f.writeUint32BE(5);
	f.writeUint32BE(UINT32_MAX);
	f.writeUint32BE(UINT32_MAX);
	uint8_t hero[30] = {};
	hero[7] = 4;
	hero[9] = 3;
	hero[13] = hero[14] = hero[15] = 255;
	hero[18] = 4;
	std::fill_n(hero + 22, 4, 255);
	f.write(hero, sizeof(hero));
	std::vector<uint8_t> grid(0x1C00);
	f.write(grid.data(), grid.size());
	f.writeUint16BE(0);
	f.writeUint32BE(11);
	f.writeByte(0);
	f.writeByte(0);
	f.writeUint16BE(1);
	f.writeUint32BE(123);
	f.writeUint32BE(1);
	f.writeUint32BE(0);
	s.state.resize(f.size());
	f.seek(0);
	f.read(s.state.data(), s.state.size());
	auto p = std::make_shared<progress::Preview>();
	p->width = 128;
	p->height = 112;
	p->rgba.resize(128 * 112 * 4);
	for (size_t i = 0; i < p->rgba.size(); ++i)
		p->rgba[i] = uint8_t(i);
	s.preview = p;
	return s;
}
int main(int argc, char **argv) {
	if (argc != 2)
		return 2;
	namespace fs = std::filesystem;
	const fs::path root = fs::u8path(argv[1]);
	fs::remove_all(root);
	fs::create_directories(root);
	const auto s = fixture();
	std::string reason;
	write(root / "fb-0-level1-01.state", {1, 2, 3});
	for (unsigned n = 0; n < 5; ++n)
		check(progress::save(root.u8string(), s, reason), "write rotating autosave");
	auto saves = progress::scan(root.u8string(), 0);
	check(saves.size() == 3 && saves[0].sequence == 5 && saves[2].sequence == 3,
	      "keep exactly three newest generations");
	check(saves[0].state == s.state && saves[0].preview &&
	          saves[0].preview->rgba == s.preview->rgba && saves[0].playTicks == 300,
	      "round trip state, preview and tick clock");
	check(bytes(root / "fb-0-level1-01.state") == std::vector<uint8_t>({1, 2, 3}),
	      "manual saves stay untouched");
	check(progress::scan(root.u8string(), 1).empty(), "edition isolation");
	const auto original = bytes(saves.front().filename);
	const auto damaged = root / "damaged.r2s";
	progress::Snapshot output;
	output.sequence = 99;
	for (size_t cut :
	     {size_t(0), size_t(51), size_t(52), original.size() / 2, original.size() - 1}) {
		write(damaged, {original.begin(), original.begin() + cut});
		check(!progress::read(damaged.u8string(), output, reason) && output.sequence == 99 &&
		          !reason.empty(),
		      "truncation rejected transactionally");
	}
	for (size_t index : {size_t(0), size_t(57), size_t(34), size_t(52 + 51 + 19),
	                     size_t(52 + s.state.size() - 9)}) {
		auto bad = original;
		bad[index] ^= 1;
		checksum(bad);
		write(damaged, bad);
		check(!progress::read(damaged.u8string(), output, reason) && !reason.empty(),
		      "unsupported schema, state or summary mismatch rejected");
	}
	auto bad = original;
	bad[150] ^= 1;
	write(saves[0].filename, bad);
	saves = progress::scan(root.u8string(), 0);
	check(saves.size() == 2 && saves[0].sequence == 4,
	      "CRC corruption falls back to older valid generation");
	check(progress::save(root.u8string(), s, reason),
	      "damaged slot reused without sacrificing older generations");
	auto invalid = s;
	invalid.health = 2;
	check(!progress::save(root.u8string(), invalid, reason), "summary must match state");
	auto preview = std::make_shared<progress::Preview>();
	preview->width = 4096;
	invalid = s;
	invalid.preview = preview;
	check(!progress::save(root.u8string(), invalid, reason), "invalid preview dimensions refused");
	saves = progress::scan(root.u8string(), 0);
	const auto blocked = fs::u8path(saves.back().filename);
	fs::remove(blocked);
	fs::create_directory(blocked);
	check(!progress::save(root.u8string(), s, reason) &&
	          progress::scan(root.u8string(), 0).size() == 2,
	      "failed atomic replacement keeps the other generations");
	std::error_code ec;
	fs::create_symlink(fs::u8path(saves.front().filename), root / "linked.r2s", ec);
	if (!ec)
		check(!progress::read((root / "linked.r2s").u8string(), output, reason),
		      "autosave reader refuses symlinks");
	// New manual bundles must atomically bind metadata, state and preview.
	auto named = s;
	named.label = "Before the lift";
	named.protectedSave = true;
	check(progress::saveManual(root.u8string(), 7, named, reason), "write protected manual bundle");
	const auto manualPath = root / progress::manualFilename(0, 0, 7);
	const auto manualBytes = bytes(manualPath);
	progress::Snapshot manual;
	check(progress::read(manualPath.u8string(), manual, reason) && manual.manual &&
	          manual.protectedSave && manual.label == named.label && manual.state == s.state &&
	          manual.preview->rgba == s.preview->rgba,
	      "manual metadata/state/preview round trip");
	check(!progress::saveManual(root.u8string(), 7, s, reason) && bytes(manualPath) == manualBytes,
	      "quicksave cannot overwrite a protected milestone");
	const auto manualStamp = manual.timestamp;
	check(progress::updateManual(root.u8string(), 0, 0, 7, "Renamed #1", false, reason) &&
	          progress::read(manualPath.u8string(), manual, reason) &&
	          manual.label == "Renamed #1" && !manual.protectedSave && manual.state == s.state &&
	          manual.timestamp == manualStamp,
	      "rename/unlock keeps state and timestamp");
	check(progress::saveManual(root.u8string(), 7, s, reason), "explicit unlock permits overwrite");
	check(progress::saveManual(root.u8string(), 0, s, reason),
	      "checkpoint bundle includes a preview");
	check(!progress::updateManual(root.u8string(), 0, 0, 0, "No", true, reason),
	      "checkpoint cannot be renamed or locked manually");
	check(!progress::validLabel("bad\nname") && !progress::validLabel(std::string(97, 'x')) &&
	          progress::validLabel("A # B / C"),
	      "portable name boundaries");
	auto catalog = saves::scanAll(root.u8string().c_str(), 0);
	check(std::any_of(catalog.begin(), catalog.end(),
	                  [](const auto &v) {
		                  return v.level == 0 && v.slot == 7 && v.modern && v.supported &&
						         v.preview;
	                  }),
	      "all-level catalog finds manual thumbnails");
	for (size_t cut : {size_t(52), size_t(53), manualBytes.size() - 1}) {
		write(damaged, {manualBytes.begin(), manualBytes.begin() + cut});
		check(!progress::read(damaged.u8string(), output, reason),
		      "manual metadata truncation rejected");
	}
	auto corruptName = manualBytes;
	corruptName[53] = 10;
	checksum(corruptName);
	write(damaged, corruptName);
	check(!progress::read(damaged.u8string(), output, reason),
	      "checksum-correct control character refused");
	const auto profile = controller::profilePath(root.u8string(), std::string(32, 'a'), false);
	check(!profile.empty() && controller::profilePath(root.u8string(), "../../x", false).empty() &&
	          profile != controller::profilePath(root.u8string(), std::string(32, 'a'), true),
	      "GUID profile paths are bounded and separate mapped/raw devices");
	settings::Preferences device;
	device.padButtons[0] = 32;
	device.stickDeadzone = 30;
	check(controller::save(profile, device, reason), "save trigger/controller profile");
	settings::Preferences global;
	global.masterVolume = 37;
	global.keys[0] = SDL_SCANCODE_Q;
	check(controller::load(profile, global, reason) && global.padButtons[0] == 32 &&
	          global.stickDeadzone == 30 && global.masterVolume == 37 &&
	          global.keys[0] == SDL_SCANCODE_Q,
	      "per-device import changes only pad/stick preferences");
	check(controller::load(profile + "missing", global, reason) && global.padButtons[0] == 32,
	      "unseen device retains global bindings");
	UserNotice notice;
	notice.push("Disk full", true, UINT32_MAX - 100);
	notice.push("Autosave stored", false, UINT32_MAX - 50);
	check(notice.visible(500) && notice.text == "Disk full", "error priority and clock wrap");
	check(!notice.visible(8000), "error expires");
	check(notice.opacity(uint32_t(notice.started + 7625)) == 0.5f &&
	          notice.opacity(uint32_t(notice.started + 8000)) == 0,
	      "errors fade at the end of their longer lifetime across clock wrap");
	JournalNotice journalNotice;
	const uint32_t noticeStart = UINT32_MAX - 100;
	journalNotice.update(1, noticeStart);
	for (uint32_t elapsed = 0; elapsed <= 4000; elapsed += 25) {
		journalNotice.update(1, noticeStart + elapsed);
		check(journalNotice.toast.visible(noticeStart + elapsed) == (elapsed < 3500),
		      "publishing an unread entry every frame does not restart its toast");
	}
	check(journalNotice.toast.opacity(noticeStart + 2750) == 1 &&
	          journalNotice.toast.opacity(noticeStart + 3125) == 0.5f &&
	          journalNotice.toast.opacity(noticeStart + 3500) == 0,
	      "journal holds then fades over its final 750ms across clock wrap");
	check(journalNotice.unread == 1, "toast expiration does not mark the journal read");
	journalNotice.update(3, noticeStart + 5000);
	check(journalNotice.toast.opacity(noticeStart + 5000) == 1,
	      "another entry notifies even while the previous entry remains unread");
	journalNotice.update(2, noticeStart + 6000);
	check(journalNotice.toast.started == noticeStart + 5000,
	      "reading one entry does not restart the remaining entry's toast");
	journalNotice.update(0, noticeStart + 6100);
	check(!journalNotice.toast.visible(noticeStart + 6100),
	      "reading all entries or disabling notifications dismisses the toast");
	journal::State reached;
	reached.chapter(0);
	reached.chapter(6);
	const auto notebook = journal::objectives(reached, 6);
	check(notebook.size() == 2 && notebook.back().current && !notebook.front().current &&
	          notebook[0].level == 0 && notebook[1].level == 6,
	      "notebook exposes only reached chapters, even after a jump");
	hints::State hints;
	check(hints.update(30, false, true, true, true) == hints::Kind::None && !hints.seen,
	      "disabled hints do not consume reminders");
	check(hints.update(30, true, false, false, false) == hints::Kind::Movement,
	      "movement reminder after first second");
	check(hints.update(30, true, false, false, true) == hints::Kind::Movement,
	      "paused ticks keep the same reminder");
	check(hints.update(270, true, false, false, false) == hints::Kind::None,
	      "hint expires after eight active seconds");
	check(hints.update(570, true, true, true, true) == hints::Kind::LowHealth,
	      "cooldown and low-health priority");
	check(hints.update(1, true, false, false, false) == hints::Kind::None,
	      "rewind resets timer without replaying seen reminder");
	settings::Preferences p;
	p.keys[size_t(settings::Binding::Action)] = SDL_SCANCODE_K;
	p.padButtons[0] = 9;
	const auto text = hints::text(hints::Kind::Movement, p);
	check(text.find("K / pad LB") != std::string::npos && text.find("Shift") == std::string::npos,
	      "hints follow keyboard and pad remaps");
	p.joystickButtons[0] = 7;
	check(hints::text(hints::Kind::Movement, p, true).find("K / joystick button 8") !=
	          std::string::npos,
	      "raw-device hints use the independent numbered profile");
	p.diskAutosave = false;
	p.diskAutosaveSeconds = 333;
	p.contextualHints = true;
	check(settings::save((root / "preferences.cfg").u8string(), p, reason),
	      "persist recovery and hint settings");
	settings::Preferences loaded;
	check(settings::load((root / "preferences.cfg").u8string(), loaded, reason) && loaded == p,
	      "new profile fields round trip");
	p.diskAutosaveSeconds = 14;
	check(!settings::validate(p, reason), "autosave interval bounds");
	std::cout << "Progress and hint checks: " << failures << " failures\n";
	return failures ? 1 : 0;
}
