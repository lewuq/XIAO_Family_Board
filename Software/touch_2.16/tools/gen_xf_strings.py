#!/usr/bin/env python3
"""Generate English/Chinese UI copy and catalogue translations, without downloads.

English JSON stays the reviewed source. Chinese translations live in this code
file so no additional data/configuration files are required. Product names,
manufacturer names, chip identifiers, units and protocol acronyms stay intact.
The locale is RAM-only and defaults to English at every boot.
"""
from __future__ import annotations
import hashlib
import json
import re
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
DESIGN = PROJECT / "resources"
DEFAULT_SOURCE = DESIGN / "ui_strings.en.json"
OUT = PROJECT / "components/xf_ui/generated"
ZH = {
    "action.add_compare": "加入对比", "action.remove_compare": "移除",
    "action.compare": "对比", "action.edit": "编辑", "action.done": "完成",
    "action.cancel": "取消", "action.clear": "清空", "action.close": "关闭",
    "action.retry": "重试", "action.browse": "浏览产品",
    "screen.autoplay.cta": "轻触查看", "screen.catalog.title": "产品",
    "screen.select.title": "选择产品", "screen.detail.title": "详情",
    "screen.compare.title": "对比", "nav.all_products": "全部产品",
    "nav.back": "返回", "filter.all": "全部",
    "selection.count": "已选 {count}/3", "selection.item": "已选 {index}",
    "selection.minimum": "至少选择 2 款", "selection.maximum": "最多选择 3 款",
    "selection.empty": "尚未选择产品", "selection.ready": "已选 {count} 款",
    "compare.tab.compute": "计算", "compare.tab.connect": "连接",
    "compare.tab.sensors": "传感", "compare.differences_only": "仅看差异",
    "section.compute": "计算与存储", "section.connect": "连接与接口",
    "section.sensors": "传感与扩展", "section.power": "供电与尺寸",
    "section.applications": "应用领域", "field.manufacturer": "芯片厂商",
    "field.processor": "处理器", "field.cpu_clock_max": "最高频率",
    "field.sram": "SRAM", "field.internal_nvm": "内部存储",
    "field.internal_nvm_type": "存储类型", "field.external_flash": "外部 Flash",
    "field.psram": "PSRAM", "field.exposed_gpio": "GPIO", "field.wireless": "无线连接",
    "field.sensors": "传感器", "field.interfaces": "接口",
    "field.onboard_battery_charging": "电池充电", "field.footprint": "板卡尺寸",
    "field.pio_state_machines": "PIO 单元", "field.npu": "NPU",
    "field.adc_resolution": "ADC", "field.dac_resolution": "DAC",
    "value.pending": "待核实", "value.conflict": "待核实", "value.none": "无",
    "value.not_applicable": "不适用", "value.true": "支持", "value.false": "不支持",
    "value.soc_scope": "芯片能力", "asset.reference_image": "参考图片",
    "asset.unavailable": "图片不可用", "data.unavailable": "数据不可用",
    "system.loading": "正在加载", "system.display_error": "显示不可用",
    "system.touch_error": "触摸不可用",
}
PHRASES = {
    "10-bit DAC": "10 位 DAC", "More GPIOs": "扩展 GPIO", "CAN FD": "CAN FD",
    "Zigbee + IMU": "Zigbee + IMU", "Arm / RISC-V": "Arm / RISC-V",
    "PIO State Machines": "PIO 状态机", "14-bit ADC": "14 位 ADC",
    "Wi-Fi + BLE": "Wi-Fi + BLE", "Dual-band Wi-Fi 6": "双频 Wi-Fi 6",
    "Wi-Fi 6 + Thread": "Wi-Fi 6 + Thread", "16 MB Flash": "16 MB Flash",
    "Camera + Mic": "摄像头与麦克风", "IMU + Mic": "IMU 与麦克风",
    "BLE 6 + IMU": "BLE 6 + IMU", "8 MB Ext. Flash": "8 MB 外部 Flash",
    "Axon NPU": "Axon NPU",
    "Analog control": "模拟控制", "Electronic instruments": "电子仪器",
    "STEM education": "科创教育", "Multi-sensor systems": "多传感器系统",
    "Embedded control": "嵌入式控制", "Robotics": "机器人",
    "CAN data logging": "CAN 数据记录", "Smart home": "智能家居",
    "Wireless motion sensing": "无线运动检测", "Architecture education": "架构教学",
    "Custom interfaces": "自定义接口", "LED control": "LED 控制",
    "Custom communication": "自定义通信", "Interactive controllers": "交互控制器",
    "Measurement instruments": "测量仪器", "Analog sensing": "模拟量采集",
    "Connected sensors": "联网传感器", "Wireless controllers": "无线控制器",
    "Dual-band connected devices": "双频联网设备", "Wireless data terminals": "无线数据终端",
    "Matter devices": "Matter 设备", "Thread sensors": "Thread 传感器",
    "Touchscreen terminals": "触屏终端", "Connected controllers": "联网控制器",
    "Vision prototypes": "视觉原型", "Sound recognition": "声音识别",
    "Gesture sensing": "手势检测", "Sound-aware wearables": "声音感知穿戴设备",
    "Bluetooth controllers": "蓝牙控制器", "Multi-sensor terminals": "多传感器终端",
    "Motion sensing": "运动检测", "Wireless wearables": "无线穿戴设备",
    "Sensor data logging": "传感数据记录", "Connected sensing": "联网感知",
    "Motion inference": "运动推理", "Sound inference": "声音推理", "Anomaly detection": "异常检测",
    "6-axis IMU": "六轴 IMU", "Analog microphone": "模拟麦克风",
    "PDM microphone": "PDM 麦克风", "Camera on Sense expansion": "Sense 扩展板摄像头",
    "Digital microphone on Sense expansion": "Sense 扩展板数字麦克风",
}
# Ordered by the stable product IDs; these explain limitations, not new specs.
NOTES_ZH = [
    "支持真实 10 位 DAC 输出；板载不支持电池充电。",
    "扩展 GPIO，配备板载电池管理。",
    "一路 CAN FD 配备板载收发器；第二路需要外接收发器。",
    "配备六轴 IMU 和模拟麦克风；无线功能需要匹配的软件协议栈。",
    "可选择 Arm Cortex-M33 或 Hazard3 RISC-V；同时运行两个处理器核心。",
    "PIO 状态机提供可编程硬件时序；板载不支持电池充电。",
    "扩展 GPIO 和板载电池管理；PIO 能力与 RP2040 相同。",
    "支持 14 位 ADC 和 12 位 DAC；CAN 控制器支持不代表配备板载 CAN 收发器。",
    "支持 2.4 GHz Wi-Fi 与低功耗蓝牙，适合紧凑联网设备。",
    "支持 2.4 GHz 和 5 GHz Wi-Fi 6；SRAM 容量不等于应用可用堆内存。",
    "最高频率指高性能核心；Thread 与 Matter 需要匹配的软件协议栈。",
    "配备 16 MB Flash 和 8 MB PSRAM；内部 SRAM 容量需单独核实。",
    "摄像头与麦克风位于 Sense 扩展板；摄像头型号取决于硬件批次。",
    "配备六轴 IMU 和 PDM 麦克风；低功耗蓝牙版本细节待核实。",
    "扩展接口；公开 GPIO 数量的统计口径不一致，尚未核实。",
    "Cortex-M33 搭配 RISC-V 协处理器；内部非易失存储标记为 NVM。",
    "配备外部 Flash 和 nPM1300 电源管理；所提供图片文件名沿用旧命名。",
    "参考图片为 LM20A；已确认参数描述 LM20B 芯片，板级存储、传感器和接口尚未核实。",
]

