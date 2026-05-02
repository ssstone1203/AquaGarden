# PhytiumPi ↔ RA6E2 SPI 通信工程设计方案

> 适用项目：AquaGarden  
> 上位机：飞腾派 Linux 主核（业务层）+ OpenAMP 裸机从核（SPI 主驱动层）  
> 下位机：RA6E2 + FreeRTOS + Renesas FSP（SPI Slave）  
> 物理层：4 线 SPI（SCK / MOSI / MISO / CS），全双工

---

## 0. 系统架构（落地版）

```
┌────────────────────────────────────────────────────────────┐
│ 飞腾派（PhytiumPi, ARM-A）                                  │
│                                                            │
│   ┌────────────────────────────┐                           │
│   │ Linux 主核（用户态）        │                           │
│   │ - aqua_ctrl 业务进程         │   命令/响应在 Linux 这一  │
│   │ - 打开 /dev/rpmsg0          │   端被构造和解析，所有     │
│   │ - 构造命令包 / 解析响应包    │   "业务语义"集中在此处。  │
│   └──────────────┬─────────────┘                           │
│                  │ RPMsg（共享内存 + IPI 中断，virtio 通道）│
│   ┌──────────────┴─────────────┐                           │
│   │ 裸机从核（remote core）     │   裸机核只做"协议中转 +    │
│   │ - openamp_core0.elf         │   物理收发"，对业务命令    │
│   │ - rpmsg endpoint 回调        │   语义无感知，便于长期    │
│   │ - 64B 命令帧 → SPI 主控发送  │   稳定不重烧固件。        │
│   │ - 接收 64B 响应帧 → 回 RPMsg │                           │
│   └──────────────┬─────────────┘                           │
└──────────────────┼─────────────────────────────────────────┘
                   │ SPI Bus (Mode 0, 1 MHz, 8-bit, MSB)
┌──────────────────┴─────────────────────────────────────────┐
│ RA6E2（FreeRTOS + FSP）                                    │
│   ┌────────────────────────────┐                           │
│   │ Communicate_Task            │                           │
│   │ - g_com_spi (SPI1 Slave)    │                           │
│   │ - 双 DMAC（TX/RX）           │                           │
│   │ - 解析命令、装配响应         │                           │
│   │ - 复用 host_apply_command 与 │                           │
│   │   host_build_uplink_frame   │                           │
│   └────────────────────────────┘                           │
└────────────────────────────────────────────────────────────┘
```

**职责切分原则：**

| 层 | 是否解析业务命令 | 是否维护设备状态 | 可热更新 |
|----|----------------|-----------------|---------|
| Linux 用户态 | 是 | 业务上下文（缓存、阈值） | ✅（重启进程即可） |
| 裸机从核 | 否（只搬字节） | 无 | ❌（需重烧 elf） |
| RA6E2 | 是（最终执行） | 全部硬件实时状态 | ⚠️（需 OTA 或 J-Link） |

> 关键含义：**新增命令时只需要改 Linux 应用 + RA6E2 固件，裸机核固件保持不变。**

---

## 1. SPI 硬件配置

| 配置项 | 选择 | 理由 |
|-------|------|------|
| SPI 模式 | **Mode 0**（CPOL=0, CPHA=0） | RA6E2 已按此模式生成 FSP 配置（`SPI_CLK_POLARITY_LOW` + `SPI_CLK_PHASE_EDGE_ODD`）；飞腾 spidev 在 Mode 0 下兼容性最好（默认无需 `SPI_IOC_WR_MODE` ioctl，避免 `EINVAL`）；Mode 0 也是绝大多数 MCU/外设的默认习惯。 |
| 通信速率 | 首发 **1 MHz**，可上调到 **4 MHz** | 250 ms 周期 + 64 字节帧 → 1 MHz 时单次事务 ≈ 0.5 ms，带宽富余 500 倍以上，根本不是瓶颈；飞腾派排针无屏蔽，1 MHz 给信号完整性留够裕度；RA6E2 SPI1 + 双 DMAC 在 4 MHz 内空载 < 5%，后续如需调高仅需改主机时钟，从机零修改。 |
| 数据位宽 | **8 bit**（按字节传输） | 协议层完全按字节流设计，避免 16/32-bit 模式下的 endian 混乱；8-bit 也最便于示波器调试和抓包分析；与 RA6E2 现有 `host_*` 系列字节级辅助函数（`host_u16_put` 等）天然契合。 |
| DMA / 中断 | **从机：双 DMAC + EOT 中断**；**主机：FIFO + 完成中断** | RA6E2 的 SPI Slave 必须用 DMA：从机无法控制时钟，CPU 轮询读 RDR 必丢字节；现有 FSP 配置已有 DMAC0 (TX) + DMAC1 (RX)。飞腾派裸机核作为主机，可用控制器自带 FIFO + 单次完成中断即可，无需 DMA。**仅在帧"完成"时通知任务，避免逐字节中断风暴。** |
| CS 管理 | **主机软件控制 GPIO，每帧一次拉低/拉高** | 不使用控制器的"自动 CS"模式（部分飞腾控制器的自动 CS 时序对从机 DMAC 启动不友好）；CS 上升沿天然是 RA6E2 的"帧结束 + DMAC 重置"信号，是抗错位最关键的一招——一旦发生错位，下一帧 CS 复位即恢复。CS 拉低后**等 ≥ 2 µs 再发 SCK**（Tcss），CS 拉高前**等末位 SCK 完成 + 1 µs**（Tcsh）。 |
| 字节序 | **小端**（Little Endian） | RA6E2 现有协议层、ARM Cortex-M / Cortex-A 默认全部 LE；不再做任何字节序转换以避免 bug。 |
| 位序 | **MSB First** | RA6E2 已配置 `SPI_BIT_ORDER_MSB_FIRST`；行业默认。 |

### 1.1 RA6E2 现有 FSP 配置（节选自 `ra_gen/Communicate_Task.c`）

```c
const spi_extended_cfg_t g_com_spi_ext_cfg = {
    .spi_clksyn   = SPI_SSL_MODE_CLK_SYN,
    .spi_comm     = SPI_COMMUNICATION_FULL_DUPLEX,
    .ssl_polarity = SPI_SSLP_LOW,
    .ssl_select   = SPI_SSL_SELECT_SSL0,
    .mosi_idle    = SPI_MOSI_IDLE_VALUE_FIXING_DISABLE,
    .parity       = SPI_PARITY_MODE_DISABLE,
    .byte_swap    = SPI_BYTE_SWAP_DISABLE,
    /* Slave 模式下 spck_div 不影响时钟，由主机决定 */
};

const spi_cfg_t g_com_spi_cfg = {
    .channel        = 1,                      /* SPI1 */
    .operating_mode = SPI_MODE_SLAVE,
    .clk_phase      = SPI_CLK_PHASE_EDGE_ODD, /* CPHA=0 */
    .clk_polarity   = SPI_CLK_POLARITY_LOW,   /* CPOL=0 */
    .bit_order      = SPI_BIT_ORDER_MSB_FIRST,
    .p_transfer_tx  = &g_com_spi_tx,          /* DMAC0 */
    .p_transfer_rx  = &g_com_spi_rx,          /* DMAC1 */
    .p_callback     = Com_SPI_Callback,
    /* ... */
};
```

