# XIAO Family AMOLED 交互终端

[English](README.md) | [简体中文](README_CN.md)

本工程是 XIAO Family 的嵌入式产品展示终端，运行于 XIAO ESP32-S3 Plus，配合 2.16 英寸、480 × 480 圆角方形触摸 AMOLED 使用。终端展示 18 款 XIAO 的图片、参数和应用领域，支持多款产品对比，并作为 I²C 主机与外部触摸及 WS2812 展板联动。

屏幕使用 CO5300，通过 QSPI 传输图像；触摸输入使用 CST9220。产品资料与图像资源编译在固件中，未连接外部触摸展板时，屏幕端仍可独立浏览产品。

## 已实现功能

| 功能 | 当前行为 |
| --- | --- |
| 首页轮播 | 按目录顺序展示全部 18 款 XIAO，每 3 秒切换一款 |
| 显示方向 | 软件旋转 180°；LVGL 对原始触摸坐标转换一次 |
| 触摸提示音 | 新的外部 DOWN 事件播放一次 600 Hz、40 ms 提示音；UP、状态快照、轮播及 ACK 重试不发声 |
| 手动切换 | 左滑切换下一款，右滑切换上一款，首尾循环 |
| 轮播指示 | 图片下方显示三个循环指示点，顶部显示产品计数；无进度条和上一个、下一个按钮 |
| 产品浏览 | 点击首页图片或背景进入详情，或通过 Browse 进入可按厂商筛选的产品列表；选择产品使用绿色实心主按钮，带轮廓及按压反馈 |
| 详情字体与阅读位置 | 字体放大约 30%（18 -> 23 像素、22 -> 29 像素）；打开另一款产品时从顶部开始 |
| 详细资料 | 展示图片、精炼特性、计算与存储、连接、传感器、供电与尺寸、应用及数据说明 |
| 参数对比 | 选择 2–3 款产品，按参数分类对比，可切换为仅显示差异 |
| 语言切换 | 开机默认英文；首页和页面顶部提供 中文 / EN 切换，保留当前产品、选择项和滚动位置 |
| 无操作恢复 | 停止触摸 6 秒后返回轮播并切换一次，之后每 3 秒切换 |
| 外部联动 | 读取从机铜皮触摸事件，并向从机下发选中产品的灯光控制命令 |
| 降级运行 | 外部展板缺失不阻止本机界面运行；屏内触摸初始化失败时记录日志并继续显示 |

按住屏幕或外部触摸盘时，不触发无操作恢复。恢复轮播会清空对比选择、筛选条件和访客导航状态，保留当前语言。语言仅保存在 RAM 中，重启后恢复英文。

界面采用 Seeed 绿 `#8FC31F`、深蓝 `#004966`、用户提供的 Seeed Logo 资源和嵌入式中英文字库。物理画面保持 480 × 480，控件在四周留出 24 像素，逻辑根容器为 432 × 432。该留白属于布局边距，不代表屏幕玻璃的实际圆角半径。

## 产品清单与固定映射

Product ID 决定目录顺序。KEY_ID 对应外部展板的 K 编号，K9 保留为空位。WS 序号由从机维护，本工程通过 I²C 下发选择命令。

