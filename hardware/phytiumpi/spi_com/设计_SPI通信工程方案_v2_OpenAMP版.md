# PhytiumPi ↔ RA6E2 SPI 通信工程设计方案 v2 (OpenAMP)

> 适用项目：AquaGarden
> 上位机：飞腾派 Linux 主核（业务层）+ 飞腾派裸机从核（OpenAMP remote core，SPI Master 物理驱动）
> 下位机：RA6E2 + FreeRTOS + Renesas FSP（SPI Slave）
> 物理层：4 线 SPI（SCK / MOSI / MISO / CS），全双工，64 字节定长帧
> 
> 本文档是 **v2**：严格遵循 `任务_功能实现_SPI通信.md` §一所要求的"PhytiumPi 跑在 OpenAMP 裸机核（remote core）"架构。
> 
> **协议层（帧格式 / CRC / 命令表）与 v1 完全一致**，唯一差别是飞腾派端 SPI Master 的实现位置：
> 
> - **v1**（`设计_SPI通信工程方案.md`，已落地工作）：Linux 用户态 daemon `aqua_spid` 直接用 spidev 驱动 SPI0
> - **v2**（本文档）：Linux 业务进程通过 RPMsg 把命令交给裸机从核固件 `openamp_spi_core0.elf`，由从核驱动 SPI0
> 
> 两版**协议字节流完全二进制兼容**——RA6E2 一份固件可同时配 v1 / v2 主机端，不需要任何改动。
> 
> v1 不会被废弃，作为 v2 的 fallback 路径保留（详见 §8）。

---

## 0. 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│ 飞腾派 PhytiumPi (PE2204, ARMv8 4 核：core0/1 大核 + core2/3 小核)   │
│                                                                     │
│ ┌─────────────── Linux 主核（运行在 core1/2/3） ──────────────────┐ │
│ │  ┌──────────────────┐    ┌──────────────────────────────────┐  │ │
│ │  │ aqua_spi_cli      │    │ 业务后端 / Web / Python / ROS2    │  │ │
│ │  │ 命令行调试工具     │    │ 任意客户端进程                    │  │ │
│ │  └────────┬──────────┘    └─────────┬────────────────────────┘  │ │
│ │           │ Unix Domain Socket      │                           │ │
│ │           │ /tmp/aqua_spi.sock      │                           │ │
│ │           └────────────┬────────────┘                           │ │
│ │                        ▼                                        │ │
│ │  ┌─────────────────────────────────────────────────────────┐    │ │
│ │  │ aqua_rpmsgd（守护进程，单线程）                           │    │ │
│ │  │ - 独占 /dev/rpmsgN（v2 路径，N 由内核动态分配，daemon 通过 │    │ │
│ │  │   扫描 /sys/class/rpmsg/ 找 name="aqua-spi" 的最大编号）  │    │ │
│ │  │   或 /dev/spidev0.0（v1 fallback 路径，自动检测）         │    │ │
│ │  │ - 周期 250 ms 自动 SENSOR_POLL_ALL → 缓存最近快照         │    │ │
│ │  │ - 处理 IPC：SEND_CMD / GET_SNAPSHOT / GET_STATS           │    │ │
│ │  │ - 维护通信统计、连续错误自愈                              │    │ │
│ │  └────────────────────────┬────────────────────────────────┘    │ │
│ └───────────────────────────│─────────────────────────────────────┘ │
│                             │ RPMsg over virtio + IPI               │
│                             │ 共享内存窗口 0xC000_0000 ~ 0xC0FF_FFFF │
│                             ▼                                       │
│ ┌──────────── 裸机从核（运行在 core0，aarch64） ────────────────┐  │
│ │  openamp_spi_core0.elf  ── 由 Linux remoteproc 启停             │ │
│ │                                                                 │ │
│ │  ┌────────────────────┐   ┌────────────────────────────────┐    │ │
│ │  │ RPMsg endpoint      │──▶│ aqua_dispatch_loop            │    │ │
│ │  │ "aqua-spi"          │   │ - 接收 64B CMD frame          │    │ │
│ │  │ - rpmsg_endpoint_cb │◀──│ - 调 fspim 发 64B CMD over SPI │    │ │
│ │  │ - rpmsg_send (RSP)  │   │ - 5 ms 后再发 64B NOP，取 RSP  │    │ │
│ │  └────────────────────┘   │ - 透传 64B RSP frame 回 RPMsg │    │ │
│ │                            │ - 维护 SPI 控制器、CS、错误计数 │    │ │
│ │                            └────────────────────────────────┘   │ │
│ │  ┌────────────────────────────────────────────────────────────┐ │ │
│ │  │ FSPIM driver (drivers/spi/fspim/, FSPI0 = 0x2803_A000)     │ │ │
│ │  │ FIOPad mux (board/phytiumpi_firefly/fio_mux.c)             │ │ │
│ │  │ libmetal + open-amp（third-party/openamp）                 │ │ │
│ │  └────────────────────────────────────────────────────────────┘ │ │
│ └────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                              │ SPI Bus (Mode 1, 1 MHz, 8-bit, MSB)
                              │ 64B CMD / 64B RSP（CMD + NOP_READ 两次事务）
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│ RA6E2（FreeRTOS + Renesas FSP，SPI1 Slave + 双 DMAC）                │
│   Communicate_Task → app_dispatch → PUMP / SENSOR / LINKAGE         │
│   （与 v1 完全相同；无任何代码改动）                                 │
└─────────────────────────────────────────────────────────────────────┘
```

### 0.1 三层职责

| 层                              | 跑在哪       | 是否解析业务语义                              | 修改方式            | 重新部署成本                                                            |
| ------------------------------ | --------- | ------------------------------------- | --------------- | ----------------------------------------------------------------- |
| 业务进程 / CLI                     | Linux 用户态 | 是（最终决策者）                              | 改 Python/C 代码   | 重启进程，秒级                                                           |
| `aqua_rpmsgd`                  | Linux 用户态 | 仅做 IPC ↔ RPMsg 转发 + CMD 帧打包 + RSP 帧解包 | 改 C 代码          | systemd 重启，秒级                                                     |
| 裸机从核固件 `openamp_spi_core0.elf` | 飞腾派 core0 | **否**（仅做 RPMsg ↔ SPI 字节透传 + 时序控制）     | 改 SDK 例程 + 重新编译 | 替换 `/lib/firmware/openamp_spi_core0.elf` + remoteproc restart，分钟级 |
| RA6E2 Communicate_Task         | RA6E2 大核  | 是（最终执行 + 状态机）                         | 改 e2studio 工程   | OTA / J-Link 重烧，分钟级                                               |

> **设计原则：** 裸机核固件**对协议语义无感知**，只搬字节。任何业务命令的新增 / 修改都不需要重烧裸机核固件 —— 只改 Linux 端 + RA6E2 端。这是把"实时 + 物理控制"和"业务逻辑 + 调试便利"同时做到的关键。

### 0.2 与 v1 的关系

| 维度              | v1                                              | v2 (本文档)                                      |
| --------------- | ----------------------------------------------- | --------------------------------------------- |
| SPI Master 实现位置 | Linux spidev 内核驱动                               | 飞腾派裸机核 fspim 驱动                               |
| 调度抖动来源          | Linux 用户态 → ioctl → 内核 → 控制器；250 ms 周期下 < 10 µs | 裸机核（无抢占，无中断风暴）；< 1 µs                         |
| 调试难度            | 低（`spi_scope_demo.c` 直接验证）                      | 中（需 console + remoteproc 日志）                  |
| 任务文档符合度         | ✗（任务文档明确要求 OpenAMP）                             | ✅                                             |
| RA6E2 端固件       | 完全一致                                            | 完全一致（**0 修改**）                                |
| 协议字节流           | v1 协议（SPI_PROTO_VERSION=0x01）                   | v1 协议（SPI_PROTO_VERSION=0x01，**100% 兼容**）     |
| Linux 端 daemon  | `aqua_spid`（持有 spidev）                          | `aqua_rpmsgd`（持有 rpmsg），共享同一份 `aqua_ipc.h` 协议 |
| 客户端 / CLI       | 不需要任何改动（连同一个 `/tmp/aqua_spi.sock`）              | 不需要任何改动                                       |

**关键不变量：**

- `include/spi_protocol.h` 完全复用，**v2 不引入任何协议层改动**
- `linux/libaqua_spi/spi_codec.{h,c}`（CRC + 帧打包/校验）完全复用
- `linux/libaqua_spi/aqua_ipc.h`（IPC 协议）完全复用
- `linux/aqua_spi_cli.c`（CLI）完全不需要改

---

## 1. SPI 硬件配置

### 1.1 控制器选型

飞腾派（PE2204）SoC 内有 4 个独立 SPI Master 控制器（FSPI0..FSPI3，参见 SDK `soc/pe220x/fparameters_comm.h`）：

| ID        | 基地址          | IRQ     | 引脚（飞腾派排针）                                        | 用途        |
| --------- | ------------ | ------- | ------------------------------------------------ | --------- |
| **FSPI0** | `0x2803A000` | **191** | **SPI0_SCK / MOSI / MISO / CSN0**（板载已 IOPad mux） | **本方案使用** |
| FSPI1     | `0x2803B000` | 192     | 未引出到排针                                           | —         |
| FSPI2     | `0x2803C000` | 193     | 未引出到排针                                           | —         |
| FSPI3     | `0x2803D000` | 194     | 未引出到排针                                           | —         |

**选 FSPI0 的理由：**

1. **就是 v1 用的同一个控制器**——飞腾派 spidev 节点 `/dev/spidev0.0` 就是 FSPI0 (`0x2803A000`)。物理走线完全复用，不需要重新接线
2. SDK 板级 `board/phytiumpi_firefly/fio_mux.c::FIOPadSetSpimMux(FSPI0_ID)` 已经把 SCLK/TXD/RXD/CSN0 的 IOPad 复用配置好（FUNC2），调用一次即可
3. 其他 SPI 控制器在飞腾派板上未引出，配置成本高且无法验证

### 1.2 配置参数表

| 配置项    | 选择                                                        | 理由                                                                                                                        |
| ------ | --------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| SPI 模式 | **Mode 1**（CPOL=0, CPHA=1）                                | 与 v1 一致；RA6E2 FSP 的 SPI Slave 不支持 CPHA=0；裸机核 FSPIM 用 `FSPIM_CPOL_LOW` + `FSPIM_CPHA_2_EDGE` 配置                            |
| 通信速率   | **1 MHz**（首发），可调 4 MHz                                    | 64 字节单帧 0.5 ms，对 250 ms 周期空载 < 1%；飞腾派排针无屏蔽，1 MHz 给信号完整性留够裕度。SDK `FSPI_DEFAULT_SCLK = 5 MHz` 太高，必须显式设 1 MHz                |
| 数据位宽   | **8 bit**（`FSPIM_1_BYTE`）                                 | 协议按字节流设计；与 RA6E2 双 DMAC 1-Byte 配置一致；避免 16-bit 模式的 endian/对齐 bug                                                           |
| 传输方式   | **Polling（`TRANS_WAY_POLL`）→ Interrupt 升级路径**             | v2 首发用 polling 简化裸机核（64 B / 1 MHz = 0.5 ms 阻塞，对 250 ms 周期可忽略）；后续如要降低 CPU 占用，改 `TRANS_WAY_INTERRUPT`，回调里给信号量再唤醒 RPMsg 端口任务 |
| DMA    | **不使用**（v2 首发）                                            | 64 字节单帧太短，DMAC 描述符建立开销 > 数据搬运时间。v3 如要并发处理多 client，可启 `TRANS_WAY_DDMA`                                                     |
| CS 管理  | **裸机核显式 `FSpimSetChipSelection(spim, TRUE/FALSE)`**（每帧一次） | 与 v1 spidev `SPI_IOC_MESSAGE` 自动管理 CS 等价；CS 上升沿是 RA6E2 DMAC "帧结束 + 复位"信号，是抗错位最关键的一招                                       |
| 字节序    | **小端 LE**                                                 | 飞腾 ARMv8 + RA6E2 ARMv7 默认皆 LE；不做转换                                                                                        |
| 位序     | **MSB First**                                             | 行业默认；RA6E2 已配 `SPI_BIT_ORDER_MSB_FIRST`                                                                                   |

### 1.3 引脚映射（与 v1 完全一致）

| 信号   | 飞腾派排针       | 裸机核 FIOPad（SDK 已实现）            | RA6E2         |
| ---- | ----------- | ------------------------------ | ------------- |
| SCK  | `SPI0_SCK`  | `FIOPAD_W55_REG0_OFFSET` FUNC2 | P102 (RSPCK1) |
| MOSI | `SPI0_MOSI` | `FIOPAD_W53_REG0_OFFSET` FUNC2 | P101 (MOSI1)  |
| MISO | `SPI0_MISO` | `FIOPAD_U55_REG0_OFFSET` FUNC2 | P100 (MISO1)  |
| CS   | `SPI0_CSN0` | `FIOPAD_U53_REG0_OFFSET` FUNC2 | P103 (SSL10)  |
| GND  | 任一 GND      | —                              | GND           |

接线 < 15 cm，必须共地。

> **重要：** 部署 v2 时**必须卸载 spidev overlay**（`overlay/install_spidev_overlay.sh remove`），否则 Linux 内核 spi-phytium 驱动会占用 `0x2803A000`，裸机核拿不到控制器。fallback 到 v1 时再 `apply` 回来。详见 §8.3。

### 1.4 远程核 FSPIM 初始化代码片段

```c
#include "fparameters.h"
#include "fspim.h"
#include "fio_mux.h"
#include "finterrupt.h"

