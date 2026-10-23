#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare Flashback WHDLoad LHA game data for REminiscence2.

LHA members are read as bytes, never extracted using archive-supplied paths.
Ancient handles the inner RNC streams; the Amiga executable and WHDLoad slave
are never run. The completed directory is published only after validation.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib
import io
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from typing import Any, Iterable, Optional, Sequence

MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_FILE_BYTES = 16 * 1024 * 1024
MAX_OUTPUT_BYTES = 128 * 1024 * 1024
MAX_MEMBERS = 4096
MANIFEST_NAME = "reminiscence2-extraction.json"
REQUIRED_FILES = {"font8.spr", "icone.spr", "icon.spr", "perso.spm", "level1.lev"}


class ExtractionError(Exception):
    """An input or dependency problem that can be explained without a traceback."""


@dataclass(frozen=True)
class Member:
    original_name: str
    path: PurePosixPath
    size: int


def archive_path(name: str) -> PurePosixPath:
    """Normalize Amiga/DOS separators before checking a member's path."""
    normalized = name.replace("\\", "/").replace("\xff", "/")
    parts = normalized.rstrip("/").split("/")
    if (
        not normalized
        or normalized.startswith("/")
        or PureWindowsPath(normalized).drive
        or any(part in ("", ".", "..") for part in parts)
        or any(ord(char) < 32 or ord(char) == 127 or char == ":" for char in normalized)
        or any(part.endswith((".", " ")) for part in parts)
    ):
        raise ExtractionError(f"Unsafe or ambiguous archive path: {name!r}")
    return PurePosixPath(*parts)


def inspect_members(infos: Iterable[Any], archive_size: int) -> list[Member]:
    members = []
    seen = set()
    total_size = 0
    for index, info in enumerate(infos):
        if index >= MAX_MEMBERS:
            raise ExtractionError(f"Archive exceeds the {MAX_MEMBERS}-member limit")
        path = archive_path(info.filename)
        key = path.as_posix().casefold()
        if key in seen:
            raise ExtractionError(f"Duplicate archive path (ignoring case): {path}")
        seen.add(key)
        if info.compress_type == b"-lhd-":
            continue
        if not 0 <= info.file_size <= MAX_FILE_BYTES:
            raise ExtractionError(f"Archive member exceeds the 16 MiB limit: {path}")
        if (
            info.compress_size < 0
            or info.file_offset < 0
            or info.file_offset + info.compress_size > archive_size
        ):
            raise ExtractionError(f"Truncated archive member: {path}")
        total_size += info.file_size
        if total_size > MAX_OUTPUT_BYTES:
            raise ExtractionError("Archive members exceed the 128 MiB total size limit")
        members.append(Member(info.filename, path, info.file_size))
    return members


def select_game_files(members: Sequence[Member]) -> list[tuple[Member, PurePosixPath]]:
    """Locate the WHDLoad data root using engine resources, not its folder name."""
    candidates = []
    for font in members:
        if font.path.name.casefold() != "font8.spr":
            continue
        root = font.path.parent
        contents = []
        for member in members:
            try:
                relative = member.path.relative_to(root)
            except ValueError:
                continue
            # WHDLoad's root executable, slave, icons and documentation are not
            # engine assets. Keep the data/cine/music hierarchy intact.
            if relative.as_posix().casefold() == "font8.spr" or (
                len(relative.parts) > 1
                and relative.parts[0].casefold() in {"data", "cine", "music"}
                and relative.suffix.casefold() not in {".info", ".slave"}
            ):
                contents.append((member, relative))
        names = {relative.name.casefold() for _, relative in contents}
        if REQUIRED_FILES <= names:
            candidates.append(contents)
    if not candidates:
        required = ", ".join(sorted(REQUIRED_FILES))
        raise ExtractionError(f"No Flashback WHDLoad data tree found; required files: {required}")
    if len(candidates) != 1:
        raise ExtractionError("Multiple Flashback data trees found; use an archive with one edition")

    selected = sorted(candidates[0], key=lambda item: item[1].as_posix().casefold())
    seen_names = set()
    for _, relative in selected:
        name = relative.name.casefold()
        # The engine searches recursively by basename, ignoring case. Two files
        # with the same basename would make resource selection order-dependent.
        if name in seen_names or name == MANIFEST_NAME:
            raise ExtractionError(f"Ambiguous engine resource basename: {relative.name}")
        seen_names.add(name)
    return selected