| Product ID | 显示型号 | MCU 芯片厂商 | KEY_ID（十六进制） | WS 序号 |
| --- | --- | --- | --- | --- |
| 0 | XIAO SAMD21 | Microchip | 0x00 | 18 |
| 1 | XIAO SAMD21 Plus | Microchip | 0x01 | 17 |
| 2 | XIAO STM32C5 | STMicroelectronics | 0x02 | 16 |
| 3 | XIAO MG24 Sense | Silicon Labs | 0x03 | 15 |
| 4 | XIAO RP2350 | Raspberry Pi | 0x04 | 14 |
| 5 | XIAO RP2040 | Raspberry Pi | 0x05 | 13 |
| 6 | XIAO RP2040 Plus | Raspberry Pi | 0x06 | 12 |
| 7 | XIAO RA4M1 | Renesas | 0x07 | 11 |
| 8 | XIAO ESP32-C3 | Espressif | 0x08 | 10 |
| 9 | XIAO ESP32-C5 | Espressif | 0x0A | 9 |
| 10 | XIAO ESP32-C6 | Espressif | 0x0B | 8 |
| 11 | XIAO ESP32-S3 Plus | Espressif | 0x0C | 7 |
| 12 | XIAO ESP32-S3 Sense | Espressif | 0x0D | 6 |
| 13 | XIAO nRF52840 Sense | Nordic Semiconductor | 0x0E | 5 |
| 14 | XIAO nRF52840 Plus | Nordic Semiconductor | 0x0F | 4 |
| 15 | XIAO nRF54L15 Sense | Nordic Semiconductor | 0x10 | 3 |
| 16 | XIAO nRF54LM20A Sense | Nordic Semiconductor | 0x11 | 2 |
| 17 | XIAO nRF54LM20B Sense | Nordic Semiconductor | 0x12 | 1 |

厂商筛选共覆盖七家 MCU 芯片厂商。Seeed Studio 是 XIAO 板卡厂商，表中的厂商名称用于区分主控芯片来源。

LM20B 条目目前使用 LM20A 的参考图片，并在详情页标注参考图片。两者拥有独立的产品 ID 和资源 ID，便于后续替换。未确认或存在来源冲突的参数显示“待核实”，资料中区分板级参数与芯片本身的能力。

## 硬件接口

