# MFSZ PCB：双 TTP229-BSF 触摸验证

仅验证两路触摸芯片并通过 XIAO 原生 USB CDC 打印结果，不初始化屏幕、I2C、WS2812、音频和无线。不得同时运行另一段驱动 D2–D5 的代码。

## 引脚与编号（按本次三张原理图）

| 总线 | TTP229-BSF 引脚 | XIAO 丝印 | ESP32-S3 GPIO | 焊盘映射 |
|---|---|---|---|---|
| CH1 数据 | 12 / SDO1 | D2 | GPIO3 | TP0–TP8 → K0–K8 |
| CH1 时钟 | 13 / SCL1 | D3 | GPIO4 | 同上 |
| CH2 数据 | 12 / SDO2 | D4 | GPIO5 | TP0–TP8 → K10–K18 |
| CH2 时钟 | 13 / SCL2 | D5 | GPIO6 | 同上 |

K0–K8、K10–K18 各为 9 个通道，总计 18 个。没有 K9，不把第二片的本地 TP0 错当成 TP10。未连接的本地 TP9–TP15 只在诊断中显示，不映射成展示位。

## 芯片配置与读取方法

根据截图，两片的 TP2 均经 820 kΩ 接地（R2/R5）：16 键模式。TP3 与 TP4 均经 820 kΩ 接地（R1/R3、R4/R6）：全多键模式。TP1 未加选项电阻：SCL 和 SDO 均低有效，SCL 空闲为高，首个下降沿开始输出 TP0。虽然只接 9 个焊盘，也必须读完整 16 位。

程序在下降沿后等待约 20 µs 再采样，之后拉高约 20 µs，约 25 kHz；每路每 20 ms 读一帧，帧间保持高电平超过 2 ms，用芯片超时机制恢复 TP0 对齐。每帧只在约 0.7 ms 内保护时序，帧内不打印。每键独立消抖 25 ms。

不是标准 I2C，不使用 Wire、不扫描地址、不增加 SDA/SCL I2C 上拉。SDO 为芯片 CMOS 输出，MCU 使用普通 INPUT。两片 3.3 V 供电、与 MCU 共地。

## 编译、烧录、串口

环境名为 `seeed-xiao-esp32-s3-plus`，保留工程使用的 SeeedStudio 平台。板定义及 build_flags 启用原生 USB CDC，串口速度 115200。无需额外 Arduino 库。

在 PlatformIO 工程终端执行：

```powershell
pio run -e seeed-xiao-esp32-s3-plus
pio device list
pio run -e seeed-xiao-esp32-s3-plus -t upload --upload-port COM实际端口
pio device monitor -b 115200 -p COM实际端口
```

也可以直接使用 PlatformIO 的 Build、Upload、Monitor 按钮。COM实际端口要替换为本机实际端口；烧录后 USB 串口可能重新枚举。若未看到启动横幅，发 `?` 或 `s`；程序没有无限等待 USB 连接，5 秒一次 ALIVE 表明主循环仍在运行。若自动烧录失败，再用 XIAO BOOT/RESET 进入下载模式。

## 验证步骤

1. 首次上电时不碰触摸盘，待稳定至少 1 秒；芯片自身上电校准约 0.5 秒。
2. 打开 USB 串口，看到 READY 或 ALIVE。发送 `s`，检查两路状态。无触摸且电路正常时，一般 raw 为 `0xFFFF`，stable 为 none；这只是预期读数，不是连接检测，SDO 断开也可能读全 1。
3. 依次触摸 K0–K8、K10–K18，每次按住约 1 秒再松开。必须是物理触摸哪个焊盘，就打印哪个编号，并分别出现 DOWN 和 UP。
4. 必测 K8 和 K18：这两键映射各芯片 TP8，能帮助发现 8/16 键模式或只读 8 位的错误。
5. 同时触摸同一片的两个焊盘，再跨两片同时触摸，检查多键结果及释放是否独立。
6. 完成后发送 `s`，查看 observed DOWN 18/18 和 still_to_test=none。覆盖计数不能替代逐键核对，也不能证明没有串扰或误触。
7. 最后放开全部焊盘观察 30 秒，正常应没有无故 DOWN；每 5 秒的 ALIVE 是正常心跳。

输出示例（是示例，不是本机已取得的实测日志）：

```text
[1234 ms] CH1 K0 TP0 DOWN raw=0xFFFE
[2345 ms] CH1 K0 TP0 UP raw=0xFFFF
[3456 ms] CH2 K18 TP8 DOWN raw=0xFEFF
[4567 ms] CH2 K18 TP8 UP raw=0xFFFF
```

串口单字符命令，无需回车：`?`/`h` 帮助；`s` 当前状态与未测试清单；`r` 开关每 250 ms 的原始帧打印；`c` 清空覆盖记录（全部松开后再触摸，新的 DOWN 才会计数）。

## 读数不正常时

| 现象 | 优先检查 |
|---|---|
| 没有串口、没有 ALIVE | 正确 USB 数据线/端口、是否打开原生 USB CDC、是否已经烧录本程序 |
| 一路始终全 1 且触摸无变化 | `r` 查看原始帧；测 26 脚 3.3 V 与 27 脚地，查 SDO/SCL 导通、虚焊和引脚映射；全 1 不等于芯片在线 |
| 一路全 0、9 键同时触发 | 查 SDO 短接地、电源、实际器件是否为 BSF、TP1 是否意外被下拉 |
| 前 8 键能用，K8/K18 不响应 | 查 TP2 820 kΩ 焊接、16 键配置及 TP8 到焊盘的连接；改变选项后应断电重启 |
| K2/K3/K4 或 K12/K13/K14 不响应 | 这些 TP 引脚同时承担上电选项；检查电阻是否确为 820 kΩ、是否误装低阻值或直接短接地 |
| raw 有变化但需要用力/大面积接触 | 43 pF 是灵敏度调节电容，增大会降低灵敏度；先完成数字接口测试，再按板上实测减小相应组电容，例如试 22 pF/10 pF，数值不是保证值 |
| 闲置误触、多个键乱跳 | 先看 raw，检查焊接、电源、触摸走线及周围接地/金属；软件消抖不能修复硬件串扰 |

截图的 43 pF（工作灵敏度）和 18/24 pF（睡眠唤醒灵敏度）都在数据手册 1–50 pF 范围内，但原理图不能证明实际焊盘大小/走线/外壳条件下的灵敏度足够。此程序用于取得实测数据，不预先判定焊接板已验证通过。

## 核对来源

- [Tontek TTP229-BSF 原厂规格书 V1.1](https://www.tontek.com.tw/uploads/product/97/TTP229-BSF_V1.1_EN.pdf)：第 7–9 页为串行、位序、时序和多键配置，第 12 页为选项表，第 14 页为灵敏度/820 kΩ 注意事项。
- [Seeed XIAO ESP32-S3 系列引脚表](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)；同时核对了本机 PlatformIO 的 `XIAO_ESP32S3_Plus/pins_arduino.h`。
