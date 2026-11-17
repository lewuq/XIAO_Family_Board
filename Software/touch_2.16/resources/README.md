# 屏幕资源维护

本目录包含屏幕固件使用的产品图片、Logo、资源映射、产品目录和英文界面文本。所有清单中的资源路径均相对于本目录解析。

| 目录 / 文件 | 定义 |
| --- | --- |
| `assets/svg/` | 18 个产品资源条目的 SVG 快照，按 `asset_key` 命名 |
| `assets/hero/` | 240 × 260 像素、透明背景的产品大图 |
| `assets/thumb/` | 96 × 104 像素、透明背景的产品缩略图 |
| `assets/brand/` | Logo 原始 SVG、裁边 SVG、PNG 及来源清单 |
| `asset_manifest.json` | 资源文件路径、产品映射、SVG SHA-256 和参考图片标记 |
| `product_catalog.json` | 产品参数及固定产品 ID |
| `ui_strings.en.json` | 英文界面文本；中文翻译表位于 `tools/gen_xf_strings.py` |

## 参考图片

`nrf54lm20b_sense_front` 使用 LM20A 的图片作为参考，并具有独立的资源 ID。更换图片时应同步更新 SVG、PNG 和清单中的 SHA-256、`is_placeholder`、`placeholder_product_id`；产品 ID、资源 ID 及外部 KEY_ID 映射保持稳定。

## 图片转换

以下命令从 `Software/touch_2.16` 目录执行，需要 Python 3.10 或更高版本及 Pillow。SVG 是原始图形资料，固件转换器读取清单指定的 PNG；修改 SVG 后须先更新对应 PNG。PNG 应保持透明背景、规定画布尺寸和产品原始宽高比。

```sh
python -B tools/convert_assets.py
```

输出为 `main/assets/img/hero_<asset_key>.bin`、`thumb_<asset_key>.bin` 和 `main/docs/asset_report.json`。二进制布局为小端 RGB565 像素平面与独立 A8 平面；每张大图 187,200 字节，每张缩略图 29,952 字节。转换报告记录 PNG SHA-256、尺寸和输出字节数。

## 数据、文本与 Logo

以下生成器默认读取本目录，不依赖外部交付目录。

```sh
python -B tools/gen_xf_data.py
python -B tools/gen_xf_strings.py
python -B tools/gen_xf_brand.py
```

输出分别位于 `components/xf_data/generated/`、`components/xf_ui/generated/` 和 `components/xf_assets/generated/`。Logo 生成器读取 `assets/brand/seeed-logo-preview.png`；产品与 Logo 的源文件校验值由相应清单记录。

## 字库生成

字库生成需要 Node.js、`lv_font_conv` 以及 `C:/Windows/Fonts/NotoSansSC-VF.ttf`。当前 `gen_xf_fonts.py` 使用该 Windows 字体路径，字体文件不包含在仓库中。

在 `Software/touch_2.16` 目录执行：

```sh
npm --prefix tools ci
python -B tools/gen_xf_strings.py
python -B tools/gen_xf_fonts.py
```

文本生成必须先于字库生成，以将新增中文字符纳入字体子集。`npm ci` 根据锁文件安装转换器，不安装字体。生成结果为 18、22、23、29、32 像素的 LVGL 4 bpp 字库源码。

`build_assets.py`、`generate_catalog.py` 和 `gen_fonts.py` 面向 `main/model/` 及旧版字库；当前 `components/xf_*` 实现使用上述 `gen_xf_*` 脚本。
