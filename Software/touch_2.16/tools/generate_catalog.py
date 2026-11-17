#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从交接包 product_catalog.json 生成固件只读产品目录。

设计约束（设计说明 §4.1 / §9 / §10.2）：
  * product_id 为稳定业务主键，不使用图片名、排序或显示文案；
  * 页面代码内禁止复制并维护另一份参数常量 —— 所有参数只在本生成物中出现；
  * status/scope 原样保留，`null` 值一律渲染为“待核实”，绝不以 0 或推测值代替；
  * 生成物为确定的、可重复的（同一输入产生逐字节相同的输出）。

用法:
    python tools/generate_catalog.py \
        --catalog "<...>/product_catalog.json" \
        --manifest "<...>/asset_manifest.json" \
        --out main/model
"""

from __future__ import annotations

import argparse
import json
import os
from typing import Any

# ---------------------------------------------------------------------------
# 字段定义表
#   key             : product_catalog.json -> products[].specs 中的键
#   name_zh         : 界面展示名
#   detail_group    : 详情页分组 (0 计算与存储 / 1 无线与接口 / 2 传感与扩展 /
#                     3 供电与尺寸 / 4 应用领域)
#   compare_category: 对比页分类 (0 计算与存储 / 1 连接与接口 / 2 传感与应用,
#                     -1 表示不参与对比)
# ---------------------------------------------------------------------------
DG_COMPUTE, DG_WIRELESS, DG_SENSE, DG_POWER, DG_APP = range(5)
CC_COMPUTE, CC_CONNECT, CC_SENSE = range(3)

# (key, 中文名, detail_group, compare_category)
FIELD_DEFS = [
    ("processor",         "MCU / 架构",        DG_COMPUTE,  CC_COMPUTE),
    ("cpu_clock_max",     "最高主频",          DG_COMPUTE,  CC_COMPUTE),
    ("sram",              "SRAM",              DG_COMPUTE,  CC_COMPUTE),
    ("internal_nvm",      "片内非易失存储",    DG_COMPUTE,  CC_COMPUTE),
    ("external_flash",    "板载外部 Flash",    DG_COMPUTE,  CC_COMPUTE),
    ("psram",             "PSRAM",             DG_COMPUTE,  CC_COMPUTE),
    ("npu_pio",           "NPU / PIO",         DG_COMPUTE,  CC_COMPUTE),
    ("wifi",              "Wi-Fi",             DG_WIRELESS, CC_CONNECT),
    ("ble",               "Bluetooth LE",      DG_WIRELESS, CC_CONNECT),
    ("mesh",              "Thread/Zigbee/Matter", DG_WIRELESS, CC_CONNECT),
    ("exposed_gpio",      "对外可用 GPIO",     DG_WIRELESS, CC_CONNECT),
    ("interfaces",        "接口",              DG_WIRELESS, CC_CONNECT),
    ("imu",               "IMU",               DG_SENSE,    CC_SENSE),
    ("microphone",        "麦克风",            DG_SENSE,    CC_SENSE),
    ("camera",            "摄像头",            DG_SENSE,    CC_SENSE),
    ("battery_charging",  "板载电池充电",      DG_POWER,    CC_SENSE),
    ("footprint",         "核心板占板尺寸",    DG_POWER,    CC_SENSE),
    ("applications",      "适用领域",          DG_APP,      CC_SENSE),
]

DETAIL_GROUP_NAMES = ["计算与存储", "无线与接口", "传感与扩展", "供电与尺寸", "应用领域"]
COMPARE_CATEGORY_NAMES = ["计算与存储", "连接与接口", "传感与应用"]

STATUS_VERIFIED, STATUS_PENDING, STATUS_CONFLICT, STATUS_NOT_APPLICABLE = range(4)
SCOPE_BOARD, SCOPE_SOC = range(2)

TEXT_PENDING = "待核实"
TEXT_CONFLICT = "待核实"
TEXT_NOT_APPLICABLE = "不适用"
TEXT_NONE = "无"
SOC_SUFFIX = "（芯片能力）"
CONFLICT_SUFFIX = "（来源口径冲突）"


# ---------------------------------------------------------------------------
# C 源码输出辅助
# ---------------------------------------------------------------------------
def c_escape(text: str) -> str:
    out = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif ord(ch) < 0x20:
            out.append("\\x%02x" % ord(ch))
        else:
            out.append(ch)
    return "".join(out)


def c_str(text: str | None) -> str:
    if text is None:
        return "NULL"
    return '"%s"' % c_escape(text)


def c_ident(text: str) -> str:
    out = []
    for ch in text.upper():
        out.append(ch if (ch.isalnum() and ord(ch) < 128) else "_")
    while "__" in "".join(out):
        idx = "".join(out).find("__")
        out = list("".join(out)[:idx] + "_" + "".join(out)[idx + 2:])
    return "".join(out)


def header_banner(rel_source: str, extra: str = "") -> str:
    lines = [
        "/*",
        " * 自动生成文件 —— 请勿手工编辑。",
        " * 由 tools/generate_catalog.py 依据交接包数据生成：",
        f" *   {rel_source}",
    ]
    if extra:
        lines.append(f" *   {extra}")
    lines += [
        " * 重新生成: python tools/build_assets.py",
        " */",
        "",
    ]
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# 数据归一化
# ---------------------------------------------------------------------------
def status_of(spec: dict[str, Any] | None) -> int:
    if not spec:
        return STATUS_PENDING
    raw = (spec.get("status") or "pending").strip()
    return {
        "verified": STATUS_VERIFIED,
        "pending": STATUS_PENDING,
        "conflict": STATUS_CONFLICT,
        "not_applicable": STATUS_NOT_APPLICABLE,
    }.get(raw, STATUS_PENDING)


def scope_of(spec: dict[str, Any] | None) -> int:
    if not spec:
        return SCOPE_BOARD
    return SCOPE_SOC if (spec.get("scope") or "board") == "soc" else SCOPE_BOARD


def decorate(text: str, spec: dict[str, Any] | None) -> str:
    """按 status/scope 追加来源标记（芯片能力 / 口径冲突）。"""
    st = status_of(spec)
    if st == STATUS_PENDING:
        return TEXT_PENDING
    if st == STATUS_CONFLICT:
        return TEXT_CONFLICT + CONFLICT_SUFFIX
    if st == STATUS_NOT_APPLICABLE:
        return TEXT_NOT_APPLICABLE
    if scope_of(spec) == SCOPE_SOC:
        return text + SOC_SUFFIX
    return text


def fmt_number(value: Any, unit: str | None) -> tuple[str, str | None]:
    """数值: 展示值 + 单位（允许分两行显示）。"""
    if isinstance(value, float):
        num = ("%g" % value)
    else:
        num = str(value)
    return num, unit


def fmt_bytes_like(spec: dict[str, Any]) -> tuple[str, str | None]:
    """容量类字段。0 只在“确认没有”时出现，此时显示“无”。"""
    value = spec.get("value")
    unit = spec.get("unit")
    if value is None:
        return TEXT_PENDING, None
    if value == 0:
        return TEXT_NONE, None
    return fmt_number(value, unit)


def fmt_list(items: list[str] | None) -> str:
    if not items:
        return TEXT_NONE
    return "\n".join(items)


def pick(specs: dict[str, Any], key: str) -> dict[str, Any] | None:
    return specs.get(key)


def build_field(product: dict[str, Any], field_key: str) -> tuple[str, str | None, int, int]:
    """返回 (展示值, 单位, status, scope)。"""
    specs = product.get("specs") or {}

    if field_key == "applications":
        apps = product.get("applications_zh") or []
        text = fmt_list(apps) if apps else TEXT_PENDING
        return text, None, STATUS_VERIFIED, SCOPE_BOARD

    if field_key in ("wifi", "ble", "mesh"):
        spec = pick(specs, "wireless")
        if spec is None:
            return TEXT_PENDING, None, STATUS_PENDING, SCOPE_BOARD
        st, sc = status_of(spec), scope_of(spec)
        if st != STATUS_VERIFIED:
            return decorate("", spec), None, st, sc
        items = [str(x) for x in (spec.get("value") or [])]
        if field_key == "wifi":
            sel = [x for x in items if "Wi-Fi" in x]
        elif field_key == "ble":
            sel = [x for x in items if "Bluetooth" in x]
        else:
            sel = [x for x in items if any(k in x for k in ("Thread", "Zigbee", "Matter"))]
        return decorate(fmt_list(sel), spec) if sel else decorate(TEXT_NONE, spec), None, st, sc

    if field_key in ("imu", "microphone", "camera"):
        spec = pick(specs, "sensors")
        if spec is None:
            return TEXT_PENDING, None, STATUS_PENDING, SCOPE_BOARD
        st, sc = status_of(spec), scope_of(spec)
        if st != STATUS_VERIFIED:
            return decorate("", spec), None, st, sc
        items = [str(x) for x in (spec.get("value") or [])]
        if field_key == "imu":
            sel = [x for x in items if "IMU" in x]
        elif field_key == "microphone":
            sel = [x for x in items if "microphone" in x.lower()]
        else:
            sel = [x for x in items if "Camera" in x]
        return decorate(fmt_list(sel) if sel else TEXT_NONE, spec), None, st, sc

    if field_key == "npu_pio":
        parts: list[str] = []
        statuses: list[int] = []
        scopes: list[int] = []
        npu = pick(specs, "npu")
        pio = pick(specs, "pio_state_machines")
        for spec in (npu, pio):
            if spec is None:
                continue
            st, sc = status_of(spec), scope_of(spec)
            if st != STATUS_VERIFIED:
                statuses.append(st)
                scopes.append(sc)
                continue
            value = spec.get("value")
            if isinstance(value, str):
                parts.append(value)
            else:
                unit = spec.get("unit")
                txt = str(value) + ((" " + unit) if unit and unit != "count" else "")
                parts.append(txt)
            statuses.append(st)
            scopes.append(sc)
        if not parts and not statuses:
            # 数据源没有该字段：不得推断为“无”，按待核实处理。
            return TEXT_PENDING, None, STATUS_PENDING, SCOPE_BOARD
        if not parts:
            st = STATUS_CONFLICT if STATUS_CONFLICT in statuses else STATUS_PENDING
            return (TEXT_CONFLICT + CONFLICT_SUFFIX if st == STATUS_CONFLICT else TEXT_PENDING), None, st, SCOPE_BOARD
        text = "\n".join(parts)
        scope = SCOPE_SOC if SCOPE_SOC in scopes else SCOPE_BOARD
        if scope == SCOPE_SOC:
            text += SOC_SUFFIX
        return text, None, STATUS_VERIFIED, scope

    if field_key == "internal_nvm":
        spec = pick(specs, "internal_nvm")
        type_spec = pick(specs, "internal_nvm_type")
        if spec is None:
            return TEXT_PENDING, None, STATUS_PENDING, SCOPE_BOARD
        st, sc = status_of(spec), scope_of(spec)
        if st != STATUS_VERIFIED:
            return decorate("", spec), None, st, sc
        value = spec.get("value")
        if value is None:
            return TEXT_PENDING, None, STATUS_PENDING, sc
        if value == 0:
            return decorate(TEXT_NONE, spec), None, st, sc
        num, unit = fmt_number(value, spec.get("unit"))
        text = num if not unit else f"{num} {unit}"
        if type_spec and type_spec.get("value"):
            text += f" {type_spec['value']}"
        return decorate(text, spec), None, st, sc

    if field_key == "battery_charging":
        spec = pick(specs, "onboard_battery_charging")
        if spec is None:
            return TEXT_PENDING, None, STATUS_PENDING, SCOPE_BOARD
        st, sc = status_of(spec), scope_of(spec)
        if st != STATUS_VERIFIED:
            return decorate("", spec), None, st, sc
        return decorate("支持" if spec.get("value") else "不支持", spec), None, st, sc

    if field_key == "footprint":
        spec = pick(specs, "footprint")
        if spec is None:
            return TEXT_PENDING, None, STATUS_PENDING, SCOPE_BOARD
        st, sc = status_of(spec), scope_of(spec)
        if st != STATUS_VERIFIED:
            return decorate("", spec), None, st, sc
        value = spec.get("value")
        if not value:
            return TEXT_PENDING, None, STATUS_PENDING, sc
        dims = value if isinstance(value, list) else [value]
        text = " × ".join(("%g" % v) if isinstance(v, float) else str(v) for v in dims)
        return decorate(text, spec), spec.get("unit"), st, sc

    # 通用：数值 / 字符串 / 列表
    spec = pick(specs, field_key)
    if spec is None:
        return TEXT_PENDING, None, STATUS_PENDING, SCOPE_BOARD
    st, sc = status_of(spec), scope_of(spec)
    if st != STATUS_VERIFIED:
        return decorate("", spec), None, st, sc
    value = spec.get("value")
    if value is None:
        return TEXT_PENDING, None, STATUS_PENDING, sc
    if isinstance(value, list):
        return decorate(fmt_list([str(v) for v in value]), spec), None, st, sc
    if isinstance(value, bool):
        return decorate("支持" if value else "不支持", spec), None, st, sc
    if isinstance(value, (int, float)):
        txt, unit = fmt_bytes_like(spec)
        return decorate(txt, spec), unit, st, sc
    return decorate(str(value), spec), spec.get("unit"), st, sc


# ---------------------------------------------------------------------------
# 生成
# ---------------------------------------------------------------------------
def generate(catalog_path: str, manifest_path: str, out_dir: str) -> None:
    with open(catalog_path, encoding="utf-8") as fp:
        catalog = json.load(fp)
    with open(manifest_path, encoding="utf-8") as fp:
        manifest = json.load(fp)

    products = catalog["products"]
    if len(products) != catalog.get("product_count", len(products)):
        raise SystemExit("product_count 与 products 数量不一致")

    manifest_by_pid = {a["product_id"]: a for a in manifest["assets"]}

    # 厂商按目录中首次出现顺序编号（筛选条顺序稳定）
    manufacturers: list[str] = []
    for p in products:
        if p["manufacturer"] not in manufacturers:
            manufacturers.append(p["manufacturer"])

    ids = [p["id"] for p in products]
    if len(set(ids)) != len(ids):
        raise SystemExit("product_id 重复")
    asset_keys = [p["asset_key"] for p in products]
    if len(set(asset_keys)) != len(asset_keys):
        raise SystemExit("asset_key 重复")

    rel_catalog = os.path.basename(catalog_path)
    rel_manifest = os.path.basename(manifest_path)

    # ---------------- product_ids.h ----------------
    h = []
    h.append(header_banner(rel_catalog, rel_manifest))
    h.append("#pragma once\n")
    h.append("/* 稳定业务主键：编号即默认轮播顺序，不得重排已有项。 */")
    h.append("typedef enum {")
    for p in products:
        h.append(f"    PRODUCT_{c_ident(p['id'])} = {products.index(p)},")
    h.append(f"    PRODUCT_COUNT = {len(products)},")
    h.append("    PRODUCT_ID_INVALID = 0xFFFF,")
    h.append("} product_id_t;\n")

    h.append("/* 图片资源键：与 product_id 一一对应，可独立替换。 */")
    h.append("typedef enum {")
    for p in products:
        h.append(f"    ASSET_{c_ident(p['asset_key'])} = {products.index(p)},")
    h.append(f"    ASSET_COUNT = {len(products)},")
    h.append("} asset_id_t;\n")

    h.append(f"#define PRODUCT_MANUFACTURER_COUNT {len(manufacturers)}")
    h.append(f"#define DETAIL_GROUP_COUNT {len(DETAIL_GROUP_NAMES)}")
    h.append(f"#define COMPARE_CATEGORY_COUNT {len(COMPARE_CATEGORY_NAMES)}")
    h.append(f"#define SPEC_FIELD_COUNT {len(FIELD_DEFS)}\n")
    h.append("/* 轮播顺序 -> product_id 查表（即 0..PRODUCT_COUNT-1） */")
    h.append(f"#define PRODUCT_AUTOPLAY_COUNT {len(products)}")
    h.append("")
    with open(os.path.join(out_dir, "product_ids.h"), "w", encoding="utf-8", newline="\n") as fp:
        fp.write("\n".join(h))

    # ---------------- product_catalog_data.c ----------------
    c = []
    c.append(header_banner(rel_catalog, rel_manifest))
    c.append('#include "product_catalog.h"\n')

    c.append("const char *const g_manufacturer_names[PRODUCT_MANUFACTURER_COUNT] = {")
    for m in manufacturers:
        c.append(f"    {c_str(m)},")
    c.append("};\n")

    c.append("const char *const g_detail_group_names[DETAIL_GROUP_COUNT] = {")
    for n in DETAIL_GROUP_NAMES:
        c.append(f"    {c_str(n)},")
    c.append("};\n")

    c.append("const char *const g_compare_category_names[COMPARE_CATEGORY_COUNT] = {")
    for n in COMPARE_CATEGORY_NAMES:
        c.append(f"    {c_str(n)},")
    c.append("};\n")

    c.append("const char *const g_field_names[SPEC_FIELD_COUNT] = {")
    for key, name, _dg, _cc in FIELD_DEFS:
        c.append(f"    {c_str(name)}, /* {key} */")
    c.append("};\n")

    c.append("const uint8_t g_field_detail_group[SPEC_FIELD_COUNT] = {")
    c.append("    " + ", ".join(str(dg) for _k, _n, dg, _cc in FIELD_DEFS))
    c.append("};\n")

    c.append("const int8_t g_field_compare_category[SPEC_FIELD_COUNT] = {")
    c.append("    " + ", ".join(str(cc) for _k, _n, _dg, cc in FIELD_DEFS))
    c.append("};\n")

    c.append("const char *const g_product_id_strings[PRODUCT_COUNT] = {")
    for p in products:
        c.append(f"    {c_str(p['id'])},")
    c.append("};\n")

    c.append("const char *const g_asset_keys[ASSET_COUNT] = {")
    for p in products:
        c.append(f"    {c_str(p['asset_key'])},")
    c.append("};\n")

    # products
    c.append("const product_t g_products[PRODUCT_COUNT] = {")
    for p in products:
        entry = manifest_by_pid.get(p["id"], {})
        is_placeholder = bool(entry.get("is_placeholder", False))
        placeholder_pid = entry.get("placeholder_product_id")
        placeholder_id = "PRODUCT_ID_INVALID"
        if placeholder_pid:
            placeholder_id = "PRODUCT_" + c_ident(placeholder_pid)
        apps = p.get("applications_zh") or []
        notes = (p.get("notes") or "").strip()
        sources = " / ".join(p.get("source_ids") or [])
        c.append("    {")
        c.append(f"        .id = PRODUCT_{c_ident(p['id'])},")
        c.append(f"        .id_string = {c_str(p['id'])},")
        c.append(f"        .name = {c_str(p['name'])},")
        c.append(f"        .short_name = {c_str(p.get('short_name') or p['name'])},")
        c.append(f"        .manufacturer = {c_str(p['manufacturer'])},")
        c.append(f"        .manufacturer_index = {manufacturers.index(p['manufacturer'])},")
        c.append(f"        .feature_en = {c_str(p.get('feature_en') or '')},")
        c.append(f"        .feature_zh = {c_str(p.get('feature_zh') or '')},")
        c.append(f"        .applications = {c_str(chr(10).join(apps)) if apps else c_str(TEXT_PENDING)},")
        c.append(f"        .notes = {c_str(notes)},")
        c.append(f"        .sources = {c_str(sources)},")
        c.append(f"        .asset = ASSET_{c_ident(p['asset_key'])},")
        c.append(f"        .is_placeholder = {'true' if is_placeholder else 'false'},")
        c.append(f"        .placeholder_product_id = {placeholder_id},")
        c.append("    },")
    c.append("};\n")

    # specs
    c.append("const spec_field_t g_specs[PRODUCT_COUNT][SPEC_FIELD_COUNT] = {")
    for p in products:
        c.append(f"    /* {p['id']} */")
        c.append("    {")
        for key, name, _dg, _cc in FIELD_DEFS:
            text, unit, st, sc = build_field(p, key)
            c.append(
                f"        [{SPEC_INDEX[key]}] = {{ .value = {c_str(text)}, .unit = {c_str(unit)}, "
                f".status = {st}, .scope = {sc} }}, /* {name} */"
            )
        c.append("    },")
    c.append("};\n")

    with open(os.path.join(out_dir, "product_catalog_data.c"), "w", encoding="utf-8", newline="\n") as fp:
        fp.write("\n".join(c))

    # ---------------- product_assets.c ----------------
    a = []
    a.append(header_banner(rel_manifest))
    a.append('#include "product_assets.h"\n')
    for p in products:
        key = p["asset_key"]
        base = c_ident(key).lower()
        a.append(f"extern const uint8_t hero_{key}_bin_start[] asm(\"_binary_hero_{key}_bin_start\");")
        a.append(f"extern const uint8_t thumb_{key}_bin_start[] asm(\"_binary_thumb_{key}_bin_start\");")
    a.append("")
    for p in products:
        key = p["asset_key"]
        for kind, w, h in (("hero", "PRODUCT_IMAGE_HERO_W", "PRODUCT_IMAGE_HERO_H"),
                           ("thumb", "PRODUCT_IMAGE_THUMB_W", "PRODUCT_IMAGE_THUMB_H")):
            a.append(f"static const lv_image_dsc_t img_{kind}_{key} = {{")
            a.append("    .header.magic = LV_IMAGE_HEADER_MAGIC,")
            a.append("    .header.cf = LV_COLOR_FORMAT_RGB565A8,")
            a.append(f"    .header.w = {w},")
            a.append(f"    .header.h = {h},")
            a.append(f"    .header.stride = {w} * 2,")
            a.append(f"    .data_size = {w} * {h} * 3,")
            a.append(f"    .data = {kind}_{key}_bin_start,")
            a.append("};")
    a.append("")
    a.append("const lv_image_dsc_t *const g_product_hero[ASSET_COUNT] = {")
    for p in products:
        a.append(f"    [ASSET_{c_ident(p['asset_key'])}] = &img_hero_{p['asset_key']},")
    a.append("};\n")
    a.append("const lv_image_dsc_t *const g_product_thumb[ASSET_COUNT] = {")
    for p in products:
        a.append(f"    [ASSET_{c_ident(p['asset_key'])}] = &img_thumb_{p['asset_key']},")
    a.append("};\n")
    with open(os.path.join(out_dir, "product_assets.c"), "w", encoding="utf-8", newline="\n") as fp:
        fp.write("\n".join(a))

    print(f"[catalog] {len(products)} products, {len(manufacturers)} manufacturers, "
          f"{len(FIELD_DEFS)} fields -> {out_dir}")


SPEC_INDEX = {key: idx for idx, (key, *_rest) in enumerate(FIELD_DEFS)}


def main() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    default_pkg = os.path.normpath(os.path.join(here, "..", "resources"))
    ap = argparse.ArgumentParser(description="Generate firmware product catalog from handoff JSON")
    ap.add_argument("--catalog", default=os.path.join(default_pkg, "product_catalog.json"))
    ap.add_argument("--manifest", default=os.path.join(default_pkg, "asset_manifest.json"))
    ap.add_argument("--out", default=os.path.join(here, "..", "main", "model"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    generate(args.catalog, args.manifest, args.out)


if __name__ == "__main__":
    main()