接线常量维护于 [xf_board.h](components/xf_bsp/include/xf_board.h)，扩展 D 引脚按 [Seeed 官方 XIAO ESP32-S3 Plus 引脚表](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/#xiao-esp32-s3-plus-front)换算为 GPIO。

| 信号 | 屏幕主板 ESP32-S3 GPIO / 分配 |
| --- | --- |
| AMOLED QSPI CS / SCK | GPIO43 / GPIO44 |
| AMOLED QSPI D0 / D1 / D2 / D3 | GPIO10 / GPIO11 / GPIO12 / GPIO13 |
| 共用 I²C SDA / SCL | GPIO5 / GPIO6 |
| CST9220 触摸中断 | GPIO3 |
| 喇叭 I2S MCLK / BCLK / WS / DOUT | GPIO38 / GPIO39 / GPIO40 / GPIO42 |
| 音频电源 / 功放使能 | PCA9535 P07 / P04 |
| PCA9535 中断 | GPIO4；当前轮询路径未使用 |
| 显示 / 触摸复位 | PCA9535 P00 / P01 |
| 模块 VCI / VBAT 命名电源轨 / VCI 使能 | PCA9535 P10 / P11 / P16 |

| 器件 | 接口 | 7 位地址 / 配置速率 |
| --- | --- | --- |
| CO5300 AMOLED | QSPI，RGB565 | 总线时钟 40 MHz |
| CST9220 触摸 | 共用 I²C 主机总线 | `0x5A`，400 kHz |
| PCA9535 扩展器 | 共用 I²C 主机总线 | `0x21`，400 kHz |
| ES8311 音频解码器 -> NS4150B 功放 -> J2 喇叭 | I2C 控制 + 标准 I2S 播放 | `0x18`，400 kHz |
| 外部触摸展板 | 同一 I²C 主机总线 | `0x42`，100 kHz |

屏幕主板作为主机运行，没有配置 I²C 从机地址。J3 接口的 6 脚为 SDA、7 脚为 SCL、5 脚为 GND。连接外部从机对应的 SDA、SCL 和 GND，使用 3.3 V 逻辑电平及设计规定的上拉。协议中的 GPIO8/GPIO9 是**外部从机**的引脚定义，屏幕主板仍使用 GPIO5/GPIO6。

应用日志通过原生 USB Serial/JTAG 输出，使 GPIO43/44 可用于屏幕，避免与 UART0 冲突。Bootloader 和应用启动前的日志保留现有控制台配置。

当前工程目标为 ESP32-S3，Flash 配置为 16 MB，未启用 PSRAM。显示采用两个内部 DMA 可用的 480 × 40 RGB565 条带缓冲区，总计 76,800 字节，使用局部刷新；显示刷新定时器为 16 ms，触摸采样为 10 ms。软件旋转额外使用一个 480 × 40 RGB565 条带缓冲区（38,400 字节），绘制及旋转缓冲区合计 115,200 字节。这些是配置参数，实际帧率需要测量。

## 外部 I²C 联动

完整约定见本地 `XIAO_Touch_I2C_Protocol_v1.md`，路径列于文末。协议辅助函数位于 [xf_touch_protocol.h](components/xf_core/include/xf_touch_protocol.h)。

- 连接后每 20 ms 轮询 STATUS，每轮最多处理八条事件，再进入后续轮询。
- 读取寄存器时，先写寄存器指针并发送 STOP，再一次性读取四字节并发送 STOP；不能替换为 repeated-start 组合读取或四次单字节读取。
- 同时处理 DOWN 和 UP，事件进入 UI 队列后，再确认其 16 位小端序号。
- ACK 结果不确定时保留已投递标记，避免重复执行同一 UI 事件。
- FIFO 溢出时清空事件队列，并读取 KEY_STATE 恢复当前按住的按键；无法重建已丢失的历史顺序。
- 定期校验按住状态；相同的状态快照不会重置无操作计时。
- 从机不可用时约每秒重试一次；重新连接后恢复当前主机选择状态。

| 屏幕状态 | 请求从机提供的灯光反馈 |
| --- | --- |
| 自动轮播 | 默认关闭灯光跟随；进入自动轮播时清除先前主机选择，后续不随图片点灯 |
| 手动轮播选择或单款产品详情 | 对应产品的 LED 显示绿色 `#8FC31F` |
| 产品列表、选择页或对比页存在已选项 | 点亮所选集合；2–3 款可同时亮起 |
| 列表中没有选择项，或清空选择 | 清除主机控制的选中灯 |
| 触摸从机铜皮触摸盘 | 从机立即提供本地绿灯反馈，DOWN 事件使屏幕打开对应产品详情 |

单选使用 `SELECT_DEVICE`（`0x10`）。多选使用 `CLEAR_SELECTION`（`0x12`），再逐项发送 `SET_RGB`（`0x11`）。从机本地触摸反馈独立有效，按住期间优先显示触摸反馈。例如选择 ESP32-S3 Sense 时，向 `0x42` 发送 `10 0D`，对应 K13 / WS6。

本工程实现屏幕主机端，外部展板需要兼容的 V1 从机固件。选中灯反馈不包含每款展示 XIAO 自身的功能演示。STATUS.RESULT=`00` 表示命令已接收，不能证明 LED 实际发光。V1 没有启动标识，ACK 不确定期间若从机重启且事件数据完全相同，主机可能无法明确区分。

提示音使用板载 ES8311（7 位地址 `0x18`）和 NS4150B 功放，喇叭接 J2 的 VoP/VoN 两端。I2S 为 16 kHz、16 位立体声，左右声道发送相同采样；本机生成 600 Hz 音调，两端各有 5 ms 渐变包络，并限制幅度。异步任务负责播放，队列深度为八；过载时可丢弃音频请求，避免阻塞协议处理。音频初始化失败时，界面与外部展板控制继续运行。

音调时长和幅度位于 `components/xf_bsp/include/xf_feedback_tone.h`，解码器增益及接线位于 `xf_board.h`。仅 `main/blink_example_main.c` 中新投递的 DOWN 分支请求提示音，无需声音文件、额外下载组件或录音路径。

### 默认屏幕亮度

配置位于 [xf_board.h](components/xf_bsp/include/xf_board.h)：

```c
#define XF_PANEL_DEFAULT_BRIGHTNESS_PERCENT 80U
```

默认开机亮度为 80%。有效范围为 0–100，0 为熄屏，100 为最高亮度；修改宏后重新编译并烧录即可生效。亮度通过 CO5300 寄存器命令设置，不改变图片像素值。

### 可选的轮播灯光跟随

配置位于 [xf_config.h](components/xf_core/include/xf_config.h)：

```c
#define XF_ENABLE_AUTOPLAY_LED_SYNC 0
```

默认 `0`：上电或空闲恢复到自动轮播时清除主机选中灯，之后图片自动切换不再下发选择或 RGB 点灯更新。手动产品选择、对比、铜皮触摸反馈、触摸事件轮询与确认、提示音均继续工作。CLEAR_SELECTION 保留从机本地触摸反馈。

需要恢复自动轮播灯光跟随时，将宏改为 `1`，重新编译并烧录。两种配置均保留每 3 秒切图和 6 秒无操作恢复规则。

## 代码结构

```text
main/blink_example_main.c       实际 app_main；启动、USB 日志和外部协议任务
components/xf_bsp/             显示、触摸、提示音任务、扩展器、电源复位时序及 I²C 传输
components/xf_core/            纯 C 状态机、轮播与无操作规则、选择及协议辅助函数
components/xf_data/            产品检索及生成的目录、ID
components/xf_assets/          图片描述、Logo 和生成字库
components/xf_ui/              LVGL 页面、主题、格式化、事件控制器及双语文本
main/assets/img/              编译进固件的大图和缩略图二进制资源
tools/                        本地数据、文本、Logo、字库和图片生成器
tests/test_xf_core.c           本机状态机、协议和双语检查
docs/pinmap.json               较早的参考映射；修正后的 GPIO 以 xf_board.h 为准
```

应用入口仅注册 `main/blink_example_main.c`，保留了早期 Blink 示例的文件名。`main/app_main.c`、`main/model/` 和 `main/assets/` 下的旧字库不是当前界面的实现入口。

UI 控制器通过 FreeRTOS 队列串行处理业务事件。`xf_core` 不依赖硬件和 LVGL，负责状态迁移；`xf_ui` 在 LVGL 锁内构建页面。控制器先处理一批有数量上限的事件，再统一提交一次画面更新。轮播复用已有控件，同款详情的选择与提示更新保留参数主体和阅读位置。提示到期使用独立定时事件，其他产品延迟到达的滚动回调会被丢弃。外部 I²C 使用独立任务，界面选择回调仅更新单元素队列，控件回调不执行总线事务。

## 编译、烧录与日志

已验证环境为 ESP-IDF 5.5.2，目标 `esp32s3`。组件清单允许 IDF >= 5.4，其他版本尚未在本工程验证。

| 依赖 | 固定版本 |
| --- | --- |
| LVGL | 9.4.0，通过 `managed_components/lvgl__lvgl` 本地覆盖 |
| esp_lvgl_port | 2.9.0 |
| esp_lcd_co5300 | 2.2.0 |
| esp_lcd_touch | 1.2.1，锁定的间接依赖 |
| esp_lcd_touch_cst9220 | 0.1.1 |
| cmake_utilities | 0.5.3，锁定的间接依赖 |

在已初始化的 ESP-IDF PowerShell 终端中，使用现有本地工具和完整依赖：

```powershell
Set-Location 'D:\XIAO_test\MFSZ26\XIAO_Family\Software\touch_2.16'
idf.py --no-ccache build
```

固件输出为 `build/touch_2.16.bin`。现有 `partitions.csv` 提供 10 MiB factory 应用分区。当前实现将资源编译进应用，无需单独上传资源文件系统。

需要烧录时，将 COM13 替换为设备实际 USB 串口：

```powershell
idf.py -p COM13 flash
idf.py -p COM13 monitor
```

使用 Ctrl+] 退出日志监视器。上述命令不包含安装 SDK 或获取素材的步骤；但 ESP-IDF 组件管理器可能自动获取缺失依赖。在禁止下载的工作范围内，应先确认本地组件完整，缺失时停止并检查。保留 LVGL 本地覆盖声明；本工程已有目标和配置，常规重新编译无需执行重置目标或 menuconfig。

