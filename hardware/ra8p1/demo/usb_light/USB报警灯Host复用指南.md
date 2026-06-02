# USB 报警灯 Host 复用指南

本文档说明如何将本工程 `hardware/ra8p1/demo/usb_light` 中的 **USB Host + CH340 + 灯控** 方案迁移到其他 RA8P1（或同 FSP USB_HS Host 能力）工程。

- 流程与 API 总览见：[USB_HOST开发说明.md](USB_HOST开发说明.md)
- 灯控 8 字节 HEX 表见：[usb_light需求.md](usb_light需求.md)

---

## 1. 适用范围与前提

| 适用 | 不适用 |
|------|--------|
| RA8P1 + FSP 6.4.x，带 **USB_HS（USB_IP1）** Host | 无 USB Host 的 MCU |
| 报警灯内置 **CH340（1A86:7523）** + 文档中的 8 字节指令 | 标准 CDC ACM 串口灯（应走 `r_usb_hcdc`） |
| 裸机主循环轮询 `R_USB_EventGet` | 需按 RTOS 模型改事件接口时 |

**硬件**：Type-C **USB HS Host** 口、**OTG 线**（能给手机充电只说明 VBUS 有电，不保证枚举成功）。

---

## 2. 可复用代码清单

建议整体拷贝 `src/` 下模块（`hal_entry.c` 仅作参考，按目标工程改写）。

| 文件 | 层级 | 复用度 | 职责 |
|------|------|--------|------|
| `ch340_host.c` / `ch340_host.h` | 桥接 | 高 | CH340 厂商控制传输、4800 8N1、Bulk OUT |
| `usb_light.c` / `usb_light.h` | 协议 | 高 | 需求文档 27 条固定 8 字节，`usb_light_set()` |
| `usb_app.c` / `usb_app.h` | 应用壳 | 中 | USB 打开/事件/bringup；含演示可删 |
| `hal_entry.c` | 入口 | 低 | 仅示例：`init` + `usb_app_process()` 循环 |

**当前与 RASC 的硬绑定**（移植时可保留命名或做一层 cfg 抽象）：

- `g_basic0_ctrl` / `g_basic0_cfg`（`hal_data.h` 生成）
- `g_usb_event_ctrl.type = USB_CLASS_HVND`（**必须**，否则 `R_USB_PipeWrite` 失败）
- `g_basic0_cfg.module_number` → 本工程为 **1**（USB_IP1）

---

## 3. RASC 配置（逐步）

### 3.1 添加 Stacks

在 **Stacks** 中按顺序添加（父子关系不能反）：

```text
g_ioport (r_ioport)
g_hvnd0 (r_usb_hvnd)
  └─ g_basic0 (r_usb_basic)   ← 挂在 hvnd 下面
```

**不要** 同时启用 `r_usb_hcdc` 与 `r_usb_hvnd` 指向同一 USB 模块。

### 3.2 `g_basic0` — USB Basic 属性

| 属性 | 本工程取值 | 说明 |
|------|------------|------|
| Name | `g_basic0` | 生成 `g_basic0_ctrl` / `g_basic0_cfg` |
| USB Mode | **Host mode** | |
| USB Speed | **Hi Speed** | |
| USB Module Number | **USB_IP1 Port** | USB_HS，非 FS 调试口 |
| USB Descriptor | NULL | Host Vendor 用 TPL，不需设备描述符表 |
| USB Callback | NULL | 裸机用 `R_USB_EventGet` 轮询 |
| 中断优先级 | 12（可按工程调整） | USBHS / FIFO 中断 |

`configuration.xml` 对应片段：

```xml
property id="module.driver.usb_basic.usb_mode" value="...host"/>
property id="module.driver.usb_basic.usb_speed" value="...hs"/>
property id="module.driver.usb_basic.usb_modulenumber" value="...1"/>
```

### 3.3 `config.driver.usb_basic` — 全局 USB 配置

| 配置项 | 本工程推荐 | 备注 |
|--------|------------|------|
| PLL clock frequency | **24 MHz** | 与 XTAL/PHY 配置一致 |
| Bus wait | 7 cycles | |
| Battery Charging | **建议 Disable** | 实机排查时开启易导致枚举异常；本工程 `r_usb_basic_cfg.h` 为 `USB_CFG_BC DISABLE` |
| Power source | **High** | VBUS 高有效（配合 P407 VBUSEN） |
| DCP | Disable | |
| Class request | Enable | |
| Double buffering | Enable | |
| DMA | Disable | |
| USB Multiport | Disable | |
| Compliance mode | Disable | |
| **TPL table** | **NULL** | 生成 `USB_CFG_TPL = USB_NOVENDOR, USB_NOPRODUCT` |