> ⚠️ **需要修改的一处：** 当前 DMAC 的 `transfer_settings_word_b.size = TRANSFER_SIZE_2_BYTE`，是为了配合 16-bit 字宽。本方案改用 8-bit 字节传输，需把 TX/RX 双 DMAC 都改为 `TRANSFER_SIZE_1_BYTE`，否则会出现"每两个字节合并/拆分"的偏移错误。改动点见附录 A。

### 1.2 飞腾派引脚映射

| 信号 | 飞腾派排针位置 | RA6E2 引脚（FSP 已配） |
|-----|---------------|----------------------|
| SCK  | `SPI0_SCK`  | P102 (RSPCK1) |
| MOSI | `SPI0_MOSI` | P101 (MOSI1) |
| MISO | `SPI0_MISO` | P100 (MISO1) |
| CS   | `SPI0_CSN0` | P103 (SSL10) |
| GND  | 任一 GND     | GND |

> 物理接线必须**短**（< 15 cm），双方共地，CS/SCK 走线尽量远离电机/水泵 PWM 走线。

---

## 2. SPI 通信协议设计

### 2.1 设计目标与约束

1. **全双工对齐**：主机发命令的同时，从机必然在反向写回。任何一帧 SPI 事务里 MOSI 与 MISO 都按相同的 64 字节"帧结构"对齐，不允许从机用 0xFF 哑字节填充——这样可以让"主机发第 N 帧命令"和"从机回第 N-1 帧响应"在同一物理事务里完成（流水线式响应）。
2. **抗错位**：定长帧 + 每帧一次 CS 上升沿复位 + CRC16 校验，三重防御。
3. **抗粘包**：定长帧根本上避免粘包；额外用 SOF（帧起始字节）双重保险。
4. **可扩展**：保留版本字段、保留字段、命令空间分段（设备类别）。

### 2.2 帧总长 = 64 字节（定长）

选择 64 字节的理由：
- 一帧覆盖**最大命令 payload (16 B) + 最大响应 payload (48 B)**，二者都能装下；
- 是 cache line 的整数倍，DMAC 搬运对齐良好；
- 1 MHz 下单帧仅 0.5 ms，对 250 ms 周期完全无压力。

### 2.3 主机 → 从机帧（MOSI 方向，CMD Frame）

```
偏移  长度  字段           说明
─────────────────────────────────────────────────────────────
 0    1    SOF0 = 0xA5    起始字节 0
 1    1    SOF1 = 0x5A    起始字节 1
 2    1    VER  = 0x01    协议版本（递增可向下兼容）
 3    1    SEQ          帧序列号（0..255 循环），用于响应配对
 4    1    DEV          设备类别 ID（见 §3）
 5    1    CMD          命令 ID（见 §3）
 6    1    LEN          payload 字节数（0..16），实际数据长度
 7    1    FLAGS        bit0=NEED_RSP（需要响应数据）, bit1..7=保留
 8   16    PAYLOAD      命令参数；不足 LEN 的部分必须填 0x00
24   38    RESERVED     保留区，必须填 0x00（向上扩展时占用）
62    2    CRC16        对前 62 字节做 CRC16/Modbus，小端
─────────────────────────────────────────────────────────────
共   64    字节
```

### 2.4 从机 → 主机帧（MISO 方向，RSP Frame）

```
偏移  长度  字段           说明
─────────────────────────────────────────────────────────────
 0    1    SOF0 = 0x5A    起始字节 0（与 CMD 镜像，便于眼看抓包）
 1    1    SOF1 = 0xA5    起始字节 1
 2    1    VER  = 0x01    协议版本
 3    1    ACK_SEQ      被确认的命令 SEQ（即上一个 CMD 帧的 SEQ）
 4    1    STATUS       0=OK, 非 0=错误码（见 §6.5）
 5    1    RSP_TYPE     响应类型（0=Ack, 1=SensorData, 2=Status, ...）
 6    1    LEN          payload 字节数（0..48）
 7    1    FLAGS        bit0=DATA_READY（缓冲区还有数据，主机应继续轮询）
                       bit1=ALARM_PENDING（有未读告警）
                       bit2..7=保留
 8   48    PAYLOAD      响应数据；不足 LEN 部分填 0x00
56    4    UPTIME_MS    从机运行时间（ms, LE32），用于诊断
60    2    RESERVED     保留区，必须填 0x00
62    2    CRC16        对前 62 字节做 CRC16/Modbus，小端
─────────────────────────────────────────────────────────────
共   64    字节
```

### 2.5 流水线时序（**重要**）

由于 SPI 全双工特性，**响应永远比请求滞后一帧**：

```
事务 #N    : MOSI = CMD[N]      MISO = RSP[N-1]   (响应上一帧)
事务 #N+1  : MOSI = CMD[N+1]    MISO = RSP[N]     (响应当前帧)
```

主机首次启动时，第 1 个事务收到的 MISO 应当是从机预先装载的 "Idle 响应"（`RSP_TYPE=0, STATUS=0, ACK_SEQ=0xFF` 表示无对应命令）。

如果业务上要求"立刻拿到响应"，主机可以在发完 CMD[N] 之后**主动再发一次 NOP 命令**（CMD=0x00），用 NOP 帧的 MISO 把 RSP[N] 取回来。

### 2.6 CRC 算法（与 RA6E2 现有代码一致）

CRC16/Modbus，多项式 `0xA001`（反向 `0x8005`），初始值 `0xFFFF`，无最终异或，结果**小端**放入帧尾。

```c
/* 与 hardware/demo_wyr/FreeRTOS drive/data_merge/src/Communicate_Task_entry.c 一致 */
static uint16_t crc16_modbus(const uint8_t *p, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}
```

### 2.7 端序声明

**所有多字节字段（u16/u32/i16/i32/float）一律小端 (Little Endian)。**

---

## 3. 命令定义表

### 3.1 设备类别 (DEV)

| DEV ID | 名称 | 说明 |
|-------|------|------|
| `0x00` | `DEV_SYSTEM` | 系统级（心跳、版本、复位） |
| `0x01` | `DEV_PUMP` | 水泵 |
| `0x02` | `DEV_SENSOR` | 各类传感器（温湿度、水温、土壤、压力、WQS） |
| `0x03` | `DEV_LINKAGE` | 联动控制（阈值、自动模式） |
| `0x04..0xEF` | 预留 | 后续新设备使用 |
| `0xF0..0xFF` | 厂商自定义 | OEM 扩展 |