#define AQUA_SPI_ID         FSPI0_ID
#define AQUA_SPI_SCLK_HZ    1000000U     /* 1 MHz */

static FSpim g_aqua_spim;

int aqua_spi_init(void)
{
    FSpimConfig cfg;
    FError err;

    FIOMuxInit();
    FIOPadSetSpimMux(AQUA_SPI_ID);

    cfg = *FSpimLookupConfig(AQUA_SPI_ID);
    cfg.work_mode    = FSPIM_DEV_MASTER_MODE;
    cfg.slave_dev_id = FSPIM_SLAVE_DEV_0;          /* CSN0 */
    cfg.cpol         = FSPIM_CPOL_LOW;             /* Mode 1 */
    cfg.cpha         = FSPIM_CPHA_2_EDGE;
    cfg.n_bytes      = FSPIM_1_BYTE;
    cfg.sclk_hz      = AQUA_SPI_SCLK_HZ;
    cfg.trans_way    = TRANS_WAY_POLL;
    cfg.en_test      = FALSE;
    cfg.en_dma       = FALSE;

    err = FSpimCfgInitialize(&g_aqua_spim, &cfg);
    if (err != FSPIM_SUCCESS) return -1;
    return 0;
}

int aqua_spi_xfer_64(const uint8_t *tx, uint8_t *rx)
{
    FError err;
    FSpimSetChipSelection(&g_aqua_spim, TRUE);     /* CS low */
    err = FSpimTransferPollFifo(&g_aqua_spim, tx, rx, SPI_FRAME_LEN);
    FSpimSetChipSelection(&g_aqua_spim, FALSE);    /* CS high */
    return (err == FSPIM_SUCCESS) ? 0 : -1;
}
```

---

## 2. SPI 通信协议设计

### 2.1 与 v1 的关系：**完全复用**

`include/spi_protocol.h` 是协议唯一定义源。v2 不动一个字节，原因：

1. RA6E2 端固件不变，物理层字节流必须 100% 兼容
2. SPI 协议层（CRC、SOF、SEQ、定长 64 B）与"主机端是 Linux 进程还是裸机核"完全正交
3. v1 单元测试 `tests/test_spi_protocol.c` 同时验证 v1/v2

### 2.2 帧总长 = 64 字节（定长）

完整定义见 v1 设计文档 §2.3 / §2.4。这里给 OpenAMP 路径的额外约束：

- RPMsg vring buffer 默认 512 B，64 B CMD/RSP 远小于上限，**永远在一个 RPMsg 包内传完**，绝不分片
- RPMsg 包 = `[64 B CMD frame]` 或 `[64 B RSP frame]`，**裸字节，不再加任何包头**——所有协议字段都在 64 B 帧内
- RPMsg 包对齐：libmetal 要求 buffer 对齐 cache line（一般 64 B），64 B 帧天然对齐

### 2.3 v2 引入的 RPMsg 服务名

| 服务名 (`RPMSG_SERVICE_NAME`) | 用途                               |
| -------------------------- | -------------------------------- |
| `"aqua-spi"`               | AquaGarden SPI 透传通道（Linux ↔ 裸机核） |

> Linux 端通过 `RPMSG_CREATE_EPT_IOCTL` 用同名 endpoint 名 `"aqua-spi"` 与裸机核协商。
> 与 SDK 自带例程的 `RPMSG_SERVICE_NAME` 不同名，避免冲突。
> 
> **设备节点动态发现：** Linux 内核 `rpmsg_char` 驱动会按 endpoint 注册顺序分配 `/dev/rpmsg0`、`/dev/rpmsg1`...
> 编号不是固定的（如果系统里还跑别的 RPMsg 服务，编号会漂移）。daemon 实际不写死路径，
> 而是先 `RPMSG_CREATE_EPT_IOCTL` 注册端点，然后扫描 `/sys/class/rpmsg/` 下所有 `rpmsgN`，
> 找 `name=aqua-spi` 中编号最大的那个再 `open`。这样无论裸机核是第一个还是第 N 个 announce 服务都能命中。

### 2.4 双向数据流约定

```
Linux 业务进程 → aqua_rpmsgd → RPMsg → 裸机核 → SPI 总线 → RA6E2
   1 个 IPC 请求(SEND_CMD)
   ─────────────────────►
                          打包 64 B CMD
                          ────────────►
                                       rpmsg_send(64 B)
                                       ────────────────►
                                                         CS↓ 发 64 B → CS↑
                                                         5 ms 间隔
                                                         CS↓ 发 64 B NOP → CS↑
                                                         拿到 64 B RSP
                                       ◄────────────────
                                       rpmsg_send(64 B RSP)
                          ◄────────────
                          解析 RSP，
                          填 IPC response
   ◄─────────────────────
   返回业务结果
```

**关键决策（v2 新增）：**

> **CMD + NOP_READ 两次 SPI 事务都在裸机核内完成，对 Linux 端透明。** 一个 RPMsg `send` 触发一次完整的"发命令 → 取响应"循环。Linux 端永远是同步的"发一包，收一包"模型，不需要管 SEQ 流水线。

这条决策避免了"两次 RPMsg 往返"造成的延迟翻倍，也对应于 v1 设计文档 §2.5 的 Q3 决策（CMD + NOP_READ 两次事务），**保留可读性收益**。

### 2.5 端序 / 位序

- 多字节字段一律 LE
- 位序 MSB First
- CRC16/Modbus，多项式 `0xA001`，初值 `0xFFFF`，结果小端
- 与 v1 100% 一致

---

## 3. 命令定义表

**完全复用 v1（`include/spi_protocol.h` + `设计_SPI通信工程方案.md` §3）。**

下表是简表，详见 v1 设计文档：

| DEV ID       | 名称                | CMD ID 范围    | 主要命令                                                                  |
| ------------ | ----------------- | ------------ | --------------------------------------------------------------------- |
| `0x00`       | `SPI_DEV_SYSTEM`  | `0x00..0x05` | NOP, PING, GET_VERSION, GET_UPTIME, RESET_ALARM, HELLO                |
| `0x01`       | `SPI_DEV_PUMP`    | `0x01..0x20` | START, STOP, SET_PWM, SET_AUTO/MANUAL, SET_CYCLE_CFG/CTRL, GET_STATUS |
| `0x02`       | `SPI_DEV_SENSOR`  | `0x10..0x14` | POLL_ALL, POLL_AIR/WATER/SOIL/PRESSURE                                |
| `0x03`       | `SPI_DEV_LINKAGE` | `0x01..0x02` | SET_SOIL_CFG, SET_RULE                                                |
| `0x04..0xEF` | 预留                | —            | 未来扩展                                                                  |
| `0xF0..0xFF` | 厂商自定义             | —            | OEM 扩展                                                                |

> **重要：** v2 不允许在文档里另立命令编号。任何新增 DEV/CMD 必须改 `include/spi_protocol.h`，然后 `bash ra6e2_patch/sync_from_canonical.sh` 同步到 RA6E2。裸机核固件**不需要重烧**——它对 DEV/CMD 完全无感知，只搬 64 字节。

---

## 4. 通信流程时序

### 4.1 总体节拍

| 节拍                      | 周期 / 阈值                       | 说明                                      |
| ----------------------- | ----------------------------- | --------------------------------------- |
| 主机周期 SENSOR_POLL_ALL    | 250 ms                        | 与 RA6E2 `Communicate_Task` 节拍对齐；与 v1 一致 |
| RPMsg 单次往返延迟            | < 1 ms（aarch64 + IPI）         | libmetal 实测，不含 SPI 物理传输                 |
| SPI CMD 帧               | 64 B / 1 MHz = 512 µs + CS 切换 | 与 v1 一致                                 |
| CMD ↔ NOP 间隔            | **5 ms**                      | 留给 RA6E2 ISR + memcpy 装 RSP             |
| Linux 端 IPC SEND_CMD 总耗 | < 7 ms                        | RPMsg 1 ms + SPI 6 ms                   |
| 单命令 CPU 占用（裸机核）         | 占用 6 ms / 250 ms ≈ 2.4%       | 周期负载 < 3%                               |

### 4.2 单命令完整时序图

```
时间轴 →

Linux 业务进程  ─►SEND_CMD                                                       ◄─返回
                       │                                                              ▲
                       ▼                                                              │
aqua_rpmsgd     ─►打包64B CMD─►write(/dev/rpmsg0, 64B)                解析RSP─►写IPC─┘
                       │                                                       ▲
                       ▼                                                       │
RPMsg/IPI       ──────►SGI 中断到 core0──────►        ◄──IPI back──────────────┘
                                              ▼                                ▲