生成后检查 `ra_cfg/fsp_cfg/r_usb_basic_cfg.h`：

```c
#define USB_CFG_HVND_USE
#define USB_CFG_MODE (USB_CFG_PERI_MODE | USB_CFG_HOST_MODE)
#define USB_CFG_TPLCNT (1)
#define USB_CFG_TPL USB_NOVENDOR, USB_NOPRODUCT
```

### 3.4 时钟（Clocks）

本工程 `ra_cfg.txt` / `ra_gen/bsp_clock_cfg.h`：

| 时钟 | 源 | 分频 | 结果 |
|------|-----|------|------|
| **USBCLK** | PLL2R | ÷10 | 48 MHz |
| **USB60CLK** | PLL2R | ÷8 | 60 MHz（RA8P1 HS 建议启用） |

若枚举不稳定，在 RASC **Clocks** 中确认 **USB60CLK** 已启用（勿仅 Disabled）。

### 3.5 引脚（Pins）

本板 **R7KA8P1KFLCAC** 接线（`ra_gen/pin_data.c`）：

| 引脚 | 功能 | 模式 |
|------|------|------|
| **P407** | USBHS_VBUSEN | Peripheral — USB_HS |
| **P408** | USBHS_VBUS | Peripheral — USB_HS |
| **USBHS_DP / DM** | 差分 | 原理图专用脚，Pin Config 中只读 |

RASC Pin Configuration 要点：

- `usbhs.usbhs_vbusen` → **P407**
- `usbhs.usbhs_vbus` → **P408**
- **USBHS_ID** 可不接（固定 Host 角色时可不配）

换板时必须按 **新板原理图** 重配 VBUSEN/VBUS，不能照搬 P407/P408。

### 3.6 生成代码

RASC **Generate Project Content** 后确认存在：

| 生成物 | 检查项 |
|--------|--------|
| `ra_gen/hal_data.c` | `g_basic0_cfg.usb_mode == USB_MODE_HOST`，`module_number == 1`，`type == USB_CLASS_HVND` |
| `ra_gen/vector_data.c` | `usbhs_interrupt_handler` 等向量 |
| `ra_cfg/fsp_cfg/r_usb_basic_cfg.h` | `#define USB_CFG_HVND_USE` |

---

## 4. 目标工程集成步骤

### 4.1 拷贝源文件

```text
目标工程/src/
  ch340_host.c
  ch340_host.h
  usb_light.c
  usb_light.h
  usb_app.c      // 可删减 demo，保留 init/poll/process
  usb_app.h
```

### 4.2 包含路径与工程文件

- Keil：加入上述 `.c`，Include 含 `src`、`ra_gen`、`ra/fsp/inc` 等（与同工程其他 FSP 模块一致）。
- 链接 FSP 库中的 **`r_usb_basic`**、**`r_usb_hvnd`**（RASC 已选组件会自动带入）。

### 4.3 入口模板（裸机）

```c
#include "hal_data.h"
#include "usb_app.h"

void hal_entry(void)
{
    if (FSP_SUCCESS != usb_app_init())
    {
        while (1) { }
    }

    while (1)
    {
        usb_app_process();   /* 内部轮询 USB + 可选演示 */

        /* 业务示例：就绪后控制灯 */
        /* if (usb_app_is_ready()) { usb_light_set(USB_LIGHT_RED_ON); } */
    }
}
```

### 4.4 USB 实例改名时

若 RASC 将实例命名为 `g_usb0` 而非 `g_basic0`：

1. 全局替换 `g_basic0_ctrl` → `g_usb0_ctrl`，`g_basic0_cfg` → `g_usb0_cfg`；或  
2. 在 `usb_app.c` / `ch340_host.c` 顶部增加宏：

```c
#define USB_ALARM_CTRL   (g_usb0_ctrl)
#define USB_ALARM_CFG    (g_usb0_cfg)
```

`module_number` 仍以 **`p_usb_cfg->module_number`** 为准（IP1 时为 1）。

---

## 5. 代码细节说明

### 5.1 调用链

