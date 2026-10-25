#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply a source-hash-bound, tracked reference plate to upscaled movie frames.

Used by upscale_movies.py. Recipes are data, not executable Python. Original
frames are never overwritten; returned pixels outside the feathered mask match
the input exactly. The SC1 reference image remains a private, separate file.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path


def validate_recipe(data: dict, source_hash: str, size: tuple[int, int], count: int) -> dict[int, list]:
    if not isinstance(data, dict) or type(data.get("schema")) is not int or data.get("schema") != 1 or data.get("source_sha256") != source_hash:
        raise ValueError("Restoration recipe does not match this source movie")
    if data.get("size") != list(size):
        raise ValueError("Restoration recipe does not match the upscale dimensions")
    polygon = data.get("polygon", [])
    if not isinstance(polygon, list) or not 3 <= len(polygon) <= 64 or any(
        not isinstance(p, list) or len(p) != 2 or
        any(not isinstance(v, (int, float)) or not math.isfinite(v) for v in p) or
        not (0 <= p[0] < size[0] and 0 <= p[1] < size[1]) for p in polygon
    ):
        raise ValueError("Invalid restoration polygon")
    kernel, sigma = data.get("feather_kernel"), data.get("feather_sigma")
    if type(kernel) is not int or not 1 <= kernel <= 101 or kernel % 2 != 1:
        raise ValueError("Invalid restoration feather kernel")
    if not isinstance(sigma, (int, float)) or not math.isfinite(sigma) or not 0 < sigma <= 20:
        raise ValueError("Invalid restoration feather sigma")
    matrices = {}
    segments = data.get("segments", [])
    if not isinstance(segments, list) or len(segments) > 100000:
        raise ValueError("Invalid restoration segments")
    for segment in segments:
        if not isinstance(segment, dict):
            raise ValueError("Invalid restoration segment")
        first, last, matrix = segment.get("first"), segment.get("last"), segment.get("matrix")
        if type(first) is not int or type(last) is not int or not 0 <= first <= last < count:
            raise ValueError("Restoration segment is outside the movie")
        if (not isinstance(matrix, list) or len(matrix) != 3 or any(
                not isinstance(row, list) or len(row) != 3 or any(
                    not isinstance(v, (int, float)) or not math.isfinite(v) or abs(v) > 1e6
                    for v in row) for row in matrix)):
            raise ValueError("Invalid restoration transform")
        for i in range(first, last + 1):
            if i in matrices:
                raise ValueError("Overlapping restoration segments")
            matrices[i] = matrix
    if not matrices:
        raise ValueError("Restoration recipe has no segments")
    return matrices


class Restoration:
    def __init__(self, recipe: Path, reference: Path, source_hash: str, size: tuple[int, int], count: int):
        import cv2
        import numpy as np
        from PIL import Image
        self.cv, self.np, self.size = cv2, np, size
        if recipe.stat().st_size > 4 * 1024 * 1024:
            raise ValueError("Restoration recipe is too large")
        data = json.loads(recipe.read_text(encoding="utf-8"))
        self.matrices = validate_recipe(data, source_hash, size, count)
        if reference.stat().st_size > 64 * 1024 * 1024:
            raise ValueError("Reference image file is too large")
        if hashlib.sha256(reference.read_bytes()).hexdigest() != data.get("reference_sha256"):
            raise ValueError("Reference image does not match the restoration recipe")
        with Image.open(reference) as image:
            if image.width * image.height > 16_000_000:
                raise ValueError("Reference image is too large")
            source = np.array(image.convert("RGB"))
        self.plate = cv2.resize(source, size, interpolation=cv2.INTER_AREA)
        self.light = self.wall_light(self.plate)
        mask = np.zeros((size[1], size[0]), np.float32)
        polygon = np.int32(data["polygon"])
        if cv2.isContourConvex(polygon):
            cv2.fillConvexPoly(mask, polygon, 1)
        else:
            cv2.fillPoly(mask, [polygon], 1)
        kernel = data["feather_kernel"]
        self.mask = cv2.GaussianBlur(mask, (kernel, kernel), data["feather_sigma"])
        self.matrices = {i: np.array(m, np.float64) for i, m in self.matrices.items()}
        if any(abs(np.linalg.det(m)) < 1e-10 for m in self.matrices.values()):
            raise ValueError("Singular restoration transform")
        self.text = data.get("text", "")

    def wall_light(self, image):
        closed = self.cv.morphologyEx(image, self.cv.MORPH_CLOSE, self.np.ones((15, 15), self.np.uint8))
        return self.cv.GaussianBlur(closed.astype(self.np.float32), (0, 0), 8)

    def apply(self, index: int, image):
        if index not in self.matrices:
            return image
        from PIL import Image
        cv, np, matrix = self.cv, self.np, self.matrices[index]
        base = np.array(image.convert("RGB"))
        warp = lambda value: cv.warpPerspective(value, matrix, self.size, flags=cv.INTER_LINEAR)
        plate, light, alpha = warp(self.plate.astype(np.float32)), warp(self.light), warp(self.mask)
        gain = np.clip((self.wall_light(base) + 2) / (light + 2), .15, 5)
        matched = np.clip(plate * gain, 0, 255)
        out = np.rint(base * (1 - alpha[..., None]) + matched * alpha[..., None]).astype(np.uint8)
        if not np.array_equal(out[alpha == 0], base[alpha == 0]):
            raise ValueError("Restoration changed pixels outside its mask")
        return Image.fromarray(out)