### 3.2 命令定义

#### 3.2.1 系统类（DEV = 0x00）

| CMD | 名称 | 参数（PAYLOAD） | 描述 | 响应类型 |
|-----|------|----------------|------|---------|
| `0x00` | `SYS_NOP` | 无 | 空操作，仅用于取回上一帧响应 | Ack |
| `0x01` | `SYS_PING` | 无 | 心跳，从机回 `STATUS=0` | Ack |
| `0x02` | `SYS_GET_VERSION` | 无 | 请求从机固件版本 | Status |
| `0x03` | `SYS_GET_UPTIME` | 无 | 请求从机已运行时间 | Status |
| `0x04` | `SYS_RESET_ALARM` | 无 | 清除从机锁存的告警位 | Ack |

#### 3.2.2 水泵类（DEV = 0x01）

| CMD | 名称 | 参数（PAYLOAD） | 描述 | 响应类型 |
|-----|------|----------------|------|---------|
| `0x01` | `PUMP_START` | `[0]=power%(0..100)` | 手动模式启动水泵 | Ack |
| `0x02` | `PUMP_STOP` | 无 | 立即停止水泵 | Ack |
| `0x03` | `PUMP_SET_PWM` | `[0]=power%(0..100)` | 设置水泵 PWM 强度（持续生效） | Ack |
| `0x04` | `PUMP_SET_AUTO` | 无 | 切回自动模式（受联动控制） | Ack |
| `0x05` | `PUMP_SET_MANUAL` | `[0]=enable, [1]=power%` | 设置手动模式开关 + 默认强度 | Ack |
| `0x10` | `PUMP_SET_CYCLE_CFG` | 11 字节，见 RA6E2 现有协议 | 配置循环灌溉 | Ack |
| `0x11` | `PUMP_SET_CYCLE_CTRL` | `[0]=enable, [1]=start, [2..5]=total_count` | 控制循环灌溉 | Ack |
| `0x20` | `PUMP_GET_STATUS` | 无 | 请求水泵当前状态 | Status |

#### 3.2.3 传感器类（DEV = 0x02）

| CMD | 名称 | 参数（PAYLOAD） | 描述 | 响应类型 |
|-----|------|----------------|------|---------|
| `0x01` | `SENSOR_ENABLE` | `[0]=mask` (位 0=空气, 位 1=水温, 位 2=土壤, 位 3=WQS, 位 4=压力) | 启用指定传感器采样 | Ack |
| `0x02` | `SENSOR_DISABLE` | `[0]=mask` | 停止指定传感器采样 | Ack |
| `0x10` | `SENSOR_POLL_ALL` | 无 | **请求全量数据快照**（核心命令） | SensorData |
| `0x11` | `SENSOR_POLL_AIR` | 无 | 仅请求温湿度 | SensorData |
| `0x12` | `SENSOR_POLL_WATER` | 无 | 仅请求水温 + WQS | SensorData |
| `0x13` | `SENSOR_POLL_SOIL` | 无 | 仅请求土壤 | SensorData |
| `0x14` | `SENSOR_POLL_PRESSURE` | 无 | 仅请求 3 路压力 | SensorData |

#### 3.2.4 联动类（DEV = 0x03）

| CMD | 名称 | 参数（PAYLOAD） | 描述 | 响应类型 |
|-----|------|----------------|------|---------|
| `0x01` | `LINK_SET_SOIL_CFG` | `[0]=threshold%, [1]=hysteresis%` | 配置土壤阈值 | Ack |
| `0x02` | `LINK_SET_RULE` | 4 字节，见 RA6E2 现有 SET_LINKAGE_CFG | 联动规则总开关 + 阈值 | Ack |

### 3.3 响应类型 1 (`SensorData`) 的 PAYLOAD 布局

48 字节 PAYLOAD，与 RA6E2 现有 `host_build_uplink_frame()` 输出**字段对齐**（直接复用）：

```
偏移  长度  字段                  类型     单位
──────────────────────────────────────────────────
 0    4    timestamp_ms          u32 LE   ms
 4    2    air_temp_x10          i16 LE   0.1°C
 6    2    air_humidity_x10      i16 LE   0.1%RH
 8    2    water_temp_x10        i16 LE   0.1°C
10    1    soil_moisture_pct     u8       %
11    1    wqi                   u8       0..100
12    1    pump_power_pct        u8       %
13    1    need_watering         u8       bool
14    2    pressure_kg_0_x100    u16 LE   0.01 kg
16    2    pressure_kg_1_x100    u16 LE   0.01 kg
18    2    pressure_kg_2_x100    u16 LE   0.01 kg
20    4    alarm_flags           u32 LE   位掩码
24    2    air_retry_count       u16 LE   -
26    2    wqs_retry_count       u16 LE   -
28    2    uwt_retry_count       u16 LE   -
30    1    pump_cycle_enable     u8       bool
31    1    pump_cycle_start      u8       bool
32    1    pump_cycle_active     u8       bool
33    1    pump_cycle_state      u8       0..N
34    1    pump_cycle_power_pct  u8       %
35    2    pump_cycle_done_count u16 LE   -
37   11    RESERVED              0x00     -
──────────────────────────────────────────────────
共   48    字节
```

---

## 4. 通信流程时序

### 4.1 总体节拍

- **主控周期**：250 ms（与 RA6E2 现有 `Communicate_Task` 节拍一致）
- **请求-响应模式**：是（每个 CMD 帧都有对应的 RSP 帧，按 SEQ 配对）
- **是否独立 ACK**：**否**。SPI 全双工天然让每帧 MOSI/MISO 同时进行，独立 ACK 帧浪费带宽。`STATUS` 字段就是 ACK——`0` 表示成功。

### 4.2 正常请求时序

```
时间轴 →

主机:  CS_n────┐                                    ┌────CS_n+1──
              │ <--- 64 字节 SPI 事务 --->          │
              │                                     │
       MOSI   │ [CMD_n: 64B]                        │ [CMD_n+1: 64B]
              │                                     │
       MISO   │ [RSP_{n-1}: 64B] ← 上一帧的响应      │ [RSP_n: 64B]

从机回调:                                ▲
                                         │
                          DMAC RX 完成中断 → Communicate_Task
                          → 解析 CMD_n → 执行 → 装好 RSP_n → 重启 DMAC
                                         │
                          ←── 必须在主机下一次 CS 拉低之前完成 ──→
                                         (典型 ≈ 100 µs，留余量到 200 ms)
```

### 4.3 主控状态机（伪码）

