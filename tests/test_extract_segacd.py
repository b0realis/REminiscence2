# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic ISO/CUE fixtures; no commercial game data is embedded."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import tempfile
import unittest
import wave
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import extract_segacd as tool


def both(n, size):
    return n.to_bytes(size, "little") + n.to_bytes(size, "big")


def record(name, sector, size, directory=False):
    name = name if isinstance(name, bytes) else name.encode("ascii")
    out = bytearray(33 + len(name) + (not len(name) % 2))
    out[0] = len(out)
    out[2:10] = both(sector, 4)
    out[10:18] = both(size, 4)
    out[25] = 2 if directory else 0
    out[28:32] = both(1, 2)
    out[32] = len(name)
    out[33:33 + len(name)] = name
    return out


def iso():
    out = bytearray(26 * 2048)
    pvd = bytearray(2048)
    pvd[:7] = b"\x01CD001\x01"
    pvd[40:72] = b"FLASHBACK".ljust(32)
    pvd[80:88] = both(26, 4)
    pvd[128:132] = both(2048, 2)
    pvd[156:190] = record(b"\0", 20, 2048, True)
    out[16*2048:17*2048] = pvd
    root = (record(b"\0", 20, 2048, True) + record(b"\1", 20, 2048, True) +
            record("DATA", 21, 2048, True) + record("SC1.MCD;1", 24, 5) + record("SC2.MCD;1", 25, 5))
    sub = record("LEVEL1.LEV;1", 22, 5) + record("VOICE.VCE;1", 23, 5)
    out[20*2048:20*2048+len(root)] = root
    out[21*2048:21*2048+len(sub)] = sub
    for n in range(22, 26): out[n*2048:n*2048+5] = bytes([n]) * 5
    return bytes(out)


def raw(data):
    return b"".join(tool.SYNC + b"\0\0\0\x01" + data[i:i+2048] + bytes(288)
                    for i in range(0, len(data), 2048))


class ExtractTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.cue = self.root / "disc.cue"
        self.output = self.root / "assets"

    def image(self, shared=False, cooked=False, data=None):
        data = iso() if data is None else data
        disc = data if cooked else raw(data)
        audio = struct.pack("<hh", 1234, -1234) * (2352 * 3 // 4)
        gap = b"G" * (2352 * 2)
        mode = "MODE1/2048" if cooked else "MODE1/2352"
        if shared:
            (self.root / "one.bin").write_bytes(disc + gap + audio)
            self.cue.write_text('FILE "one.bin" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n TRACK 02 AUDIO\n INDEX 00 00:00:26\n INDEX 01 00:00:28\n')
        else:
            (self.root / "data.bin").write_bytes(disc)
            (self.root / "audio.bin").write_bytes(gap + audio)
            self.cue.write_text(f'FILE "data.bin" BINARY\n TRACK 01 {mode}\n INDEX 01 00:00:00\nFILE "audio.bin" BINARY\n TRACK 02 AUDIO\n INDEX 00 00:00:00\n INDEX 01 00:00:02\n')
        return audio

    def test_shared_and_separate_and_cooked(self):
        for shared, cooked in [(True, False), (False, False), (False, True)]:
            with self.subTest(shared=shared, cooked=cooked):
                expected = self.image(shared, cooked)
                dest = self.root / f"output-{shared}-{cooked}"
                original = {p: hashlib.sha256(p.read_bytes()).digest() for p in self.root.glob("*.bin")}
                report = tool.extract(self.cue, dest)
                self.assertEqual((dest / "DATA/VOICE.VCE").read_bytes(), bytes([23])*5)
                with wave.open(str(dest / "music/track02.wav")) as wav:
                    self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate()), (2, 2, 44100))
                    self.assertEqual(wav.readframes(wav.getnframes()), expected)
                self.assertEqual(report, json.loads((dest / tool.MANIFEST).read_text()))
                for path, digest in original.items(): self.assertEqual(hashlib.sha256(path.read_bytes()).digest(), digest)

    def test_list_and_existing_destination(self):
        self.image()
        self.assertEqual(len(tool.extract(self.cue, None, list_only=True)["files"]), 4)
        self.assertFalse(self.output.exists())
        self.output.mkdir(); (self.output / "mine").write_text("keep")
        with self.assertRaises(tool.DiscError): tool.extract(self.cue, self.output)
        self.assertEqual((self.output / "mine").read_text(), "keep")

    def test_unsafe_names(self):
        for name in ["../outside", "/etc/passwd", "C:\\secrets", "a/../b", "a//b", "a.", "a\n",
                     "NUL.MCD", "data/CON", "COM1.BIN", "LPT¹.WAV"]:
            with self.subTest(name=name), self.assertRaises(tool.DiscError): tool.safe_path(name)
        self.image(); self.cue.write_text('FILE "../outside.bin" BINARY\n')
        with self.assertRaises(tool.DiscError): tool.read_cue(self.cue)

    def test_malformed_iso_and_no_partial_output(self):
        for change in ["endian", "extent", "traversal", "cycle"]:
            data = bytearray(iso())
            if change == "endian": data[16*2048+84] ^= 1
            if change == "extent": data[21*2048+2:21*2048+10] = both(999, 4)
            if change == "traversal": data[21*2048+33:21*2048+36] = b"../"
            if change == "cycle": data[21*2048+25] = 2; data[21*2048+2:21*2048+10] = both(20, 4)
            self.image(data=bytes(data))
            with self.subTest(change=change), self.assertRaises(tool.DiscError): tool.extract(self.cue, self.output)
            self.assertFalse(self.output.exists())
            self.assertEqual(list(self.root.glob(".segacd-*")), [])

    def test_bad_sector_and_unsupported_cue(self):
        self.image(); p = self.root / "data.bin"; b = bytearray(p.read_bytes()); b[16*2352] = 1; p.write_bytes(b)
        with self.assertRaises(tool.DiscError): tool.extract(self.cue, self.output)
        self.image(); self.cue.write_text(self.cue.read_text().replace("MODE1/2352", "MODE2/2352"))
        with self.assertRaises(tool.DiscError): tool.read_cue(self.cue)

    def test_endian_option(self):
        expected = self.image(); tool.extract(self.cue, self.output, audio_endian="big")
        with wave.open(str(self.output / "music/track02.wav")) as wav: actual = wav.readframes(wav.getnframes())
        self.assertEqual(actual[0::2], expected[1::2]); self.assertEqual(actual[1::2], expected[0::2])


if __name__ == "__main__": unittest.main()