裸机核 RPMsg cb                           dispatch_loop:                       │
                                              │                                │
                                       CS↓  64 B CMD over SPI  CS↑             │
                                       (~512 µs)                               │
                                       ─────────► RA6E2 SPI ISR                │
                                                  EOT → app_dispatch           │
                                                  装 s_tx_frame (~200 µs)      │
                                       └──── 5 ms wait ────►                   │
                                       CS↓  64 B NOP over SPI  CS↑             │
                                       (~512 µs)                               │
                                       ◄───── RA6E2 写回 RSP                   │
                                              │                                │
                                       rpmsg_send(64 B RSP) ────────────────►──┘
```

### 4.3 时间预算

| 阶段                          | 时间                    |
| --------------------------- | --------------------- |
| Linux IPC accept + 包打包      | ~50 µs                |
| RPMsg write → SGI → 裸机回调    | ~500 µs               |
| 裸机核 SPI 事务 #1（CMD）          | 512 µs SCLK + 5 µs CS |
| 等 RA6E2 装好 RSP              | 5 ms                  |
| 裸机核 SPI 事务 #2（NOP_READ）     | 512 µs SCLK + 5 µs CS |
| RPMsg send → SGI → Linux 唤醒 | ~500 µs               |
| daemon 解析 RSP + 填 IPC       | ~100 µs               |
| **单命令端到端**                  | **~7 ms**             |
| 周期 250 ms 利用率               | < 3%                  |

### 4.4 主机端状态机（aqua_rpmsgd 主循环，伪码）

```
启动:
    # daemon 不主动启动 remoteproc——那是 systemd 单元 aqua-openamp-load.service
    # 在 daemon 之前完成的事（remoteproc start + modprobe rpmsg_char +
    # /sys/bus/rpmsg/.../driver_override = rpmsg_chrdev）。
    # daemon 这里只做 autoselect：
    backend = aqua_backend_autoselect():
        if access(/dev/rpmsg_ctrl0, RW) == 0:
            ctrl_fd = open(/dev/rpmsg_ctrl0)
            ioctl(ctrl_fd, RPMSG_CREATE_EPT_IOCTL, {name="aqua-spi", src=0, dst=ANY})
            best = scan(/sys/class/rpmsg/, name=="aqua-spi")  # 找最大编号
            rpmsg_fd = open(/dev/rpmsg<best>)
            return rpmsg_backend
        if access(/dev/spidev0.0, RW) == 0:
            spidev_fd = open(/dev/spidev0.0)
            ioctl(SPI_IOC_WR_BITS_PER_WORD=8, SPI_IOC_WR_MAX_SPEED_HZ=1MHz)
            return spidev_backend
        return NULL  # 两个后端都不可用，daemon 启动失败

    listen_socket(/tmp/aqua_spi.sock)
    SYS_HELLO + SYS_PING （5 s 内重试至 STATUS=OK；超时不退出，
                            交给周期重试自愈）

主循环（poll, 唤醒源 = 周期定时 / 客户端连接 / 信号）:
    if 周期到:
        do_send_cmd(SENSOR, POLL_ALL, ...) → 缓存 SensorData
    if 客户端连接:
        accept → ipc_handle() → close
    if 信号(SIGINT/SIGTERM):
        backend->close()
        退出

do_send_cmd(dev, cmd, payload, len, flags):     # 后端无关
    spi_pack_cmd(frame_64B, ++seq, dev, cmd, payload, len, flags|NEED_RSP)
    rc = backend->xfer(ctx, tx_frame_64B, rx_frame_64B)
        # 实现：
        # rpmsg : write(rpmsg_fd, tx, 64) + read(rpmsg_fd, rx, 64) + poll() 超时 100 ms
        # spidev: ioctl(SPI_IOC_MESSAGE, tx) + nanosleep(5 ms) +
        #         ioctl(SPI_IOC_MESSAGE, NOP→rx)
    if rc != 0:
        stats.io_err++; consec_err++
        if consec_err >= 5: backend->reset(ctx); consec_err = 0
        return rc
    if spi_validate_frame(rx) != 0:        # CRC / SOF 错
        stats.crc_err 或 sof_err ++; consec_err++
        if consec_err >= 5: backend->reset(ctx)
        return -EBADMSG
    stats.rx_rsp_ok++; consec_err = 0
    return rx_frame
```

### 4.5 裸机核端状态机（aqua_dispatch_loop，伪码）

```
全局静态缓冲（避免 ISR 上下文里 alloc）:
    s_nop_frame  [64]   # 上电预先 spi_pack_cmd(SYS_NOP) 一次，后续直接复用
    s_dummy_rx   [64]   # 第 1 次事务 MISO（是上一帧滞后响应，丢弃）
    s_rsp_frame  [64]   # 第 2 次事务收回的 RSP，**rpmsg_send 用这个本端缓冲，
                          # 不要直接重发回调入参 data 指针——那是 vring buf**
    s_busy_frame [64]   # 上电预生成 STATUS=BUSY 的伪 RSP，SPI 出错时塞给 Linux

init:
    init_system()
    aqua_spi_master_init()                # FSPIM cfg + IOPad mux
    aqua_pack_static_frames()             # 预打包 s_nop_frame / s_busy_frame
    platform_create_proc(slave_priv, kick_dev)
    platform_setup_src_table()
    platform_setup_share_mems()
    rpdev = platform_create_rpmsg_vdev()
    rpmsg_create_ept(&lept, rpdev, "aqua-spi", 0, RPMSG_ADDR_ANY,
                     aqua_rpmsg_cb, aqua_rpmsg_unbind)

aqua_rpmsg_cb(ept, data, len, src, priv):
    if (len != 64): log_drop; return RPMSG_SUCCESS    # 协议不识别，忽略但不退出
    ept->dest_addr = src                              # 锁定回包目标地址

    if aqua_spi_master_xfer_64(data, s_dummy_rx) != 0:
        rpmsg_send(ept, s_busy_frame, 64); return RPMSG_SUCCESS   # 防 Linux 阻塞
    fsleep_microsec(5000)                       # 5 ms 让 RA6E2 装 RSP
    if aqua_spi_master_xfer_64(s_nop_frame, s_rsp_frame) != 0:
        rpmsg_send(ept, s_busy_frame, 64); return RPMSG_SUCCESS
    rpmsg_send(ept, s_rsp_frame, 64)            # 透传回 Linux
    return RPMSG_SUCCESS

main_loop:
    while !shutdown_req && !rproc_get_stop_flag():
        platform_poll(remoteproc)              # 收 IPI、跑回调
    rpmsg_destroy_ept(&lept)
    platform_release_rpmsg_vdev(rpdev, &remoteproc)
    aqua_spi_master_deinit()
    platform_cleanup(remoteproc)
    FPsciCpuOff()                              # 不再返回；等 Linux remoteproc 重 start
```

> **实现注记：** 不要把 `data`（回调入参）直接喂给 `rpmsg_send`——
> 它指向的是 vring 接收缓冲，rpmsg_send 内部要走自己的 TX vring，**复用同一块内存
> 会污染 RX 路径**。所以裸机核固件用本端 `s_rsp_frame` 缓冲；这与 SDK echo 例程
> 的写法不同，是本工程刻意的稳健化。

> **关键：** 整个裸机核固件不识别 DEV/CMD/STATUS 任何字段。它只看 `len == 64`，看其他都是黑盒。这就是"任何业务命令新增不需要重烧裸机固件"的来源。

---

## 5. 双端代码框架

### 5.1 v2 目录结构（增量，不破坏 v1）

```
hardware/phytiumpi/spi_com/
├── 任务_功能实现_SPI通信.md
├── 任务_RA6E2侧.md
├── 设计_SPI通信工程方案.md            # v1（spidev 直驱版本，已落地）
├── 设计_SPI通信工程方案_v2_OpenAMP版.md   # 本文档
├── HANDOFF_x86_build.md               # x86 主机编译/scp 部署交接
├── 使用说明.md                        # 飞腾派开机准备 + 日常使用流程
├── README.md                          # 顶部含 v1/v2 切换章节
├── Makefile                           # 目标 test / linux / linux-native / clean
│
├── include/
│   └── spi_protocol.h                 # 协议唯一定义源（0 修改，v1/v2/RA6E2 共用）
│
├── linux/                             # Linux 端用户态
│   ├── libaqua_spi/
│   │   ├── spi_codec.{h,c}            # CRC + 帧打包/校验（共享）
│   │   └── aqua_ipc.h                 # daemon ↔ CLI Unix Socket 协议
│   ├── aqua_backend.h                 # backend 抽象 ops 表 + spidev/rpmsg ctx
│   ├── aqua_backend_spidev.c          # v1 后端实现
│   ├── aqua_backend_rpmsg.c           # v2 后端实现 + autoselect()
│   ├── aqua_daemon_core.{h,c}         # daemon 主循环 + IPC（v1/v2 共用 95% 代码）
│   ├── aqua_spid.c                    # v1 入口（强制 spidev 后端）
│   ├── aqua_rpmsgd.c                  # v2 入口（默认 -B auto，可强制 rpmsg/spidev）
│   └── aqua_spi_cli.c                 # CLI（与后端无关，0 修改）
│
├── tests/test_spi_protocol.c          # 协议层 PC 单元测试
├── ra6e2_patch/                       # RA6E2 端代码与同步脚本（0 修改）
│
├── overlay/                           # v1 spidev overlay
│   ├── phytium_pi_spidev0.dts
│   └── install_spidev_overlay.sh
│
├── deploy/                            # ★ 飞腾派端部署辅助
│   ├── README.md
│   ├── overlay/                       # v2 reserved-memory overlay（fallback；
│   │   ├── phytium_pi_openamp.dts     #   优先使用飞腾官方 v3-openamp DTB）
│   │   └── install_openamp_overlay.sh
│   ├── systemd/                       # 三个互斥的服务单元
│   │   ├── aqua-spid.service               (v1)
│   │   ├── aqua-rpmsgd.service             (v2)
│   │   └── aqua-openamp-load.service       (v2 必需的前置：远程核加载 + rpmsg_chrdev 绑定)
│   └── scripts/
│       ├── switch_to_v1.sh
│       └── switch_to_v2.sh
│
└── openamp_core/                      # ★ v2 飞腾派裸机核工程
    ├── README.md
    ├── main.c                         # 只调 aqua_spi_slave_run()
    ├── makefile                       # SDK_DIR 默认指 ../../../../../phytium-standalone-sdk
    ├── ft_openamp.ld                  # 复用 SDK 链接脚本（resource_table 段地址）
    ├── sdkconfig / sdkconfig.h        # menuconfig 已生成
    ├── Kconfig
    ├── configs/
    │   └── pe2204_aarch64_phytiumpi_aquaspi_core0.config
    │      （基于 SDK 自带 pe2204_aarch64_phytiumpi_openamp_core0.config 改，
    │       仅多 CONFIG_USE_SPI=y / CONFIG_USE_FSPIM=y 两行）
    ├── common/
    │   ├── memory_layout.h            # 0xC000_0000 共享区，0xB010_0000 镜像区
    │   ├── openamp_configs.h
    │   └── libmetal_configs.h         # SLAVE_00_SGI = KICK_SGI_NUM_9
    ├── inc/
    │   └── aqua_spi_slave.h
    └── src/
        ├── aqua_spi_slave.c           # RPMsg endpoint + dispatch loop（含 BUSY 伪 RSP）
        ├── aqua_spi_master.{h,c}      # FSPIM Master 初始化 + 64B 单帧收发