```
state = INIT
loop every 250 ms:
    case INIT:
        send SYS_PING (CMD frame), discard MISO
        if RSP STATUS == 0: state = RUNNING
        else: state = INIT (重试)

    case RUNNING:
        # 1. 发本周期命令（如有用户操作）
        if cmd_queue.has(): cmd = cmd_queue.pop()
        else: cmd = build(SENSOR_POLL_ALL)   # 默认就轮询数据
        rsp_prev = spi_transact(cmd)         # 同时取回上一帧响应

        # 2. 处理响应
        if not validate(rsp_prev): continue  # 见 §6
        deliver_to_app(rsp_prev)

        # 3. 如有 NEED_RSP 标志，再发一次 NOP 取本帧响应
        if cmd.flags & NEED_RSP:
            rsp_now = spi_transact(build(SYS_NOP))
            if validate(rsp_now): deliver_to_app(rsp_now)
```

### 4.4 时间预算（1 MHz）

| 阶段 | 时间 |
|------|------|
| CS 建立（Tcss） | ≥ 2 µs |
| 64 字节 SCK 传输 | 64 × 8 / 1 MHz = 512 µs |
| CS 保持（Tcsh） | ≥ 1 µs |
| 帧间隔（让 RA6E2 完成解析+装载） | 1 ms |
| **单事务总耗** | **≈ 1.5 ms** |

250 ms 周期下利用率 < 1%。

---

## 5. 双端代码框架

### 5.1 Linux 用户态 — RPMsg 客户端 (`aqua_spi_client.c`)

> 改造自 `hardware/phytiumpi/openamp/demo/rpmsg-demo-single.c`。  
> 安装位置：`/usr/local/bin/aqua_spi_client`

```c
/* aqua_spi_client.c — Linux 用户态 SPI 命令客户端
 * 通过 RPMsg 把 64 字节 CMD 帧透传给裸机核，并接收 64 字节 RSP 帧。
 *
 * 编译: aarch64-linux-gnu-gcc -O2 -Wall -o aqua_spi_client aqua_spi_client.c
 * 运行: sudo ./aqua_spi_client poll_all
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/rpmsg.h>

#define SPI_FRAME_LEN       64u
#define CMD_PAYLOAD_MAX     16u
#define RSP_PAYLOAD_MAX     48u

/* RPMsg 包头：告诉裸机核如何处理这一包数据 */
#define RPMSG_OP_SPI_XFER   0x01u   /* 透传一帧 SPI 事务 */
#define RPMSG_OP_PING       0x02u   /* 心跳裸机核 */

typedef struct __attribute__((packed)) {
    uint32_t op;                    /* RPMSG_OP_* */
    uint16_t length;                /* 紧随其后的 data 长度 */
    uint16_t reserved;
    uint8_t  data[SPI_FRAME_LEN];   /* 一帧 64B */
} rpmsg_packet_t;

/* SPI 帧字段偏移 */
enum {
    OFF_SOF0 = 0, OFF_SOF1 = 1, OFF_VER = 2, OFF_SEQ = 3,
    OFF_DEV = 4, OFF_CMD = 5, OFF_LEN = 6, OFF_FLAGS = 7,
    OFF_PAYLOAD = 8, OFF_CRC = 62
};

#define SOF0_CMD 0xA5u
#define SOF1_CMD 0x5Au
#define SOF0_RSP 0x5Au
#define SOF1_RSP 0xA5u

/* CRC16/Modbus，与 RA6E2 一致 */
static uint16_t crc16_modbus(const uint8_t *p, uint16_t len) {
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
    }
    return crc;
}

static void build_cmd(uint8_t *frame, uint8_t seq, uint8_t dev, uint8_t cmd,
                      const uint8_t *payload, uint8_t payload_len, uint8_t flags) {
    memset(frame, 0, SPI_FRAME_LEN);
    frame[OFF_SOF0]  = SOF0_CMD;
    frame[OFF_SOF1]  = SOF1_CMD;
    frame[OFF_VER]   = 0x01u;
    frame[OFF_SEQ]   = seq;
    frame[OFF_DEV]   = dev;
    frame[OFF_CMD]   = cmd;
    frame[OFF_LEN]   = payload_len;
    frame[OFF_FLAGS] = flags;
    if (payload && payload_len)
        memcpy(&frame[OFF_PAYLOAD], payload, payload_len);

    uint16_t crc = crc16_modbus(frame, OFF_CRC);
    frame[OFF_CRC]     = (uint8_t)(crc & 0xFFu);
    frame[OFF_CRC + 1] = (uint8_t)((crc >> 8) & 0xFFu);
}

static int validate_rsp(const uint8_t *frame) {
    if (frame[OFF_SOF0] != SOF0_RSP || frame[OFF_SOF1] != SOF1_RSP) return -1;
    uint16_t crc = crc16_modbus(frame, OFF_CRC);
    uint16_t rx  = (uint16_t)frame[OFF_CRC] | ((uint16_t)frame[OFF_CRC + 1] << 8);
    return (crc == rx) ? 0 : -2;
}

/* 与裸机核做一次"发 64B / 收 64B"事务 */
static int spi_transact(int rpmsg_fd, const uint8_t *tx, uint8_t *rx) {
    rpmsg_packet_t pkt = { .op = RPMSG_OP_SPI_XFER, .length = SPI_FRAME_LEN };
    memcpy(pkt.data, tx, SPI_FRAME_LEN);

    if (write(rpmsg_fd, &pkt, sizeof(pkt)) < 0) { perror("write"); return -1; }

    rpmsg_packet_t reply;
    ssize_t n = read(rpmsg_fd, &reply, sizeof(reply));
    if (n < 0) { perror("read"); return -1; }
    if (reply.length != SPI_FRAME_LEN) {
        fprintf(stderr, "短回包 %zd\n", n);
        return -1;
    }
    memcpy(rx, reply.data, SPI_FRAME_LEN);
    return 0;
}

static int open_rpmsg(int *p_ctrl_fd, int *p_data_fd) {
    int ctrl = open("/dev/rpmsg_ctrl0", O_RDWR);
    if (ctrl < 0) { perror("/dev/rpmsg_ctrl0"); return -1; }

    struct rpmsg_endpoint_info ept = {0};
    strncpy(ept.name, "aqua-spi", sizeof(ept.name) - 1);
    ept.src = 0; ept.dst = 0;
    if (ioctl(ctrl, RPMSG_CREATE_EPT_IOCTL, &ept) < 0) {
        perror("RPMSG_CREATE_EPT_IOCTL");
        close(ctrl); return -1;
    }
    int data = open("/dev/rpmsg0", O_RDWR);
    if (data < 0) { perror("/dev/rpmsg0"); close(ctrl); return -1; }

    *p_ctrl_fd = ctrl; *p_data_fd = data;
    return 0;
}

int main(int argc, char **argv) {
    int ctrl_fd, rpmsg_fd;
    if (open_rpmsg(&ctrl_fd, &rpmsg_fd) < 0) return 1;

    static uint8_t tx[SPI_FRAME_LEN], rx[SPI_FRAME_LEN];
    static uint8_t seq = 0;

    /* 示例：发 SENSOR_POLL_ALL，并立刻发 NOP 取回响应 */
    build_cmd(tx, seq++, 0x02 /*DEV_SENSOR*/, 0x10 /*POLL_ALL*/, NULL, 0, 0x01 /*NEED_RSP*/);
    if (spi_transact(rpmsg_fd, tx, rx) < 0) goto out;
    /* 这里 rx 是上一帧的响应（首启时为 Idle），通常丢弃 */

    build_cmd(tx, seq++, 0x00 /*DEV_SYSTEM*/, 0x00 /*NOP*/, NULL, 0, 0);
    if (spi_transact(rpmsg_fd, tx, rx) < 0) goto out;

    if (validate_rsp(rx) == 0 && rx[5] == 1 /*RSP_TYPE=SensorData*/) {
        int16_t  air_t  = (int16_t)((uint16_t)rx[OFF_PAYLOAD + 4] | ((uint16_t)rx[OFF_PAYLOAD + 5] << 8));
        uint8_t  soil   = rx[OFF_PAYLOAD + 10];
        uint8_t  pump   = rx[OFF_PAYLOAD + 12];
        uint32_t alarms = (uint32_t)rx[OFF_PAYLOAD + 20]
                       | ((uint32_t)rx[OFF_PAYLOAD + 21] << 8)
                       | ((uint32_t)rx[OFF_PAYLOAD + 22] << 16)
                       | ((uint32_t)rx[OFF_PAYLOAD + 23] << 24);
        printf("空气温度 %.1f °C  土壤 %u%%  泵 %u%%  alarms=0x%08X\n",
               air_t / 10.0f, soil, pump, alarms);
    } else {
        fprintf(stderr, "响应校验失败或类型不匹配\n");
    }

out:
    close(rpmsg_fd);
    close(ctrl_fd);
    return 0;
}
```

