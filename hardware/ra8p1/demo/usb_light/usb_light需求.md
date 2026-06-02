# 需求

基于RA8P1实现对USB灯的控制。

当前使用电脑串口调试助手可以控制USB灯，插入电脑后，设备管理器出现USB-SERIAL CH340 COM X的字样

软件环境：fsp v6.4.0

目前的控制指令（必须以这个为准）：

红灯常亮
01 06 00 C2 00 11 E8 3A

黄灯常亮
01 06 00 C2 00 12 A8 3B

绿灯常亮
01 06 00 C2 00 13 69 FB

红灯常亮+喇叭
01 06 00 C2 00 14 28 39

白灯常亮
01 06 00 C2 00 15 E9 F9

青灯常亮
01 06 00 C2 00 16 A9 F8

紫灯常亮
01 06 00 C2 00 17 68 38

蓝灯常亮
01 06 00 C2 00 18 28 3C

红灯慢闪
01 06 00 C2 00 21 E9 FE

黄灯慢闪
01 06 00 C2 00 22 A9 FF

绿灯慢闪
01 06 00 C2 00 23 68 3F

红灯慢闪+喇叭
01 06 00 C2 00 24 29 FD

白灯慢闪
01 06 00 C2 00 25 E8 3D

青灯慢闪
01 06 00 C2 00 26 A8 3C

紫灯慢闪
01 06 00 C2 00 27 69 FC

蓝灯慢闪
01 06 00 C2 00 28 29 F8

红灯快闪
01 06 00 C2 00 31 E8 37

黄灯快闪
01 06 00 C2 00 32 A8 36

绿灯快闪
01 06 00 C2 00 33 69 F6

红灯快闪+喇叭
01 06 00 C2 00 34 28 34

白灯快闪
01 06 00 C2 00 35 E9 F4

青灯快闪
01 06 00 C2 00 36 A9 F5

紫灯快闪
01 06 00 C2 00 37 68 35

蓝灯快闪
01 06 00 C2 00 38 28 31

喇叭开
01 06 00 C2 00 40 29 E2

喇叭关
01 06 00 C2 00 41 E8 22

关闭灯和喇叭
01 06 00 C2 00 60 E9 EB

## 相关文档

- **[USB_HOST开发说明.md](USB_HOST开发说明.md)**：USB Host 整体流程图、FSP 关键 API、分层与调试说明（面向不熟悉 USB 的读者）。
- **[USB报警灯Host复用指南.md](USB报警灯Host复用指南.md)**：迁移到其他工程时的 RASC 配置、源码集成、踩坑与验收清单。

## 固件实现（`hardware/ra8p1/demo/usb_light`）

- **USB**：Host + HS + `USB_IP1`，`r_usb_hvnd`；上电 `R_USB_Open` 后 `R_USB_VbusSet(USB_ON)`（P407 VBUSEN）。
- **CH340**：`src/ch340_host.c` 按 Linux ch341 做厂商控制传输 + Bulk OUT；4800 8N1，发完即走。
- **灯控 API**：`usb_light_set(usb_light_mode_t mode)` 仅 **原样发送** 上文每条 8 字节 HEX（固件内已写死，**不算校验、不改字节**）。
- **与串口助手一致**：HEX 发 `01 06 00 C2 00 22 A9 FF` 等，共 8 字节；**不要**发送新行/空行（固件同样只发 8 字节）。
- **演示**：`hal_entry` 主循环 `usb_app_poll()`；就绪后每 **3 s** 一步：红常亮 → 绿常亮 → 蓝常亮 → 红慢闪 → 红快闪+喇叭 → 关灯。
- **编译**：Keil 工程 `usb_light.uvprojx` 已包含 `ch340_host.c` / `usb_light.c` / `usb_app.c`。
- **调试变量**：Keil Watch `g_usb_app_debug` — `configured_seen==1` 表示枚举成功；`ch340_ready==1` 表示 CH340 已初始化；`last_light_err==0` 表示 Modbus 帧已发出；`last_usb_event` 为最近一次 USB 事件码（如 `USB_STATUS_CONFIGURED`）。

## 无反应时排查

1. **供电 ≠ 通信**：口能给手机充电只说明 VBUS 有电，仍需 `configured_seen` 为 1。
2. **线材**：请用 **USB OTG**（Type-C 口需支持 Host），普通充电线/仅 Device 口可能无法枚举。
3. **接法**：灯插在 RA8P1 的 **USB HS（USB_IP1）** 口，不是 FS 调试口。
4. **RASC**：若仍不枚举，可尝试启用 **USB60CLK**（PLL2R÷8）；USB 模块里 **Battery Charging** 建议关闭（工程里已关）。

