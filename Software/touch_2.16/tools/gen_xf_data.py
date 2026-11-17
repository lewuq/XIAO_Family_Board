#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate the read-only product catalog consumed by ``xf_data``.

Inputs (read-only handoff data):
    product_catalog.json    catalogue records, English runtime fields only
    asset_manifest.json     stable asset keys / placeholder mapping
    ui_strings.en.json      message keys used for field and section labels

Outputs (writable whitelist: components/xf_data/**):
    components/xf_data/generated/xf_product_ids.h
    components/xf_data/generated/xf_product_catalog.c

Editorial Chinese fields (``feature_zh``, ``applications_zh``,
``editorial_notes_zh`` and per-field ``note``) are dropped so they can never be
compiled into visible copy (design note 6.1.1 / code rules 8.2).

Usage:
    python tools/gen_xf_data.py
"""

from __future__ import annotations

import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
OUT_DIR = os.path.join(PROJECT, "components", "xf_data", "generated")
DEFAULT_PACKAGE = r"C:\Users\seeed\Documents\ChatGPT\MFSZ 2026\屏幕交互设计"

# Display order of the spec rows. Mirrors design note section 9.2 and the
# section list of section 6.4; the message key is resolved by xf_ui.
FIELDS = (
    # (spec json key, message key, section, compare category or None)
    ("processor", "field.processor", "compute", "compute"),
    ("cpu_clock_max", "field.cpu_clock_max", "compute", "compute"),
    ("sram", "field.sram", "compute", "compute"),
    ("internal_nvm", "field.internal_nvm", "compute", "compute"),
    ("internal_nvm_type", "field.internal_nvm_type", "compute", "compute"),
    ("external_flash", "field.external_flash", "compute", "compute"),
    ("psram", "field.psram", "compute", "compute"),
    ("npu", "field.npu", "compute", "compute"),
    ("pio_state_machines", "field.pio_state_machines", "compute", "compute"),
    ("wireless", "field.wireless", "connect", "connect"),
    ("exposed_gpio", "field.exposed_gpio", "connect", "connect"),
    ("interfaces", "field.interfaces", "connect", "connect"),
    ("adc_resolution", "field.adc_resolution", "connect", "connect"),
    ("dac_resolution", "field.dac_resolution", "connect", "connect"),
    ("sensors", "field.sensors", "sensors", "sensors"),
    ("onboard_battery_charging", "field.onboard_battery_charging", "power", "sensors"),
    ("footprint", "field.footprint", "power", "sensors"),
    ("applications", "section.applications", "applications", "sensors"),
)

SECTIONS = (
    ("compute", "section.compute"),
    ("connect", "section.connect"),
    ("sensors", "section.sensors"),
    ("power", "section.power"),
    ("applications", "section.applications"),
)

COMPARE_CATEGORIES = (
    ("compute", "compare.tab.compute"),
    ("connect", "compare.tab.connect"),
    ("sensors", "compare.tab.sensors"),
)

# Spec values whose source text is a status marker rather than a quantity.
STATUS_MAP = {
    "verified": "XF_SPEC_VERIFIED",
    "pending": "XF_SPEC_PENDING",
    "conflict": "XF_SPEC_CONFLICT",
    "not_applicable": "XF_SPEC_NOT_APPLICABLE",
}


def c_string(text: str | None) -> str:
    """Return a C string literal; None becomes NULL."""
    if text is None:
        return "NULL"
    out = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif 0x20 <= ord(ch) < 0x7F:
            out.append(ch)
        else:
            out.append("\\x%02x\"\"" % ord(ch))
    return '"' + "".join(out) + '"'


def ident(text: str) -> str:
    return text.upper()


def to_decimal_string(value) -> str:
    """Render a number without binary float noise (17.8 stays 17.8)."""
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return str(value)
    text = ("%.4f" % float(value)).rstrip("0").rstrip(".")
    return text


def normalize_spec(spec: dict | None) -> dict:
    """Map one source spec object to the generated C representation."""
    if spec is None:
        return {
            "status": "XF_SPEC_NOT_APPLICABLE",
            "scope": "XF_SCOPE_BOARD",
            "kind": "XF_SPEC_KIND_EMPTY",
            "text": None,
            "unit": None,
            "boolean": False,
        }

    status = STATUS_MAP[spec["status"]]
    scope = "XF_SCOPE_SOC" if spec.get("scope") == "soc" else "XF_SCOPE_BOARD"
    value = spec.get("value")
    unit = spec.get("unit")
    if unit == "count":
        unit = None

    if spec["status"] in ("pending", "conflict"):
        # No usable value: the status alone drives the rendered text.
        return {"status": status, "scope": scope, "kind": "XF_SPEC_KIND_EMPTY",
                "text": None, "unit": unit, "boolean": False}

    if value is None:
        return {"status": "XF_SPEC_NOT_APPLICABLE", "scope": scope,
                "kind": "XF_SPEC_KIND_EMPTY", "text": None, "unit": unit,
                "boolean": False}

    if isinstance(value, bool):
        return {"status": status, "scope": scope, "kind": "XF_SPEC_KIND_BOOL",
                "text": None, "unit": None, "boolean": value}

    if isinstance(value, list):
        if not value:
            return {"status": status, "scope": scope, "kind": "XF_SPEC_KIND_NONE",
                    "text": None, "unit": None, "boolean": False}
        return {"status": status, "scope": scope, "kind": "XF_SPEC_KIND_TEXT",
                "text": "\n".join(str(item) for item in value), "unit": None,
                "boolean": False}

    if isinstance(value, (int, float)):
        return {"status": status, "scope": scope, "kind": "XF_SPEC_KIND_NUMBER",
                "text": to_decimal_string(value), "unit": unit, "boolean": False}

    return {"status": status, "scope": scope, "kind": "XF_SPEC_KIND_TEXT",
            "text": str(value), "unit": unit, "boolean": False}


def footprint_text(value) -> str:
    if isinstance(value, list):
        return " x ".join(to_decimal_string(item) for item in value)
    return to_decimal_string(value)


def build_ids_header(products: list[dict], manufacturer_count: int, sha: str) -> str:
    lines = []
    lines.append("/**")
    lines.append(" * @file xf_product_ids.h")
    lines.append(" * @brief Generated file. Do not edit.")
    lines.append(" *")
    lines.append(" * Stable product, asset and spec-field identifiers for the 18 fixed")
    lines.append(" * XIAO models. Order equals the default carousel order (design note 4.1)")
    lines.append(" * and must never be renumbered.")
    lines.append(" *")
    lines.append(" * Generator: tools/gen_xf_data.py")
    lines.append(" * Source: product_catalog.json + asset_manifest.json")
    lines.append(" * product_catalog.json sha256: %s" % sha)
    lines.append(" */")
    lines.append("")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#ifdef __cplusplus")
    lines.append('extern "C" {')
    lines.append("#endif")
    lines.append("")
    lines.append("/** Stable business key; the numeric value is also the carousel order. */")
    lines.append("typedef enum {")
    for index, product in enumerate(products):
        lines.append("    XF_PRODUCT_%s = %d," % (ident(product["id"]), index))
    lines.append("    XF_PRODUCT_COUNT = %d," % len(products))
    lines.append("    XF_PRODUCT_INVALID = 0xFF,")
    lines.append("} xf_product_id_t;")
    lines.append("")
    lines.append("/** Image asset key; one per product, replaceable without touching identity. */")
    lines.append("typedef enum {")
    for index, product in enumerate(products):
        lines.append("    XF_ASSET_%s = %d," % (ident(product["asset_key"]), index))
    lines.append("    XF_ASSET_COUNT = %d," % len(products))
    lines.append("} xf_asset_id_t;")
    lines.append("")
    lines.append("/** Spec rows rendered on DETAIL and COMPARE, in display order. */")
    lines.append("typedef enum {")
    for index, (key, _, _, _) in enumerate(FIELDS):
        lines.append("    XF_SPEC_FIELD_%s = %d," % (ident(key), index))
    lines.append("    XF_SPEC_FIELD_COUNT = %d," % len(FIELDS))
    lines.append("} xf_spec_field_t;")
    lines.append("")
    lines.append("/** Detail page groups (design note 6.4). */")
    lines.append("typedef enum {")
    for index, (key, _) in enumerate(SECTIONS):
        lines.append("    XF_SECTION_%s = %d," % (ident(key), index))
    lines.append("    XF_SECTION_COUNT = %d," % len(SECTIONS))
    lines.append("} xf_spec_section_t;")
    lines.append("")
    lines.append("/** COMPARE tabs (design note 6.5). */")
    lines.append("typedef enum {")
    for index, (key, _) in enumerate(COMPARE_CATEGORIES):
        lines.append("    XF_COMPARE_CATEGORY_%s = %d," % (ident(key), index))
    lines.append("    XF_COMPARE_CATEGORY_COUNT = %d," % len(COMPARE_CATEGORIES))
    lines.append("} xf_compare_category_t;")
    lines.append("")
    lines.append("/** Number of distinct manufacturers referenced by the catalogue. */")
    lines.append("#define XF_MANUFACTURER_COUNT %d" % manufacturer_count)
    lines.append("")
    lines.append("/** Maximum number of products in one comparison (design note 4.2). */")
    lines.append("#define XF_COMPARE_CAPACITY %d" % 3)
    lines.append("")
    lines.append("/** Catalog size as a compile-time constant. */")
    lines.append("#define XF_PRODUCT_CATALOG_SIZE %d" % len(products))
    lines.append("")
    lines.append("#ifdef __cplusplus")
    lines.append("}")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def build_catalog_source(products: list[dict], manifest: dict, sha: str) -> str:
    by_id = {entry["product_id"]: entry for entry in manifest["assets"]}
    manufacturers: list[str] = []
    for product in products:
        if product["manufacturer"] not in manufacturers:
            manufacturers.append(product["manufacturer"])

    lines = []
    lines.append("/**")
    lines.append(" * @file xf_product_catalog.c")
    lines.append(" * @brief Generated file. Do not edit.")
    lines.append(" *")
    lines.append(" * Immutable product catalogue. Only English runtime fields are emitted;")
    lines.append(" * Chinese editorial fields and per-field notes are filtered out.")
    lines.append(" * Field and section labels are message keys resolved by xf_ui, so all")
    lines.append(" * visible copy still originates from ui_strings.en.json.")
    lines.append(" *")
    lines.append(" * Generator: tools/gen_xf_data.py")
    lines.append(" * Sources: product_catalog.json (sha256 %s)" % sha)
    lines.append(" *          asset_manifest.json")
    lines.append(" */")
    lines.append("")
    lines.append("#include <stddef.h>  /* NULL in the spec initialisers */")
    lines.append("")
    lines.append('#include "xf_product.h"')
    lines.append("")
    lines.append("const char *const xf_product_id_strings[XF_PRODUCT_COUNT] = {")
    for product in products:
        lines.append("    %s," % c_string(product["id"]))
    lines.append("};")
    lines.append("")
    lines.append("const char *const xf_asset_key_strings[XF_ASSET_COUNT] = {")
    for product in products:
        lines.append("    %s," % c_string(product["asset_key"]))
    lines.append("};")
    lines.append("")
    lines.append("const char *const xf_manufacturer_names[XF_MANUFACTURER_COUNT] = {")
    for name in manufacturers:
        lines.append("    %s," % c_string(name))
    lines.append("};")
    lines.append("")
    lines.append("const char *const xf_spec_field_msg_keys[XF_SPEC_FIELD_COUNT] = {")
    for _, msg_key, _, _ in FIELDS:
        lines.append("    %s," % c_string(msg_key))
    lines.append("};")
    lines.append("")
    lines.append("const uint8_t xf_spec_field_section[XF_SPEC_FIELD_COUNT] = {")
    section_index = {key: index for index, (key, _) in enumerate(SECTIONS)}
    lines.append("    " + ", ".join(str(section_index[section]) for _, _, section, _ in FIELDS))
    lines.append("};")
    lines.append("")
    lines.append("const char *const xf_section_msg_keys[XF_SECTION_COUNT] = {")
    for _, msg_key in SECTIONS:
        lines.append("    %s," % c_string(msg_key))
    lines.append("};")
    lines.append("")
    lines.append("const char *const xf_compare_category_msg_keys[XF_COMPARE_CATEGORY_COUNT] = {")
    for _, msg_key in COMPARE_CATEGORIES:
        lines.append("    %s," % c_string(msg_key))
    lines.append("};")
    lines.append("")
    category_index = {key: index for index, (key, _) in enumerate(COMPARE_CATEGORIES)}
    lines.append("const int8_t xf_spec_field_compare_category[XF_SPEC_FIELD_COUNT] = {")
    lines.append("    " + ", ".join(
        str(category_index[category]) if category else "-1"
        for _, _, _, category in FIELDS))
    lines.append("};")
    lines.append("")

    lines.append("const xf_product_t xf_products[XF_PRODUCT_COUNT] = {")
    for index, product in enumerate(products):
        entry = by_id[product["id"]]
        applications = "\n".join(product.get("applications_en") or [])
        lines.append("    [XF_PRODUCT_%s] = {" % ident(product["id"]))
        lines.append("        .id = XF_PRODUCT_%s," % ident(product["id"]))
        lines.append("        .id_string = %s," % c_string(product["id"]))
        lines.append("        .name = %s," % c_string(product["name"]))
        lines.append("        .short_name = %s," % c_string(product["short_name"]))
        lines.append("        .manufacturer_index = %d," % manufacturers.index(product["manufacturer"]))
        lines.append("        .feature = %s," % c_string(product.get("feature_en")))
        lines.append("        .applications = %s," % c_string(applications))
        lines.append("        .notes = %s," % c_string(product.get("notes_en")))
        lines.append("        .asset = XF_ASSET_%s," % ident(product["asset_key"]))
        lines.append("        .is_placeholder = %s," % ("true" if entry.get("is_placeholder") else "false"))
        placeholder = entry.get("placeholder_product_id")
        lines.append("        .placeholder_product_id = %s," % (
            "XF_PRODUCT_%s" % ident(placeholder) if placeholder else "XF_PRODUCT_INVALID"))
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("const xf_spec_t xf_product_specs[XF_PRODUCT_COUNT][XF_SPEC_FIELD_COUNT] = {")
    for product in products:
        lines.append("    [XF_PRODUCT_%s] = {" % ident(product["id"]))
        for key, _, _, _ in FIELDS:
            if key == "applications":
                applications = product.get("applications_en") or []
                spec = {
                    "status": "XF_SPEC_VERIFIED" if applications else "XF_SPEC_PENDING",
                    "scope": "XF_SCOPE_BOARD",
                    "kind": "XF_SPEC_KIND_TEXT" if applications else "XF_SPEC_KIND_EMPTY",
                    "text": "\n".join(applications) if applications else None,
                    "unit": None,
                    "boolean": False,
                }
            else:
                raw = (product.get("specs") or {}).get(key)
                if raw is not None and key == "footprint" and isinstance(raw.get("value"), list):
                    raw = dict(raw, value=footprint_text(raw["value"]))
                spec = normalize_spec(raw)
            lines.append("        [XF_SPEC_FIELD_%s] = { .status = %s, .scope = %s, "
                         ".kind = %s, .text = %s, .unit = %s, .boolean = %s },"
                         % (ident(key), spec["status"], spec["scope"], spec["kind"],
                            c_string(spec["text"]), c_string(spec["unit"]),
                            "true" if spec["boolean"] else "false"))
        lines.append("    },")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    package = DEFAULT_PACKAGE
    for index, arg in enumerate(sys.argv):
        if arg == "--package" and index + 1 < len(sys.argv):
            package = sys.argv[index + 1]

    catalog_path = os.path.join(package, "product_catalog.json")
    manifest_path = os.path.join(package, "asset_manifest.json")
    with open(catalog_path, "rb") as fp:
        raw = fp.read()
    sha = hashlib.sha256(raw).hexdigest()
    catalog = json.loads(raw.decode("utf-8"))
    with open(manifest_path, encoding="utf-8") as fp:
        manifest = json.load(fp)

    products = catalog["products"]
    errors: list[str] = []
    if len(products) != 18:
        errors.append("expected 18 products, found %d" % len(products))
    identifiers = [product["id"] for product in products]
    asset_keys = [product["asset_key"] for product in products]
    if len(set(identifiers)) != len(identifiers):
        errors.append("duplicate product id")
    if len(set(asset_keys)) != len(asset_keys):
        errors.append("duplicate asset key")
    if sorted(entry["product_id"] for entry in manifest["assets"]) != sorted(identifiers):
        errors.append("asset manifest does not cover the catalogue one to one")
    if catalog.get("product_count") != len(products):
        errors.append("product_count does not match the product list")

    manufacturers = []
    for product in products:
        if product["manufacturer"] not in manufacturers:
            manufacturers.append(product["manufacturer"])

    if errors:
        raise SystemExit("[data] " + "; ".join(errors))

    os.makedirs(OUT_DIR, exist_ok=True)
    header = build_ids_header(products, len(manufacturers), sha)
    with open(os.path.join(OUT_DIR, "xf_product_ids.h"), "w", encoding="utf-8", newline="\n") as fp:
        fp.write(header)
    with open(os.path.join(OUT_DIR, "xf_product_catalog.c"), "w", encoding="utf-8", newline="\n") as fp:
        fp.write(build_catalog_source(products, manifest, sha))

    print("[data] %d products, %d manufacturers, %d spec rows" %
          (len(products), len(manufacturers), len(FIELDS)))
    print("[data] wrote %s" % OUT_DIR)


if __name__ == "__main__":
    main()