### 5.2 飞腾裸机从核 — RPMsg 服务端 + SPI 主驱动

> 基于 `phytium-standalone-sdk` 的 SPI HAL（`FSpi*`）和 OpenAMP 库。  
> 工程位置建议：`baremetal/aqua_openamp_spi/`，构建产物 `openamp_core0.elf` → 部署到 `/lib/firmware/`。  
> 为遵守"不依赖具体寄存器地址"的约束，全部走 `FSpi` HAL。

#### 5.2.1 SPI 主驱动封装 `spi_master.c`

```c
/* spi_master.c — 飞腾派裸机 SPI 主驱动封装
 * 不直接访问寄存器，依赖 phytium-standalone-sdk 的 FSpi HAL。
 */

#include "fspi.h"           /* phytium-standalone-sdk: SPI HAL */
#include "fgpio.h"          /* phytium-standalone-sdk: GPIO HAL */
#include "fparameters.h"    /* SPI 实例号、引脚号都来自这里，不写死地址 */
#include "fdebug.h"
#include <string.h>

#define SPI_FRAME_LEN   64u
#define SPI_INSTANCE_ID FSPI0_ID         /* SPI0；具体由开发板配置决定 */
#define CS_GPIO_PORT    FGPIO_PORT_A
#define CS_GPIO_PIN     0                /* 由 BSP 决定 */

static FSpiCtrl   s_spi_ctrl;
static FGpioPin   s_cs_pin;
static int        s_inited = 0;

int spi_master_init(uint32_t freq_hz)
{
    FSpiConfig cfg = *FSpiLookupConfig(SPI_INSTANCE_ID);
    cfg.MaxFreqHz   = freq_hz;            /* 1_000_000 */
    cfg.SlaveMode   = FSPI_MASTER_MODE;
    cfg.CPHA        = FSPI_CPHA_1EDGE;    /* CPHA=0 */
    cfg.CPOL        = FSPI_CPOL_LOW;      /* CPOL=0 */
    cfg.NBits       = FSPI_N_BITS_8;
    cfg.BitOrder    = FSPI_BIT_ORDER_MSB_FIRST;
    cfg.CsManual    = TRUE;               /* 由我们手工控 GPIO */

    if (FSpiCfgInitialize(&s_spi_ctrl, &cfg) != FT_SUCCESS)
        return -1;

    /* 把 CS 当 GPIO，初始拉高 */
    FGpioPinInitialize(&s_cs_pin, CS_GPIO_PORT, CS_GPIO_PIN);
    FGpioPinSetDirection(&s_cs_pin, FGPIO_DIR_OUTPUT);
    FGpioPinWrite(&s_cs_pin, FGPIO_PIN_HIGH);

    s_inited = 1;
    return 0;
}

/* 一次完整的 64B 全双工事务：CS 拉低 → 收发 → CS 拉高 */
int spi_master_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    if (!s_inited || len != SPI_FRAME_LEN) return -1;

    FGpioPinWrite(&s_cs_pin, FGPIO_PIN_LOW);
    f_udelay(2);   /* Tcss ≥ 2 µs */

    FSpiTransfer xfer = {
        .TxBuf = (u8 *)tx,
        .RxBuf = rx,
        .Len   = len,
    };
    int ret = FSpiBlockingTransfer(&s_spi_ctrl, &xfer); /* 内部使用 FIFO + 完成中断 */

    f_udelay(1);   /* Tcsh ≥ 1 µs */
    FGpioPinWrite(&s_cs_pin, FGPIO_PIN_HIGH);

    return (ret == FT_SUCCESS) ? 0 : -1;
}
```

#### 5.2.2 OpenAMP RPMsg 服务端 `rpmsg_server.c`

