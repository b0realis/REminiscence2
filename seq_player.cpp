
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "file.h"
#include "decode_seq.h"
#include "fs.h"
#include "mixer.h"
#include "seq_player.h"
#include "systemstub.h"
#include "util.h"

bool SeqDemuxer::open(File *f) {
	_f = f;
	_failed = false;
	memset(_buffers, 0, sizeof(_buffers));
	const auto size = _f->size();
	if (size < 316 || size > 1024u * 1024 * 1024)
		return false;
	_fileSize = size;
	memset(_buffers, 0, sizeof(_buffers));
	_frameOffset = 0;
	if (!readHeader()) {
		close();
		return false;
	}
	return true;
}

void SeqDemuxer::close() {
	_f = 0;
	for (int i = 0; i < kBuffersCount; ++i) {
		free(_buffers[i].data);
	}
	memset(_buffers, 0, sizeof(_buffers));
}

bool SeqDemuxer::readHeader() {
	for (int i = 0; i < 256; i += 4) {
		if (_f->readUint32LE() != 0) {
			return false;
		}
	}
	for (int i = 0; i < kBuffersCount; ++i) {
		const int size = _f->readUint16LE();
		if (size != 0) {
			_buffers[i].size = 0;
			_buffers[i].avail = size;
			_buffers[i].data = (uint8_t *)malloc(size);
			if (!_buffers[i].data) {
				error("Unable to allocate %d bytes for SEQ buffer %d", size, i);
			}
		}
	}
	for (int i = 1; i <= 100; ++i) {
		if (!readFrameData())
			return false;
	}
	return !_f->ioErr();
}

bool SeqDemuxer::readFrameData() {
	_frameOffset += kFrameSize;
	if (_frameOffset > _fileSize - 16) {
		return false;
	}
	_f->seek(_frameOffset);
	_audioDataOffset = _f->readUint16LE();
	_paletteDataOffset = _f->readUint16LE();
	uint8_t num[4];
	for (int i = 0; i < 4; ++i) {
		num[i] = _f->readByte();
	}
	uint16_t offsets[4];
	for (int i = 0; i < 4; ++i) {
		offsets[i] = _f->readUint16LE();
	}
	for (int i = 0; i < 3; ++i) {
		if (offsets[i] != 0) {
			int e = i + 1;
			while (e < 3 && offsets[e] == 0) {
				++e;
			}
			if (!fillBuffer(num[i + 1], offsets[i], offsets[e] - offsets[i])) {
				_failed = true;
				return false;
			}
		}
	}
	if (num[0] != 255) {
		if (num[0] >= kBuffersCount || !_buffers[num[0]].data) {
			_failed = true;
			return false;
		}
		_videoData = num[0];
	} else {
		_videoData = -1;
	}
	_failed = _f->ioErr();
	return !_failed;
}

bool SeqDemuxer::fillBuffer(int num, int offset, int size) {
	if (num < 0 || num >= kBuffersCount || offset < 16 || size < 0 || offset > kFrameSize ||
	    size > kFrameSize - offset || offset > _fileSize - _frameOffset ||
	    size > _fileSize - _frameOffset - offset || !_buffers[num].data ||
	    size > _buffers[num].avail - _buffers[num].size)
		return false;
	_f->seek(_frameOffset + offset);
	if (_f->read(_buffers[num].data + _buffers[num].size, size) != unsigned(size))
		return false;
	_buffers[num].size += size;
	return !_f->ioErr();
}

void SeqDemuxer::clearBuffer(int num) {
	_buffers[num].size = 0;
}

bool SeqDemuxer::readPalette(uint8_t *dst) {
	if (_paletteDataOffset < 16 || _paletteDataOffset > kFrameSize - 768 ||
	    _paletteDataOffset > _fileSize - _frameOffset - 768)
		return false;
	_f->seek(_frameOffset + _paletteDataOffset);
	return _f->read(dst, 768) == 768 && !_f->ioErr();
}

bool SeqDemuxer::readAudio(int16_t *dst) {
	if (_audioDataOffset < 16 || _audioDataOffset > kFrameSize - kAudioBufferSize * 2 ||
	    _audioDataOffset > _fileSize - _frameOffset - kAudioBufferSize * 2)
		return false;
	_f->seek(_frameOffset + _audioDataOffset);
	for (int i = 0; i < kAudioBufferSize; ++i)
		dst[i] = _f->readUint16BE();
	return !_f->ioErr();
}

