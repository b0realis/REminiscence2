# SPDX-License-Identifier: GPL-3.0-or-later
"""Catalog/menu checks plus an optional real-engine round trip with synthetic assets."""
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("play_cutscenes", ROOT / "tools/play_cutscenes.py")
player = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = player
spec.loader.exec_module(player)


class MenuTests(unittest.TestCase):
    def setUp(self):
        self.scenes = [player.Cutscene("intro", "Introduction", 1), player.Cutscene("chute", "Falling", 2)]

    def test_names_numbers_parts_and_case(self):
        self.assertEqual(player.select(" INTRO ", self.scenes), (self.scenes[0], 0))
        self.assertEqual(player.select("chute:1", self.scenes), (self.scenes[1], 1))
        self.assertEqual(player.select("2:1", self.scenes, numbers=True), (self.scenes[1], 1))
        for text in ("0", "3", "absent", "intro:1", "intro:", "chute:-1", "chute:0:1", "../intro"):
            with self.subTest(text=text), self.assertRaises(player.PlayerError):
                player.select(text, self.scenes, numbers=True)

    def test_catalog_contract_and_empty_catalog(self):
        data = {"schema": 1, "edition": "Amiga", "cutscenes": [entry.__dict__ for entry in self.scenes]}
        self.assertEqual(player.decode_catalog(json.dumps(data)), ("Amiga", self.scenes))
        data["cutscenes"] = []
        self.assertEqual(player.decode_catalog(json.dumps(data)), ("Amiga", []))
        for text in ("old engine help", "[]", "null", '{"schema":2}',
                     json.dumps({**data, "schema": True}),
                     json.dumps({**data, "cutscenes": [{"name": "intro", "title": "x", "parts": True}]}),
                     json.dumps({**data, "cutscenes": [self.scenes[0].__dict__] * 2})):
            with self.subTest(text=text), self.assertRaises(player.PlayerError):
                player.decode_catalog(text)

    def test_inspection_failure_and_timeout_are_actionable(self):
        with patch.object(player.subprocess, "run", return_value=subprocess.CompletedProcess([], 1, "", "bad assets")):
            with self.assertRaisesRegex(player.PlayerError, "bad assets"):
                player.read_catalog(["engine"])
        with patch.object(player.subprocess, "run", side_effect=subprocess.TimeoutExpired("engine", 30)):
            with self.assertRaisesRegex(player.PlayerError, "timed out"):
                player.read_catalog(["engine"])

    def test_play_preserves_argument_boundaries(self):
        command = ["/some folder/engine", "--datapath=/assets with spaces/$(literal)"]
        with patch.object(player.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as run:
            self.assertEqual(player.play(command, self.scenes[1], 1), 0)
            self.assertEqual(run.call_args.args[0], [*command, "--play-cutscene=chute:1"])
            self.assertNotIn("shell", run.call_args.kwargs)

    def test_invalid_engine_is_reported(self):
        with self.assertRaisesRegex(player.PlayerError, "executable not found"):
            player.find_engine("/does-not-exist/reminiscence2")


@unittest.skipUnless(os.environ.get("REMINISCENCE2_ENGINE"), "set REMINISCENCE2_ENGINE for real-engine checks")
class EngineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.assets = self.root / "assets with spaces"
        self.assets.mkdir()
        (self.assets / "LEVEL1.LEV").write_bytes(b"")
        # Two command parts: display the cleared screen, then stop. No original
        # game pixels, text or audio are needed to test the entire launch path.
        commands = bytes.fromhex("0002 0000 0002 0080 0080")
        polygons = bytes(20)
        cmp = b"".join(struct.pack(">i", -len(data)) + data for data in (polygons, commands))
        (self.assets / "INTRO.CMP").write_bytes(cmp)
        (self.assets / "FONT8.SPR").write_bytes(bytes(2048))
        (self.assets / "ENGCINE.TXT").write_text("\n" * 117)
        self.command = [sys.executable, str(ROOT / "tools/play_cutscenes.py"), str(self.assets),
                        f"--engine={os.environ['REMINISCENCE2_ENGINE']}"]
        self.env = {**os.environ, "SDL_VIDEODRIVER": "dummy", "SDL_AUDIODRIVER": "dummy", "SDL_RENDER_DRIVER": "software"}

    def run_tool(self, *args):
        return subprocess.run([*self.command, *args], cwd=self.root, env=self.env,
                              text=True, capture_output=True, timeout=15, check=False)

    def test_json_catalog_from_real_engine(self):
        result = self.run_tool("--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["cutscenes"],
                         [{"name": "intro", "title": "Introduction", "parts": 2}])
        self.assertFalse((self.root / "SAVE").exists())
        self.assertFalse((self.root / "TUNES").exists())

    def test_one_part_plays_and_preserves_assets(self):
        before = {path.name: path.read_bytes() for path in self.assets.iterdir()}
        result = self.run_tool("--play", "intro:1")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Playing intro:1", result.stdout)
        self.assertEqual(before, {path.name: path.read_bytes() for path in self.assets.iterdir()})
        self.assertFalse((self.root / "SAVE").exists())
        self.assertFalse((self.root / "TUNES").exists())

    def test_noninteractive_input_and_invalid_selection(self):
        result = self.run_tool()
        self.assertEqual(result.returncode, 1)
        self.assertIn("needs a terminal", result.stderr)
        result = self.run_tool("--play", "intro:99")
        self.assertEqual(result.returncode, 1)
        self.assertIn("parts 0 through 1", result.stderr)

    def test_filter_help_and_empty_result(self):
        result = self.run_tool("--list", "--filter", "INTRO")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1 files, 2 selectable parts", result.stdout)
        result = self.run_tool("--list", "--filter", "missing")
        self.assertEqual(result.returncode, 1)
        result = self.run_tool("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Requirements: Python 3.9+", result.stdout)


if __name__ == "__main__":
    unittest.main()
