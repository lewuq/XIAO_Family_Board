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

## 文件说明

仓库忽略 `build/`、`.pio/`、`node_modules/`、Python 缓存及编译中间文件。屏幕图片的 `.bin` 资源、生成的字体/产品数据源码、`sdkconfig` 和依赖锁文件属于构建输入，予以保留。

`touch_2.16/managed_components` 保留现有依赖源码，包括项目通过本地路径引用的 LVGL 9.4.0。第三方组件的许可证随其源码保留。需要重新生成字体时，可在 `touch_2.16/tools` 中执行 `npm ci` 恢复字体转换工具。

项目原有文档和辅助脚本中的 `D:\...`、`C:\...` 路径对应原开发电脑；在其他电脑上使用时，请按本地 SDK、字体及素材位置调整。普通固件构建使用仓库内现有的生成资源。
