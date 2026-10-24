// SPDX-License-Identifier: GPL-3.0-or-later
#include "weapon_fx.h"
#include "game.h"
#include "fs.h"
#include "decode_mac.h"
#include <memory>
#include "mixer.h"
#include "headless_system.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
Options g_options;
const char *g_caption = "Weapon presentation tests";
using Bytes = std::vector<uint8_t>;
static void be16(Bytes &b, size_t at, int value) {
	b.at(at) = value >> 8;
	b.at(at + 1) = value;
}
static void be32(Bytes &b, size_t at, int value) {
	be16(b, at, value >> 16);
	be16(b, at + 2, value);
}
static uint8_t *owned(const Bytes &bytes) {
	auto *p = static_cast<uint8_t *>(malloc(bytes.size()));
	std::copy(bytes.begin(), bytes.end(), p);
	return p;
}
static bool nativePositions(ResourceType edition) {
	FileSystem files(".");
	HeadlessSystem platform;
	auto game = std::make_unique<Game>(&platform, &files, &files, ".", 0, edition, LANG_EN,
	                                   kWidescreenNone, false, 0);
	auto &res = game->_res;
	res._isSegaCD = edition == kResourceTypeSega;
	Bytes pixels(64);
	for (int y = 2; y <= 4; ++y)
		for (int x = 1; x <= 6; ++x)
			pixels[y * 8 + x] = 1;
	Bytes source, bank;
	if (edition == kResourceTypeMac) {
		const int offset = 8 + 289 * 4;
		source.resize(offset);
		be16(source, 0, 0xC211);
		be16(source, 2, 289);
		for (int i = 0; i < 289; ++i)
			be32(source, 8 + i * 4, offset);
		for (int frame = 287; frame <= 288; ++frame) {
			const size_t start = source.size();
			be32(source, 8 + frame * 4, start);
			source.resize(start + 8);
			be16(source, start, 8);
			be16(source, start + 2, 8);
			be16(source, start + 4, 3);
			be16(source, start + 6, frame == 287 ? 8 : 9);
			for (int y = 0; y < 8; ++y) {
				source.push_back(y ? 0xE8 : 0x68); // C211 literal row.
				source.insert(source.end(), pixels.begin() + y * 8, pixels.begin() + (y + 1) * 8);
			}
			source.push_back(0x41);
		}
		res._mac = new ResourceMac("", nullptr);
	} else {
		source.resize(289 * 2 + 20);
		for (int i = 0; i < 289; ++i)
			be16(source, i * 2, 289 * 2);
		for (int frame = 287; frame <= 288; ++frame) {
			const size_t offset = 289 * 2 + (frame - 287) * 10;
			be16(source, frame * 2, offset);
			source[offset + 1] = 3;
			source[offset + 2] = frame == 287 ? 7 : 8;
			source[offset + 5] = 1;
			source[offset + 7] = 2;
			source[offset + 8] = 1;
		}
		bank.resize(32);
		for (int i = 0; i < 32; ++i)
			bank[i] = pixels[i * 2] << 4 | pixels[i * 2 + 1];
		Bytes directory(6);
		be16(directory, 4, 1);
		res._mbk = owned(directory);
		res._mbkSize = directory.size();
		std::copy(bank.begin(), bank.end(), res._bankData);
		res._bankBuffers[0] = {0, res._bankData};
		res._bankBuffersCount = 1;
		if (res.isSegaCD()) {
			// The CD flash lives in a separate, raw object MBK. The background
			// directory deliberately cannot supply it, catching wrong-bank reads.
			Bytes objects(6);
			be32(objects, 0, 6);
			be16(objects, 4, 0xffff);
			objects.insert(objects.end(), bank.begin(), bank.end());
			res._objectMbk = owned(objects);
			res._objectMbkSize = objects.size();
		}
	}
	res._spc = owned(source);
	res._spcSize = source.size();
	res._numSpc = 289;
	if (res._mac)
		res._mac->rememberData(res._spc, source.size());
	res._pgeNum = 3;
	res._pgeInit[0].data[0] = 1;
	res._pgeInit[1].data[0] = 2;
	game->_pgeLive[0].life = 6;
	auto &effect = game->_pgeLive[2];
	effect.index = 2;
	effect.init_PGE = &res._pgeInit[2];
	game->_currentRoom = effect.room_location = 0;
	const int scale = game->_vid._layerScale;
	for (bool right : {false, true})
		for (int height : {70, 82}) {
			effect.pos_x = 60;
			effect.pos_y = height;
			effect.flags = 12 | (right ? 3 : 0);
			effect.anim_number = 287;
			std::fill_n(game->_vid._frontLayer, game->_vid._layerSize, 7);
			game->_eraseBackground = false;
			if (res.isMac())
				game->_vid.MAC_drawSprite(68, height + 2, res._spc, 287, right, false);
			else
				game->drawObject(res._spc + READ_BE_UINT16(res._spc + 287 * 2), 68, height + 2,
				                 effect.flags);
			int left = 1000, last = -1, top = 1000, bottom = -1;
			for (int y = 0; y < game->_vid._h; ++y)
				for (int x = 0; x < game->_vid._w; ++x)
					if (game->_vid._frontLayer[y * game->_vid._w + x] != 7) {
						left = std::min(left, x);
						last = std::max(last, x);
						top = std::min(top, y);
						bottom = std::max(bottom, y);
					}
			const int referenceX = right ? left : last, referenceY = (top + bottom) / 2;
			// Deliberately different edition offsets, including odd Mac pixels.
			if (referenceX != (res.isMac() ? (right ? 131 : 139) : (right ? 62 : 73)) ||
			    referenceY != (res.isMac() ? height * 2 - 1 : height - 1))
				return false;
			for (auto style : {weapon::Style::Heavy, weapon::Style::Laser}) {
				game->_preferences.pistolStyle = style;
				if (game->effectivePistolStyle() != style)
					return false;
				for (int color = 0; color < 5; ++color) {
					res.selectObjectBank((color & 1) != 0);
					const bool originalBank = res._usingObjectBank;
					const auto *originalMbk = res._mbk;
					game->_preferences.laserColor = weapon::LaserColor(color);
					game->_weaponFx.clear();
					effect.anim_number = 287;
					game->updateConradShot(&effect, 299);
					if (res._usingObjectBank != originalBank || res._mbk != originalMbk)
						return false;
					if (game->_weaponFx.next != 1 || !game->_weaponFx.nativeFlashMatched)
						return false;
					const auto &shot = game->_weaponFx.shots[0];
					if (shot.x != referenceX || shot.y != referenceY ||
					    shot.direction != (right ? 1 : -1))
						return false;
					for (int age = 0; age < 12; ++age) {
						if (age == 1) {
							effect.anim_number = 288;
							game->updateConradShot(&effect, 287);
							if (shot.flashY != referenceY - 1 || shot.y != referenceY)
								return false; // Flash moves; launched beam must not follow.
						}
						std::fill_n(game->_vid._frontLayer, game->_vid._layerSize, 7);
						game->drawWeaponFx();
						const int x = shot.x + shot.direction * (age * 24 + 12) * scale;
						if (style == weapon::Style::Laser && x >= 0 && x < game->_vid._w) {
							for (int y = 0; y < game->_vid._h; ++y) {
								const auto pixel = game->_vid._frontLayer[y * game->_vid._w + x];
								if ((pixel == weapon::PaletteBase + 5) !=
								    (y >= referenceY && y < referenceY + scale))
									return false;
							}
							const auto expected = weapon::palette(weapon::LaserColor(color))[5];
							const auto actual = platform.palette[weapon::PaletteBase + 5];
							if (actual.r != expected.r || actual.g != expected.g ||
							    actual.b != expected.b)
								return false;
						}
						game->_weaponFx.tick();
					}
				}
			}
		}
	// Cosmetic decoding must leave both imported sprite bytes and banks intact.
	if (!std::equal(source.begin(), source.end(), res._spc) ||
	    (!bank.empty() && !std::equal(bank.begin(), bank.end(), res._bankData)))
		return false;
	// Failed geometry does not suppress the original flash or invent a beam.
	game->_weaponFx.clear();
	effect.anim_number = 287;
	if (res._mac)
		res._mac->rememberData(res._spc, 4);
	else
		res._spcSize = 4;
	game->updateConradShot(&effect, 299);
	return !game->_weaponFx.next && !game->_weaponFx.nativeFlashMatched;
}