## 内容与资源维护

普通编译使用现有生成资源，无需重新转换。重新生成需要本地源资料和已安装的转换工具。

| 内容 | 维护来源 / 生成器 |
| --- | --- |
| 产品数据和固定 ID | 交付包中的 `product_catalog.json`、`asset_manifest.json`；`tools/gen_xf_data.py` |
| 英文界面与中文翻译 | 交付包中的 `ui_strings.en.json`，以及 `tools/gen_xf_strings.py` 中的翻译表 |
| 字库 | `tools/gen_xf_fonts.py`；使用已安装的 `C:\Windows\Fonts\NotoSansSC-VF.ttf` 和本地 `tools/node_modules/lv_font_conv` |
| 品牌 Logo | 提供的 SVG 经交付包品牌资源生成；`tools/gen_xf_brand.py` |
| 产品图片 | 交付包资源清单及转换后的大图、缩略图；`tools/convert_assets.py` |

修改翻译文案后，必须先生成文本，再生成字体，以包含新增汉字：

```powershell
python -B tools/gen_xf_strings.py
python -B tools/gen_xf_fonts.py
```

字体生成器需要 PATH 中已有 Node.js 和现有本地转换器，不会自动安装依赖。保留产品 ID、资源 ID 和 KEY_ID 映射；通过资源映射替换图片，在真实图片就绪前保留参考图片标记。应重新生成 C/H 资源，不直接修改生成表。普通构建不需要执行旧版生成脚本。