```c
/* rpmsg_server.c — 裸机核 RPMsg 服务端
 * 监听 Linux 主核发来的 RPMSG_OP_SPI_XFER 包，调 spi_master_xfer 后把响应回发。
 *
 * 该文件用 OpenAMP rpmsg-lite / libmetal API；与 phytium-embedded-docs 中
 * open-amp/rpmsg_echo 例子结构一致。
 */

#include "openamp/open_amp.h"
#include "metal/sys.h"
#include <string.h>

#include "spi_master.h"

#define RPMSG_OP_SPI_XFER   0x01u
#define RPMSG_OP_PING       0x02u
#define SPI_FRAME_LEN       64u

typedef struct __attribute__((packed)) {
    uint32_t op;
    uint16_t length;
    uint16_t reserved;
    uint8_t  data[SPI_FRAME_LEN];
} rpmsg_packet_t;

static struct rpmsg_device     *s_rdev;
static struct rpmsg_endpoint    s_ept;

/* RPMsg 收到 Linux 来包时被回调 */
static int ept_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
                  uint32_t src, void *priv)
{
    (void)priv; (void)src;
    if (len < sizeof(rpmsg_packet_t)) return RPMSG_SUCCESS;

    const rpmsg_packet_t *req = (const rpmsg_packet_t *)data;
    rpmsg_packet_t reply = { .op = req->op, .length = SPI_FRAME_LEN };

    switch (req->op) {
    case RPMSG_OP_SPI_XFER:
        if (req->length != SPI_FRAME_LEN) {
            reply.length = 0;
            break;
        }
        if (spi_master_xfer(req->data, reply.data, SPI_FRAME_LEN) != 0) {
            /* SPI 物理层错误，回空帧 + length=0 让上层重试 */
            memset(reply.data, 0, SPI_FRAME_LEN);
            reply.length = 0;
        }
        break;

    case RPMSG_OP_PING:
        memset(reply.data, 0, SPI_FRAME_LEN);
        reply.data[0] = 'O'; reply.data[1] = 'K';
        break;

    default:
        reply.length = 0;
        break;
    }

    rpmsg_send(ept, &reply, sizeof(reply));
    return RPMSG_SUCCESS;
}

/* 入口（被 main() 调用） */
int rpmsg_server_init(void)
{
    /* 依赖 platform_init 已完成 metal/openamp 初始化 */
    s_rdev = platform_create_rpmsg_vdev(NULL, 0, VIRTIO_DEV_DEVICE, NULL, NULL);
    if (!s_rdev) return -1;

    return rpmsg_create_ept(&s_ept, s_rdev, "aqua-spi",
                            RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
                            ept_cb, NULL);
}
```

#### 5.2.3 裸机 main `main.c`

```c
/* main.c — 裸机核入口 */
#include "fdebug.h"
#include "spi_master.h"
#include "openamp_init.h"

int main(void)
{
    /* 1. 平台初始化（中断向量、cache、串口打印等） */
    platform_init_baremetal();

    /* 2. SPI 主机 1 MHz */
    if (spi_master_init(1000000u) != 0) {
        FT_DEBUG_PRINT_E("SPI", "init failed");
        return -1;
    }

    /* 3. OpenAMP + RPMsg 服务端 */
    if (rpmsg_server_init() != 0) {
        FT_DEBUG_PRINT_E("RPMSG", "init failed");
        return -1;
    }

    /* 4. 主循环：处理 RPMsg 事件 */
    for (;;) {
        platform_poll();   /* 内部驱动 RPMsg 中断 + 回调 */
    }
}
```

### 5.3 RA6E2 FreeRTOS — SPI Slave 任务

> 改造现有 `src/Communicate_Task_entry.c`：**协议层、命令解析、响应装配 95% 复用，仅替换"通道"**。

#### 5.3.1 头文件新增（`src/spi_protocol.h`）

```c
#ifndef SPI_PROTOCOL_H
#define SPI_PROTOCOL_H

#include <stdint.h>

#define SPI_FRAME_LEN       64u
#define SPI_CMD_OFF_SOF0    0u
#define SPI_CMD_OFF_SOF1    1u
#define SPI_CMD_OFF_VER     2u
#define SPI_CMD_OFF_SEQ     3u
#define SPI_CMD_OFF_DEV     4u
#define SPI_CMD_OFF_CMD     5u
#define SPI_CMD_OFF_LEN     6u
#define SPI_CMD_OFF_FLAGS   7u
#define SPI_CMD_OFF_PAYLOAD 8u
#define SPI_OFF_CRC         62u

#define SPI_RSP_OFF_ACK_SEQ  3u
#define SPI_RSP_OFF_STATUS   4u
#define SPI_RSP_OFF_TYPE     5u
#define SPI_RSP_OFF_LEN      6u
#define SPI_RSP_OFF_FLAGS    7u
#define SPI_RSP_OFF_PAYLOAD  8u
#define SPI_RSP_OFF_UPTIME   56u

#define SPI_SOF0_CMD 0xA5u
#define SPI_SOF1_CMD 0x5Au
#define SPI_SOF0_RSP 0x5Au
#define SPI_SOF1_RSP 0xA5u

#define SPI_FLAG_NEED_RSP    (1u << 0)
#define SPI_FLAG_DATA_READY  (1u << 0)
#define SPI_FLAG_ALARM       (1u << 1)

enum { DEV_SYSTEM = 0x00, DEV_PUMP = 0x01, DEV_SENSOR = 0x02, DEV_LINKAGE = 0x03 };

enum {
    STATUS_OK              = 0x00,
    STATUS_CRC_ERR         = 0x01,
    STATUS_UNKNOWN_DEV     = 0x02,
    STATUS_UNKNOWN_CMD     = 0x03,
    STATUS_BAD_PAYLOAD_LEN = 0x04,
    STATUS_BUSY            = 0x05,
};

enum { RSP_ACK = 0, RSP_SENSOR_DATA = 1, RSP_STATUS = 2 };

#endif
```

#### 5.3.2 Communicate_Task_entry.c 改造（关键片段）

