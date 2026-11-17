#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""一条命令重建全部构建期资源。

顺序很重要：先生成/更新固件源码（含中文文案），再按最终源码扫描字符集生成字库，
否则新增文案的字形不会被包含。

    python tools/build_assets.py            # 图片 + 目录 + 字库
    python tools/build_assets.py --no-font  # 只更新图片与目录
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
DEFAULT_PACKAGE = os.path.join(PROJECT, "resources")


def run(script: str, *args: str) -> None:
    cmd = [sys.executable, os.path.join(HERE, script), *args]
    print("\n==> " + " ".join(cmd))
    subprocess.run(cmd, check=True)


def main() -> None:
    ap = argparse.ArgumentParser(description="Rebuild generated firmware assets")
    ap.add_argument("--package", default=DEFAULT_PACKAGE, help="设计交接包根目录")
    ap.add_argument("--no-assets", action="store_true", help="跳过图片转换")
    ap.add_argument("--no-catalog", action="store_true", help="跳过产品目录生成")
    ap.add_argument("--no-font", action="store_true", help="跳过字库生成")
    ap.add_argument("--crop", action="store_true", help="图片转换时裁掉透明边距")
    args = ap.parse_args()

    catalog = os.path.join(args.package, "product_catalog.json")
    manifest = os.path.join(args.package, "asset_manifest.json")

    if not args.no_assets:
        extra = ("--crop",) if args.crop else ()
        run("convert_assets.py", "--package", args.package, "--manifest", manifest, *extra)
    if not args.no_catalog:
        run("generate_catalog.py", "--catalog", catalog, "--manifest", manifest,
            "--out", os.path.join(PROJECT, "main", "model"))
    if not args.no_font:
        run("gen_fonts.py", "--catalog", catalog, "--out", os.path.join(PROJECT, "main", "assets"))

    print("\n资源构建完成。接下来： tools\\idf.bat build")


if __name__ == "__main__":
    main()
