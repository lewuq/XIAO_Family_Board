#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""按固件实际使用到的字符，生成最小化的 LVGL 中文字库。

字符集来源（自动扫描，避免手工维护子集表造成缺字）：
    1. main/**/*.c、main/**/*.h 中的全部字符串；
    2. 交接包 product_catalog.json 中的全部文案；
    3. 少量界面符号（●、✓、×、—、≥ …）。

设计说明 §5.2 允许“按实际使用字符做子集化”，但要求任何设计文案改动后重新生成字体。
因此本脚本每次构建都会重新扫描：新增文案若不在字库中，会在构建日志中以
"[font] MISSING GLYPH" 提示（由 verify_charset() 完成）。
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

EXTRA_SYMBOLS = "·×÷±°—–…≥≤→←↑↓●○◆◇★☆✓✔✕“”‘’、。，；：！？（）【】《》"

# (符号名, 字号, bpp)：32 标题 / 22 正文 / 18 辅助说明（设计说明 §5.2）
FONT_SIZES = [
    ("font_ui_32", 32, 4),
    ("font_ui_22", 22, 4),
    ("font_ui_18", 18, 4),
]

FONT_CANDIDATES = [
    r"C:\Windows\Fonts\NotoSansSC-VF.ttf",
    r"C:\Windows\Fonts\msyh.ttc",
    r"C:\Windows\Fonts\simhei.ttf",
]


def collect_chars(paths: list[str], catalog_json: str | None) -> list[str]:
    chars: set[str] = set()
    for path in paths:
        try:
            with open(path, encoding="utf-8") as fp:
                text = fp.read()
        except (UnicodeDecodeError, OSError):
            continue
        chars.update(text)
    if catalog_json and os.path.exists(catalog_json):
        with open(catalog_json, encoding="utf-8") as fp:
            chars.update(fp.read())
    chars.update(EXTRA_SYMBOLS)
    # 可打印 ASCII 全量（数字/拉丁/标点由 --range 0x20-0x7F 覆盖，这里保留过滤）
    out = sorted(ch for ch in chars if ch >= " " and ch != "\u00a0")
    return out


def find_font() -> str:
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return path
    raise SystemExit("找不到可用中文字体（尝试过：" + ", ".join(FONT_CANDIDATES) + "）")


def font_conv_cli() -> str:
    """定位本地安装的 lv_font_conv。

    不使用 `npx`：Windows 下 npx 是 .cmd 脚本，必须经 cmd.exe 解析，而中文字符集参数里
    含有 `>`、`&` 一类的 cmd 元字符，会导致命令被截断（`> was unexpected at this time`）。
    直接以 node + 脚本路径调用，参数由 CreateProcess 传递，无需任何 shell 转义。
    """
    node = shutil.which("node")
    if not node:
        raise SystemExit("找不到 node，请先安装 Node.js")
    here = os.path.dirname(os.path.abspath(__file__))
    cli = os.path.join(here, "node_modules", "lv_font_conv", "lv_font_conv.js")
    if not os.path.exists(cli):
        print("[font] 首次运行，安装 lv_font_conv@1.5.2 ...")
        subprocess.run([shutil.which("npm") or "npm", "install", "lv_font_conv@1.5.2",
                        "--no-audit", "--no-fund"], cwd=here, check=True)
    if not os.path.exists(cli):
        raise SystemExit(f"lv_font_conv 未安装成功：{cli}")
    return cli


def run_font_conv(font_path: str, symbol: str, size: int, bpp: int, out_path: str,
                  symbols: list[str], cli: str | None = None) -> None:
    cli = cli or font_conv_cli()
    cmd = [
        shutil.which("node") or "node", cli,
        "--font", font_path,
        "--range", "0x20-0x7F",
        "--symbols", "".join(symbols),
        "--size", str(size),
        "--bpp", str(bpp),
        "--format", "lvgl",
        "--lv-include", "lvgl.h",
        "--no-compress",
        "--force-fast-kern-format",
        "-o", out_path,
    ]
    print(f"[font] {symbol} size={size} bpp={bpp} chars={len(symbols) + 96}")
    proc = subprocess.run(cmd, capture_output=True, text=True, shell=False)
    if proc.stdout.strip():
        print(proc.stdout.strip())
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        raise SystemExit(f"lv_font_conv 失败: {symbol}")
    warns = [ln for ln in (proc.stderr or "").splitlines() if "warn" in ln.lower()]
    if warns:
        print("[font] " + "\n[font] ".join(warns[:10]))


def verify_charset(charset: list[str], sources: list[str]) -> None:
    """文案里的可打印字符是否都在字库中（字库按同一字符集生成，此处只做兜底提示）。"""
    have = set(charset)
    missing: set[str] = set()
    for path in sources:
        try:
            with open(path, encoding="utf-8") as fp:
                text = fp.read()
        except (UnicodeDecodeError, OSError):
            continue
        for ch in text:
            if ch > "\x7f" and ch not in have:
                missing.add(ch)
    if missing:
        print("[font] MISSING GLYPH: " + "".join(sorted(missing)))


def main() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    project = os.path.normpath(os.path.join(here, ".."))
    default_catalog = os.path.join(project, "resources", "product_catalog.json")

    ap = argparse.ArgumentParser(description="Generate subset CJK LVGL fonts")
    ap.add_argument("--src", default=os.path.join(project, "main"), help="扫描字符的源码目录")
    ap.add_argument("--catalog", default=default_catalog)
    ap.add_argument("--out", default=os.path.join(project, "main", "assets"))
    ap.add_argument("--font", default=None)
    args = ap.parse_args()

    sources = sorted(glob.glob(os.path.join(args.src, "**", "*.c"), recursive=True) +
                     glob.glob(os.path.join(args.src, "**", "*.h"), recursive=True))
    charset = collect_chars(sources, args.catalog)
    cjk = [c for c in charset if ord(c) > 0x7F]
    print(f"[font] scanned {len(sources)} sources, {len(cjk)} non-ASCII chars")

    os.makedirs(args.out, exist_ok=True)
    font_path = args.font or find_font()
    print(f"[font] using {font_path}")

    cli = font_conv_cli()
    for symbol, size, bpp in FONT_SIZES:
        run_font_conv(font_path, symbol, size, bpp,
                      os.path.join(args.out, f"{symbol}.c"), charset, cli)

    verify_charset(charset, sources)

    with open(os.path.join(args.out, "font_charset.txt"), "w", encoding="utf-8", newline="\n") as fp:
        fp.write("".join(charset) + "\n")


if __name__ == "__main__":
    main()
