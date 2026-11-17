# MFSZ PCB：触摸、RGB 与 I2C 从机 V1

本板 XIAO ESP32S3 Plus 始终是 I2C 从机，七位地址为 `0x42`。两片 TTP229-BSF 提供 K0–K8、K10–K18，D1 控制 18 颗 WS2812。源码注释使用英文。

本版实现已确认的双向 V1 协议。**与上一版“直接读四字节触摸位图”的接口不同：现在必须先选择寄存器。** 上电默认读取 STATUS；触摸位图在寄存器 `0x02`，按下/松开事件在 `0x01`。旧主机程序需一起更新，不能把 STATUS 当作触摸位图解析。

## 接线与文件

| 功能 | XIAO 引脚 | GPIO |
|---|---|---|
| WS2812 DIN | D1 | 2 |
| CH1 SDO / SCL | D2 / D3 | 3 / 4 |
| CH2 SDO / SCL | D4 / D5 | 5 / 6 |
| 对外 I2C SDA / SCL | D9 / D10 | 8 / 9 |
| 板载用户 LED，低有效 | LED_BUILTIN | 21 |

主机与本板共地，SDA/SCL 上拉到 3.3 V；短线、100 kHz 可从每根 4.7 kΩ 起选，已有合适上拉时不重复加。WS2812 使用板上 5 V 供电，D1 是数据线。D8/EXT_IO1 的实际接线尚未确认，本版不启用 IRQ，主机按协议轮询。

- `src/main.cpp`：TTP229 时序、逐键消抖、Wire 从机回调、RMT 灯链和 USB 诊断。
- `include/TouchProtocol.h`：协议解析、32 条事件 FIFO、序号确认和 RGB 合成；本身不依赖 Arduino，便于本机测试。
- `include/XiaoDeviceMap.h`：18 款 XIAO 的 KEY_ID、名称、物理灯珠映射。
- `docs/XIAO_Touch_I2C_Protocol_v1.md`：完整命令、返回格式、型号映射和交互示例。
- `examples/I2C_Master_Read/I2C_Master_Read.ino`：运行在第二块主机控制板上的新版示例。
- `test/protocol_native.cpp`：协议核心的本机 C++ 回归测试。

不初始化屏幕、音频或无线，不要再运行另一段占用 D1–D5 或 D9/D10 的代码。TTP229 的 SCL/SDO 是专用串行接口，和新增的 D9/D10 标准 I2C 总线分开。

## 两种交互同时工作

1. 铜皮按键消抖确认按下：无需等待主机回传，对应 WS2812 立即亮 Seeed 绿，并生成 DOWN 事件；松开生成 UP。
2. 主机选中设备：下发 `SELECT_DEVICE` 或 `SET_RGB`，对应灯保持选中色，不要求手指按住。
3. 同一设备同时被触摸、被主机选中：触摸时优先显示 Seeed 绿，松开后恢复主机指定颜色。未被主机选中时随松开熄灭。
4. 清除主机选中项不会关闭正在触摸的绿灯。主机灯控命令不会生成虚假触摸事件。
5. 板载用户 LED 只跟随是否存在触摸，不受屏幕选择影响。

基础绿为 `#8FC31F`，默认 `kWs2812Brightness=64`（约 25%）。使用 24 位 GRB、800 kbit/s RMT 波形，400 us 复位。仅在最终颜色改变时刷新整条灯链，实际发送在锁外执行；主机只改颜色、不改选中设备时也会正确刷新。

## I2C 快速使用

所有字节以下均为十六进制；地址 `0x42` 不包含在数据字节内。主机先写读寄存器地址并结束写事务，再一次读取四字节。

| 操作 | 主机向 0x42 写入 | 后续读取 |
|---|---|---|
| STATUS | `00` | 4 字节：版本、标志、队列数量、上条写命令结果 |
| 查看队首事件 | `01` | 4 字节：动作、KEY_ID、SEQ_L、SEQ_H |
| 当前触摸状态 | `02` | 4 字节，小端序，bit n=Kn，bit9 固定为 0 |
| 单选 K17 / WS2 | `10 11` | 可读 STATUS 核对 RESULT=00 |
| K0 / WS18 显示蓝色，保留其他选择 | `11 00 00 00 FF` | 同上 |
| 清除全部主机选择 | `12` | 手指按住的灯仍亮 |
| 确认序号 1 的事件 | `20 01 00` | 匹配时出队；重复 ACK 不误删下一条 |
| 清空事件及溢出标志 | `21` | 不重置下一事件序号，不改变触摸和灯光 |