```

### 5.2 共享协议头（0 修改，完全复用 v1）

参考 v1 设计文档 §5.3。`spi_protocol.h` / `spi_codec.h`/`.c` 在 v2 下原封不动 include，**裸机核也直接用同一份 `include/spi_protocol.h`**（通过 makefile 加 `-I`）。

### 5.3 Linux 端：抽象后端接口

`linux/aqua_backend.h` 实际接口（与代码 1:1 对应，便于后续维护时 grep 上下文）：

```c
typedef enum
{
    AQUA_BACKEND_NONE   = 0,
    AQUA_BACKEND_SPIDEV = 1,    /* v1: /dev/spidev0.0 */
    AQUA_BACKEND_RPMSG  = 2,    /* v2: /dev/rpmsgN (OpenAMP) */
} aqua_backend_kind_t;

typedef void (*aqua_log_fn)(int prio, const char *fmt, ...);

typedef struct aqua_backend_ops
{
    /* 阻塞收发：发 64B CMD，收 64B RSP。
     * 返回 0 = OK；负值 = -errno（-EIO/-ETIMEDOUT/...）。
     * CRC 校验由 daemon_core 做，backend 不解析帧内容。 */
    int  (*xfer)(void *ctx,
                 const uint8_t cmd_frame[SPI_FRAME_LEN],
                 uint8_t       rsp_frame[SPI_FRAME_LEN]);

    /* 连续错误自愈：close 后重新 open（spidev 重置 / rpmsg 重连）。 */
    int  (*reset)(void *ctx);
    void (*close)(void *ctx);

    aqua_backend_kind_t kind;
    const char         *name;     /* "spidev" / "rpmsg" */
} aqua_backend_ops_t;

/* ---- spidev 后端 (v1) ---- */
typedef struct
{
    /* 调用方设置 */
    const char *device_path;      /* "/dev/spidev0.0" */
    uint32_t    speed_hz;         /* 1_000_000 */
    uint32_t    frame_gap_us;     /* 5000 = CMD/NOP 间隔 */
    aqua_log_fn log;
    /* 内部 */
    int      fd;
    uint64_t io_err_count;        /* 累计 ioctl 失败次数 */
} aqua_backend_spidev_ctx_t;

extern const aqua_backend_ops_t aqua_backend_spidev_ops;
int aqua_backend_spidev_open(aqua_backend_spidev_ctx_t *ctx);

/* ---- rpmsg 后端 (v2 OpenAMP) ---- */
typedef struct
{
    /* 调用方设置 */
    const char *ctrl_path;        /* "/dev/rpmsg_ctrl0" */
    const char *device_path;      /* "/dev/rpmsg0" — 仅 hint，open 前会被
                                   *   resolved_device_path 覆盖（按 service name 扫描） */
    const char *service_name;     /* "aqua-spi" */
    uint32_t    timeout_ms;       /* 单次 read 超时；默认 100 */
    aqua_log_fn log;
    /* 内部 */
    int      ctrl_fd;
    int      rpmsg_fd;
    char     resolved_device_path[64];   /* /sys/class/rpmsg 扫描出来的实际路径 */
    uint64_t io_err_count;
} aqua_backend_rpmsg_ctx_t;

extern const aqua_backend_ops_t aqua_backend_rpmsg_ops;
int aqua_backend_rpmsg_open(aqua_backend_rpmsg_ctx_t *ctx);

/* ---- 自动选择 ----
 * 优先 rpmsg（探测 /dev/rpmsg_ctrl0 + 尝试 open）；失败回退 spidev。
 * 调用方需要先在两个 ctx 里准备好 device_path / speed_hz / log 等"输入字段"。 */
int aqua_backend_autoselect(aqua_backend_spidev_ctx_t *spidev_ctx,
                            aqua_backend_rpmsg_ctx_t  *rpmsg_ctx,
                            const aqua_backend_ops_t **out_ops,
                            void                     **out_ctx);

/* daemon GET_STATS 时用：把 backend 内部累计 IO 错搬到 IPC 统计字段。 */
uint64_t aqua_backend_io_err_count(const aqua_backend_ops_t *ops, void *ctx);
```

实现要点：

- `aqua_backend_spidev.c`：`xfer()` 内部跑两次 `SPI_IOC_MESSAGE`(CMD) + `nanosleep(5 ms)` + `SPI_IOC_MESSAGE`(NOP→RSP)；
  `reset()` = close + 10 ms gap（让 RA6E2 DMAC 复位）+ 重新 open。
- `aqua_backend_rpmsg.c`：`xfer()` = `write_all(64 B)` + `poll(timeout_ms)` + `read_exact(64 B)`，
  裸机核负责双 SPI 事务。`reset()` = close + 重新 `RPMSG_CREATE_EPT_IOCTL` + 重新扫描 `/sys/class/rpmsg/`。
- `aqua_rpmsgd.c` 与 `aqua_spid.c`：仅做命令行解析 + 选 backend，主循环全部委托给
  `aqua_daemon_core.c::aqua_daemon_run()`。两个 daemon 共享 95% 代码。

### 5.4 v2 后端实现（`aqua_backend_rpmsg.c` 关键片段）

```c
#define _GNU_SOURCE
#include "aqua_backend.h"
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/rpmsg.h>

#define DEFAULT_RPMSG_CTRL     "/dev/rpmsg_ctrl0"
#define DEFAULT_RPMSG_DEV      "/dev/rpmsg0"        /* 仅 hint，会被覆盖 */
#define DEFAULT_RPMSG_SERVICE  "aqua-spi"
#define DEFAULT_RSP_TIMEOUT_MS 100u

/* 扫描 /sys/class/rpmsg/，找 name=service_name 中编号最大的 rpmsgN，
 * 写到 out 中。这样无论 endpoint 是第一个还是第 N 个 announce 出来都能命中。 */
static int rpmsg_pick_latest_dev(const char *service_name, char *out, size_t out_len)
{
    DIR *dir = opendir("/sys/class/rpmsg");
    if (!dir) return -1;
    int best_id = -1;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL)
    {
        int id; char extra;
        if (sscanf(de->d_name, "rpmsg%d%c", &id, &extra) != 1) continue;
        char path[320], name[64] = {0};
        snprintf(path, sizeof path, "/sys/class/rpmsg/%s/name", de->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;
        if (fgets(name, sizeof name, fp)) {
            name[strcspn(name, "\r\n")] = 0;
            if (strcmp(name, service_name) == 0 && id > best_id) best_id = id;
        }
        fclose(fp);
    }
    closedir(dir);
    if (best_id < 0) return -1;
    snprintf(out, out_len, "/dev/rpmsg%d", best_id);
    return 0;
}

static int rpmsg_open_inner(aqua_backend_rpmsg_ctx_t *c)
{
    struct rpmsg_endpoint_info ept = {0};

    c->ctrl_fd = open(c->ctrl_path, O_RDWR);
    if (c->ctrl_fd < 0) return -1;

    snprintf(ept.name, sizeof ept.name, "%s", c->service_name);
    ept.src = 0;
    ept.dst = 0xFFFFFFFFu;          /* "any"，由内核去匹配裸机核 announce 的 endpoint */
    if (ioctl(c->ctrl_fd, RPMSG_CREATE_EPT_IOCTL, &ept) < 0) {
        close(c->ctrl_fd); c->ctrl_fd = -1; return -1;
    }
    /* 探测真实节点；找不到就保留 device_path 默认值（/dev/rpmsg0）兜底 */
    if (rpmsg_pick_latest_dev(c->service_name,
                              c->resolved_device_path,
                              sizeof c->resolved_device_path) == 0) {
        c->device_path = c->resolved_device_path;
    }
    c->rpmsg_fd = open(c->device_path, O_RDWR);
    if (c->rpmsg_fd < 0) { close(c->ctrl_fd); c->ctrl_fd = -1; return -1; }
    return 0;
}

static int op_xfer(void *_c,
                   const uint8_t cmd[SPI_FRAME_LEN],
                   uint8_t       rsp[SPI_FRAME_LEN])
{
    aqua_backend_rpmsg_ctx_t *c = _c;
    if (c->rpmsg_fd < 0) return -EIO;

    /* write_all 处理 EINTR/部分写 */
    if (write_all(c->rpmsg_fd, cmd, SPI_FRAME_LEN) != SPI_FRAME_LEN) {
        c->io_err_count++; return -EIO;
    }
    /* poll(timeout_ms) → read_exact，避免裸机核挂死时 daemon 卡死 */
    if (read_exact(c->rpmsg_fd, rsp, SPI_FRAME_LEN, c->timeout_ms) != SPI_FRAME_LEN) {
        c->io_err_count++; return -EIO;
    }
    return 0;
}

const aqua_backend_ops_t aqua_backend_rpmsg_ops = {
    .xfer  = op_xfer,
    .reset = op_reset,        /* close + 重新 open（含重扫 sysfs） */
    .close = op_close,
    .kind  = AQUA_BACKEND_RPMSG,
    .name  = "rpmsg",
};
```

> **稳健性细节：**
> 
> 1. `rpmsg_pick_latest_dev` 解决了 RPMsg 设备号漂移问题 —
>    系统里只要还有别的 RPMsg 服务（如 SDK 自带 echo demo），编号就不固定。
> 2. `op_xfer` 用 `poll` 设了 100 ms 默认超时，比裸机核挂死时 `read` 永久阻塞要好。
>    daemon 主循环单线程，read 永久阻塞 = daemon 死锁。
> 3. `reset()` 不仅 close+open ctrl_fd，还会重扫 sysfs。如果 remoteproc restart 过，
>    新的 rpmsg 节点编号会变，必须重新 pick。

### 5.5 裸机核端：`openamp_core/src/aqua_spi_slave.c` 关键片段

骨架来自 SDK `example/system/amp/openamp_for_linux/src/slaver_00_example.c`，**保留资源表 / 共享内存 / kick driver 全部不动**，只替换 endpoint 回调与服务名。
关键稳健化（与 SDK echo 例程的两处差异）：

1. `rpmsg_send` 用本端预分配缓冲 `s_rsp_frame`，**不要直接重发回调入参 `data`**
   （`data` 是 vring RX buffer，复用同一块内存会污染 RX 路径）；
2. SPI 出错时塞一个上电预生成的 STATUS=BUSY 伪 RSP，避免 Linux 端 `read()` 阻塞超时。

```c
#define AQUA_TAG               "AQUA_SPI"
#define AQUA_RPMSG_SERVICE     "aqua-spi"
#define AQUA_RA6E2_PREP_US     5000U     /* 5 ms 让 RA6E2 装好 RSP */

/* 资源表 / kick driver / slave_priv 与 SDK 例程同布局，
 * 地址来自 common/memory_layout.h（SLAVE00_SHARE_MEM_ADDR=0xFFFFFFFF
 * 表示等 Linux 主核分配；SLAVE_00_SGI = KICK_SGI_NUM_9 = 9） */

static u8 s_nop_frame  [SPI_FRAME_LEN];   /* 上电 spi_pack_cmd(SYS_NOP) 一次 */
static u8 s_dummy_rx   [SPI_FRAME_LEN];   /* 第 1 次事务 MISO（丢弃） */
static u8 s_rsp_frame  [SPI_FRAME_LEN];   /* 第 2 次事务取回的 RSP，本端缓冲 */
static u8 s_busy_frame [SPI_FRAME_LEN];   /* SPI 错时的伪 BUSY RSP，预生成 */

