# XIAO Family Board

XIAO Family 展示板的软件，包括外部触摸/LED 展板固件和 2.16 英寸 AMOLED 触摸屏固件。

| 项目 | 说明 | 构建环境 |
| --- | --- | --- |
| [Software/MFSZ_PCB](Software/MFSZ_PCB) | 铜箔触摸、WS2812 LED 和 I²C 从机协议 | PlatformIO / Arduino，XIAO ESP32-S3 Plus |
| [Software/touch_2.16](Software/touch_2.16) | 产品浏览、对比、中英文界面及 I²C 主机交互 | ESP-IDF 5.5.2，ESP32-S3 |

## 构建

触摸展板固件，在已安装项目所需 PlatformIO 平台的环境中运行：

```sh
cd Software/MFSZ_PCB
pio run
```

屏幕固件，在已初始化的 ESP-IDF 终端中运行：

```sh
cd Software/touch_2.16
idf.py --no-ccache build
```

具体接线、协议、功能和验证方式请参阅：

- [触摸展板说明](Software/MFSZ_PCB/README_TOUCH_TEST.md)
- [I²C 协议](Software/MFSZ_PCB/docs/XIAO_Touch_I2C_Protocol_v1.md)
- [屏幕固件中文说明](Software/touch_2.16/README_CN.md)
- [Screen firmware documentation](Software/touch_2.16/README.md)

## 资源与构建输入

屏幕工程的资源文件位于 [Software/touch_2.16/resources](Software/touch_2.16/resources)。下表路径均相对于 `Software/touch_2.16`。

| 路径 | 用途 |
| --- | --- |
| `resources/assets/svg/` | 18 个产品资源条目的 SVG 源文件 |
| `resources/assets/hero/`、`resources/assets/thumb/` | 18 张产品大图和 18 张缩略图，PNG 格式 |
| `resources/assets/brand/` | Logo SVG、PNG 及来源校验清单 |
| `resources/asset_manifest.json` | 产品 ID、资源 ID、文件路径、SHA-256 和参考图片映射 |
| `resources/product_catalog.json`、`resources/ui_strings.en.json` | 产品数据及界面文本的生成输入 |
| `main/assets/img/` | 36 个 RGB565A8 二进制图片，由 `xf_assets` 组件链接到应用固件 |
| `components/xf_assets/generated/`、`components/xf_data/generated/`、`components/xf_ui/generated/` | 字库、Logo、产品数据和双语文本的生成源码 |
| `sdkconfig`、`sdkconfig.defaults.esp32s3`、`partitions.csv` | ESP32-S3 构建配置及 Flash 分区定义 |

`idf.py build` 使用已生成的图片与 C/H 资源；该命令不执行图片转换、文本生成或字库生成。图片和字体更新后的再生成步骤见[资源维护说明](Software/touch_2.16/resources/README.md)。

`nrf54lm20b_sense` 当前使用 `nrf54lm20a_sense` 的参考图片，映射关系由 `asset_manifest.json` 中的 `is_placeholder` 和 `placeholder_product_id` 声明。

## 依赖管理

屏幕工程通过 `main/idf_component.yml` 声明组件版本，通过 `dependencies.lock` 记录解析结果。`managed_components/` 包含组件源码，LVGL 9.4.0 使用清单中的 `override_path` 配置。各第三方组件适用其目录中的许可证。

字体转换工具由 `tools/package.json` 和 `tools/package-lock.json` 管理。仅重新生成字库时需要在 `Software/touch_2.16/tools` 目录执行 `npm ci` 安装 `lv_font_conv`；此外还需 Node.js 和字库源文件。安装字体转换工具不包含字体文件安装。

## 版本控制范围

`.gitignore` 排除 ESP-IDF/CMake 的 `build/`、PlatformIO 的 `.pio/`、Node.js 的 `node_modules/`、Python 缓存及编译中间文件。`main/assets/img/*.bin` 是图片链接输入，纳入版本控制；应用和引导程序的二进制输出位于 `build/`，由目录规则排除。