def ident(key: str) -> str:
    return "XF_STRING_" + re.sub(r"[^A-Za-z0-9]+", "_", key).strip("_").upper()

def literal(value: str) -> str:
    # Literal UTF-8 lets the offline font generator discover exactly these glyphs.
    return json.dumps(value, ensure_ascii=False)

def main() -> None:
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SOURCE
    raw = source.read_bytes()
    data = json.loads(raw.decode("utf-8-sig"))
    if data.get("language") != "en":
        raise SystemExit("expected reviewed English source")
    en = {k:v for k,v in data.items() if k not in {"language", "brand_asset_key"}}
    en["action.browse"] = "Browse"
    if any(not isinstance(v, str) for v in en.values()) or set(en) != set(ZH):
        raise SystemExit("translation keys must exactly match English keys")
    en = dict(sorted(en.items()))
    if len({ident(k) for k in en}) != len(en):
        raise SystemExit("duplicate generated identifiers")

    products = json.loads((DESIGN / "product_catalog.json").read_text(encoding="utf-8-sig"))["products"]
    if len(products) != len(NOTES_ZH):
        raise SystemExit("catalogue count changed; review translations")
    phrases = dict(PHRASES)
    for product, note in zip(products, NOTES_ZH):
        phrases[product["notes_en"]] = note
        if product["feature_en"] not in phrases:
            raise SystemExit("missing feature translation: " + product["id"])
        for values in [product["applications_en"], product["specs"]["sensors"]["value"]]:
            if not isinstance(values, list) or not values:
                continue
            if any(v not in PHRASES for v in values):
                raise SystemExit("missing catalogue phrase: " + repr(values))
            phrases["\n".join(values)] = "\n".join(PHRASES[v] for v in values)

    fingerprint = hashlib.sha256(raw + Path(__file__).read_bytes()).hexdigest()
    banner = "/** Generated by tools/gen_xf_strings.py; inputs sha256: " + fingerprint + " */\n"
    header = banner + """#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
"""
    header += "".join("    " + ident(k) + ",\n" for k in en)
    header += """    XF_STRING_COUNT
} xf_string_id_t;
/** UI-task/LVGL-lock only. 0 = English (boot default), 1 = Chinese. */
void xf_string_set_language(uint8_t language);
uint8_t xf_string_language(void);
/** Destination-language label: 中文 in English mode, EN in Chinese mode. */
const char *xf_language_button_text(void);
const char *xf_string(xf_string_id_t id);
const char *xf_string_lookup(const char *key);
const char *xf_string_key(xf_string_id_t id);
/** Exact catalogue phrase lookup; technical identifiers pass through unchanged. */
const char *xf_translate_text(const char *english);
#ifdef __cplusplus
}
#endif
"""
    output = banner + '#include "xf_strings.h"\n#include <string.h>\n\n'
    output += "static uint8_t s_language; /* C startup initializes this to English. */\n"
    for name, values in [("s_keys", {k:k for k in en}), ("s_en", en), ("s_zh", ZH)]:
        output += "static const char *const " + name + "[XF_STRING_COUNT] = {\n"
        output += "".join("    [" + ident(k) + "] = " + literal(values[k]) + ",\n" for k in en)
        output += "};\n"
    output += "static const struct { const char *en, *zh; } s_phrases[] = {\n"
    output += "".join("    {" + literal(k) + ", " + literal(v) + "},\n" for k,v in sorted(phrases.items()))
    output += """};
void xf_string_set_language(uint8_t language) { s_language = language == 1U ? 1U : 0U; }
uint8_t xf_string_language(void) { return s_language; }
const char *xf_language_button_text(void) { return s_language == 0U ? "中文" : "EN"; }
const char *xf_string(xf_string_id_t id)
{
    if ((unsigned)id >= (unsigned)XF_STRING_COUNT) return "";
    return s_language == 1U ? s_zh[id] : s_en[id];
}
const char *xf_string_key(xf_string_id_t id)
{
    return (unsigned)id < (unsigned)XF_STRING_COUNT ? s_keys[id] : "";
}
const char *xf_string_lookup(const char *key)
{
    if (key == NULL) return NULL;
    for (unsigned i = 0; i < XF_STRING_COUNT; ++i) {
        if (strcmp(key, s_keys[i]) == 0) return xf_string((xf_string_id_t)i);
    }
    return NULL;
}
const char *xf_translate_text(const char *english)
{
    if (english == NULL) return "";
    if (s_language == 1U) {
        for (size_t i = 0; i < sizeof(s_phrases) / sizeof(s_phrases[0]); ++i) {
            if (strcmp(english, s_phrases[i].en) == 0) return s_phrases[i].zh;
        }
    }
    return english;
}
"""
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "xf_strings.h").write_text(header, encoding="utf-8", newline="\n")
    (OUT / "xf_strings.c").write_text(output, encoding="utf-8", newline="\n")
    print(f"[strings] {len(en)} bilingual keys, {len(phrases)} catalogue phrases -> {OUT}")

if __name__ == "__main__":
    main()
