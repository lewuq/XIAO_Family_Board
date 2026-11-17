/*
 * 自动生成文件 —— 请勿手工编辑。
 * 由 tools/generate_catalog.py 依据交接包数据生成：
 *   product_catalog.json
 *   asset_manifest.json
 * 重新生成: python tools/build_assets.py
 */

#include "product_catalog.h"

const char *const g_manufacturer_names[PRODUCT_MANUFACTURER_COUNT] = {
    "Microchip",
    "STMicroelectronics",
    "Silicon Labs",
    "Raspberry Pi",
    "Renesas",
    "Espressif",
    "Nordic Semiconductor",
};

const char *const g_detail_group_names[DETAIL_GROUP_COUNT] = {
    "计算与存储",
    "无线与接口",
    "传感与扩展",
    "供电与尺寸",
    "应用领域",
};

const char *const g_compare_category_names[COMPARE_CATEGORY_COUNT] = {
    "计算与存储",
    "连接与接口",
    "传感与应用",
};

const char *const g_field_names[SPEC_FIELD_COUNT] = {
    "MCU / 架构", /* processor */
    "最高主频", /* cpu_clock_max */
    "SRAM", /* sram */
    "片内非易失存储", /* internal_nvm */
    "板载外部 Flash", /* external_flash */
    "PSRAM", /* psram */
    "NPU / PIO", /* npu_pio */
    "Wi-Fi", /* wifi */
    "Bluetooth LE", /* ble */
    "Thread/Zigbee/Matter", /* mesh */
    "对外可用 GPIO", /* exposed_gpio */
    "接口", /* interfaces */
    "IMU", /* imu */
    "麦克风", /* microphone */
    "摄像头", /* camera */
    "板载电池充电", /* battery_charging */
    "核心板占板尺寸", /* footprint */
    "适用领域", /* applications */
};

const uint8_t g_field_detail_group[SPEC_FIELD_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4
};

const int8_t g_field_compare_category[SPEC_FIELD_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2
};

const char *const g_product_id_strings[PRODUCT_COUNT] = {
    "samd21",
    "samd21_plus",
    "stm32c5",
    "mg24_sense",
    "rp2350",
    "rp2040",
    "rp2040_plus",
    "ra4m1",
    "esp32_c3",
    "esp32_c5",
    "esp32_c6",
    "esp32_s3_plus",
    "esp32_s3_sense",
    "nrf52840_sense",
    "nrf52840_plus",
    "nrf54l15_sense",
    "nrf54lm20a_sense",
    "nrf54lm20b_sense",
};

const char *const g_asset_keys[ASSET_COUNT] = {
    "samd21_front",
    "samd21_plus_front",
    "stm32c5_front",
    "mg24_sense_front",
    "rp2350_front",
    "rp2040_front",
    "rp2040_plus_front",
    "ra4m1_front",
    "esp32_c3_front",
    "esp32_c5_front",
    "esp32_c6_front",
    "esp32_s3_plus_front",
    "esp32_s3_sense_front",
    "nrf52840_sense_front",
    "nrf52840_plus_front",
    "nrf54l15_sense_front",
    "nrf54lm20a_sense_front",
    "nrf54lm20b_sense_front",
};

