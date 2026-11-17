#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate the UI fonts used by the XIAO Family 2.16 firmware.

English and the Chinese glyphs used by the generated UI copy are embedded.
Only installed fonts and the existing local converter are used; no downloads.
Output goes to ``components/xf_assets/generated`` and is embedded by the
``xf_assets`` component; nothing outside that directory is written.

Usage:
    python tools/gen_xf_fonts.py
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
OUT_DIR = os.path.join(PROJECT, "components", "xf_assets", "generated")

# (C symbol name, pixel size, bits per pixel)
# Base: 32 px title, 22 px body, 18 px helper; detail: 29/23 px (~130%).
# (design note section 6.1 typography minimum is 18 px).
FONTS = (
    ("xf_font_ui_32", 32, 4),
    ("xf_font_ui_22", 22, 4),
    ("xf_font_ui_18", 18, 4),
    ("xf_font_detail_23", 23, 4),
    ("xf_font_detail_29", 29, 4),
)

FONT_CANDIDATES = (
    r"C:\Windows\Fonts\NotoSansSC-VF.ttf",
    r"C:\Windows\Fonts\segoeui.ttf",
    r"C:\Windows\Fonts\arial.ttf",
)

HEADER_TEMPLATE = """/**
 * @file xf_fonts.h
 * @brief Generated file. Do not edit.
 *
 * Source: {font_name}, printable ASCII plus the used Chinese subset, LVGL 9 at 4 bpp.
 * Regenerate with: python tools/gen_xf_fonts.py
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {{
#endif

{decls}

#ifdef __cplusplus
}}
#endif
"""


def find_font() -> str:
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return path
    raise SystemExit("no usable TTF font found; expected one of: " + ", ".join(FONT_CANDIDATES))


def font_conv_cli() -> str:
    node = shutil.which("node")
    if not node:
        raise SystemExit("node not found on PATH")
    cli = os.path.join(HERE, "node_modules", "lv_font_conv", "lv_font_conv.js")
    if not os.path.exists(cli):
        raise SystemExit("lv_font_conv is not installed locally: " + cli)
    return cli


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    font_path = find_font()
    # Read only the generated firmware text, not comments or unrelated files.
    strings = Path(PROJECT, "components/xf_ui/generated/xf_strings.c").read_text(encoding="utf-8")
    symbols = "".join(sorted({c for c in strings if ord(c) > 127}))
    if not symbols:
        raise SystemExit("generate bilingual strings before fonts")
    if os.path.basename(font_path) != "NotoSansSC-VF.ttf":
        raise SystemExit("installed Chinese font NotoSansSC-VF.ttf is required; no fallback download")
    print("[font] Chinese/supplemental glyphs: %d" % len(symbols))
    cli = font_conv_cli()
    node = shutil.which("node")

    for symbol, size, bpp in FONTS:
        out = os.path.join(OUT_DIR, symbol + ".c")
        cmd = [
            node, cli,
            "--font", font_path,
            "--range", "0x20-0x7F",
            "--symbols", symbols,
            "--size", str(size),
            "--bpp", str(bpp),
            "--format", "lvgl",
            "--lv-include", "lvgl.h",
            "--no-compress",
            "--force-fast-kern-format",
            "-o", out,
        ]
        print("[font] %s size=%d bpp=%d" % (symbol, size, bpp))
        proc = subprocess.run(cmd, capture_output=True, text=True, shell=False)
        if proc.returncode != 0:
            sys.stderr.write(proc.stderr)
            raise SystemExit("lv_font_conv failed for " + symbol)

    decls = "\n".join("extern const lv_font_t %s;" % name for name, _, _ in FONTS)
    with open(os.path.join(OUT_DIR, "xf_fonts.h"), "w", encoding="utf-8", newline="\n") as fp:
        fp.write(HEADER_TEMPLATE.format(font_name=os.path.basename(font_path), decls=decls))

    total = sum(os.path.getsize(os.path.join(OUT_DIR, n + ".c")) for n, _, _ in FONTS)
    print("[font] wrote %d files, %d bytes of C source -> %s" % (len(FONTS), total, OUT_DIR))


if __name__ == "__main__":
    main()