int main() {
	if (!nativePositions(kResourceTypeDOS) || !nativePositions(kResourceTypeMac) ||
	    !nativePositions(kResourceTypeSega)) {
		std::cerr << "Native DOS/Mac/Sega CD shot geometry or laser height failed\n";
		return 16;
	}
	weapon::State fx;
	std::vector<uint8_t> original(256 * 224, 7), pixels = original;
	fx.fire(weapon::Style::Original, 100, 70, 1, 0, 58);
	fx.draw(pixels.data(), 256, 224, 1, 0);
	if (pixels != original)
		return 1;
	for (auto style : {weapon::Style::Heavy, weapon::Style::Laser}) {
		fx.clear();
		fx.fire(style, 100, 70, 1, 0, 58);
		if (fx.shots[0].x != 100 || fx.shots[0].y != 70 || fx.recoil(99))
			return 2;
		pixels = original;
		fx.draw(pixels.data(), 256, 224, 1, 0);
		if (pixels == original)
			return 3;
		std::vector<uint8_t> hidden(original.size(), 0x87);
		fx.draw(hidden.data(), 256, 224, 1, 0);
		if (!std::all_of(hidden.begin(), hidden.end(), [](uint8_t p) { return p == 0x87; }))
			return 4;
		pixels = original;
		fx.draw(pixels.data(), 256, 224, 1, 1);
		if (pixels != original)
			return 5;
		for (int tick = 0; tick < 16; ++tick)
			fx.tick();
		pixels = original;
		fx.draw(pixels.data(), 256, 224, 1, 0);
		if (pixels != original || fx.recoil(58))
			return 6;
	}
	for (int scale : {1, 2})
		for (int direction : {-1, 1})
			for (int x : {-40, 0, 255, 290}) {
				std::vector<uint8_t> clipped(256 * 224 * scale * scale + 32, 0xF7);
				std::fill(clipped.begin() + 16, clipped.end() - 16, 0);
				fx.clear();
				fx.fire(weapon::Style::Laser, x, 14, direction, 0, 58);
				for (int age = 0; age < 16; ++age) {
					fx.draw(clipped.data() + 16, 256 * scale, 224 * scale, scale, 0);
					fx.tick();
				}
				for (int i = 0; i < 16; ++i)
					if (clipped[i] != 0xF7 || clipped[clipped.size() - 1 - i] != 0xF7)
						return 7;
			}
	// Unsupported edition formats retain their original presentation, including
	// audio, palette writes, transient recoil or native flash suppression.
	FileSystem files(".");
	for (auto edition : {kResourceTypePC98, kResourceTypeSega}) {
		HeadlessSystem platform;
		auto game = std::make_unique<Game>(&platform, &files, &files, ".", 0, edition, LANG_EN,
		                                   kWidescreenNone, false, 0);
		for (auto style : {weapon::Style::Heavy, weapon::Style::Laser}) {
			game->_preferences.pistolStyle = style;
			game->_weaponFx.fire(style, 100, 70, 1, 0, 58);
			std::fill_n(game->_vid._frontLayer, game->_vid._layerSize, 7);
			game->drawWeaponFx();
			if (game->effectivePistolStyle() != weapon::Style::Original ||
			    game->playConradShotSound(&game->_pgeLive[0], 4, 0) ||
			    !std::all_of(game->_vid._frontLayer, game->_vid._frontLayer + game->_vid._layerSize,
			                 [](uint8_t p) { return p == 7; }))
				return 13;
			for (const auto &color : platform.palette)
				if (color.r || color.g || color.b)
					return 14;
		}
	}
	// Bass does not change playback position or duration and survives callback partitioning.
	HeadlessSystem stub;
	Mixer mixer(nullptr, &stub);
	mixer.init();
	std::vector<uint8_t> pcm(2205);
	for (size_t i = 0; i < pcm.size(); ++i)
		pcm[i] = uint8_t(int8_t((i / 20) % 2 ? 70 : -70));
	std::vector<int16_t> dry(5000), wet(5000), split(5000);
	mixer.play(pcm.data(), pcm.size(), 22050, 64);
	mixer.mix(dry.data(), 2500);
	mixer.play(pcm.data(), pcm.size(), 22050, 64, MixerTone::Bass);
	mixer.mix(wet.data(), 2500);
	mixer.play(pcm.data(), pcm.size(), 22050, 64, MixerTone::Bass);
	mixer.mix(split.data(), 333);
	mixer.mix(split.data() + 666, 2167);
	if (dry == wet || wet != split || mixer.isPlaying(pcm.data()))
		return 8;
	const auto &laser = weapon::laserSample();
	if (laser.front() != 0 || laser.back() != 0 || laser.size() < 500 ||
	    std::all_of(laser.begin(), laser.end(), [](uint8_t v) { return v == 0; }))
		return 9;
	// Reusing a currently playing sample must also switch its tone back to dry.
	mixer.play(pcm.data(), pcm.size(), 22050, 64, MixerTone::Bass);
	mixer.mix(split.data(), 10);
	mixer.play(pcm.data(), pcm.size(), 22050, 64);
	mixer.mix(split.data(), 2500);
	if (split != dry)
		return 10;
	// Check the audible bass/treble balance at common output rates, using quiet
	// tones so the overload protection cannot hide an incorrect shelf response.
	struct RateSystem : HeadlessSystem {
		uint32_t rate = 22050;
		uint32_t getOutputSampleRate() override { return rate; }
	};
	for (MixerTone profile : {MixerTone::Bass, MixerTone::DeepBass})
		for (uint32_t rate : {22050, 44100, 48000}) {
			RateSystem platform;
			platform.rate = rate;
			Mixer toneMixer(nullptr, &platform);
			toneMixer.init();
			for (int frequency : {100, 3000}) {
				std::vector<uint8_t> tone(rate / 4);
				for (size_t i = 0; i < tone.size(); ++i)
					tone[i] =
					    uint8_t(int8_t(12 * std::sin(6.283185307179586 * frequency * i / rate)));
				std::vector<int16_t> plain(tone.size() * 2), bass(plain.size());
				toneMixer.play(tone.data(), tone.size(), rate, Mixer::MAX_VOLUME);
				toneMixer.mix(plain.data(), tone.size());
				toneMixer.play(tone.data(), tone.size(), rate, Mixer::MAX_VOLUME, profile);
				toneMixer.mix(bass.data(), tone.size());
				double dryEnergy = 0, wetEnergy = 0;
				for (size_t i = rate / 100; i < tone.size(); ++i) {
					dryEnergy += double(plain[2 * i]) * plain[2 * i];
					wetEnergy += double(bass[2 * i]) * bass[2 * i];
				}
			if (dryEnergy == 0 ||
			    (frequency == 100 ? wetEnergy < (profile == MixerTone::DeepBass ? 40 : 3) * dryEnergy
			                     : wetEnergy > (profile == MixerTone::DeepBass ? 1.6 : .64) * dryEnergy))
				return 11;
			toneMixer.stopAll();
			}
			// Full-scale positive and negative input must stay bounded and retain
			// polarity rather than wrap or clip flat at the 16-bit rails.
			for (int value : {-128, 127}) {
				std::vector<uint8_t> loud(rate / 10, uint8_t(int8_t(value)));
				std::vector<int16_t> output(loud.size() * 2);
				toneMixer.play(loud.data(), loud.size(), rate, Mixer::MAX_VOLUME, profile);
				toneMixer.mix(output.data(), loud.size());
				for (int sample : output)
					if (std::abs(sample) >= 32767 || sample * value <= 0)
						return 12;
				toneMixer.stopAll();
			}
		}
	std::cout << "Weapon presentation, clipping, lifetime and audio checks passed.\n";
}
