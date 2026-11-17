#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate the brand logo image resource from the read-only handoff assets.

Inputs (read-only):
    assets/brand/seeed-logo-preview.png   raster snapshot of the official SVG
    assets/brand/brand_manifest.json      SHA-256 of the original SVG

Output (writable whitelist: components/xf_assets/**):
    components/xf_assets/generated/xf_brand_logo.c

Rules applied (design note 6.1.2 / acceptance case AC-18):
    * The logo shape is never redrawn or re-lettered; only the two source inks
      are mapped onto the UI primary colours.
    * Aspect ratio is preserved inside the 240 x 36 px header box, centred.
    * The output is a plain RGB565A8 LVGL image descriptor, so no decoder is
      needed at run time.

Usage:
    python tools/gen_xf_brand.py
"""

from __future__ import annotations

import hashlib
import json
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
OUT_DIR = os.path.join(PROJECT, "components", "xf_assets", "generated")
DEFAULT_PACKAGE = os.path.join(PROJECT, "resources")

# Header logo box from design note 6.1: 480 x 64 bar, logo within 240 x 36.
BOX_W = 240
BOX_H = 36

# Source inks in the official SVG and the UI primary colours they map to.
REMAP = (
    ((0x88, 0xB9, 0x28), (0x8F, 0xC3, 0x1F)),  # brand green
    ((0x00, 0x39, 0x48), (0x00, 0x49, 0x66)),  # brand teal
)
TOLERANCE = 24


def load_manifest(package: str) -> dict:
    path = os.path.join(package, "assets", "brand", "brand_manifest.json")
    with open(path, encoding="utf-8") as fp:
        return json.load(fp)


def remap_ink(pixel: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    """Map a source ink to its UI colour, keeping alpha untouched.

    Antialiasing residue in the snapshot is folded onto the nearest of the two
    official inks instead of being left as a third colour, so the generated
    resource only ever contains #8FC31F and #004966.
    """
    red, green, blue, alpha = pixel
    if alpha == 0:
        return pixel
    for source, target in REMAP:
        if (abs(red - source[0]) <= TOLERANCE and
                abs(green - source[1]) <= TOLERANCE and
                abs(blue - source[2]) <= TOLERANCE):
            return target[0], target[1], target[2], alpha

    best = None
    best_distance = None
    for source, target in REMAP:
        distance = ((red - source[0]) ** 2 +
                    (green - source[1]) ** 2 +
                    (blue - source[2]) ** 2)
        if best_distance is None or distance < best_distance:
            best_distance = distance
            best = target
    return best[0], best[1], best[2], alpha


def resize_premultiplied(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Resize RGBA without darkening semi-transparent edge pixels.

    Pillow resamples straight alpha, which drags the transparent black border
    into the glyph edges. Multiplying by alpha first and dividing afterwards
    keeps the logo outline clean. An area-average filter is used because a
    wider kernel (Lanczos) overshoots on the hard ink edges and would tint the
    flat areas away from the required #8FC31F / #004966.
    """
    premultiplied = Image.new("RGBA", image.size)
    premultiplied.putdata([
        ((red * alpha + 127) // 255, (green * alpha + 127) // 255,
         (blue * alpha + 127) // 255, alpha)
        for red, green, blue, alpha in image.getdata()
    ])
    # Resample each band separately: Pillow does not treat the four channels of
    # an RGBA resize consistently, and mixing premultiplied colour with a
    # separately scaled alpha plane would push channel values above 255 once
    # they are divided again.
    reduced = Image.merge("RGBA", [
        premultiplied.getchannel(band).resize(size, Image.BOX)
        for band in range(4)
    ])

    result = []
    for red, green, blue, alpha in reduced.getdata():
        if alpha == 0:
            result.append((0, 0, 0, 0))
        else:
            result.append((
                min(255, (red * 255 + alpha // 2) // alpha),
                min(255, (green * 255 + alpha // 2) // alpha),
                min(255, (blue * 255 + alpha // 2) // alpha),
                alpha,
            ))
    output = Image.new("RGBA", size)
    output.putdata(result)
    return output


def to_rgb565a8(image: Image.Image) -> bytes:
    """Serialise to LVGL RGB565A8: RGB565 LE plane, then the A8 plane."""
    image = image.convert("RGBA")
    payload = bytearray()
    alpha_plane = bytearray()
    for red, green, blue, alpha in image.getdata():
        value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        payload += bytes((value & 0xFF, value >> 8))
        alpha_plane.append(alpha)
    payload += alpha_plane
    return bytes(payload)


def format_bytes(data: bytes, width: int = 12) -> str:
    lines = []
    for start in range(0, len(data), width):
        chunk = data[start:start + width]
        lines.append("    " + " ".join("0x%02x," % byte for byte in chunk))
    return "\n".join(lines)


def main() -> None:
    package = DEFAULT_PACKAGE
    for index, arg in enumerate(sys.argv):
        if arg == "--package" and index + 1 < len(sys.argv):
            package = sys.argv[index + 1]

    manifest = load_manifest(package)
    source_png = os.path.join(package, "assets", "brand", "seeed-logo-preview.png")
    with open(source_png, "rb") as fp:
        png_bytes = fp.read()
    png_sha = hashlib.sha256(png_bytes).hexdigest()

    image = Image.open(source_png).convert("RGBA")
    rect = image.getbbox()
    if rect is None:
        raise SystemExit("[brand] the logo snapshot is fully transparent")
    image = image.crop(rect)

    remapped = image.copy()
    remapped.putdata([remap_ink(pixel) for pixel in image.getdata()])

    scale = min(BOX_W / remapped.width, BOX_H / remapped.height)
    target = (max(1, round(remapped.width * scale)), max(1, round(remapped.height * scale)))
    resized = resize_premultiplied(remapped, target)
    payload = to_rgb565a8(resized)

    stride = target[0] * 2
    if len(payload) != stride * target[1] + target[0] * target[1]:
        raise SystemExit("[brand] unexpected payload size")

    os.makedirs(OUT_DIR, exist_ok=True)
    out_path = os.path.join(OUT_DIR, "xf_brand_logo.c")
    with open(out_path, "w", encoding="utf-8", newline="\n") as fp:
        fp.write("""/**
 * @file xf_brand_logo.c
 * @brief Generated file. Do not edit.
 *
 * Official Seeed Studio logo, sourced from the read-only handoff snapshot and
 * converted to an RGB565A8 LVGL image descriptor (no decoder needed at run
 * time, no heap allocation).
 *
 * Source SVG      : %(svg)s
 * Source SVG SHA  : %(svg_sha)s
 * Source PNG SHA  : %(png_sha)s
 * Ink remap       : #88B928 -> #8FC31F, #003948 -> #004966 (UI primary colours)
 * Display size    : %(width)d x %(height)d px, aspect preserved inside the
 *                   240 x 36 px header box (design note 6.1.2)
 * Regenerate with : python tools/gen_xf_brand.py
 */

#include "lvgl.h"

#include "xf_asset.h"

/** @brief Provenance record used by the traceability check (AC-18). */
const char *const xf_brand_logo_source_sha256 = "%(svg_sha)s";

static const uint8_t xf_brand_logo_map[] = {
%(payload)s
};

const lv_image_dsc_t xf_brand_logo = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565A8,
        .flags = 0,
        .w = %(width)d,
        .h = %(height)d,
        .stride = %(stride)d,
    },
    .data_size = sizeof(xf_brand_logo_map),
    .data = xf_brand_logo_map,
};
""" % {
            "svg": manifest.get("source_svg") or manifest["source_svg_absolute"],
            "svg_sha": manifest["source_sha256"],
            "png_sha": png_sha,
            "width": target[0],
            "height": target[1],
            "stride": stride,
            "payload": format_bytes(payload),
        })

    print("[brand] %dx%d px, %d bytes payload" % (target[0], target[1], len(payload)))
    print("[brand] wrote %s" % out_path)


if __name__ == "__main__":
    main()