```c
/* Communicate_Task_entry.c — SPI 版（替换原 UART 版本） */

#include "Communicate_Task.h"
#include "spi_protocol.h"
#include "sensor_fusion.h"
#include "wqs_sensor.h"
#include "ds18b20.h"
#include "sht30.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

extern const spi_instance_t g_com_spi;     /* FSP 生成 */

/* 双缓冲：当前正在收发的，与下一帧准备装载的 */
static uint8_t s_tx_frame[SPI_FRAME_LEN];   /* 装下一帧响应 */
static uint8_t s_rx_frame[SPI_FRAME_LEN];   /* 接收主机命令 */

static SemaphoreHandle_t s_spi_done_sem;
static volatile spi_event_t s_last_event;

/* CRC16/Modbus 复用原实现（略） */
extern uint16_t host_crc16_modbus(const uint8_t *p, uint16_t len);

/* SPI 完成回调（中断上下文）—— 唤醒任务 */
void Com_SPI_Callback(spi_callback_args_t *p_args)
{
    BaseType_t hp_woken = pdFALSE;
    s_last_event = p_args->event;
    xSemaphoreGiveFromISR(s_spi_done_sem, &hp_woken);
    portYIELD_FROM_ISR(hp_woken);
}

/* ── 装配 RSP 帧 ── */

static void rsp_finalize(uint8_t *frame, uint8_t ack_seq, uint8_t status,
                         uint8_t type, uint8_t payload_len, uint8_t flags)
{
    frame[0] = SPI_SOF0_RSP;
    frame[1] = SPI_SOF1_RSP;
    frame[2] = 0x01;                   /* version */
    frame[SPI_RSP_OFF_ACK_SEQ] = ack_seq;
    frame[SPI_RSP_OFF_STATUS]  = status;
    frame[SPI_RSP_OFF_TYPE]    = type;
    frame[SPI_RSP_OFF_LEN]     = payload_len;
    frame[SPI_RSP_OFF_FLAGS]   = flags;

    uint32_t uptime = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    frame[SPI_RSP_OFF_UPTIME + 0] = (uint8_t)(uptime);
    frame[SPI_RSP_OFF_UPTIME + 1] = (uint8_t)(uptime >> 8);
    frame[SPI_RSP_OFF_UPTIME + 2] = (uint8_t)(uptime >> 16);
    frame[SPI_RSP_OFF_UPTIME + 3] = (uint8_t)(uptime >> 24);

    uint16_t crc = host_crc16_modbus(frame, SPI_OFF_CRC);
    frame[SPI_OFF_CRC]     = (uint8_t)(crc & 0xFF);
    frame[SPI_OFF_CRC + 1] = (uint8_t)((crc >> 8) & 0xFF);
}

/* SensorData 类型的 PAYLOAD 由现有 host_build_uplink_frame 改造 */
extern uint16_t host_build_sensor_payload(uint8_t *p_payload);  /* 输出 48B */

/* ── 命令分发 ── */

static void handle_cmd(const uint8_t *rx, uint8_t *tx)
{
    /* 1. 校验帧头与 CRC */
    if (rx[0] != SPI_SOF0_CMD || rx[1] != SPI_SOF1_CMD) {
        memset(tx, 0, SPI_FRAME_LEN);
        rsp_finalize(tx, 0xFF, STATUS_CRC_ERR, RSP_ACK, 0, 0);
        return;
    }
    uint16_t calc = host_crc16_modbus(rx, SPI_OFF_CRC);
    uint16_t got  = (uint16_t)rx[SPI_OFF_CRC] | ((uint16_t)rx[SPI_OFF_CRC + 1] << 8);
    if (calc != got) {
        g_comm_rx_crc_error_count++;
        g_alarm_flags |= SENSOR_ALARM_COMM_RX_ERROR;
        memset(tx, 0, SPI_FRAME_LEN);
        rsp_finalize(tx, rx[SPI_CMD_OFF_SEQ], STATUS_CRC_ERR, RSP_ACK, 0, 0);
        return;
    }

    /* 2. 分发 */
    uint8_t dev = rx[SPI_CMD_OFF_DEV];
    uint8_t cmd = rx[SPI_CMD_OFF_CMD];
    uint8_t len = rx[SPI_CMD_OFF_LEN];
    const uint8_t *payload = &rx[SPI_CMD_OFF_PAYLOAD];

    memset(tx, 0, SPI_FRAME_LEN);
    uint8_t status = STATUS_OK;
    uint8_t rsp_type = RSP_ACK;
    uint8_t rsp_len = 0;

    switch (dev) {
    case DEV_SYSTEM:
        if (cmd == 0x00 || cmd == 0x01) {
            /* NOP / PING：仅回 Ack */
        } else if (cmd == 0x04) {
            g_alarm_latched_flags = 0;
        } else {
            status = STATUS_UNKNOWN_CMD;
        }
        break;

    case DEV_SENSOR:
        if (cmd == 0x10 /*POLL_ALL*/) {
            rsp_len  = (uint8_t)host_build_sensor_payload(&tx[SPI_RSP_OFF_PAYLOAD]);
            rsp_type = RSP_SENSOR_DATA;
        } else {
            status = STATUS_UNKNOWN_CMD;
        }
        break;

    case DEV_PUMP:
    case DEV_LINKAGE:
        /* 直接复用现有 host_apply_command 的命令空间映射 */
        host_apply_command(cmd, payload, len);
        break;

    default:
        status = STATUS_UNKNOWN_DEV;
        break;
    }

    uint8_t flags = 0;
    if (g_alarm_flags) flags |= SPI_FLAG_ALARM;
    rsp_finalize(tx, rx[SPI_CMD_OFF_SEQ], status, rsp_type, rsp_len, flags);
    g_comm_rx_cmd_count++;
}

/* ── 任务入口 ── */

void Communicate_Task_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    s_spi_done_sem = xSemaphoreCreateBinary();
    configASSERT(s_spi_done_sem);

    /* 1. 打开 SPI Slave */
    fsp_err_t err = g_com_spi.p_api->open(g_com_spi.p_ctrl, g_com_spi.p_cfg);
    configASSERT(err == FSP_SUCCESS);

    /* 2. 装好"开机 Idle 响应"（此时还没有任何 CMD 可 ack） */
    rsp_finalize(s_tx_frame, 0xFF, STATUS_OK, RSP_ACK, 0, 0);

    /* 3. 主循环：每次启动一次全双工事务，等待 CS+SCK 完成 */
    for (;;) {
        err = g_com_spi.p_api->writeRead(g_com_spi.p_ctrl,
                                         s_tx_frame, s_rx_frame,
                                         SPI_FRAME_LEN, SPI_BIT_WIDTH_8_BITS);
        if (err != FSP_SUCCESS) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        /* 等待主机完成本次事务（CS 拉高 + DMAC EOT） */
        if (xSemaphoreTake(s_spi_done_sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
            /* 超时：主机长时间无事务，循环重试 */
            continue;
        }
        if (s_last_event != SPI_EVENT_TRANSFER_COMPLETE) {
            /* 错误事件，记录后重试 */
            g_alarm_flags |= SENSOR_ALARM_COMM_RX_ERROR;
            continue;
        }

        /* 4. 解析本帧命令，装下一帧响应 */
        handle_cmd(s_rx_frame, s_tx_frame);
    }
}
```

> 关键点：`writeRead` 是**异步发起**——它把 `s_tx_frame` 装进 DMAC TX、`s_rx_frame` 装进 DMAC RX 之后立刻返回。SPI Slave 不主动产生时钟，必须等主机驱动 SCK，DMAC 才会真正搬数据；搬完后 EOT 中断触发 `Com_SPI_Callback` → 二值信号量唤醒任务。

---

## 6. 错误处理机制

### 6.1 校验失败（CRC 不匹配）

| 端 | 处理 |
|----|------|
| 主机收到 RSP CRC 错 | 丢弃该帧；不重传命令（命令已被从机执行）；递增 `comm_rx_crc_err` 计数；连续 5 次错误 → 重启 SPI |
| 从机收到 CMD CRC 错 | 不执行命令；返回 `STATUS_CRC_ERR`；递增 `g_comm_rx_crc_error_count`；置位 `SENSOR_ALARM_COMM_RX_ERROR` |

### 6.2 超时