static void aqua_pack_static_frames(void)
{
    spi_pack_cmd(s_nop_frame,
                 SPI_SEQ_IDLE,        /* 0xFF */
                 SPI_DEV_SYSTEM, SPI_CMD_SYS_NOP,
                 NULL, 0, 0);
    spi_pack_rsp(s_busy_frame,
                 SPI_SEQ_IDLE, SPI_STATUS_BUSY,
                 SPI_RSP_TYPE_ACK,
                 NULL, 0, 0, 0);
}

static volatile int s_shutdown = 0;

static int aqua_rpmsg_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
                         uint32_t src, void *priv)
{
    int rc; (void)priv;

    if (len != SPI_FRAME_LEN) {
        FT_DEBUG_PRINT_W(AQUA_TAG, "drop pkt len=%u", (unsigned)len);
        return RPMSG_SUCCESS;
    }
    ept->dest_addr = src;       /* 锁定回包目标地址 */

    /* 第 1 次事务：发 CMD，丢 MISO（是上一帧的滞后响应） */
    rc = aqua_spi_master_xfer_64((const u8 *)data, s_dummy_rx);
    if (rc) goto err_busy;

    /* 让 RA6E2 ISR + 任务唤醒 + memcpy 准备好 RSP */
    fsleep_microsec(AQUA_RA6E2_PREP_US);

    /* 第 2 次事务：发 NOP，MISO 即本次命令的响应 */
    rc = aqua_spi_master_xfer_64(s_nop_frame, s_rsp_frame);
    if (rc) goto err_busy;

    /* 注意：用本端 s_rsp_frame，不要把 data 指针直接重发——见小节顶部说明 */
    rpmsg_send(ept, s_rsp_frame, SPI_FRAME_LEN);
    return RPMSG_SUCCESS;

err_busy:
    /* SPI 出错时塞 BUSY 伪 RSP，防 Linux 端 read() 阻塞到超时；
     * daemon_core 看到 STATUS=BUSY 会累计 stats、上层判定失败。 */
    rpmsg_send(ept, s_busy_frame, SPI_FRAME_LEN);
    return RPMSG_SUCCESS;
}

static void aqua_rpmsg_unbind(struct rpmsg_endpoint *ept)
{
    (void)ept;
    s_shutdown = 1;
}

int aqua_spi_slave_run(void)
{
    struct rpmsg_endpoint lept = {0};
    struct remoteproc rproc;
    struct rpmsg_device *rpdev;

    init_system();
    if (aqua_spi_master_init()) return -1;
    aqua_pack_static_frames();

    if (!platform_create_proc(&rproc, &slave_aqua_priv, &kick_driver_aqua)) return -1;
    rproc.rsc_table = &resources;
    if (platform_setup_src_table(&rproc, rproc.rsc_table)) return -1;
    if (platform_setup_share_mems(&rproc)) return -1;
    rpdev = platform_create_rpmsg_vdev(&rproc, 0, VIRTIO_DEV_DEVICE, NULL, NULL);
    if (!rpdev) return -1;

    if (rpmsg_create_ept(&lept, rpdev, AQUA_RPMSG_SERVICE,
                         0, RPMSG_ADDR_ANY,
                         aqua_rpmsg_cb, aqua_rpmsg_unbind)) return -1;

    while (!s_shutdown && !rproc_get_stop_flag()) {
        platform_poll(&rproc);
    }

    rpmsg_destroy_ept(&lept);
    platform_release_rpmsg_vdev(rpdev, &rproc);
    aqua_spi_master_deinit();
    platform_cleanup(&rproc);
    FPsciCpuOff();        /* PSCI CPU off；不再返回，等下一次 remoteproc start */
    return 0;
}
```

`openamp_core/main.c`：

```c
#include <stdio.h>
#include "ftypes.h"
#include "fdebug.h"
#include "sdkconfig.h"
#include "aqua_spi_slave.h"

int main(void)
{
    printf("AquaGarden OpenAMP SPI core, build %s %s\r\n", __DATE__, __TIME__);
    return aqua_spi_slave_run();
}
```

### 5.6 RA6E2 端：**完全不变**

`ra6e2_patch/Communicate_Task_entry.c` 一字不改。RA6E2 看到的物理 SPI 字节流与 v1 完全相同（同一个控制器、同一份 CRC、同一份 64 B 帧布局）。

---

## 6. 错误处理机制

### 6.1 校验失败

| 端                         | 处理                                                                                                        |
| ------------------------- | --------------------------------------------------------------------------------------------------------- |
| Linux daemon 收到 RSP CRC 错 | `stats.rx_crc_err_count++`；不重传命令；连续 5 次错 → 触发后端 `reset()`（v2 = remoteproc restart；v1 = close+open spidev） |
| 裸机核收到非 64 B 长度 RPMsg 包    | 丢弃 + 日志，不发 SPI                                                                                            |
| 裸机核 SPI 控制器返回错误           | 给 Linux 回 STATUS=BUSY 的伪 RSP（防 Linux 阻塞），同时 `stats.spi_err++`                                             |
| RA6E2 收到 CMD CRC 错        | 不执行命令；返回 `STATUS_CRC_ERR`；置 `g_alarm_flags                                                                |

### 6.2 超时

| 场景                                   | 阈值                     | 行为                                    |
| ------------------------------------ | ---------------------- | ------------------------------------- |
| Linux：`read(rpmsg_fd)`               | 100 ms（用 `poll()` 设超时） | 视为裸机核挂死，触发 remoteproc restart 自愈      |
| Linux：客户端 SEND_CMD 等响应               | 默认 200 ms              | 单线程同步执行，几乎不会超时                        |
| 裸机核：`FSpimTransferPollFifo`          | SDK 内部超时               | 错误返回 → 装 STATUS_BUSY 伪 RSP 回 Linux    |
| RA6E2：`xSemaphoreTake(spi_done_sem)` | 1000 ms                | close + open `g_com_spi` 重置 SPI Slave |

### 6.3 裸机核 OpenAMP 异常

| 场景                                    | 处理                                                                                          |
| ------------------------------------- | ------------------------------------------------------------------------------------------- |
| RPMsg endpoint unbind（Linux 关 daemon） | 设 `s_shutdown = 1`，跳出主循环，`platform_cleanup()` + `FPsciCpuOff()`，等 Linux remoteproc 重新 start |
| RPMsg `rpmsg_send` 失败                 | 日志一行，不重试（业务进程会因超时重发）                                                                        |
| `platform_create_rpmsg_vdev` 失败       | 启动失败，远程核 panic 退出，remoteproc state 变 crashed，Linux daemon 自动 fallback 到 v1                  |

### 6.4 未知设备 / 命令 / SPI 错位

完全复用 v1 §6.3 / §6.4 行为。**裸机核对此完全无感知**——所有 STATUS 都由 RA6E2 决定，由 Linux daemon 解析。

### 6.5 状态码统一表

完全复用 v1 §6.5（`spi_status_t`）。

### 6.6 日志通道

| 端            | 通道                                                                                |
| ------------ | --------------------------------------------------------------------------------- |
| Linux daemon | `journalctl -u aqua-rpmsgd -f`                                                    |
| 裸机核          | UART1 串口（飞腾派 `ttyAMA1`），115200 8N1。SDK 默认 `CONFIG_DEFAULT_DEBUG_PRINT_UART1=y` 已选 |
| RA6E2        | 板载 USB-CDC，与 v1 一致                                                                |

---

## 7. 可扩展性设计

### 7.1 新增设备 / 新增命令

**与 v1 完全一致**——只改 `include/spi_protocol.h` + `ra6e2_patch/sync_from_canonical.sh` + RA6E2 `app_dispatch()`。

> **裸机核固件不需要重新编译。** 这是 v2 架构最大的工程收益：业务迭代速度与 v1 持平。

### 7.2 多 client 并发（v3 升级路径）

当前 v2 单 endpoint 串行处理。如果业务需要多业务进程并发：

1. 裸机核增加多个 RPMsg endpoint：`"aqua-spi-fast"`（高优先级）/`"aqua-spi-batch"`（低优先级）
2. SPI 控制器仍然只有一个，需要在裸机核内做 mutex 排队
3. Linux 端：每个 endpoint 一个独立 daemon

### 7.3 直接暴露物理 SPI 给业务进程（v3 调试路径）

裸机核可以再开一个 RPMsg endpoint `"aqua-spi-raw"`：直接收 N 字节 → 转一次 SPI 事务 → 回 N 字节。用于物理层调试，不参与协议层 CRC。

### 7.4 加密 / 多帧 sequence

与 v1 §7.4 完全一致：在 `VER=0x02` 下平滑过渡，v1 解析路径保留。

### 7.5 预留字段

完全复用 v1 §7.3 的 RESERVED 区。

---

## 8. v1 ↔ v2 fallback 与并存策略

### 8.1 设计原则

1. **v1 和 v2 共享同一份协议层和 IPC 协议**——上层业务感知不到差异
2. **运行时自动选择**：daemon 启动时探测可用后端，优先 v2，失败回 v1
3. **互斥使用 SPI0 控制器**：v2 跑时必须把 spidev overlay 卸了，反之亦然

### 8.2 daemon 自动检测逻辑

实际签名带两个 ctx 入参（让调用方 daemon 自己控制 ctx 生命周期，而不是 backend 内部 static），
其余逻辑与设计意图一致：

```c
int aqua_backend_autoselect(aqua_backend_spidev_ctx_t *spidev_ctx,
                            aqua_backend_rpmsg_ctx_t  *rpmsg_ctx,
                            const aqua_backend_ops_t **out_ops,
                            void                     **out_ctx)
{
    /* 优先 v2 OpenAMP：探测 /dev/rpmsg_ctrl0 是否可访问。
     * 注意不去探测 /dev/rpmsg0，因为编号是动态的——open 内部会扫 sysfs 找 service。 */
    if (rpmsg_ctx) {
        if (!rpmsg_ctx->ctrl_path)   rpmsg_ctx->ctrl_path   = "/dev/rpmsg_ctrl0";
        if (!rpmsg_ctx->device_path) rpmsg_ctx->device_path = "/dev/rpmsg0";
        if (access(rpmsg_ctx->ctrl_path, R_OK | W_OK) == 0) {
            if (aqua_backend_rpmsg_open(rpmsg_ctx) == 0) {
                *out_ops = &aqua_backend_rpmsg_ops; *out_ctx = rpmsg_ctx; return 0;
            }
        }
    }

    /* 回退 v1 spidev */
    if (spidev_ctx) {
        if (!spidev_ctx->device_path) spidev_ctx->device_path = "/dev/spidev0.0";
        if (access(spidev_ctx->device_path, R_OK | W_OK) == 0) {
            if (aqua_backend_spidev_open(spidev_ctx) == 0) {
                *out_ops = &aqua_backend_spidev_ops; *out_ctx = spidev_ctx; return 0;
            }
        }
    }
    return -1;
}
```

`aqua_rpmsgd` 命令行还提供 `-B {auto|rpmsg|spidev}` 三种强制模式，
便于联调时绕过 autoselect 直接走某条链路。

启动时打印：