```text
usb_app_init()
  ch340_host_init()
  R_USB_Open(&g_basic0_ctrl, &g_basic0_cfg)    // 内部也会拉 VBUS
  R_USB_VbusSet(..., USB_ON)
  delay 200 ms

usb_app_process()  [每 ~10ms 一轮]
  usb_app_poll() ×10 + delay 1ms
    R_USB_EventGet → CONFIGURED / DETACH / ...
    延迟 ~100 次 poll → ch340_host_bringup()
  若 ready 且满 3s → usb_app_run_demo_step()
    usb_light_set(mode)
      ch340_host_write(8 bytes)
        R_USB_PipeWrite(Bulk OUT)
```

### 5.2 `usb_app.c`

| 符号 | 说明 |
|------|------|
| `CH340_BRINGUP_DELAY_LOOPS` (100) | CONFIGURED 后约 100 次 `poll` 再 `bringup` |
| `g_demo_sequence[]` | 演示 6 步，可删改 |
| `USB_APP_DEMO_INTERVAL_MS` (3000) | 演示步进间隔 |
| `g_usb_app_debug` | 调试结构，建议保留 |

**事件处理**：

- `USB_STATUS_CONFIGURED` → `ch340_host_on_configured(addr)`，置 `g_need_ch340_bringup`
- `USB_STATUS_DETACH` → 复位 CH340 与 demo 索引

**注意**：主循环必须用 `usb_app_process()`（或高频 `usb_app_poll()`），**不能** 长时间 `SoftwareDelay` 阻塞而不轮询 USB。

### 5.3 `ch340_host.c`

| 步骤 | API / 行为 |
|------|------------|
| 找 Bulk OUT 管道 | `R_USB_UsedPipesGet` + `R_USB_PipeInfoGet`（Bulk、非 IN） |
| 读版本 | 厂商 IN `0x5F` |
| 串口初始化 | 厂商 OUT `0xA1` |
| 波特率 4800 8N1 | 厂商 OUT `0x9A`（`ch341_get_divisor`） |
| DTR/RTS | 厂商 OUT `0xA4` |
| 发灯数据 | `R_USB_PipeWrite`，长度 **固定 8** |

控制传输 `setup.request_type` 打包方式：

```c
setup.request_type = (uint16_t) (((uint16_t) request << 8) | bmRequestType);
```

等待完成：`R_USB_EventGet` 直到 `USB_STATUS_REQUEST_COMPLETE`（`usb_wait_event`）。

发送前 **`ch340_usb_ctrl_prepare()`**：

```c
g_usb_event_ctrl.module_number = g_basic0_cfg.module_number;
g_usb_event_ctrl.type          = USB_CLASS_HVND;
```

### 5.4 `usb_light.c`

- **`g_usb_light_frames[][]`**：与 `usb_light需求.md` **逐字节一致**，**不做 CRC 计算**。
- **`usb_light_set(mode)`**：`memcpy` 8 字节 → `ch340_host_write`。
- **`g_usb_light_last_frame[8]`**：最后一次发送内容，便于与串口助手对比。

扩展灯态：在 `usb_light.h` 增加枚举项，在 `g_usb_light_frames` 增加对应一行 8 字节（须在需求文档中验证过的 HEX）。

### 5.5 演示序列（可删除）

默认 `g_demo_sequence[]`：

1. 红常亮 → 2. 绿常亮 → 3. 蓝常亮 → 4. 红慢闪 → 5. 红快闪+喇叭 → 6. 关灯 → 循环。

不需要演示时：在 `usb_app_process()` 中去掉 `usb_app_run_demo_step()` 相关逻辑，仅在业务代码里调用 `usb_light_set()`。

---

## 6. 应用 API 速查

| API | 返回值 / 行为 |
|-----|----------------|
| `usb_app_init()` | 成功 `FSP_SUCCESS`；失败勿继续 |
| `usb_app_process()` | 无返回值；主循环调用 |
| `usb_app_poll()` | 仅 USB/CH340 状态机 |
| `usb_app_is_ready()` | `true` 表示可 `usb_light_set` |
| `usb_light_set(mode)` | `FSP_SUCCESS` 表示 Bulk 提交成功；未就绪 `FSP_ERR_INVALID_STATE` |

FSP 6.4 **无** `FSP_ERR_NOT_READY`，本工程用 `FSP_ERR_INVALID_STATE`。

---