const product_t g_products[PRODUCT_COUNT] = {
    {
        .id = PRODUCT_SAMD21,
        .id_string = "samd21",
        .name = "XIAO SAMD21",
        .short_name = "SAMD21",
        .manufacturer = "Microchip",
        .manufacturer_index = 0,
        .feature_en = "10-bit DAC",
        .feature_zh = "模拟电压输出",
        .applications = "模拟控制\n电子乐器\n入门教学",
        .notes = "电池充电不受支持；DAC 为真实模拟输出。",
        .sources = "samd21",
        .asset = ASSET_SAMD21_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_SAMD21_PLUS,
        .id_string = "samd21_plus",
        .name = "XIAO SAMD21 Plus",
        .short_name = "SAMD21 Plus",
        .manufacturer = "Microchip",
        .manufacturer_index = 0,
        .feature_en = "More GPIOs",
        .feature_zh = "扩展输入输出",
        .applications = "多传感器采集\n多路控制",
        .notes = "",
        .sources = "samd21",
        .asset = ASSET_SAMD21_PLUS_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_STM32C5,
        .id_string = "stm32c5",
        .name = "XIAO STM32C5",
        .short_name = "STM32C5",
        .manufacturer = "STMicroelectronics",
        .manufacturer_index = 1,
        .feature_en = "CAN FD",
        .feature_zh = "CAN FD 总线连接",
        .applications = "机器人节点\n总线数据记录",
        .notes = "一通道带板载 CAN 收发器；另一通道需外接收发器。GPIO 总数未按电源焊盘计入。",
        .sources = "stm32c5",
        .asset = ASSET_STM32C5_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_MG24_SENSE,
        .id_string = "mg24_sense",
        .name = "XIAO MG24 Sense",
        .short_name = "MG24 Sense",
        .manufacturer = "Silicon Labs",
        .manufacturer_index = 2,
        .feature_en = "Zigbee + IMU",
        .feature_zh = "无线连接与运动感知",
        .applications = "智能家居\n无线姿态检测",
        .notes = "",
        .sources = "mg24",
        .asset = ASSET_MG24_SENSE_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_RP2350,
        .id_string = "rp2350",
        .name = "XIAO RP2350",
        .short_name = "RP2350",
        .manufacturer = "Raspberry Pi",
        .manufacturer_index = 3,
        .feature_en = "Arm / RISC-V",
        .feature_zh = "可选处理器架构",
        .applications = "嵌入式控制\n架构教学\n自定义时序",
        .notes = "运行时使用两个处理核心；不得表述为四核心同时运行。",
        .sources = "rp2350 / rp2350_soc / pio",
        .asset = ASSET_RP2350_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_RP2040,
        .id_string = "rp2040",
        .name = "XIAO RP2040",
        .short_name = "RP2040",
        .manufacturer = "Raspberry Pi",
        .manufacturer_index = 3,
        .feature_en = "PIO State Machines",
        .feature_zh = "可编程硬件时序",
        .applications = "灯光控制\n自定义通信接口",
        .notes = "PIO 是独立硬件状态机；电池充电与 Plus 版分开记录。",
        .sources = "rp2040 / pio",
        .asset = ASSET_RP2040_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_RP2040_PLUS,
        .id_string = "rp2040_plus",
        .name = "XIAO RP2040 Plus",
        .short_name = "RP2040 Plus",
        .manufacturer = "Raspberry Pi",
        .manufacturer_index = 3,
        .feature_en = "More GPIOs",
        .feature_zh = "更多外设连接",
        .applications = "交互控制器\n多传感器连接",
        .notes = "",
        .sources = "rp2040 / pio",
        .asset = ASSET_RP2040_PLUS_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_RA4M1,
        .id_string = "ra4m1",
        .name = "XIAO RA4M1",
        .short_name = "RA4M1",
        .manufacturer = "Renesas",
        .manufacturer_index = 4,
        .feature_en = "14-bit ADC",
        .feature_zh = "模拟信号采集",
        .applications = "测量仪表\n模拟传感器采集",
        .notes = "CAN 控制器能力与是否集成总线收发器分开表述。",
        .sources = "ra4m1",
        .asset = ASSET_RA4M1_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_ESP32_C3,
        .id_string = "esp32_c3",
        .name = "XIAO ESP32-C3",
        .short_name = "ESP32-C3",
        .manufacturer = "Espressif",
        .manufacturer_index = 5,
        .feature_en = "Wi-Fi + BLE",
        .feature_zh = "基础无线物联网",
        .applications = "联网传感器\n手机控制",
        .notes = "",
        .sources = "esp32c3",
        .asset = ASSET_ESP32_C3_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_ESP32_C5,
        .id_string = "esp32_c5",
        .name = "XIAO ESP32-C5",
        .short_name = "ESP32-C5",
        .manufacturer = "Espressif",
        .manufacturer_index = 5,
        .feature_en = "Dual-band Wi-Fi 6",
        .feature_zh = "双频无线连接",
        .applications = "5 GHz 联网终端\n无线数据传输",
        .notes = "片内 SRAM 采用规格所列 384 KiB；可分配堆容量由固件测量，不能等同该值。",
        .sources = "esp32c5 / esp32c5_soc",
        .asset = ASSET_ESP32_C5_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_ESP32_C6,
        .id_string = "esp32_c6",
        .name = "XIAO ESP32-C6",
        .short_name = "ESP32-C6",
        .manufacturer = "Espressif",
        .manufacturer_index = 5,
        .feature_en = "Wi-Fi 6 + Thread",
        .feature_zh = "智能家居连接",
        .applications = "Matter 设备\nThread 传感器",
        .notes = "主频字段记录 HP 核；无线协议及 Matter 功能依赖相应软件栈。",
        .sources = "esp32c6",
        .asset = ASSET_ESP32_C6_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_ESP32_S3_PLUS,
        .id_string = "esp32_s3_plus",
        .name = "XIAO ESP32-S3 Plus",
        .short_name = "ESP32-S3 Plus",
        .manufacturer = "Espressif",
        .manufacturer_index = 5,
        .feature_en = "16 MB Flash",
        .feature_zh = "大容量应用与图形资源",
        .applications = "带屏交互终端\n联网控制器",
        .notes = "片内 SRAM 待与芯片资料确认；不得以 PSRAM 替代 SRAM 字段。",
        .sources = "esp32s3",
        .asset = ASSET_ESP32_S3_PLUS_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_ESP32_S3_SENSE,
        .id_string = "esp32_s3_sense",
        .name = "XIAO ESP32-S3 Sense",
        .short_name = "ESP32-S3 Sense",
        .manufacturer = "Espressif",
        .manufacturer_index = 5,
        .feature_en = "Camera + Mic",
        .feature_zh = "图像与声音采集",
        .applications = "视觉识别原型\n声音识别",
        .notes = "传感器位于 Sense 扩展板；相机为供货批次相关型号，不固定展示 OV2640 / OV3660 / OV5640 的某一个。外形字段表示核心板占板尺寸。",
        .sources = "esp32s3",
        .asset = ASSET_ESP32_S3_SENSE_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_NRF52840_SENSE,
        .id_string = "nrf52840_sense",
        .name = "XIAO nRF52840 Sense",
        .short_name = "nRF52840 Sense",
        .manufacturer = "Nordic Semiconductor",
        .manufacturer_index = 6,
        .feature_en = "IMU + Mic",
        .feature_zh = "运动与声音采集",
        .applications = "手势识别\n声音感知穿戴设备",
        .notes = "当前版本按最新展示清单使用 Sense 普通版；不得静默替换为 Sense Plus。BLE 数字版本在来源中不一致，界面只显示 Bluetooth LE。",
        .sources = "nrf52840",
        .asset = ASSET_NRF52840_SENSE_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_NRF52840_PLUS,
        .id_string = "nrf52840_plus",
        .name = "XIAO nRF52840 Plus",
        .short_name = "nRF52840 Plus",
        .manufacturer = "Nordic Semiconductor",
        .manufacturer_index = 6,
        .feature_en = "More GPIOs",
        .feature_zh = "蓝牙外设扩展",
        .applications = "蓝牙控制器\n多传感器终端",
        .notes = "官方页面同时存在 18 与 20 的 GPIO 口径；数量待核实，不显示猜测数值。",
        .sources = "nrf52840",
        .asset = ASSET_NRF52840_PLUS_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_NRF54L15_SENSE,
        .id_string = "nrf54l15_sense",
        .name = "XIAO nRF54L15 Sense",
        .short_name = "nRF54L15 Sense",
        .manufacturer = "Nordic Semiconductor",
        .manufacturer_index = 6,
        .feature_en = "BLE 6 + IMU",
        .feature_zh = "新一代低功耗感知",
        .applications = "运动传感\n无线穿戴设备",
        .notes = "内部存储采用 NVM 命名；协处理器与主处理器分开描述。",
        .sources = "nrf54l15",
        .asset = ASSET_NRF54L15_SENSE_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_NRF54LM20A_SENSE,
        .id_string = "nrf54lm20a_sense",
        .name = "XIAO nRF54LM20A Sense",
        .short_name = "nRF54LM20A Sense",
        .manufacturer = "Nordic Semiconductor",
        .manufacturer_index = 6,
        .feature_en = "8 MB Ext. Flash",
        .feature_zh = "传感数据与资源存储",
        .applications = "数据记录\n无线智能感知",
        .notes = "源文件名称含 nRF54L15LM20A，按现有素材作为 LM20A 图片；需与实物复核。nPM1300 电源管理。",
        .sources = "lm20a",
        .asset = ASSET_NRF54LM20A_SENSE_FRONT,
        .is_placeholder = false,
        .placeholder_product_id = PRODUCT_ID_INVALID,
    },
    {
        .id = PRODUCT_NRF54LM20B_SENSE,
        .id_string = "nrf54lm20b_sense",
        .name = "XIAO nRF54LM20B Sense",
        .short_name = "nRF54LM20B Sense",
        .manufacturer = "Nordic Semiconductor",
        .manufacturer_index = 6,
        .feature_en = "Axon NPU",
        .feature_zh = "本地 AI 运算加速",
        .applications = "动作模型推理\n声音模型推理\n异常识别",
        .notes = "暂用 LM20A 图像占位；身份、图片资源键、参数记录均独立。已核实值仅为 SoC 能力；XIAO 板载传感器、存储和引出接口待独立资料核实。",
        .sources = "lm20b_soc",
        .asset = ASSET_NRF54LM20B_SENSE_FRONT,
        .is_placeholder = true,
        .placeholder_product_id = PRODUCT_NRF54LM20A_SENSE,
    },
};

