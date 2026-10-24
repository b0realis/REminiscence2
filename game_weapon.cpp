// SPDX-License-Identifier: GPL-3.0-or-later
#include "game.h"
#include "systemstub.h"
#include "decode_mac.h"
#include <algorithm>

bool Game::isConradShotEffect(const LivePGE *effect) const {
	if (!_res._pgeNum || !effect)
		return false;
	// Follow the same script links as Conrad's firing message, rather than
	// assuming an actor index (which differs between levels and editions).
	const int pistol = _res._pgeInit[0].data[0];
	if (pistol <= 0 || pistol >= _res._pgeNum)
		return false;
	const int shot = _res._pgeInit[pistol].data[0];
	return shot > 0 && shot < _res._pgeNum && effect == &_pgeLive[shot];
}
weapon::Style Game::effectivePistolStyle() const {
	// Each supported edition supplies its own native flash geometry.
	return (_res.isAmiga() || _res.isDOS() || _res.isMac()) ? _preferences.pistolStyle
	                                                        : weapon::Style::Original;
}

namespace {
struct FlashBounds {
	int left = 32767, right = -32768, top = 32767, bottom = -32768;
	void include(const uint8_t *pixels, int width, int height, int x, int y, bool flip) {
		for (int yy = 0; yy < height; ++yy)
			for (int xx = 0; xx < width; ++xx)
				if (pixels[yy * width + xx]) {
					const int px = x + (flip ? width - 1 - xx : xx);
					left = std::min(left, px);
					right = std::max(right, px);
					top = std::min(top, y + yy);
					bottom = std::max(bottom, y + yy);
				}
	}
};
} // namespace

bool Game::conradFlashOrigin(const LivePGE *effect, int &x, int &y) {
	if (!effect || !_res._spc)
		return false;
	const bool mirror = (effect->flags & 2) != 0;
	FlashBounds bounds;
	if (_res.isMac()) {
		MacImage image;
		if (!_res._mac || !decodeMacImage(_res._spc, _res._mac->decodedSize(_res._spc),
		                                  effect->anim_number, image))
			return false;
		// Keep native 512x448 pixels: dividing by two would lose odd offsets.
		int left = (effect->pos_x + 8) * _vid._layerScale;
		int top = (effect->pos_y + 2) * _vid._layerScale;
		positionMacSprite(image, mirror, left, top);
		bounds.include(image.pixels.data(), image.width, image.height, left, top, mirror);
	} else {
		if ((!_res.isAmiga() && !_res.isDOS()) || effect->anim_number >= _res._numSpc ||
		    size_t(effect->anim_number) * 2 + 2 > _res._spcSize)
			return false;
		const size_t offset = READ_BE_UINT16(_res._spc + effect->anim_number * 2);
		const size_t header = _res.isAmiga() ? 9 : 6;
		if (offset > _res._spcSize || header > _res._spcSize - offset)
			return false;
		const uint8_t *object = _res._spc + offset;
		const int count = object[header - 1];
		if (size_t(count) * 4 > _res._spcSize - offset - header || object[0] >= sizeof(_res._rp))
			return false;
		const int bank = _res._rp[object[0]];
		const uint8_t *data = _res.findBankData(bank);
		if (!data)
			data = _res.loadBankData(bank);
		if (!data)
			return false;
		const int bankSize = _res.getBankDataSize(bank);
		// Use drawObject's edition-specific layout and original sprite offsets.
		// Decoding writes only to scratch; source artwork is never changed.
		const int originX = effect->pos_x + 8 + (mirror ? 1 : -1) * int(int8_t(object[1]));
		const int originY = effect->pos_y + 2 - int(int8_t(object[2]));
		for (int i = 0; i < count; ++i) {
			const uint8_t *tile = object + header + i * 4;
			const int width = (((tile[3] >> 2) & 3) + 1) * 8;
			const int height = ((tile[3] & 3) + 1) * 8;
			const int bankOffset = int(tile[0]) * 32;
			if (bankOffset > bankSize || width * height / 2 > bankSize - bankOffset)
				return false;
			if (_res.isAmiga())
				_vid.AMIGA_decodeSpc(data + bankOffset, width, height, _res._scratchBuffer);
			else
				_vid.DOS_decodeSpc(data + bankOffset, width, height, _res._scratchBuffer);
			const int tileX = originX + (mirror ? -int(tile[1]) - width : int(tile[1]));
			const int tileY = originY + tile[2];
			const bool flip = ((tile[3] & 0x10) != 0) != mirror;
			bounds.include(_res._scratchBuffer, width, height, tileX, tileY, flip);
		}
	}
	if (bounds.left > bounds.right)
		return false;
	// The edge nearest the pistol and the original centerline launch the beam.
	x = (effect->flags & 1) ? bounds.left : bounds.right;
	y = (bounds.top + bounds.bottom) / 2;
	return true;
}

void Game::updateConradShot(const LivePGE *effect, int previousFrame) {
	const auto style = effectivePistolStyle();
	if (style == weapon::Style::Original || !isConradShotEffect(effect) ||
	    (effect->anim_number != 287 && effect->anim_number != 288) || !(effect->flags & 4) ||
	    _pgeLive[0].life <= 0)
		return;
	int x, y;
	if (!conradFlashOrigin(effect, x, y))
		return;
	if (effect->anim_number == 287 && previousFrame != 287)
		_weaponFx.fire(style, x, y, (effect->flags & 1) ? 1 : -1, effect->room_location,
		               _pgeLive[0].obj_type);
	else
		_weaponFx.followFlash(x, y);
}
bool Game::playConradShotSound(const LivePGE *pge, uint8_t sound, uint8_t softVol) {
	if (effectivePistolStyle() == weapon::Style::Original || sound != 4 || !isConradShotEffect(pge))
		return false;
	const int volume = (_res.isMac() ? Mixer::MAX_VOLUME / 2 : Mixer::MAX_VOLUME) >> (2 * softVol);
	if (effectivePistolStyle() == weapon::Style::Laser) {
		// The shared sound table already maps the edition's own laser effect
		// (laser205.spl on Amiga). Keep Flashback's timbre whenever it is present.
		if (_res._sfxList && _res._numSfx > 3) {
			const auto &sfx = _res._sfxList[3];
			if (sfx.data && sfx.len && sfx.freq) {
				_mix.play(sfx.data, sfx.len, sfx.freq, volume, MixerTone::DeepBass);
				return true;
			}
		}
		const auto &sample = weapon::laserSample();
		_mix.play(sample.data(), sample.size(), 22050, volume, MixerTone::DeepBass);
	} else if (_res._sfxList && sound < _res._numSfx) {
		const auto &sfx = _res._sfxList[sound];
		_mix.play(sfx.data, sfx.len, sfx.freq, volume, MixerTone::Bass);
	}
	return true;
}
void Game::drawWeaponFx() {
	if (effectivePistolStyle() == weapon::Style::Original)
		return;
	const auto &colors = weapon::palette(_preferences.laserColor);
	for (size_t i = 0; i < colors.size(); ++i)
		_stub->setPaletteEntry(weapon::PaletteBase + i, &colors[i]);
	_weaponFx.draw(_vid._frontLayer, _vid._w, _vid._h, _vid._layerScale, _currentRoom);
	// The original dirty-block renderer must also erase the previous beam/casing.
	bool visible = false;
	for (const auto &shot : _weaponFx.shots)
		visible |= shot.age <= 12;
	if (visible)
		_vid.fullRefresh();
}