def _crc16_entry(value: int) -> int:
    for _ in range(8):
        value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return value


_CRC16_TABLE = tuple(_crc16_entry(value) for value in range(256))


def crc16(data: bytes) -> int:
    """RNC's CRC-16/ARC, initial value zero, reflected polynomial 0xA001."""
    value = 0
    for byte in data:
        value = (value >> 8) ^ _CRC16_TABLE[(value ^ byte) & 0xFF]
    return value


def rnc_header(data: bytes, name: str) -> Optional[tuple[int, int]]:
    if not data.startswith(b"RNC"):
        return None
    if len(data) < 18:
        raise ExtractionError(f"Truncated RNC header: {name}")
    if data[3] not in (1, 2):
        raise ExtractionError(f"Unsupported RNC method {data[3]}: {name}")
    unpacked_size, packed_size, unpacked_crc, packed_crc = struct.unpack(">IIHH", data[4:16])
    if not 0 < unpacked_size <= MAX_FILE_BYTES:
        raise ExtractionError(f"RNC output exceeds the 16 MiB limit or is empty: {name}")
    if packed_size != len(data) - 18:
        raise ExtractionError(f"RNC packed size does not match its header: {name}")
    if crc16(data[18:]) != packed_crc:
        raise ExtractionError(f"RNC packed checksum mismatch: {name}")
    return unpacked_size, unpacked_crc