const spec_field_t g_specs[PRODUCT_COUNT][SPEC_FIELD_COUNT] = {
    /* samd21 */
    {
        [0] = { .value = "ATSAMD21G18A / Cortex-M0+", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "48", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "32", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "256 KiB Flash", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "11", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nUSB\nDAC", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "不支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "模拟控制\n电子乐器\n入门教学", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* samd21_plus */
    {
        [0] = { .value = "ATSAMD21G18A / Cortex-M0+", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "48", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "32", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "256 KiB Flash", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "27", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "2x I2C\nUART\nSPI\nI2S\nUSB\nDAC", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "多传感器采集\n多路控制", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* stm32c5 */
    {
        [0] = { .value = "STM32C5A3CG / Cortex-M33", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "144", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "256", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "1024 KiB Flash", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "16", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nUSB\n2x CAN FD", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "6-axis IMU", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "机器人节点\n总线数据记录", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* mg24_sense */
    {
        [0] = { .value = "EFR32MG24 / Cortex-M33", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "78", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "256", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "1536 KiB Flash", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "4", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "Thread\nZigbee\nMatter", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "19", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\n2x UART\n2x SPI", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "6-axis IMU", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "Analog microphone", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "智能家居\n无线姿态检测", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* rp2350 */
    {
        [0] = { .value = "RP2350 / Dual Cortex-M33 or Hazard3 RISC-V", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "150", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "520", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "2", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "12（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "19", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "2x I2C\n2x UART\n2x SPI\nUSB\nPIO", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "嵌入式控制\n架构教学\n自定义时序", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* rp2040 */
    {
        [0] = { .value = "RP2040 / Dual Cortex-M0+", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "133", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "264", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "2", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "8（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "11", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nUSB\nPIO", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "不支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "灯光控制\n自定义通信接口", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* rp2040_plus */
    {
        [0] = { .value = "RP2040 / Dual Cortex-M0+", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "133", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "264", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "2", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "8（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "26", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "2x I2C\nUART\nSPI\nUSB\nPIO", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "交互控制器\n多传感器连接", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* ra4m1 */
    {
        [0] = { .value = "RA4M1 / Cortex-M4F", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "48", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "32", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "256 KiB Flash", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "19", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "2x I2C\n2x UART\n2x SPI\nUSB\nCAN\n12-bit DAC", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "测量仪表\n模拟传感器采集", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* esp32_c3 */
    {
        [0] = { .value = "ESP32-C3 / RISC-V", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "160", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "400", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "4", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "2.4 GHz Wi-Fi 4", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "11", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nUSB Serial/JTAG", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "联网传感器\n手机控制", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* esp32_c5 */
    {
        [0] = { .value = "ESP32-C5 / RISC-V", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "240", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "384", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "8", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "8", .unit = "MiB", .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "2.4 / 5 GHz Wi-Fi 6", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "11", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "5 GHz 联网终端\n无线数据传输", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* esp32_c6 */
    {
        [0] = { .value = "ESP32-C6 / RISC-V HP + LP", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "160", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "512", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "4", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "2.4 GHz Wi-Fi 6", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "Thread\nZigbee\nMatter", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "11", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nSDIO", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "Matter 设备\nThread 传感器", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* esp32_s3_plus */
    {
        [0] = { .value = "ESP32-S3 / Dual Xtensa LX7", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "240", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* SRAM */
        [3] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "16", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "8", .unit = "MiB", .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "2.4 GHz Wi-Fi", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "18", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "2x UART\nI2C\n2x SPI\nI2S\nUSB", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "带屏交互终端\n联网控制器", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* esp32_s3_sense */
    {
        [0] = { .value = "ESP32-S3 / Dual Xtensa LX7", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "240", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* SRAM */
        [3] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "8", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "8", .unit = "MiB", .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "2.4 GHz Wi-Fi", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "11", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nI2S\nUSB\nmicroSD on expansion", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "Digital microphone on Sense expansion", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "Camera on Sense expansion", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "视觉识别原型\n声音识别", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* nrf52840_sense */
    {
        [0] = { .value = "nRF52840 / Cortex-M4F", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "64", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "256", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "1024 KiB Flash", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "2", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE\nBluetooth Mesh", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "11", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nUSB\nNFC\nSWD", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "6-axis IMU", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "PDM microphone", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "手势识别\n声音感知穿戴设备", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* nrf52840_plus */
    {
        [0] = { .value = "nRF52840 / Cortex-M4F", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "64", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "256", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "1024 KiB Flash", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "2", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE\nBluetooth Mesh", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "待核实（来源口径冲突）", .unit = NULL, .status = 2, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\n2x UART\n2x SPI\nI2S\nUSB\nNFC\nSWD", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "蓝牙控制器\n多传感器终端", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* nrf54l15_sense */
    {
        [0] = { .value = "nRF54L15 / Cortex-M33 + RISC-V coprocessor", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "128", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "256", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "1536 KiB NVM", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE 6.0", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "Thread\nZigbee\nMatter", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nNFC", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "6-axis IMU", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "PDM microphone", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "运动传感\n无线穿戴设备", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* nrf54lm20a_sense */
    {
        [0] = { .value = "nRF54LM20A / Cortex-M33 + RISC-V coprocessor", .unit = NULL, .status = 0, .scope = 0 }, /* MCU / 架构 */
        [1] = { .value = "128", .unit = "MHz", .status = 0, .scope = 0 }, /* 最高主频 */
        [2] = { .value = "512", .unit = "KiB", .status = 0, .scope = 0 }, /* SRAM */
        [3] = { .value = "2048 KiB NVM", .unit = NULL, .status = 0, .scope = 0 }, /* 片内非易失存储 */
        [4] = { .value = "8", .unit = "MiB", .status = 0, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* PSRAM */
        [6] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* NPU / PIO */
        [7] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE 6.0", .unit = NULL, .status = 0, .scope = 0 }, /* Bluetooth LE */
        [9] = { .value = "Thread\nZigbee\nMatter", .unit = NULL, .status = 0, .scope = 0 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "28", .unit = "count", .status = 0, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "I2C\nUART\nSPI\nNFC", .unit = NULL, .status = 0, .scope = 0 }, /* 接口 */
        [12] = { .value = "6-axis IMU", .unit = NULL, .status = 0, .scope = 0 }, /* IMU */
        [13] = { .value = "PDM microphone", .unit = NULL, .status = 0, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "无", .unit = NULL, .status = 0, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "支持", .unit = NULL, .status = 0, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "21 × 17.8", .unit = "mm", .status = 0, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "数据记录\n无线智能感知", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
    /* nrf54lm20b_sense */
    {
        [0] = { .value = "nRF54LM20B / Cortex-M33 + RISC-V coprocessor（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* MCU / 架构 */
        [1] = { .value = "128（芯片能力）", .unit = "MHz", .status = 0, .scope = 1 }, /* 最高主频 */
        [2] = { .value = "512（芯片能力）", .unit = "KiB", .status = 0, .scope = 1 }, /* SRAM */
        [3] = { .value = "2048 KiB NVM（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* 片内非易失存储 */
        [4] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 板载外部 Flash */
        [5] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* PSRAM */
        [6] = { .value = "Axon NPU（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* NPU / PIO */
        [7] = { .value = "无（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* Wi-Fi */
        [8] = { .value = "Bluetooth LE（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* Bluetooth LE */
        [9] = { .value = "Thread\nZigbee\nMatter（芯片能力）", .unit = NULL, .status = 0, .scope = 1 }, /* Thread/Zigbee/Matter */
        [10] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 对外可用 GPIO */
        [11] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 接口 */
        [12] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* IMU */
        [13] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 麦克风 */
        [14] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 摄像头 */
        [15] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 板载电池充电 */
        [16] = { .value = "待核实", .unit = NULL, .status = 1, .scope = 0 }, /* 核心板占板尺寸 */
        [17] = { .value = "动作模型推理\n声音模型推理\n异常识别", .unit = NULL, .status = 0, .scope = 0 }, /* 适用领域 */
    },
};
