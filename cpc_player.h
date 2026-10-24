// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CPC_PLAYER_H__
#define CPC_PLAYER_H__
#include "intern.h"
#include "file.h"
#include <vector>
struct FileSystem;
struct Mixer;
// Transactional bounded decoder, shared by the player and malformed-input tests.
bool decodeCpc(const uint8_t *, size_t, std::vector<int16_t> &, unsigned &rate);
struct CpcPlayer {
	Mixer *_mix;
	FileSystem *_fs;
	std::vector<int16_t> _pcm;
	unsigned _sourceRate = 0, _phase = 0;
	size_t _cursor = 0;
	CpcPlayer(Mixer *, FileSystem *);
	~CpcPlayer();
	bool playTrack(int num);
	void stopTrack();
	void pauseTrack();
	void resumeTrack();
	bool mix(int16_t *, int len);
	static bool mixCallback(void *, int16_t *, int len);
};
#endif