事件 ACTION：`01`=DOWN，`00`=UP，`FF`=无事件。空队列返回 `FF FF 00 00`。例如 `01 11 01 00` 表示 K17（XIAO nRF54LM20A Sense）按下，序号 1。

读取事件不会出队，主机处理后必须确认，UP 也需取走并确认。FIFO 满时保留已有事件、丢弃新事件并报告溢出；当前状态位图和触摸灯继续更新。主机遇到溢出时清空队列并读取 `0x02` 恢复当前状态。没有主机消费事件时，按下/松开超过 32 条后出现溢出标志是预期行为。

Wire 回调与主循环共享协议状态时使用短临界区；只在回调中解析命令或复制四字节，不在回调中打印 USB、扫描触摸或刷新 WS2812。

## 编译与串口

PlatformIO 环境沿用 `seeed-xiao-esp32-s3-plus`、SeeedStudio 平台、Arduino-ESP32 3.x，无额外库依赖。使用 Build、Upload、Monitor；USB CDC 为 115200。

```powershell
pio run -e seeed-xiao-esp32-s3-plus
pio run -e seeed-xiao-esp32-s3-plus -t upload --upload-port COM实际端口
pio device monitor -b 115200 -p COM实际端口
```

从机 USB 单字符命令：`?` 帮助，`s` 状态/原始数据/队首事件/主机选中状态，`m` 完整型号映射，`r` 原始帧输出开关，`c` 仅清空逐键测试覆盖记录。`c` 不会清空主机尚未确认的事件。

主机示例运行于第二块板：默认使用该主机板的 Wire 引脚；若也是 XIAO ESP32S3 Plus，默认是 D4/D5，将其连接到从机 D9/D10。主机 USB 发送 `g` 选中 K17 绿灯，`b` 加入 K0 蓝灯，`x` 取消主机选择，`s` 查看状态；事件自动读取并 ACK。串口重连视为新会话，屏幕业务应使用幂等的“设置当前设备”操作。

## 实机验证顺序

1. 上电不要触摸焊盘，待 READY。发送 `m` 确认 18 个型号、KEY_ID 和 WS 序号。
2. 依次按住各键约一秒再松开，核对 USB DOWN/UP、型号、对应绿灯和板载用户灯。必测 K8、K18 的第九通道。
3. 主机读 STATUS 应为版本 1、READY 标志有效。读取 K17 的事件应包含 KEY_ID=11，反复读取但不 ACK 时队首不变。
4. 确认该序号后再重复确认，检查没有删掉下一条事件。
5. 无人触摸时，在主机示例发送 `g`，WS2 应持续亮绿；发送 `b`，WS18 加入蓝色。此时触摸 K0 应显示绿，松开恢复蓝。
6. 主机选中 K0 的同时按住 K17，两灯均应亮；发送 `x` 后 WS18 熄灭，WS2 保持触摸绿，松开 K17 后熄灭。
7. 多键同时触摸，核对事件顺序、触摸状态和灯光独立性；放开全部焊盘观察至少 30 秒是否误触。

先前 K17 持续被检测为按住的问题不会被软件屏蔽；若仍存在，WS2 和 bit17 会继续反映该状态，需按硬件排查流程处理。raw 全 1 也不能单独证明触摸芯片已在线。

## 本机协议测试

Windows 上安装并配置 g++ 后，在工程终端执行：

```powershell
powershell -ExecutionPolicy Bypass -File test\run_native.ps1
```

测试直接调用固件使用的同一个协议类，覆盖 18 款映射、参数校验、按下/松开和多键、重复读取/确认、灯光并行与颜色恢复、32 条 FIFO 溢出恢复及 16 位序号回绕。它不连接硬件，也不能替代 I2C 电气、实际灯光或焊接验证。实际软件验证结果以本轮交付说明为准。

## 参考

- [Tontek TTP229-BSF V1.1](https://www.tontek.com.tw/uploads/product/97/TTP229-BSF_V1.1_EN.pdf)
- [Seeed XIAO ESP32S3 引脚](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [Arduino-ESP32 I2C 从机 API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html#i2c-slave-mode)
- [Arduino-ESP32 RMT API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/rmt.html)