## 本机验证

在项目根目录执行，要求已安装 GCC 并可通过 PATH 调用：

```powershell
New-Item -ItemType Directory -Force build/host_tests | Out-Null
gcc -std=gnu17 -Wall -Wextra -Werror `
  -finput-charset=UTF-8 -fexec-charset=UTF-8 `
  -I components/xf_core/include `
  -I components/xf_bsp/include `
  -I components/xf_data/include `
  -I components/xf_data/generated `
  -I components/xf_ui/generated `
  tests/test_xf_core.c `
  components/xf_core/xf_compare.c `
  components/xf_core/xf_idle.c `
  components/xf_core/xf_state_reducer.c `
  components/xf_data/xf_product_lookup.c `
  components/xf_data/generated/xf_product_catalog.c `
  components/xf_ui/generated/xf_strings.c `
  -o build/host_tests/test_xf_core.exe
& ./build/host_tests/test_xf_core.exe
```

**2026-11-17** 最近一次软件验证结果：

- 2,945 项本机检查全部通过，覆盖目录约束、对比规则、轮播与无操作时序、双语文本、协议编号、事件序号、按住状态恢复、跨产品滚动位置、延迟滚动事件拒绝、独立提示定时及提示音包络、幅度和周期。
- ESP-IDF 完整编译和分区大小检查通过。
- 18、22、23、29、32 像素字库已覆盖生成文本所需的中文字符。

屏幕与触摸的实际表现、实际帧率、外部展板通信、灯光和喇叭输出仍需实机验证。屏幕初始化时序和信号完整性与实际板卡有关。启动刷色测试已关闭，上电直接进入产品界面。

## 本地资料路径

| 资料 | 本地路径 |
| --- | --- |
| 固件工程 | `D:\XIAO_test\MFSZ26\XIAO_Family\Software\touch_2.16` |
| 主板原理图 | `D:\XIAO_test\MFSZ26\XIAO_Family\Hardware\XIAO-Console-AMOLED-Mainboard` |
| 屏幕规格书 | `D:\XIAO_test\MFSZ26\XIAO_Family\Hardware\Display` |
| 原始 XIAO SVG 图片 | `D:\XIAO_test\MFSZ26\XIAO_Family\Hardware\picture` |
| 交互设计、数据与资源交付包 | `C:\Users\seeed\Documents\ChatGPT\MFSZ 2026\屏幕交互设计` |
| 外部 I²C 协议 | `C:\Users\seeed\Documents\ChatGPT\MFSZ 2026\协议\XIAO_Touch_I2C_Protocol_v1.md` |

这些绝对路径对应当前工作站。普通编译可直接使用现有生成资源；重新生成时需要对应的本地源资料。维护应限定在已授权的文件和任务范围内；下载依赖、修改 SDK 或全局设置、烧录设备均需对应授权。
