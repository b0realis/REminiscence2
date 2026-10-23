# SPDX-License-Identifier: GPL-3.0-or-later
"""Asset-free tests for the WHDLoad preparation tool.

Fixtures are synthetic stored LHA members. RNC subprocess tests exercise
validation/rollback with a stand-in; the private archive tests the real decoder.
"""

import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import extract_amiga as tool


def stored_lha(files):
    archive = bytearray()
    for name, payload in files:
        encoded = name.encode("latin-1")
        body = (
            struct.pack("<5sII4sBBB", b"-lh0-", len(payload), len(payload), b"\0" * 4,
                        0x20, 0, len(encoded))
            + encoded + struct.pack("<H", tool.crc16(payload))
        )
        archive += bytes([len(body), sum(body) & 0xFF]) + body + payload
    return bytes(archive) + b"\0"


def fixture_files():
    return [
        ("Flashback/data/font8.spr", b"font"),
        ("Flashback/data/data/icone.spr", b"icons"),
        ("Flashback/data/data/icon.spr", b"inventory"),
        ("Flashback/data/data/perso.spm", b"character"),
        ("Flashback/data/data/level1.lev", b"level"),
        ("Flashback/data/cine/debut.cmp", b"cinematic"),
        ("Flashback/data/music/intro", b"music"),
        ("Flashback/data/flashback", b"Amiga executable, never needed"),
        ("Flashback/Flashback.slave", b"WHDLoad slave, never executed"),
        ("Flashback/ReadMe", b"package notes"),
    ]


def rnc_fixture(output=b"decoded fixture"):
    packed = b"synthetic RNC payload; subprocess is mocked"
    return (
        b"RNC\1"
        + struct.pack(">IIHHBB", len(output), len(packed), tool.crc16(output),
                      tool.crc16(packed), 0, 1)
        + packed
    )


class ExtractAmigaTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="reminiscence2-amiga-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / "input archive.lha"
        self.output = self.root / "prepared assets"
        self.which = mock.patch.object(tool.shutil, "which", return_value="/fake/ancient")
        self.which.start()
        self.addCleanup(self.which.stop)

    def write_archive(self, files=None):
        self.archive.write_bytes(stored_lha(fixture_files() if files is None else files))

    def prepare(self):
        return tool.prepare_assets(self.archive, self.output)

    def assert_no_output(self):
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.root.glob(".prepared assets.extract-*")), [])

    def test_stored_assets_and_manifest_preserve_data_and_source(self):
        self.write_archive()
        source = self.archive.read_bytes()
        report = self.prepare()
        self.assertEqual(report["file_count"], 7)
        self.assertEqual(report["rnc_count"], 0)
        self.assertEqual(report["skipped_files"], 3)
        self.assertEqual((self.output / "data/level1.lev").read_bytes(), b"level")
        self.assertEqual((self.output / "music/intro").read_bytes(), b"music")
        self.assertFalse((self.output / "flashback").exists())
        self.assertFalse((self.output / "Flashback.slave").exists())
        self.assertEqual(self.archive.read_bytes(), source)
        saved = json.loads((self.output / tool.MANIFEST_NAME).read_text())
        self.assertEqual(saved, report)
        self.assertEqual(report["archive"]["sha256"], hashlib.sha256(source).hexdigest())
        for record in report["files"]:
            output = (self.output / record["path"]).read_bytes()
            self.assertEqual(hashlib.sha256(output).hexdigest(), record["output_sha256"])

    def test_crc_standard_vector(self):
        self.assertEqual(tool.crc16(b"123456789"), 0xBB3D)
        self.assertEqual(tool.crc16(b""), 0)

    def test_rnc_sizes_and_both_checksums_are_enforced(self):
        packed = rnc_fixture()
        self.assertEqual(tool.rnc_header(packed, "fixture"),
                         (len(b"decoded fixture"), tool.crc16(b"decoded fixture")))
        with self.assertRaisesRegex(tool.ExtractionError, "checksum mismatch"):
            tool.rnc_header(packed[:-1] + bytes([packed[-1] ^ 1]), "fixture")
        with self.assertRaisesRegex(tool.ExtractionError, "packed size"):
            tool.rnc_header(packed[:-1], "fixture")
        with self.assertRaisesRegex(tool.ExtractionError, "Truncated RNC"):
            tool.rnc_header(b"RNC\1", "fixture")
        oversized = packed[:4] + struct.pack(">I", tool.MAX_FILE_BYTES + 1) + packed[8:]
        with self.assertRaisesRegex(tool.ExtractionError, "16 MiB"):
            tool.rnc_header(oversized, "fixture")

    def test_rnc_result_is_published_only_after_validation(self):
        files = fixture_files()
        files[4] = (files[4][0], rnc_fixture())
        self.write_archive(files)

        def decompress(command, **kwargs):
            self.assertEqual(command[:2], ["/fake/ancient", "decompress"])
            self.assertEqual(kwargs["timeout"], 30)
            Path(command[-1]).write_bytes(b"decoded fixture")
            return subprocess.CompletedProcess(command, 0, "", "")

        with mock.patch.object(tool.subprocess, "run", side_effect=decompress):
            report = self.prepare()
        self.assertEqual(report["rnc_count"], 1)
        self.assertEqual((self.output / "data/level1.lev").read_bytes(), b"decoded fixture")

    def test_failed_decoder_rolls_back_partial_conversion(self):
        files = fixture_files()
        files[4] = (files[4][0], rnc_fixture())
        self.write_archive(files)
        result = subprocess.CompletedProcess([], 1, "", "fixture decoder failure")
        with mock.patch.object(tool.subprocess, "run", return_value=result):
            with self.assertRaisesRegex(tool.ExtractionError, "fixture decoder failure"):
                self.prepare()
        self.assert_no_output()

    def test_corrupt_unpacked_result_rolls_back(self):
        files = fixture_files()
        files[4] = (files[4][0], rnc_fixture())
        self.write_archive(files)

        def bad_decompress(command, **kwargs):
            Path(command[-1]).write_bytes(b"Decoded fixture")
            return subprocess.CompletedProcess(command, 0, "", "")

        with mock.patch.object(tool.subprocess, "run", side_effect=bad_decompress):
            with self.assertRaisesRegex(tool.ExtractionError, "unpacked checksum"):
                self.prepare()
        self.assert_no_output()

    def test_existing_output_and_dangling_symlink_are_not_overwritten(self):
        self.output.mkdir()
        marker = self.output / "keep.txt"
        marker.write_text("keep me")
        with self.assertRaisesRegex(tool.ExtractionError, "Output already exists"):
            self.prepare()
        self.assertEqual(marker.read_text(), "keep me")
        if sys.platform != "win32":
            link = self.root / "dangling-output"
            link.symlink_to(self.root / "missing")
            with self.assertRaisesRegex(tool.ExtractionError, "Output already exists"):
                tool.prepare_assets(self.archive, link)
            self.assertTrue(link.is_symlink())

    def test_unsafe_paths_are_rejected_before_writing(self):
        for name in ["../outside", "data/../../outside", "/absolute", "C:\\outside",
                     "\\\\server\\share", "data\\..\\outside", "data/\0bad"]:
            with self.subTest(name=name):
                with self.assertRaises(tool.ExtractionError):
                    tool.archive_path(name)
        self.write_archive(fixture_files() + [("../outside", b"do not extract")])
        with self.assertRaisesRegex(tool.ExtractionError, "Unsafe"):
            self.prepare()
        self.assertFalse((self.root.parent / "outside").exists())
        self.assert_no_output()

    def test_case_conflicts_and_recursive_basename_conflicts_are_rejected(self):
        for path in ["Flashback/data/data/ICON.SPR", "Flashback/data/music/ICON.SPR"]:
            self.write_archive(fixture_files() + [(path, b"conflicting resource")])
            with self.subTest(path=path):
                with self.assertRaisesRegex(tool.ExtractionError, "Duplicate|Ambiguous"):
                    self.prepare()
                self.assert_no_output()

    def test_missing_game_data_or_multiple_editions_are_rejected(self):
        self.write_archive([("ReadMe", b"not Flashback")])
        with self.assertRaisesRegex(tool.ExtractionError, "No Flashback"):
            self.prepare()
        files = fixture_files()
        self.write_archive(files + [(name.replace("Flashback/", "Other/"), data)
                                    for name, data in files])
        with self.assertRaisesRegex(tool.ExtractionError, "Multiple Flashback"):
            self.prepare()
        self.assert_no_output()

    def test_lha_crc_failure_is_reported_without_publishing(self):
        self.write_archive()
        broken = self.archive.read_bytes().replace(b"character", b"Character")
        self.archive.write_bytes(broken)
        with self.assertRaisesRegex(tool.ExtractionError, "LHA member failed size/CRC"):
            self.prepare()
        self.assert_no_output()

    def test_missing_dependencies_have_actionable_errors(self):
        self.write_archive()
        with mock.patch.object(tool.importlib, "import_module", side_effect=ImportError):
            with self.assertRaisesRegex(tool.ExtractionError, "requirements-amiga.txt"):
                self.prepare()
        with mock.patch.object(tool.shutil, "which", return_value=None):
            with self.assertRaisesRegex(tool.ExtractionError, "--ancient"):
                self.prepare()
        self.assert_no_output()


if __name__ == "__main__":
    unittest.main()
