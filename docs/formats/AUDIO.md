<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Audio formats and routing

[Index](README.md) · [Player music mapping](../../manual.md#music-sources)

The mixer outputs signed 16-bit stereo at the negotiated device rate. Imported
audio is read-only. Effects, voices, music and movie PCM have different resource
paths; changing the music source does not substitute dialogue or effects.
See [mixer.cpp](../../mixer.cpp) for priority, premix ownership and source fallback.

## DOS FIB and PC-98 SFX.PAQ

`GLOBAL.FIB`: u16le effect count, then six-byte records (u32le payload offset,
u16le encoded length). A nonempty payload starts with one signed 8-bit sample;
remaining bytes encode two deltas, high nibble first. Delta lookup:

```text
-34 -21 -13 -8 -5 -3 -2 -1 0 1 2 3 5 8 13 21
```

Accumulate each delta, output clipped signed 8 samples; decoded length is
`encodedLength*2 - 1`. The effect base frequency is 6000 Hz; playback can apply
event pitch/volume. `SFX.PAQ` stores 12-byte LE offset/packed/expanded records
with this same effect codec. The implementation expects 66 entries and equal
stored packed/expanded lengths in that special sound directory.
Source: `decodeSfxFibonacci()`, `load_FIB()`, `PC98_loadSounds()` in
[resource.cpp](../../resource.cpp).

## Amiga and Sega CD SPL

There are 66 sequential effect slots. Each begins with u16be size, immediately
followed by that slot's sample bytes when present; it is not an offset directory.
Amiga treats any high-bit length as absent. **Sega CD only treats `ffff` as absent**:
valid effects can exceed 32767 bytes. Nonempty sample sizes must be even and fit
the file. Amiga samples are signed 8; Sega CD samples are 8-bit sign-magnitude:
`sample = byte&128 ? -(byte&127) : byte`. The engine converts CD samples before
its legacy gain normalization. Base frequency is `3546897 / 650` Hz.
The Amiga path skips slot 64 as in the inherited implementation; CD does not.

## VOICE.VCE: dialogue

`Resource::load_VCE(number, segment)` reads segments using the compiled
`Resource::_voicesOffsetsTable` in [staticres.cpp](../../staticres.cpp).
The first 50 words are byte offsets to entries (`ffff` absent); an entry gives
start sector, segment count and each segment's sector length. This directory is
edition-specific metadata, not a self-describing header inside VCE.

Sector size is 2048 bytes. Each group of five sectors carries four skipped sectors
then 2048 sign-magnitude mono PCM samples. Segment lengths must be multiples of
five. For segment starting at sector S, block B reads `(S + B*5 + 4)*2048`.
Decoded byte count is `segmentSectorCount/5*2048`. Current dialogue playback uses
32000 Hz in `Game::drawStoryTexts()`. The movie path uses a separate timing model
and 30720 Hz; do not silently conflate them. Playback speed/pitch fidelity still
benefits from comparison with console hardware.

The US CD integration test reads 95 nonempty dialogue segments and checks their
decoded lengths. A successful segment decode does not verify every in-story
trigger or subtitle synchronization through a complete playthrough.

## Sega CD movie audio

MCD embeds one PCM sector after every four video sectors. It is streamed separately
from VCE and includes narration/dialogue belonging to the movie. See the
[MCD specification](../SEGA_CD.md#mcd-movies). No separate audio files need to be
manufactured for these scenes. CD-DA soundtrack tracks are a third, independent
audio source, extracted losslessly to WAV.

## ProTracker MOD

[mod_player.cpp](../../mod_player.cpp) contains an internal four-channel player;
some builds select libmodplug instead. Internal parser:

- 20-byte song name; 31 sample headers of 30 bytes each: 22-byte name, u16be
  length in words, u8 finetune (0..15), u8 volume (0..64), u16be repeat start
  and length in words.
- u8 order count (1..128), one restart/reserved byte, 128-byte order table.
- Four-byte signature at 1080: `M.K.`, `M!K!`, or `4CHN`.
- Pattern blocks: 64 rows × 4 channels × 4 bytes, through the highest used pattern.
  A note packs sample number, 12-bit period, effect nibble and effect parameter.
- Signed8 sample data follows, with byte lengths twice the header word counts.

The internal player supports the effects implemented by its dispatcher; it is
not a promise of compatibility with every tracker module variant. Original scene
module names and optional `.MOD` names are mapped in this source. Tick timing
carries fractional output samples so device rate does not change song tempo.

## PRF / MIDI / INS (DOS music)

Parsers: [music_midi.cpp](../../music_midi.cpp); scheduling:
[music_prf.cpp](../../music_prf.cpp). PRF is exactly **753 bytes**:

| Offset | Field |
| --- | --- |
| 0 | 16 instrument filename fields × 30 bytes; loader uses up to 8 meaningful name bytes |
| 480 | 16 s16le AdLib note adjustments |
| 512 | 16 s16le AdLib velocity adjustments |
| 544 | u32le timer ticks |
| 548 | u16le timer modulus |
| 550 | 20-byte MIDI filename |
| 570 | u16le AdLib instrument lookup flag |
| 572 | 16 s16le AdLib program lookup values |
| 604 | 16 s16le MT-32 program lookup values |
| 636 | 16 s16le MT-32 velocity adjustments |
| 668 | 16 s16le MT-32 note adjustments |
| 700 | 16 u8 channel mappings |
| 716 | 16 u8 program mappings |
| 732 | 16 u8 no-loop flags |
| 748 | u32le duration in PRF ticks (zero means no duration cap) |
| 752 | u8 channel lookup flag |

Timer ticks/modulus must be nonzero. Scheduler clock is 2082 for AdLib or 2242 for
MT-32-style routing; PRF supplies timing, so MIDI tempo meta-events do not select
the playback clock. A looping track restarts on its PRF modulus boundary.

MIDI: Standard MIDI `MThd`/`MTrk`, BE lengths, formats 0/1, at most 16 tracks,
positive PPQN (no SMPTE division). Delta times are at most four VLQ bytes.
Running channel status is supported; meta/SysEx payloads are skipped while
preserving their delays, and a definite End-of-Track is required. Event count is
bounded to one million per track. PRF filename fields resolve logical local names,
not absolute paths or traversal strings.

INS is exactly 80 bytes: u8 mode at 0, u8 channel at 1, two operators each containing
13 u16le parameters at 2; bytes 54..73 are ignored; modulator waveform at 74,
carrier waveform at 76 (interleaved ignored bytes), u16le version 1 at 78.
Mode is 0/1, rhythm-mode channel6..10, waveforms0..3. Parameter translation to
OPL registers is in `music_synth.cpp`; preserve the parameter order.

AdLib synthesis is built in. FluidSynth SoundFonts and Munt MT-32 ROMs are optional
external backend inputs; their parsing belongs to those libraries, and they are
not bundled. Available backends are reported by `--version`.

## Ogg Vorbis and PCM WAV

[ogg_player.cpp](../../ogg_player.cpp) uses the configured Vorbis decoder and a
streaming resampler. Supported playback is mono/stereo, 8000–192000 Hz, looping
until its scene or background is stopped. Background naming/mapping is in the
manual; scene cues currently use `.ogg`. Ogg parsing is delegated to the decoder
library, not a new container implementation in this engine.

[wav_stream.cpp](../../wav_stream.cpp) accepts LE RIFF `WAVE`, bounded by
`u32le(file+4)+8` (maximum 1 GiB). Chunks contain four-byte tag + u32le payload
size, padded to even byte boundaries. One `fmt ` and one nonempty `data` required;
unknown chunks are skipped. `fmt ` must contain at least 16 bytes:

| Relative offset | Required field |
| --- | --- |
| 0 | u16le format 1 (integer PCM) |
| 2 | u16le channels 1 or 2 |
| 4 | u32le sample rate 8000..192000 |
| 8 | u32le byte rate = sample rate × channels ×2 |
| 12 | u16le block alignment = channels ×2 |
| 14 | u16le bits per sample 16 |

Data length must be an integer number of frames. Samples are signed 16 LE;
mono duplicates to both output channels. A 4096-byte read buffer loops at data
end. Input is scaled by 3/4, matching the Ogg path, then resampled to the device.
Float, compressed WAV, WAVE extensible, RF64 and arbitrary bit depths are not
supported. Prefer `trackNN.wav` for unmodified extracted CD audio; no transcoder
is required. If matching `.ogg` and `.wav` both exist, Ogg is tried first.

## CPC / SDX2

[cpc_player.cpp](../../cpc_player.cpp) reads BE chunks: four-byte tag, u32be total
chunk length including its8-byte header. For `SNDS`, subtype is at offset 16:

- `SHDR`: minimum 56 bytes; sample rate u32be at 44, channels at 48 (must 2),
  compression tag at 52 (`SDX2`). Rate range8000..192000, consistent across headers.
- `SSMP`: minimum 24 bytes; u32be encoded sample count at 20; signed 8 samples begin
  at 24, alternating L/R, with an even count.

For signed encoded byte D, compute `square = D * abs(D) * 2`; if bit 0 is set,
add previous sample on that channel, otherwise replace it; retain signed 16 result.
State continues across sample chunks. Encoded input cap 32 MiB; decoded cap 64 MiB.
This is optional 3DO-style music, not an Amstrad CPC emulator or 3DO movie player.

## Macintosh snd resources

`Resource::MAC_loadSounds()` maps selected resource-list indexes to effect slots, reads
the classic `snd ` command/sample-header layout, and converts unsigned8 PCM by
subtracting 128. The consumed header is 36 bytes: sample count is u32be at 0x12,
and the integer part of the fixed-point frequency is u16be at 0x16. Sample bytes
follow at 0x24.
This inherited adapter expects the Flashback sample layout; it is not a general
QuickTime, compressed-snd or arbitrary Mac sound-bank decoder. Real Macintosh
audio fidelity remains unverified without original assets.