```
[INF] aqua_rpmsgd 启动
[INF] 后端探测: rpmsg(openamp) ✓
[INF] 后端选择: rpmsg(openamp)
[INF] RA6E2 上线 (uptime=1234 ms)
```

或：

```
[INF] aqua_rpmsgd 启动
[WRN] 后端探测: rpmsg(openamp) ✗ (/dev/rpmsg0 不存在)
[INF] 后端探测: spidev ✓
[INF] 后端选择: spidev (v1 fallback)
```

### 8.3 systemd 编排（防止 v1/v2 同时启用）

实际工程提供 **3 个互斥的 service unit**，互斥靠 `Conflicts=`，**不需要额外的"互斥锁 service"**。
全部位于 `deploy/systemd/`，部署时 `cp /etc/systemd/system/`：

#### `aqua-spid.service`（v1）

```ini
[Unit]
Description=AquaGarden SPI daemon (v1: Linux spidev direct)
After=local-fs.target
Conflicts=aqua-rpmsgd.service       # 起 v1 自动停 v2

[Service]
Type=simple
ExecStartPre=/usr/bin/test -c /dev/spidev0.0
ExecStart=/opt/aqua/bin/aqua_spid -f -p 250 -g 5000
Restart=on-failure
```

#### `aqua-openamp-load.service`（v2 必需的前置步骤）

这一步在设计 v0 中只笼统说"启动 remoteproc"，实际比这复杂得多：
要在远程核 `start` 之后**手动绑定 rpmsg_chrdev** 才能拿到 `/dev/rpmsgN`。
service 内全部做了，并且**幂等**（同 firmware 已 running 时不重启）：

```ini
[Unit]
Description=Load AquaGarden remoteproc firmware (OpenAMP SPI core)

[Service]
Type=oneshot
RemainAfterExit=yes

# 1) 备份当前 firmware 名（stop 时恢复，不破坏 SDK 自带 echo demo）
ExecStartPre=/bin/sh -c 'cat /sys/class/remoteproc/remoteproc0/firmware \
                         > /run/aqua_prev_firmware.txt 2>/dev/null || true'

# 2) 幂等启动远程核（只有 state≠running 或 firmware 名不对时才 stop+start）
ExecStart=/bin/sh -c 'state=$(cat /sys/class/remoteproc/remoteproc0/state); \
                      fw=$(cat /sys/class/remoteproc/remoteproc0/firmware); \
                      [ "$state" = "running" ] && [ "$fw" = "openamp_spi_core0.elf" ] && exit 0; \
                      [ "$state" = "running" ] && echo stop > /sys/.../state; \
                      echo openamp_spi_core0.elf > /sys/.../firmware; \
                      echo start > /sys/.../state'

# 3) 等远程核 announce endpoint（最多 3 秒）
ExecStart=/bin/sh -c 'for i in 1 2 3 4 5 6; do sleep 0.5; \
                      ls /sys/bus/rpmsg/devices/virtio0.aqua-spi.* 2>/dev/null && exit 0; done; \
                      echo "[WARN] aqua-spi endpoint 在 3 秒内未出现" >&2'

# 4) ★ 关键：modprobe rpmsg_char + 写 driver_override 才会出 /dev/rpmsgN
ExecStart=/sbin/modprobe rpmsg_char
ExecStart=/bin/sh -c 'for d in /sys/bus/rpmsg/devices/virtio0.aqua-spi.*/driver_override; \
                      do [ -e "$d" ] && echo rpmsg_chrdev > "$d"; done'
ExecStart=/bin/sh -c 'for dev in /sys/bus/rpmsg/devices/virtio0.aqua-spi.*; do \
                       [ -e "$dev" ] || continue; [ -e "$dev/driver" ] && continue; \
                       echo "$(basename "$dev")" > /sys/bus/rpmsg/drivers/rpmsg_chrdev/bind; \
                      done; exit 0'
ExecStart=/usr/bin/udevadm settle

# 5) Stop 时恢复原 firmware 名（不真正 stop 远程核——
#    当前 phytium-remoteproc 驱动 stop 后无法可靠二次 start）
ExecStop=/bin/sh -c 'p=$(cat /run/aqua_prev_firmware.txt 2>/dev/null); \
                     [ -n "$p" ] && echo "$p" > /sys/.../firmware || true'
```

> **为什么必须 `modprobe rpmsg_char + driver_override`？**
> 飞腾派 BSP 的 `rpmsg_char` 模块默认不自动绑定到非 SDK 例程的 endpoint。
> 不显式 `driver_override = rpmsg_chrdev` 并 `bind`，`/dev/rpmsgN` 就不会出现，
> 进而 `aqua_rpmsgd` autoselect 会 fallback 到 spidev 或直接失败。

#### `aqua-rpmsgd.service`（v2 daemon）

```ini
[Unit]
Description=AquaGarden SPI daemon (v2: OpenAMP/RPMsg)
After=local-fs.target aqua-openamp-load.service
Requires=aqua-openamp-load.service     # systemd 自动先拉起 load
Conflicts=aqua-spid.service            # 起 v2 自动停 v1

[Service]
Type=simple
ExecStart=/opt/aqua/bin/aqua_rpmsgd -f -B auto -p 250
Restart=on-failure
```

| 部署模式            | 启用的 service                                                          |
| --------------- | -------------------------------------------------------------------- |
| **v2 OpenAMP**  | `aqua-openamp-load.service`（自动被 Requires 拉起） + `aqua-rpmsgd.service` |
| **v1 fallback** | （手动）`overlay/install_spidev_overlay.sh apply` + `aqua-spid.service`  |

### 8.4 切换脚本

实际位置在 `deploy/scripts/`，**含完善的前置检查**（DTB 是否含 reserved-memory、固件是否就位、remoteproc 是否启用等），失败会带错误码退出而不是闷头继续。

`deploy/scripts/switch_to_v2.sh` 摘要：

```bash
#!/usr/bin/env bash
set -euo pipefail
[[ ${EUID} -eq 0 ]] || { echo "需要 root"; exit 1; }

# 前置检查
[[ -f /lib/firmware/openamp_spi_core0.elf ]] || { echo "缺裸机核固件"; exit 2; }
[[ -d /sys/class/remoteproc/remoteproc0 ]]   || { echo "内核未启用 phytium-remoteproc"; exit 3; }
[[ -d /sys/firmware/devicetree/base/reserved-memory/rproc@b0100000 ]] || {
    echo "DTB 未含 reserved-memory，sudo ln -snf phytium-pi-board-v3-openamp.dtb \\
          /boot/phytium-pi-board.dtb && sudo reboot"; exit 4; }

# 1) 停 v1 + 卸载 spidev overlay（释放 SPI0 IOPad 给裸机核）
systemctl stop aqua-spid.service 2>/dev/null || true
../../overlay/install_spidev_overlay.sh remove || true

# 2) 启动远程核 + 绑 rpmsg_chrdev
systemctl restart aqua-openamp-load.service

# 3) 启动 v2 daemon
systemctl restart aqua-rpmsgd.service
```

`deploy/scripts/switch_to_v1.sh` 摘要：

```bash
#!/usr/bin/env bash
set -euo pipefail
systemctl stop aqua-rpmsgd.service       2>/dev/null || true
systemctl stop aqua-openamp-load.service 2>/dev/null || true   # 注意：不真正 stop 远程核
                                                                # 见 §8.3 ExecStop 注释
../../overlay/install_spidev_overlay.sh apply
systemctl restart aqua-spid.service
```

> **注：** 当前 phytium-remoteproc 驱动的 `stop` 后**无法可靠二次 start**
> （sysfs 显示 offline 但 PSCI 仍 already-on）。
> 所以 `switch_to_v1.sh` 只 stop systemd unit、不真正下电远程核；
> 重切回 v2 时 `aqua-openamp-load.service` 会幂等检测"已 running 同 firmware"直接 exit 0。

---

## 9. 飞腾 Standalone SDK 关键 API 对照表

> SDK 路径：`~/phytium-standalone-sdk`（用户本机）
> SDK 版本：仓库当前 HEAD（含 `openamp_for_linux` v1.3+，2025-02 更新的 start/stop framework）

### 9.1 OpenAMP / RPMsg

| API                          | 头文件               | 用途                          | 在本工程位置                    |
| ---------------------------- | ----------------- | --------------------------- | ------------------------- |
| `platform_create_proc`       | `helper.h`        | 创建 remoteproc 实例            | `aqua_spi_slave_run()`    |
| `platform_setup_src_table`   | `helper.h`        | 注册 resource table           | 同上                        |
| `platform_setup_share_mems`  | `helper.h`        | 申请共享内存                      | 同上                        |
| `platform_create_rpmsg_vdev` | `helper.h`        | 创建 RPMsg virtio device      | 同上                        |
| `rpmsg_create_ept`           | `openamp/rpmsg.h` | 注册 endpoint + name service  | 同上，service = `"aqua-spi"` |
| `rpmsg_send`                 | `openamp/rpmsg.h` | 发包到对端                       | `aqua_rpmsg_cb()`         |
| `platform_poll`              | `helper.h`        | 主循环驱动（处理 IPI / virtio）      | 主循环                       |
| `rproc_get_stop_flag`        | `helper.h`        | 检查是否被 Linux remoteproc stop | 主循环                       |

### 9.2 SPI Master (FSPIM)

| API                                                | 头文件       | 用途                   |
| -------------------------------------------------- | --------- | -------------------- |
| `FSpimLookupConfig(FSPI0_ID)`                      | `fspim.h` | 取默认配置（base_addr/irq） |
| `FSpimCfgInitialize`                               | `fspim.h` | 初始化实例                |
| `FSpimSetChipSelection(spim, on)`                  | `fspim.h` | 主动控 CS（E2000 系列）     |
| `FSpimTransferPollFifo(spim, tx, rx, len)`         | `fspim.h` | 阻塞收发，本工程主用           |
| `FSpimDeInitialize`                                | `fspim.h` | 去初始化                 |
| `FSpimSetOption(spim, FSPIM_FREQUENCY_OPTION, hz)` | `fspim.h` | 运行时改速率               |

### 9.3 IO 引脚 mux

| API                          | 头文件         | 用途                                         |
| ---------------------------- | ----------- | ------------------------------------------ |
| `FIOMuxInit()`               | `fio_mux.h` | 初始化 IOPad 控制器                              |
| `FIOPadSetSpimMux(FSPI0_ID)` | `fio_mux.h` | 一次性配好 SCK/MOSI/MISO/CSN0 复用为 SPI 功能（板级已实现） |

### 9.4 关键 SDK kconfig（v2 必启）

