# SPDX-License-Identifier: GPL-3.0-or-later
"""Asset-free regression tests for movie export, timing and output protection."""
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import upscale_movies as tool
from restore_movie_region import validate_recipe


def packed(codec, value):
    literals, refs = bytearray(), bytearray()
    produced = 0
    while produced < 16384:
        flag = len(literals); literals.append(0)
        for bit in range(8):
            if produced == 16384: break
            if produced == 0 or produced == 16383:
                literals[flag] |= (1 << bit) if codec == "lsb" else (128 >> bit)
                literals.append(value); produced += 1
            else:
                count = min(16384 - produced, 17)
                (refs if codec == "split" else literals).extend(struct.pack(">H", count - 2))
                produced += count
    return struct.pack(">H", len(literals)) + literals + refs if codec == "split" else literals


def movie_fixture(codec):
    video = bytearray()
    for ticks, pixel, color in [(2, 0x22, 0x00e), (4, 0x33, 0x0e0)]:
        frame = bytearray(36)
        frame[32:36] = bytes([1, ticks, 0, 2])
        at = (pixel & 15) * 2
        frame[at:at + 2] = struct.pack(">H", color)
        frame.extend(packed(codec, pixel))
        frame.extend(bytes(ticks * 2048 - len(frame)))
        video.extend(frame)
    video.extend(bytes(16384 - len(video)))
    # Two distinct PCM blocks expose wrong interleave offsets and trimming.
    block1 = bytes([0, 127, 255, 128]) * 512
    block2 = bytes([1, 129, 64, 192]) * 512
    return video[:8192] + block1 + video[8192:] + block2


class MovieTools(unittest.TestCase):
    def test_timing_preserves_variable_holds_and_rejects_gaps(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "frames.tsv"
            p.write_text("0\t0\t2\n1\t2\t4\n")
            self.assertEqual(tool.read_timing(p), [(0, 0, 2), (1, 2, 4)])
            for text in ("", "0 1 2\n", "0 0 0\n", "0 0 8\n", "0 0 2\n1 3 2\n", "1 0 1\n"):
                p.write_text(text)
                with self.assertRaises(ValueError): tool.read_timing(p)

    def test_names_and_case_ambiguity(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); (root / "SC1.MCD").touch(); (root / "SPY.MCD").touch()
            self.assertEqual(list(tool.movies(root)), ["sc1", "spy"])
            self.assertEqual(tool.output_stem("sc1", 3, 3), "sc1_cugan_pro_3x")
            self.assertNotEqual(tool.output_stem("sc1", 3, 0), tool.output_stem("sc1", 3, 3))
            (root / "bad name.MCD").touch()
            with self.assertRaises(ValueError): tool.movies(root)

    def test_publication_never_clobbers_existing_movie(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); source = root / "source"; dest = root / "final.mp4"
            source.write_bytes(b"new"); dest.write_bytes(b"original")
            with self.assertRaises(FileExistsError): tool.publish(source, dest)
            self.assertEqual(dest.read_bytes(), b"original")
            target = root / "new.mp4"; tool.publish(source, target)
            self.assertEqual(target.read_bytes(), b"new")
            self.assertFalse(list(root.glob(".upscale-*")))

    def test_recipe_is_bound_to_source_size_and_nonoverlapping_frames(self):
        original = json.loads((ROOT / "tools/movie_recipes/sc1_dock25.json").read_text())
        self.assertEqual(len(validate_recipe(original, original["source_sha256"], (768, 384), 691)), 76)
        with self.assertRaises(ValueError): validate_recipe(original, "wrong", (768, 384), 691)
        with self.assertRaises(ValueError): validate_recipe(original, original["source_sha256"], (512, 256), 691)
        for value in (None, "invalid", {}):
            bad = copy.deepcopy(original); bad["polygon"] = value
            with self.assertRaises(ValueError): validate_recipe(bad, bad["source_sha256"], (768, 384), 691)
        for kind in ("overlap", "out-of-range", "nan"):
            bad = copy.deepcopy(original)
            if kind == "overlap": bad["segments"].append(bad["segments"][0])
            if kind == "out-of-range": bad["segments"][0]["last"] = 691
            if kind == "nan": bad["segments"][0]["matrix"][0][0] = float("nan")
            with self.assertRaises(ValueError): validate_recipe(bad, bad["source_sha256"], (768, 384), 691)

    @unittest.skipUnless(os.environ.get("REMINISCENCE2_MCD_EXPORTER"), "compiled exporter not supplied")
    def test_export_all_codecs_exact_pcm_timing_and_no_overwrite(self):
        exporter = os.environ["REMINISCENCE2_MCD_EXPORTER"]
        with tempfile.TemporaryDirectory(prefix="movie export spaces ") as tmp:
            root = Path(tmp)
            for codec, name in (("lsb", "DEBUT"), ("msb", "BRIDGE"), ("split", "SPY")):
                with self.subTest(codec=codec):
                    source = root / (name + ".MCD"); raw = movie_fixture(codec); source.write_bytes(raw)
                    output = root / (name + " output")
                    result = subprocess.run([exporter, str(source), str(output)], capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(tool.read_timing(output / "frames.tsv"), [(0, 0, 2), (1, 2, 4)])
                    self.assertEqual((output / "frames/000000.ppm").read_bytes(), b"P6\n256 128\n255\n" + bytes([255, 0, 0]) * 32768)
                    self.assertEqual((output / "frames/000001.ppm").read_bytes(), b"P6\n256 128\n255\n" + bytes([0, 255, 0]) * 32768)
                    with wave.open(str(output / "audio.wav"), "rb") as audio:
                        self.assertEqual((audio.getframerate(), audio.getnchannels(), audio.getsampwidth(), audio.getnframes()), (30720, 1, 2, 3072))
                        expected = struct.pack("<4h", 0, 32512, -32512, 0) * 512 + struct.pack("<4h", 256, -256, 16384, -16384) * 256
                        self.assertEqual(audio.readframes(3072), expected)
                    self.assertEqual(source.read_bytes(), raw)
                    sentinel = output / "sentinel"; sentinel.write_bytes(b"keep")
                    self.assertNotEqual(subprocess.run([exporter, str(source), str(output)], capture_output=True).returncode, 0)
                    self.assertEqual(sentinel.read_bytes(), b"keep")
            broken = root / "BAD.MCD"; broken.write_bytes(raw[:-1])
            self.assertNotEqual(subprocess.run([exporter, str(broken), str(root / "bad")], capture_output=True).returncode, 0)
            self.assertFalse((root / "bad").exists())


if __name__ == "__main__": unittest.main()
