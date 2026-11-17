#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""将 resources 中的产品 PNG 转换为固件使用的 RGB565A8 数据。

产物（main/assets/img/hero_<asset_key>.bin、thumb_<asset_key>.bin）由
components/xf_assets/CMakeLists.txt 通过 target_add_binary_data 链接到固件，
运行时直接读取像素数据，无需图片解码。

像素格式与 LVGL 9 的 LV_COLOR_FORMAT_RGB565A8 完全一致：
    [w*h 个 uint16 little-endian RGB565] + [w*h 个 uint8 alpha]

设计说明 §5 要求：
  * 只缩放、不裁剪、不拉伸主体图形（保持原始宽高比，居中留白）；
  * 透明背景，不使用白底方图；
  * LM20B 允许复用 LM20A 素材，但必须在运行时标注“占位”。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct

from PIL import Image

HERO_W, HERO_H = 240, 260
THUMB_W, THUMB_H = 96, 104


def fit_canvas(img: Image.Image, width: int, height: int, crop: bool) -> tuple[Image.Image, dict]:
    """等比缩放到画布内并居中，保持宽高比，不做非等比拉伸。"""
    src_w, src_h = img.size
    bbox = img.getchannel("A").getbbox()
    info: dict = {"source_size": [src_w, src_h], "content_bbox": list(bbox) if bbox else None}

    work = img
    if crop and bbox:
        work = img.crop(bbox)

    w, h = work.size
    scale = min(width / w, height / h)
    new_w = max(1, int(round(w * scale)))
    new_h = max(1, int(round(h * scale)))
    resized = work.resize((new_w, new_h), Image.LANCZOS)

    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    canvas.paste(resized, ((width - new_w) // 2, (height - new_h) // 2), resized)
    info.update({"scale": round(scale, 4), "placed_size": [new_w, new_h]})
    return canvas, info


def to_rgb565a8(img: Image.Image) -> bytes:
    """RGBA -> RGB565(LE) 平面 + A8 平面。"""
    rgba = img.convert("RGBA")
    px = rgba.load()
    w, h = rgba.size
    rgb = bytearray(w * h * 2)
    alpha = bytearray(w * h)
    idx = 0
    aidx = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            # 预乘会在深色背景上产生灰边，这里保持直通颜色 + 独立 alpha
            value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            rgb[idx] = value & 0xFF
            rgb[idx + 1] = (value >> 8) & 0xFF
            idx += 2
            alpha[aidx] = a
            aidx += 1
    return bytes(rgb) + bytes(alpha)


def convert(pkg_root: str, manifest_path: str, out_dir: str, crop: bool) -> dict:
    with open(manifest_path, encoding="utf-8") as fp:
        manifest = json.load(fp)

    os.makedirs(out_dir, exist_ok=True)
    report = {"assets": [], "hero": [HERO_W, HERO_H], "thumb": [THUMB_W, THUMB_H],
              "pixel_format": "RGB565A8 (uint16 LE RGB565 plane + uint8 alpha plane)"}
    total = 0

    for entry in manifest["assets"]:
        asset_key = entry["asset_key"]
        record: dict = {"asset_key": asset_key, "product_id": entry["product_id"],
                        "is_placeholder": bool(entry.get("is_placeholder")),
                        "placeholder_product_id": entry.get("placeholder_product_id")}
        for kind, (w, h) in (("hero", (HERO_W, HERO_H)), ("thumb", (THUMB_W, THUMB_H))):
            rel = entry.get(f"{kind}_png")
            if not rel:
                raise SystemExit(f"{asset_key}: manifest 缺少 {kind}_png")
            src = os.path.join(pkg_root, rel.replace("/", os.sep))
            if not os.path.exists(src):
                raise SystemExit(f"缺少图片：{src}")
            with open(src, "rb") as fp:
                png_sha = hashlib.sha256(fp.read()).hexdigest()
            img = Image.open(src).convert("RGBA")
            canvas, info = fit_canvas(img, w, h, crop)
            data = to_rgb565a8(canvas)
            dst = os.path.join(out_dir, f"{kind}_{asset_key}.bin")
            with open(dst, "wb") as fp:
                fp.write(data)
            total += len(data)
            record[kind] = {
                "src": rel,
                "png_sha256": png_sha,
                "bin": os.path.basename(dst),
                "bin_bytes": len(data),
                **info,
            }
        report["assets"].append(record)

    report["total_bytes"] = total
    report_path = os.path.join(os.path.dirname(out_dir), "..", "docs", "asset_report.json")
    report_path = os.path.normpath(report_path)
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, "w", encoding="utf-8", newline="\n") as fp:
        json.dump(report, fp, ensure_ascii=False, indent=2)
        fp.write("\n")

    print(f"[assets] {len(report['assets'])} assets, {total} bytes "
          f"({total / 1024 / 1024:.2f} MiB) -> {out_dir}")
    return report


def main() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    default_pkg = os.path.normpath(os.path.join(here, "..", "resources"))
    ap = argparse.ArgumentParser(description="Convert handoff PNGs to LVGL RGB565A8 blobs")
    ap.add_argument("--package", default=default_pkg, help="交接包根目录")
    ap.add_argument("--manifest", default=None)
    ap.add_argument("--out", default=os.path.join(here, "..", "main", "assets", "img"))
    ap.add_argument("--crop", action="store_true",
                    help="先裁掉透明边距再等比缩放（交接包 PNG 已完成构图，默认关闭）")
    args = ap.parse_args()

    manifest = args.manifest or os.path.join(args.package, "asset_manifest.json")
    convert(args.package, manifest, os.path.normpath(args.out), args.crop)


if __name__ == "__main__":
    main()