| KConfig                                                               | 取值           | 作用                                                            |
| --------------------------------------------------------------------- | ------------ | ------------------------------------------------------------- |
| `CONFIG_PHYTIUMPI_FIREFLY_BOARD`                                      | `y`          | 选板                                                            |
| `CONFIG_TARGET_PE2204`                                                | `y`          | 选 SoC                                                         |
| `CONFIG_ARCH_ARMV8_AARCH64`                                           | `y`          | 与 Linux 5.10 一致；4.19 内核需切 aarch32                             |
| `CONFIG_INTERRUPT_ROLE_SLAVE`                                         | `y`          | 远程核做 SGI 接收方                                                  |
| `CONFIG_USE_AMP=y` + `CONFIG_USE_LIBMETAL=y` + `CONFIG_USE_OPENAMP=y` | `y`          | 启用 OpenAMP                                                    |
| `CONFIG_USE_OPENAMP_IPI=y`                                            | `y`          | 用 SGI IPI 触发（不是轮询）                                            |
| `CONFIG_SKIP_SHBUF_IO_WRITE=y`                                        | `y`          | Linux 主核管 vring；裸机不写自己的 shbuf 区描述                             |
| `CONFIG_USE_MASTER_VRING_DEFINE=y`                                    | `y`          | vring 地址由 Linux 决定                                            |
| `CONFIG_USE_CACHE_COHERENCY=y`                                        | `y`          | 共享内存走 normal cacheable，依赖 Linux 在 reserved-memory 配 cacheable |
| `CONFIG_USE_SPI=y` + `CONFIG_USE_FSPIM=y`                             | `y`          | **新增**：启用 FSPIM 驱动                                            |
| `CONFIG_USE_IOMUX=y` + `CONFIG_ENABLE_IOPAD=y`                        | `y`          | 配 SPI 引脚复用                                                    |
| `CONFIG_IMAGE_LOAD_ADDRESS=0xb0100000`                                | `0xb0100000` | 与 SDK 模板对齐；与 SLAVE00_SHARE_MEM_ADDR (0xC000_0000) 不冲突         |

---

## 10. 部署步骤

### 10.1 飞腾派 Linux 内核要求

| Kconfig                                                            | 状态  | 备注                                       |
| ------------------------------------------------------------------ | --- | ---------------------------------------- |
| `CONFIG_REMOTEPROC=y`                                              | 必须  | 提供 `/sys/class/remoteproc/`              |
| `CONFIG_PHYTIUM_REMOTEPROC=y`                                      | 必须  | 飞腾远程核驱动（buildroot `openamp_*.config` 已含） |
| `CONFIG_RPMSG=y` + `CONFIG_RPMSG_VIRTIO=y` + `CONFIG_RPMSG_CHAR=y` | 必须  | 提供 `/dev/rpmsg_ctrl0` `/dev/rpmsg0`      |
| `CONFIG_OF_RESERVED_MEM=y`                                         | 必须  | 远程核内存窗口                                  |

> 检查方法：飞腾派上 `zcat /proc/config.gz | grep -E 'REMOTEPROC|RPMSG'`
> 如果缺，在 buildroot 用 `phytium_ubuntu_defconfig + openamp_*.config` 重编内核（参见 `hardware/phytiumpi/README.md` 中"openamp_xxx.config" 一节）。

### 10.2 设备树 reserved-memory

需要在飞腾派 DTB 中保留 `0xC000_0000 ~ 0xC0FF_FFFF`（共 16 MB）给 OpenAMP 共享区，并 reserve `0xB010_0000 ~ 0xB0FF_FFFF`（约 15 MB）给远程核固件加载。

如果出厂 DTB 没有，写一份 overlay：

```dts
/* overlay/phytium_pi_openamp.dts */
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target-path = "/reserved-memory";
        __overlay__ {
            #address-cells = <2>;
            #size-cells = <2>;
            ranges;

            openamp_firmware: openamp_firmware@b0100000 {
                reg = <0x0 0xb0100000 0x0 0x00f00000>;
                no-map;
            };
            openamp_share: openamp_share@c0000000 {
                reg = <0x0 0xc0000000 0x0 0x01000000>;
                no-map;
            };
        };
    };
};
```

通过现有 `overlay/install_spidev_overlay.sh` 同款 configfs 机制 apply（或写到 `/boot/dtb/...`）。

> **细节确认项：** 飞腾官方 OpenAMP 文档《飞腾嵌入式OpenAMP技术解决方案与用户操作手册v1.4》对 reserved-memory 节点名和 `phytium_remoteproc` 驱动 binding 有明确要求。落地时按手册微调 overlay。

### 10.3 编译远程核固件

工程的 `openamp_core/makefile` 已经做过两处适配，**不再需要** `source set_toolchain.sh` 或
`install.py`：

- `SDK_DIR ?= $(CURDIR)/../../../../../phytium-standalone-sdk` — 默认就指到用户家目录的 SDK；
- `TOOL_CHAIN_PREFIX ?= aarch64-none-elf-` — 直接用 `PATH` 里的 Linaro/官方 bare-metal 编译器
  （Ubuntu 上 `apt install gcc-aarch64-none-elf` 即可），无需 export 任何环境变量。

```bash
# 在 x86 主机（PATH 里有 aarch64-none-elf-gcc）：
cd hardware/phytiumpi/spi_com/openamp_core/

# 首次编译（sdkconfig 已 git 入库，不需要再跑 menuconfig）
make all -j$(nproc)
# → 产物 ./pe2204_aarch64_phytiumpi_openamp_spi_core0.elf （SDK 标准命名规则）

# 或者用一键打包目标（自动改名为 openamp_spi_core0.elf 拷到 /tmp/aqua_firmware/）
make image
# → /tmp/aqua_firmware/openamp_spi_core0.elf
```

> **重要：SDK 产物名 ≠ /lib/firmware/ 期望名**
> SDK makefile 输出的是 `pe2204_aarch64_phytiumpi_openamp_spi_core0.elf`，
> 而 `aqua-openamp-load.service` 加载的是 `/lib/firmware/openamp_spi_core0.elf`。
> **scp 到飞腾派时一定要改名**（`make image` / `make scp` 目标已经处理了；手动 `scp` 时记得带新名字）。

### 10.4 部署到飞腾派

推荐用 `rsync` 整树同步（保证 `deploy/scripts/` 与 `overlay/` 的相对路径关系不被破坏，
切换脚本里有 `../../overlay/install_spidev_overlay.sh` 这种相对引用）：

```bash
# x86 主机：
cd ~/AquaGarden/hardware/phytiumpi/spi_com

# 1) 交叉编译 Linux 端
make linux                  # 输出 build/linux/{aqua_spid, aqua_rpmsgd, aqua_spi_cli}

# 2) 把整树同步到飞腾派 /opt/aqua/spi_com/
rsync -av --delete \
  --exclude=build/ --exclude=openamp_core/build/ \
  ./ user@<飞腾派IP>:/opt/aqua/spi_com/

# 3) 把三个二进制单独装到 /opt/aqua/bin/
rsync -av build/linux/{aqua_spid,aqua_rpmsgd,aqua_spi_cli} \
  user@<飞腾派IP>:/opt/aqua/bin/

# 4) 把裸机核固件改名 scp 到 /lib/firmware/（注意改名！见 §10.3 警告）
scp openamp_core/pe2204_aarch64_phytiumpi_openamp_spi_core0.elf \
    user@<飞腾派IP>:/tmp/openamp_spi_core0.elf

# 5) 飞腾派上：装固件 + service + 切换到 v2
ssh user@<飞腾派IP> <<'EOF'
sudo mv /tmp/openamp_spi_core0.elf /lib/firmware/
sudo cp /opt/aqua/spi_com/deploy/systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo /opt/aqua/spi_com/deploy/scripts/switch_to_v2.sh
EOF
```

### 10.5 启停远程核（手工调试用）

#### 手动启动远程核 + 出 `/dev/rpmsgN`

实际比"echo start"多两步:必须 `modprobe rpmsg_char` 并写 `driver_override`,否则
`/dev/rpmsgN` 不会出现。`aqua-openamp-load.service` 已经把这些步骤打包,手动调试时可参考:

```bash
# 1) 加载固件并启动远程核
sudo bash -c 'echo openamp_spi_core0.elf > /sys/class/remoteproc/remoteproc0/firmware'
sudo bash -c 'echo start > /sys/class/remoteproc/remoteproc0/state'

# 应在 dmesg 看到：
#   remoteproc remoteproc0: powering up phytium-remoteproc
#   remoteproc remoteproc0: Booting fw image openamp_spi_core0.elf, size XXXX
#   virtio_rpmsg_bus virtio0: creating channel aqua-spi addr 0x...

# UART1 (ttyAMA1, 115200 8N1) 应输出：
#   AquaGarden OpenAMP SPI core, build ...
#   [I] AQUA_SPI    endpoint 创建成功，等待 Linux 端连接

# 2) ★ 关键：让 rpmsg_char 接管 aqua-spi 端点（不做这步 /dev/rpmsgN 永远不出现）
sudo modprobe rpmsg_char
for d in /sys/bus/rpmsg/devices/virtio0.aqua-spi.*/driver_override; do
    sudo bash -c "echo rpmsg_chrdev > $d"
done
for dev in /sys/bus/rpmsg/devices/virtio0.aqua-spi.*; do
    [ -e "$dev/driver" ] && continue
    sudo bash -c "echo $(basename $dev) > /sys/bus/rpmsg/drivers/rpmsg_chrdev/bind"
done
sudo udevadm settle

# 3) 验证
ls /dev/rpmsg*               # 应出现 /dev/rpmsgN（N 由内核分配，不一定是 0）
ls /sys/class/rpmsg/         # 同上；cat .../name 应能看到 "aqua-spi"
```

#### 停（用得很少；当前驱动 stop 后无法可靠二次 start）

```bash
sudo bash -c 'echo stop > /sys/class/remoteproc/remoteproc0/state'
```

> **当前 BSP 限制：** `phytium-remoteproc` 驱动 `stop` 后，sysfs `state` 显示 `offline`，
> 但底层 PSCI 的 CPU 仍是 already-on 状态，再次 `start` 会失败。
> 所以 `aqua-openamp-load.service` 是**只 start 不 stop** 的设计——把 firmware 名字改回去
> 就当 stop 了，等下次重启系统再实际下电。手动调试也尽量不要 stop。

### 10.6 联调流程

```bash
# 1. 启 v2 daemon（前台 + verbose；注意默认走 -B auto 自动探测后端）
sudo /opt/aqua/bin/aqua_rpmsgd -f -v -B rpmsg     # -B rpmsg 强制走 v2 不退回

# 期待日志（前台模式走 stderr）：
#   [INF] 已打开 RPMsg endpoint 'aqua-spi' via /dev/rpmsg0
#   [INF] aqua_rpmsgd 启动：backend=rpmsg 周期=250 ms
#   [INF] 监听 /tmp/aqua_spi.sock
#   [INF] RA6E2 上线，uptime=1234 ms

# 2. 另开终端，用 CLI（与 v1 完全一样，0 修改）
/opt/aqua/bin/aqua_spi_cli ping              # 检查 daemon 活着
/opt/aqua/bin/aqua_spi_cli sys ping          # 心跳 RA6E2
/opt/aqua/bin/aqua_spi_cli snapshot          # 取缓存的传感器快照
/opt/aqua/bin/aqua_spi_cli sensor poll       # 强制走一次 SPI 拉数据
/opt/aqua/bin/aqua_spi_cli pump start 80     # 启泵 80%
/opt/aqua/bin/aqua_spi_cli stats             # 看通信统计（spidev_io_err=后端 IO 错累计）
```

---

## 11. 决策记录（v2）

> 在 v1 已有的 9 个决策（Q1..Q9）基础上追加。