SeqPlayer::SeqPlayer(SystemStub *stub, Mixer *mixer)
	: _stub(stub), _buf(0), _mix(mixer) {
}

SeqPlayer::~SeqPlayer() {
}

bool SeqPlayer::play(File *f) {
	_completed = false;
	bool presented = false, valid = true, skipped = false;
	if (_demux.open(f)) {
		uint8_t palette[256 * 3];
		_stub->getPalette(palette, 256);
		_mix->setPremixHook(mixCallback, this);
		memset(_buf, 0, 256 * 224);
		bool clearScreen = true;
		while (true) {
			const uint32_t nextFrameTimeStamp = _stub->getTimeStamp() + 1000 / 25;
			_stub->processEvents();
			if (_stub->_pi.quit || _stub->_pi.backspace) {
				skipped = _stub->_pi.backspace;
				_stub->_pi.backspace = false;
				break;
			}
			if (!_demux.readFrameData()) {
				valid = !_demux._failed;
				break;
			}
			if (_demux._audioDataOffset != 0) {
				std::array<int16_t, SeqDemuxer::kAudioBufferSize> audio{};
				if (!_demux.readAudio(audio.data())) {
					valid = false;
					break;
				}
				LockAudioStack lock(_stub);
				if (_audioCount == _audioBuffers.size()) {
					valid = false;
					break;
				}
				_audioBuffers[_audioWrite] = audio;
				_audioWrite = (_audioWrite + 1) % _audioBuffers.size();
				++_audioCount;
				if (_audioCount >= kSoundPreloadSize)
					_audioReady = true;
			}
			if (_demux._paletteDataOffset != 0) {
				uint8_t buf[256 * 3];
				if (!_demux.readPalette(buf)) {
					valid = false;
					break;
				}
				for (int i = 0; i < 256 * 3; ++i) {
					buf[i] = (buf[i] << 2) | (buf[i] >> 4);
				}
				_stub->setPalette(buf, 256);
			}
			if (_demux._videoData != -1) {
				const int y0 = (224 - kVideoHeight) / 2;
				const auto &buffer = _demux._buffers[_demux._videoData];
				if (!decodeSeqFrame(buffer.data, buffer.size, _buf, 256 * 224)) {
					valid = false;
					break;
				}
				_demux.clearBuffer(_demux._videoData);
				presented = true;
				if (clearScreen) {
					clearScreen = false;
					_stub->copyRect(0, 0, kVideoWidth, 224, _buf, 256);
				} else {
					_stub->copyRect(0, y0, kVideoWidth, kVideoHeight, _buf, 256);
				}
				_stub->updateScreen(0);
			}
			const int diff = nextFrameTimeStamp - _stub->getTimeStamp();
			if (diff > 0) {
				_stub->sleep(diff);
			}
		}
		// restore level palette
		_stub->setPalette(palette, 256);
		_mix->setPremixHook(0, 0);
		_demux.close();
		LockAudioStack lock(_stub);
		_audioRead = _audioWrite = _audioCount = _audioOffset = _audioPhase = 0;
		_audioReady = false;
	}
	_completed = valid && presented && !skipped && !_stub->_pi.quit;
	return valid && (presented || skipped || _stub->_pi.quit);
}

bool SeqPlayer::mix(int16_t *buf, int samples) {
	if (!_audioReady)
		return true;
	const unsigned rate = _mix->getSampleRate();
	if (!rate)
		return false;
	// SEQ PCM is 22050 Hz mono regardless of the negotiated device rate.
	// Fractional advancement preserves duration at 8/44.1/48/192 kHz outputs.
	for (int i = 0; i < samples && _audioCount; ++i) {
		const int16_t sample = _audioBuffers[_audioRead][_audioOffset];
		*buf++ = sample;
		*buf++ = sample;
		_audioPhase += 22050;
		unsigned advance = _audioPhase / rate;
		_audioPhase %= rate;
		while (advance && _audioCount) {
			const unsigned count =
			    MIN(advance, unsigned(SeqDemuxer::kAudioBufferSize) - _audioOffset);
			_audioOffset += count;
			advance -= count;
			if (_audioOffset == SeqDemuxer::kAudioBufferSize) {
				_audioOffset = 0;
				_audioRead = (_audioRead + 1) % _audioBuffers.size();
				--_audioCount;
			}
		}
	}
	return true;
}

bool SeqPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return ((SeqPlayer *)param)->mix(buf, len);
}