## 7. 调试与验收

### 7.1 Keil Watch

| 变量 | 期望 |
|------|------|
| `g_usb_app_debug.configured_seen` | 插灯后 `1` |
| `g_usb_app_debug.ch340_ready` | bringup 后 `1` |
| `g_usb_app_debug.last_light_err` | 发灯成功 `0` |
| `g_usb_light_last_frame[0..7]` | 与需求文档 HEX 一致 |

### 7.2 与串口助手对照

| 项目 | PC 串口助手 | 本固件 |
|------|-------------|--------|
| 波特率 | 4800, 8N1 | `ch340_host_bringup` |
| 数据 | HEX 8 字节 | Bulk OUT 8 字节 |
| 换行 | **不要** 发空行/0D0A | 不发送 |

示例黄灯慢闪：`01 06 00 C2 00 22 A9 FF` → `g_usb_light_last_frame` 应完全相同。

### 7.3 验收清单

- [ ] RASC：Host + HS + IP1 + hvnd 栈
- [ ] 引脚 VBUSEN/VBUS 与板图一致
- [ ] USBCLK 48 MHz，USB60CLK 已开（建议）
- [ ] OTG 线 + 报警灯插入 HS 口
- [ ] `configured_seen == 1`
- [ ] `ch340_ready == 1`
- [ ] 灯态与 `usb_light_set` / 演示一致

---

## 8. 已知问题与对策（实机排查记录）

以下为本工程开发过程中已遇到问题，复用时可优先对照。

| 现象 / 问题 | 原因 | 对策 |
|-------------|------|------|
| 能充电，灯不亮 | VBUS ≠ USB 枚举成功 | 查 `configured_seen`；OTG 线、HS 口 |
| `R_USB_PipeWrite` 失败 | `p_ctrl->type` 未设 `USB_CLASS_HVND` | `ch340_usb_ctrl_prepare()` |
| 黄灯慢闪等不亮，常亮可能正常 | 用标准 Modbus 重算 CRC 与文档不一致 | **只发需求文档固定 8 字节**，见 `usb_light.c` 表 |
| 串口助手能亮，MCU 不亮 | 发了 `A8 2F` 而非 `A9 FF` 等 | 禁止运行时 CRC，用 `g_usb_light_frames` |
| 编译 `FSP_ERR_NOT_READY` | FSP 6.4 无此错误码 | 使用 `FSP_ERR_INVALID_STATE` |
| 长时间延时后 USB 异常 | 阻塞期间未 `EventGet` | 用 `usb_app_process` 1ms 切片轮询 |
| CH340 init 在事件里卡死 | 控制传输依赖 `EventGet` | 可接受；勿在 init 中再阻塞秒级 |
| 枚举失败 | BC 干扰、时钟、线材 | BC Disable；查 USB60CLK；换 OTG |
| 发给手机的线能充电 | 正常 | 不能说明 CH340 已配置 |

---

## 9. 不建议复用的部分

- `ra/`、`ra_gen/` 整目录：应由目标工程 RASC 重新生成。
- `usb_light.uvprojx`：仅作参考，目标工程用自己的工程文件。
- 演示逻辑 `g_demo_sequence`：按产品需求重写。

---

## 10. 可选：进一步解耦（方式 B）

若多工程共用，建议新增 `usb_alarm_host_cfg_t`：

```c
typedef struct {
    usb_instance_ctrl_t * p_usb_ctrl;
    usb_cfg_t const     * p_usb_cfg;
} usb_alarm_host_cfg_t;
```

`usb_app_init(cfg)` / `ch340_host` 内使用 `cfg->p_usb_cfg->module_number`，不再 `#include "hal_data.h"` 写死 `g_basic0`。本 demo 尚未拆该层，拷贝后全局替换实例名即可快速移植。

---

## 11. 文档索引

| 文档 | 内容 |
|------|------|
| [usb_light需求.md](usb_light需求.md) | 27 条 HEX 指令 |
| [USB_HOST开发说明.md](USB_HOST开发说明.md) | 流程图、FSP API、分层 |
| **本文档** | RASC 配置 + 复用步骤 + 踩坑 |
| `docs/模块资料/USB可编程台灯/USB报警灯使用说明书V2.md` | 产品协议说明 |

---

*文档随 `src/` 与 RASC 配置维护；实例名 `g_basic0`、引脚 P407/P408 以目标板为准。*