def unpack_rnc(
    data: bytes, target: Path, workspace: Path, ancient: str, name: str,
    expected: tuple[int, int],
) -> None:
    packed_file = workspace / "member.rnc"
    packed_file.write_bytes(data)
    try:
        result = subprocess.run(
            [ancient, "decompress", str(packed_file), str(target)],
            capture_output=True, text=True, errors="replace", timeout=30, check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise ExtractionError(f"RNC decompression timed out: {name}") from exc
    if result.returncode:
        detail = (result.stderr or result.stdout).strip()[-2000:]
        raise ExtractionError(f"Ancient could not decompress {name}: {detail}")
    if not target.is_file() or target.stat().st_size != expected[0]:
        raise ExtractionError(f"RNC unpacked size does not match its header: {name}")
    if crc16(target.read_bytes()) != expected[1]:
        raise ExtractionError(f"RNC unpacked checksum mismatch: {name}")


def prepare_assets(
    archive_pathname: Path, destination: Path, ancient_command: str = "ancient",
    verbose: bool = False,
) -> dict[str, Any]:
    # Do not resolve the final component before testing it: a dangling output
    # symlink also counts as an existing destination and must not be followed.
    destination = Path(os.path.abspath(destination.expanduser()))
    if os.path.lexists(destination):
        raise ExtractionError(f"Output already exists; choose a new folder: {destination}")
    archive_pathname = archive_pathname.expanduser().resolve()
    with archive_pathname.open("rb") as stream:
        archive_bytes = stream.read(MAX_ARCHIVE_BYTES + 1)
    if not archive_bytes or len(archive_bytes) > MAX_ARCHIVE_BYTES:
        raise ExtractionError("Input must be a nonempty LHA archive of at most 64 MiB")
    try:
        lhafile = importlib.import_module("lhafile")
    except ImportError as exc:
        raise ExtractionError(
            "Missing 'lhafile'. Install tools/requirements-amiga.txt with pip in your "
            "Python virtual environment; see --help for setup commands."
        ) from exc
    ancient = shutil.which(ancient_command)
    if ancient is None:
        raise ExtractionError(
            "Ancient was not found. Install it (macOS: brew install ancient), "
            "or supply --ancient /path/to/ancient."
        )
    # Freeze the source archive in memory so every checksum and member refers
    # to the same input, even if the original archive changes during conversion.
    with io.BytesIO(archive_bytes) as stream:
        try:
            archive = lhafile.Lhafile(stream)
            members = inspect_members(archive.infolist(), len(archive_bytes))
        except ExtractionError:
            raise
        except Exception as exc:
            raise ExtractionError(f"Cannot read the LHA archive: {exc}") from exc
        selected = select_game_files(members)

        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(
            prefix=f".{destination.name}.extract-", dir=destination.parent,
        ) as temporary:
            workspace = Path(temporary)
            staged = workspace / "assets"
            staged.mkdir()
            records = []
            total_output = 0
            for member, relative in selected:
                name = relative.as_posix()
                if verbose:
                    print(f"Preparing {name}", flush=True)
                try:
                    data = archive.read(member.original_name)
                except Exception as exc:
                    raise ExtractionError(f"LHA member failed size/CRC validation: {name}: {exc}") from exc
                if len(data) != member.size:
                    raise ExtractionError(f"LHA member size mismatch: {name}")
                expected = rnc_header(data, name)
                total_output += expected[0] if expected else len(data)
                if total_output > MAX_OUTPUT_BYTES:
                    raise ExtractionError("Unpacked game files exceed the 128 MiB total size limit")
                target = staged.joinpath(*relative.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                if expected:
                    unpack_rnc(data, target, workspace, ancient, name, expected)
                else:
                    target.write_bytes(data)
                output = target.read_bytes()
                records.append({
                    "path": name,
                    "archive_member": member.original_name,
                    "operation": f"rnc{data[3]}" if expected else "copy",
                    "input_bytes": len(data),
                    "output_bytes": len(output),
                    "input_sha256": hashlib.sha256(data).hexdigest(),
                    "output_sha256": hashlib.sha256(output).hexdigest(),
                })
            manifest = {
                "format_version": 1,
                "archive": {
                    "name": archive_pathname.name,
                    "bytes": len(archive_bytes),
                    "sha256": hashlib.sha256(archive_bytes).hexdigest(),
                },
                "file_count": len(records),
                "rnc_count": sum(record["operation"] != "copy" for record in records),
                "skipped_files": len(members) - len(selected),
                "output_bytes": total_output,
                "files": records,
            }
            (staged / MANIFEST_NAME).write_text(
                json.dumps(manifest, indent=2, ensure_ascii=True) + "\n", encoding="utf-8",
            )
            if os.path.lexists(destination):
                raise ExtractionError(f"Output appeared during extraction; leaving it unchanged: {destination}")
            staged.rename(destination)
    return manifest


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Extract Flashback WHDLoad LHA data and unpack inner RNC files for REminiscence2.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Setup (Python 3.9 or newer):
  python3 -m venv .local/extract-venv
  .local/extract-venv/bin/python -m pip install -r tools/requirements-amiga.txt
  brew install ancient                 # macOS; no lha command is required

Example, from the project folder:
  .local/extract-venv/bin/python tools/extract_amiga.py \\
    .local/reference/Flashback_v3.4_1163.lha .local/assets/amiga-prepared

The output folder must not already exist. A failed conversion removes its
temporary files and leaves the archive and any existing output untouched.
The result includes data, cine and music subfolders, FONT8.SPR, and a JSON
manifest with sizes and SHA-256 hashes. WHDLoad launchers and icons are skipped.
Only standard 18-byte-header RNC methods 1 and 2 are supported.
Ancient: https://github.com/temisu/ancient
""",
    )
    parser.add_argument("archive", type=Path, help="your Flashback WHDLoad .lha archive")
    parser.add_argument("output", type=Path, help="new folder for engine-ready Amiga game files")
    parser.add_argument("--ancient", default="ancient", metavar="PATH", help="Ancient executable (default: find ancient on PATH)")
    parser.add_argument("-v", "--verbose", action="store_true", help="print each game file as it is prepared")
    args = parser.parse_args(argv)
    try:
        report = prepare_assets(args.archive, args.output, args.ancient, args.verbose)
    except (ExtractionError, OSError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Extraction interrupted.", file=sys.stderr)
        return 130
    output = Path(os.path.abspath(args.output.expanduser()))
    print(f"Prepared {report['file_count']} game files: {report['rnc_count']} RNC-unpacked, "
          f"{report['file_count'] - report['rnc_count']} copied; "
          f"{report['skipped_files']} package files skipped.")
    print(f"Output: {output}\nManifest: {output / MANIFEST_NAME}\n")
    executable = Path(__file__).resolve().parents[1] / "build" / "release" / (
        "reminiscence2.exe" if os.name == "nt" else "reminiscence2"
    )
    command = [str(executable), f"--datapath={output}", "--window"]
    print("Play with:")
    print(subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command))
    return 0


if __name__ == "__main__":
    sys.exit(main())