| 场景 | 超时阈值 | 行为 |
|------|---------|------|
| 主机：RPMsg 等待裸机核回包 | 50 ms | 重试 1 次，仍失败则报应用层错误 |
| 主机：裸机核 SPI 事务 | 10 ms | 中止事务、CS 强制拉高、复位 SPI 控制器 |
| 从机：`xSemaphoreTake(s_spi_done_sem)` | 1000 ms | 不视为错误（主机可能空闲），继续重启 `writeRead` |

### 6.3 未知命令

从机返回 `STATUS_UNKNOWN_DEV` 或 `STATUS_UNKNOWN_CMD`，主机应用层据此决定是否升级从机固件。

### 6.4 SPI 丢帧 / 错位

定长 64B + 每帧 CS 上升沿复位是天然防护。一旦发生错位（CRC 必失败），下一次 CS 拉低时 RA6E2 的 DMAC 自动从字节 0 开始填，不会持续累积偏移。

如果连续 N 次（默认 5）CRC 失败：
- 主机：复位飞腾 SPI 控制器，重新拉低 CS 再开始
- 从机：调 `g_com_spi.p_api->close()` → `open()`，同时把 `g_alarm_latched_flags` 置上诊断位

### 6.5 状态码统一表

| 值 | 名称 | 含义 |
|----|------|------|
| `0x00` | `STATUS_OK` | 成功 |
| `0x01` | `STATUS_CRC_ERR` | CRC 校验失败 |
| `0x02` | `STATUS_UNKNOWN_DEV` | 设备类别未知 |
| `0x03` | `STATUS_UNKNOWN_CMD` | 命令 ID 未知 |
| `0x04` | `STATUS_BAD_PAYLOAD_LEN` | payload 长度不符 |
| `0x05` | `STATUS_BUSY` | 资源忙（如水泵正在启动中） |
| `0x80..0xFF` | 厂商扩展 | 留给业务层 |

---

## 7. 可扩展性设计

### 7.1 新增设备

按 §3.1 设备类别空间分配 ID（`0x04..0xEF` 待用）：

1. 在 `spi_protocol.h` 新增 `DEV_xxx` 枚举。
2. RA6E2 `handle_cmd()` switch 新增分支。
3. Linux 用户态客户端新增对应封装函数。
4. **裸机核固件不需要改动**——它对 DEV/CMD 字段无感知。

### 7.2 新增命令

在已有设备类别下新增 CMD ID：

1. RA6E2 在对应 `case DEV_xxx` 下追加 `if (cmd == ...)`。
2. Linux 客户端新增构造函数。
3. 保持 `payload_len ≤ 16`，超过则要拆分为多帧或考虑升级到协议 v2（VER=0x02）。

### 7.3 预留字段

| 字段 | 位置 | 用途建议 |
|------|------|---------|
| CMD 帧 24..61 字节 | 38B 保留 | 长 payload、批量配置、未来 OTA chunk |
| RSP 帧 60..61 字节 | 2B 保留 | 未来加报警子码 |
| FLAGS bit2..7 | 6 位 | 优先级标志、加密标志、压缩标志 |
| VER 字段 | 1B | 协议演进；从机看到不认识的 VER 应回 `STATUS_UNKNOWN_CMD` |

### 7.4 协议升级路径（v2 草案）

如果未来需要：
- payload > 48B → 引入 **多帧 sequence**（FLAGS bit2=`MORE`，主机连续发 N 帧）
- 实时事件（从机主动通知）→ 引入 **DATA_READY GPIO**，让从机能在 250 ms 周期之外打断主机
- 加密 → 在 PAYLOAD 区前加 4B nonce + 4B MAC，用 AES-CMAC

均可在不破坏现有帧布局的前提下，通过 `VER=0x02` 平滑过渡。

---

## 附录 A：RA6E2 现有工程改造清单

为接入本协议，需要在 `hardware/demo_wyr/FreeRTOS drive/data_merge` 工程中做如下修改：

1. **`ra_gen/Communicate_Task.c`** — DMAC 字宽
   - `g_com_spi_rx_info.transfer_settings_word_b.size` : `TRANSFER_SIZE_2_BYTE` → **`TRANSFER_SIZE_1_BYTE`**
   - `g_com_spi_tx_info.transfer_settings_word_b.size` : 同上
   - 这两处必须配合 `bit_width = SPI_BIT_WIDTH_8_BITS` 才不会出现"字节顺序错乱"。

2. **`src/Communicate_Task_entry.c`** — 替换主循环
   - 删除 `host_uart0_*` 全部函数
   - 删除 `host_parse_downlink_stream()`
   - 把 §5.3.2 给出的 `Communicate_Task_entry` 替换上去
   - 保留 `host_crc16_modbus`、`host_apply_command`、`host_update_alarm_flags`、`host_build_uplink_frame`（重命名/拆出 `host_build_sensor_payload`）

3. **`src/spi_protocol.h`** — 新建（§5.3.1）

4. **FSP 配置（`configuration.xml`）**
   - SCI0 UART 模块仍可保留作日志，但与上位机的"通信角色"已让位给 SPI
   - 确认 SPI1 Slave 的 `tei_irq` IPL 优先级 ≥ FreeRTOS `configMAX_SYSCALL_INTERRUPT_PRIORITY`（当前 12 OK）

## 附录 B：物理链路验证（spi_scope_demo）

在裸机核固件就绪前，可以用 `hardware/phytiumpi/spi0_scope_demo/spi_scope_demo.c` 通过 Linux spidev **直接验证物理链路与 RA6E2 SPI Slave 是否通**：

```bash
sudo ./spi_scope_demo -s 1000000 -g 200 -c 5
```

预期：示波器在 SCK 上看到 5 次突发，每次 8 字节；RA6E2 端的 `Com_SPI_Callback` 应触发 5 次。

> 注：这条路径**只用于验证布线和电平**，正式通信走 RPMsg + 裸机核。验证完后切回。

## 附录 C：参考文件索引

| 文件 | 用途 |
|------|------|
| `hardware/phytiumpi/openamp/demo/rpmsg-demo-single.c` | Linux ↔ 裸机核 RPMsg 模板，§5.1 改造起点 |
| `hardware/phytiumpi/spi0_scope_demo/spi_scope_demo.c` | spidev 验证工具，附录 B |
| `hardware/demo_wyr/FreeRTOS drive/data_merge/ra_gen/Communicate_Task.c` | RA6E2 SPI Slave + DMAC 现有 FSP 配置 |
| `hardware/demo_wyr/FreeRTOS drive/data_merge/src/Communicate_Task_entry.c` | 现有 UART 协议实现，本方案复用其 CRC、命令解析、payload 装配逻辑 |
| `hardware/demo_wyr/FreeRTOS drive/data_merge/src/sensor_fusion.h` | RA6E2 全局传感器/水泵/告警变量声明，响应帧 PAYLOAD 直接取自这里 |