| ID  | 问题                        | 选择                                                                     | 理由                                                                    |
| --- | ------------------------- | ---------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Q10 | 是否真要按任务文档实现 OpenAMP？      | **是**（用户决定）                                                            | 任务文档明确要求 OpenAMP 路径；即便 v1 已工作也要给出符合任务定义的 v2                           |
| Q11 | v1 是否废弃？                  | **保留为 fallback**                                                       | 已稳定工作；v2 发布初期可能有未知问题；运行时自动检测后端                                        |
| Q12 | 远程核运行时                    | **纯裸机（baremetal）**                                                     | 任务文档原话；FreeRTOS 引入额外调度复杂度，无收益（裸机核任务单一）                                |
| Q13 | 远程核加载方式                   | **Linux remoteproc**（不是 U-Boot bootelf）                                | 用户选；运行时可热启停，便于调试与升级；与 SDK `openamp_for_linux` 例程方向一致                  |
| Q14 | RPMsg 包大小 vs SPI 帧        | **1 RPMsg 包 = 1 SPI 帧 (64 B)**                                         | RPMsg vring buffer 默认 512 B 远超 64 B，永不分片；语义清晰                         |
| Q15 | CMD + NOP_READ 两次事务在哪一端发？ | **裸机核内部完成**                                                            | 一次 RPMsg 往返抵两次 SPI 事务，对 Linux 透明；避免 RPMsg 延迟翻倍                        |
| Q16 | 裸机核是否解析协议字段               | **不解析，纯透传**                                                            | 业务命令新增时无需重烧裸机固件；与"任意业务进程通过共享 daemon 访问"目标一致                           |
| Q17 | FSPIM 传输方式                | **POLL（首发）**                                                           | 64 B / 1 MHz 仅 0.5 ms，poll 阻塞代价可忽略；裸机核反正没别的事干。后续 INTERRUPT/DMA 升级路径预留 |
| Q18 | RPMsg endpoint 服务名        | **`"aqua-spi"`**                                                       | 与 SDK 例程默认服务名不冲突；命名清晰                                                 |
| Q19 | v1/v2 切换粒度                | **每次部署选其一**（互斥）                                                        | 同一 SPI0 控制器不能同时被 spidev 内核驱动和裸机核占用；用 systemd Conflicts 强制互斥           |
| Q20 | Linux 端代码组织               | **抽 `aqua_backend_ops_t` 接口**                                          | v1/v2 daemon 共享 95% 代码；上层 IPC + CLI 完全不动                              |
| Q21 | RPMsg 设备节点编号              | **不写死 `/dev/rpmsg0`，按 service name 扫描 `/sys/class/rpmsg/`**            | 系统里其它 RPMsg 服务（如 SDK echo demo）会改变编号；扫描法 robust                       |
| Q22 | 裸机核 `rpmsg_send` 用什么缓冲    | **本端 alloc 的 `s_rsp_frame`**，不直接用回调入参 `data`                           | `data` 是 vring RX buffer；rpmsg_send 内部走 TX vring，复用同块内存会污染 RX         |
| Q23 | SPI 出错时怎么应对 Linux 端       | **裸机核回 STATUS=BUSY 伪 RSP**（非"什么都不发"）                                   | Linux 端 `read(rpmsg_fd)` 设了 100 ms poll 超时；不发包会让 daemon 累计 timeout    |
| Q24 | 远程核 stop 后能不能马上 start     | **不能。** 当前 phytium-remoteproc 驱动有 BSP 级 bug                            | `aqua-openamp-load.service` 用幂等检测代替 stop/start；stop 时只恢复 firmware 名   |
| Q25 | 出 `/dev/rpmsgN` 需要做什么     | **必须 `modprobe rpmsg_char` + 写 `driver_override=rpmsg_chrdev` + bind** | BSP 默认不自动绑定。`aqua-openamp-load.service` ExecStart 已包含                 |

---

## 附录 A：内存布局

| 范围                              | 用途                        | 来源                                                           |
| ------------------------------- | ------------------------- | ------------------------------------------------------------ |
| `0x8000_0000 ~ 0xAFFF_FFFF`     | Linux 主核内核 / 用户态（512 MB-） | 内核默认                                                         |
| **`0xB010_0000 ~ 0xB0FF_FFFF`** | **裸机固件 image 区**（约 15 MB） | `CONFIG_IMAGE_LOAD_ADDRESS=0xb0100000`（与 SDK PHYTIUMPI 模板对齐） |
| **`0xC000_0000 ~ 0xC0FF_FFFF`** | **OpenAMP 共享内存区**（16 MB）  | `SLAVE00_SHARE_MEM_ADDR`（与 SDK 例程对齐）                         |
| `0xC022_4000`                   | Kick / IPI mailbox 寄存器影子区 | `SLAVE00_KICK_IO_ADDR`                                       |
| `0x2803_A000`                   | FSPI0 控制器寄存器（裸机核独占）       | `FSPI0_BASE_ADDR`                                            |
| `9`（SGI 编号）                     | Linux ↔ 裸机核 IPI           | `SLAVE_00_SGI = KICK_SGI_NUM_9`（`common/libmetal_configs.h`） |

> **关于 SGI 编号：** 早期 SDK 例程文档曾写 `0x2F (47)`，但当前 standalone-sdk 的
> `openamp_for_linux` 例程统一用 SGI 9（`KICK_SGI_NUM_9`），飞腾官方
> `phytium-pi-board-v3-openamp.dtb` 的 `phytium-remoteproc` 节点也按 9 配置。
> **本工程跟 SDK 例程保持一致用 9**。如果你的 DTB 用了别的 SGI 号，
> 联调时表现是"远程核启动正常但 Linux 收不到任何 RPMsg 回包"——
> 改 `common/libmetal_configs.h::SLAVE_00_SGI` 重编固件即可。
> 
> 实际地址必须与飞腾派 device-tree reserved-memory 一致，详见 §10.2。

---

## 附录 B：参考文件索引

| 文件                                                                                         | 用途                                         |
| ------------------------------------------------------------------------------------------ | ------------------------------------------ |
| `任务_功能实现_SPI通信.md`                                                                         | 顶层任务定义（v1/v2 共同遵循）                         |
| `设计_SPI通信工程方案.md`                                                                          | v1 spidev 直驱方案（已落地）                        |
| **本文档**                                                                                    | v2 OpenAMP 方案                              |
| `设计_SPI通信工程方案_v0_RPMsg版.md.bak`                                                            | 早期 v0 RPMsg 设想（与 v2 思路相近，但裸机核细节未对齐 SDK）    |
| `~/phytium-standalone-sdk/example/system/amp/openamp_for_linux/`                           | **直接对标的 SDK 例程**——v2 远程核基于此改造              |
| `~/phytium-standalone-sdk/example/peripherals/spi/src/spim_polled_loopback_mode_example.c` | FSPIM polling 模式参考代码                       |
| `~/phytium-standalone-sdk/board/phytiumpi_firefly/fio_mux.c`                               | 板级 SPI IOPad 配置实现                          |
| `~/phytium-standalone-sdk/soc/pe220x/fparameters_comm.h`                                   | FSPI0..3 基地址 / IRQ                         |
| `~/phytium-standalone-sdk/example/system/amp/README.md`                                    | 多元异构（MSDF）部署框架说明                           |
| `hardware/phytiumpi/openamp/demo/rpmsg-demo-single.c`                                      | Linux 端 RPMsg 客户端参考实现                      |
| `hardware/phytiumpi/spi0_scope_demo/spi_scope_demo.c`                                      | SPI 物理链路验证（v1 也用）                          |
| `hardware/demo_wyr/FreeRTOS drive/data_merge/ra_gen/Communicate_Task.c`                    | RA6E2 SPI Slave + DMAC 现有 FSP 配置（v1/v2 共用） |

---

## 附录 C：v1 → v2 路径切换检查表（运维 Cheat Sheet）

切到 v2 前确认：

- [ ] `~/phytium-standalone-sdk` 已就位且能编 `openamp_for_linux` 例程

- [ ] 飞腾派内核 `zcat /proc/config.gz | grep -E 'REMOTEPROC|RPMSG'` 全 `=y`

- [ ] **DTB 已切到 OpenAMP 版**（验证：`ls /sys/firmware/devicetree/base/reserved-memory/rproc@b0100000`）
  
      没切的话：`sudo ln -snf phytium-pi-board-v3-openamp.dtb /boot/phytium-pi-board.dtb && sudo reboot`

- [ ] `/lib/firmware/openamp_spi_core0.elf` 已部署（注意 SDK 产物名不同，scp 时务必改名 — 见 §10.3）

- [ ] **spidev overlay 已 remove**（`overlay/install_spidev_overlay.sh status` 应为未挂载）

- [ ] `/opt/aqua/bin/aqua_rpmsgd` 与 `/opt/aqua/bin/aqua_spi_cli` 已部署并可执行

- [ ] 三个 service 已 `cp /etc/systemd/system/` + `systemctl daemon-reload`

切到 v2:`sudo /opt/aqua/spi_com/deploy/scripts/switch_to_v2.sh`(脚本内会再次自动检查所有前置条件)。

切回 v1:`sudo /opt/aqua/spi_com/deploy/scripts/switch_to_v1.sh`。

---

## 附录 D：实施进度备忘(2026-05 更新)

设计 → 实现 → 联调三阶段都已完成,本节做最后一次状态对账,后续维护者可从这里入手:

| 阶段  | 工作项                                                       | 状态       | 落地位置                                                         |
| --- | --------------------------------------------------------- | -------- | ------------------------------------------------------------ |
| 实现  | 抽 `aqua_backend.h` 接口,v1 daemon 拆成 backend + core         | ✅        | `linux/aqua_backend*.{h,c}` + `linux/aqua_daemon_core.{h,c}` |
| 实现  | 新增 v2 daemon `aqua_rpmsgd`(`-B auto/rpmsg/spidev`)        | ✅        | `linux/aqua_rpmsgd.c`                                        |
| 实现  | 裸机核工程 `openamp_core/`(基于 SDK echo 例程改)                    | ✅        | `openamp_core/{main.c, src/, common/, configs/}`             |
| 实现  | x86 主机交叉编译路径(`SDK_DIR` 自动识别 + PATH 工具链)                   | ✅        | `openamp_core/makefile`                                      |
| 部署  | `systemd` 三件套 + Conflicts 互斥                              | ✅        | `deploy/systemd/`                                            |
| 部署  | v2 reserved-memory overlay(fallback,优先用官方 v3-openamp DTB) | ✅        | `deploy/overlay/`                                            |
| 部署  | 一键切换脚本(含前置检查)                                             | ✅        | `deploy/scripts/switch_to_v{1,2}.sh`                         |
| 文档  | 顶层 README 加 v1/v2 章节                                      | ✅        | `README.md`                                                  |
| 文档  | x86 编译交接(`HANDOFF_x86_build.md`)                          | ✅        | 工程根目录                                                        |
| 文档  | 飞腾派开机准备 + 日常使用流程                                          | ✅        | `使用说明.md`                                                    |
| 联调  | 全链路(Linux daemon → 裸机核 → SPI → RA6E2)                     | ⏳ 待飞腾派部署 | —                                                            |

> **下一步**(已超出本设计文档范围,详见 `使用说明.md`):
> 飞腾派开机后按"开机准备清单"过一遍 → `switch_to_v2.sh` → CLI 验证。
