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
│ │  │ - 独占 /dev/rpmsg0（v2 路径）                             │    │ │
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
                              │ SPI Bus (Mode 0, 1 MHz, 8-bit, MSB)
                              │ 64B CMD / 64B RSP（CMD + NOP_READ 两次事务）
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│ RA6E2（FreeRTOS + Renesas FSP，SPI1 Slave + 双 DMAC）                │
│   Communicate_Task → app_dispatch → PUMP / SENSOR / LINKAGE         │
│   （与 v1 完全相同；无任何代码改动）                                 │
└─────────────────────────────────────────────────────────────────────┘
```

### 0.1 三层职责

| 层 | 跑在哪 | 是否解析业务语义 | 修改方式 | 重新部署成本 |
|----|--------|-----------------|---------|-------------|
| 业务进程 / CLI | Linux 用户态 | 是（最终决策者） | 改 Python/C 代码 | 重启进程，秒级 |
| `aqua_rpmsgd` | Linux 用户态 | 仅做 IPC ↔ RPMsg 转发 + CMD 帧打包 + RSP 帧解包 | 改 C 代码 | systemd 重启，秒级 |
| 裸机从核固件 `openamp_spi_core0.elf` | 飞腾派 core0 | **否**（仅做 RPMsg ↔ SPI 字节透传 + 时序控制） | 改 SDK 例程 + 重新编译 | 替换 `/lib/firmware/openamp_spi_core0.elf` + remoteproc restart，分钟级 |
| RA6E2 Communicate_Task | RA6E2 大核 | 是（最终执行 + 状态机） | 改 e2studio 工程 | OTA / J-Link 重烧，分钟级 |

> **设计原则：** 裸机核固件**对协议语义无感知**，只搬字节。任何业务命令的新增 / 修改都不需要重烧裸机核固件 —— 只改 Linux 端 + RA6E2 端。这是把"实时 + 物理控制"和"业务逻辑 + 调试便利"同时做到的关键。

### 0.2 与 v1 的关系

| 维度 | v1 | v2 (本文档) |
|------|----|-----------|
| SPI Master 实现位置 | Linux spidev 内核驱动 | 飞腾派裸机核 fspim 驱动 |
| 调度抖动来源 | Linux 用户态 → ioctl → 内核 → 控制器；250 ms 周期下 < 10 µs | 裸机核（无抢占，无中断风暴）；< 1 µs |
| 调试难度 | 低（`spi_scope_demo.c` 直接验证） | 中（需 console + remoteproc 日志） |
| 任务文档符合度 | ✗（任务文档明确要求 OpenAMP） | ✅ |
| RA6E2 端固件 | 完全一致 | 完全一致（**0 修改**） |
| 协议字节流 | v1 协议（SPI_PROTO_VERSION=0x01） | v1 协议（SPI_PROTO_VERSION=0x01，**100% 兼容**） |
| Linux 端 daemon | `aqua_spid`（持有 spidev） | `aqua_rpmsgd`（持有 rpmsg），共享同一份 `aqua_ipc.h` 协议 |
| 客户端 / CLI | 不需要任何改动（连同一个 `/tmp/aqua_spi.sock`） | 不需要任何改动 |

**关键不变量：**
- `include/spi_protocol.h` 完全复用，**v2 不引入任何协议层改动**
- `linux/libaqua_spi/spi_codec.{h,c}`（CRC + 帧打包/校验）完全复用
- `linux/libaqua_spi/aqua_ipc.h`（IPC 协议）完全复用
- `linux/aqua_spi_cli.c`（CLI）完全不需要改

---

## 1. SPI 硬件配置

### 1.1 控制器选型

飞腾派（PE2204）SoC 内有 4 个独立 SPI Master 控制器（FSPI0..FSPI3，参见 SDK `soc/pe220x/fparameters_comm.h`）：

| ID | 基地址 | IRQ | 引脚（飞腾派排针） | 用途 |
|----|--------|-----|-------------------|------|
| **FSPI0** | `0x2803A000` | **191** | **SPI0_SCK / MOSI / MISO / CSN0**（板载已 IOPad mux） | **本方案使用** |
| FSPI1 | `0x2803B000` | 192 | 未引出到排针 | — |
| FSPI2 | `0x2803C000` | 193 | 未引出到排针 | — |
| FSPI3 | `0x2803D000` | 194 | 未引出到排针 | — |

**选 FSPI0 的理由：**
1. **就是 v1 用的同一个控制器**——飞腾派 spidev 节点 `/dev/spidev0.0` 就是 FSPI0 (`0x2803A000`)。物理走线完全复用，不需要重新接线
2. SDK 板级 `board/phytiumpi_firefly/fio_mux.c::FIOPadSetSpimMux(FSPI0_ID)` 已经把 SCLK/TXD/RXD/CSN0 的 IOPad 复用配置好（FUNC2），调用一次即可
3. 其他 SPI 控制器在飞腾派板上未引出，配置成本高且无法验证

### 1.2 配置参数表

| 配置项 | 选择 | 理由 |
|-------|------|------|
| SPI 模式 | **Mode 0**（CPOL=0, CPHA=0） | 与 v1 一致；RA6E2 FSP 默认即此模式；裸机核 FSPIM 用 `FSPIM_CPOL_LOW` + `FSPIM_CPHA_1_EDGE` 配置 |
| 通信速率 | **1 MHz**（首发），可调 4 MHz | 64 字节单帧 0.5 ms，对 250 ms 周期空载 < 1%；飞腾派排针无屏蔽，1 MHz 给信号完整性留够裕度。SDK `FSPI_DEFAULT_SCLK = 5 MHz` 太高，必须显式设 1 MHz |
| 数据位宽 | **8 bit**（`FSPIM_1_BYTE`） | 协议按字节流设计；与 RA6E2 双 DMAC 1-Byte 配置一致；避免 16-bit 模式的 endian/对齐 bug |
| 传输方式 | **Polling（`TRANS_WAY_POLL`）→ Interrupt 升级路径** | v2 首发用 polling 简化裸机核（64 B / 1 MHz = 0.5 ms 阻塞，对 250 ms 周期可忽略）；后续如要降低 CPU 占用，改 `TRANS_WAY_INTERRUPT`，回调里给信号量再唤醒 RPMsg 端口任务 |
| DMA | **不使用**（v2 首发） | 64 字节单帧太短，DMAC 描述符建立开销 > 数据搬运时间。v3 如要并发处理多 client，可启 `TRANS_WAY_DDMA` |
| CS 管理 | **裸机核显式 `FSpimSetChipSelection(spim, TRUE/FALSE)`**（每帧一次） | 与 v1 spidev `SPI_IOC_MESSAGE` 自动管理 CS 等价；CS 上升沿是 RA6E2 DMAC "帧结束 + 复位"信号，是抗错位最关键的一招 |
| 字节序 | **小端 LE** | 飞腾 ARMv8 + RA6E2 ARMv7 默认皆 LE；不做转换 |
| 位序 | **MSB First** | 行业默认；RA6E2 已配 `SPI_BIT_ORDER_MSB_FIRST` |

### 1.3 引脚映射（与 v1 完全一致）

| 信号 | 飞腾派排针 | 裸机核 FIOPad（SDK 已实现） | RA6E2 |
|-----|-----------|---------------------------|-------|
| SCK  | `SPI0_SCK`  | `FIOPAD_W55_REG0_OFFSET` FUNC2 | P102 (RSPCK1) |
| MOSI | `SPI0_MOSI` | `FIOPAD_W53_REG0_OFFSET` FUNC2 | P101 (MOSI1) |
| MISO | `SPI0_MISO` | `FIOPAD_U55_REG0_OFFSET` FUNC2 | P100 (MISO1) |
| CS   | `SPI0_CSN0` | `FIOPAD_U53_REG0_OFFSET` FUNC2 | P103 (SSL10) |
| GND  | 任一 GND     | — | GND |

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
    cfg.cpol         = FSPIM_CPOL_LOW;             /* Mode 0 */
    cfg.cpha         = FSPIM_CPHA_1_EDGE;
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

| 服务名 (`RPMSG_SERVICE_NAME`) | 用途 |
|-----------------------------|------|
| `"aqua-spi"` | AquaGarden SPI 透传通道（Linux ↔ 裸机核） |

> Linux 端通过 `RPMSG_CREATE_EPT_IOCTL` 用同名 endpoint 名 `"aqua-spi"` 与裸机核协商。
> 与 SDK 自带例程的 `RPMSG_SERVICE_NAME` 不同名，避免冲突。

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

| DEV ID | 名称 | CMD ID 范围 | 主要命令 |
|--------|------|------------|---------|
| `0x00` | `SPI_DEV_SYSTEM` | `0x00..0x05` | NOP, PING, GET_VERSION, GET_UPTIME, RESET_ALARM, HELLO |
| `0x01` | `SPI_DEV_PUMP` | `0x01..0x20` | START, STOP, SET_PWM, SET_AUTO/MANUAL, SET_CYCLE_CFG/CTRL, GET_STATUS |
| `0x02` | `SPI_DEV_SENSOR` | `0x10..0x14` | POLL_ALL, POLL_AIR/WATER/SOIL/PRESSURE |
| `0x03` | `SPI_DEV_LINKAGE` | `0x01..0x02` | SET_SOIL_CFG, SET_RULE |
| `0x04..0xEF` | 预留 | — | 未来扩展 |
| `0xF0..0xFF` | 厂商自定义 | — | OEM 扩展 |

> **重要：** v2 不允许在文档里另立命令编号。任何新增 DEV/CMD 必须改 `include/spi_protocol.h`，然后 `bash ra6e2_patch/sync_from_canonical.sh` 同步到 RA6E2。裸机核固件**不需要重烧**——它对 DEV/CMD 完全无感知，只搬 64 字节。

---

## 4. 通信流程时序

### 4.1 总体节拍

| 节拍 | 周期 / 阈值 | 说明 |
|------|-----------|------|
| 主机周期 SENSOR_POLL_ALL | 250 ms | 与 RA6E2 `Communicate_Task` 节拍对齐；与 v1 一致 |
| RPMsg 单次往返延迟 | < 1 ms（aarch64 + IPI） | libmetal 实测，不含 SPI 物理传输 |
| SPI CMD 帧 | 64 B / 1 MHz = 512 µs + CS 切换 | 与 v1 一致 |
| CMD ↔ NOP 间隔 | **5 ms** | 留给 RA6E2 ISR + memcpy 装 RSP |
| Linux 端 IPC SEND_CMD 总耗 | < 7 ms | RPMsg 1 ms + SPI 6 ms |
| 单命令 CPU 占用（裸机核）| 占用 6 ms / 250 ms ≈ 2.4% | 周期负载 < 3% |

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

| 阶段 | 时间 |
|------|------|
| Linux IPC accept + 包打包 | ~50 µs |
| RPMsg write → SGI → 裸机回调 | ~500 µs |
| 裸机核 SPI 事务 #1（CMD） | 512 µs SCLK + 5 µs CS |
| 等 RA6E2 装好 RSP | 5 ms |
| 裸机核 SPI 事务 #2（NOP_READ） | 512 µs SCLK + 5 µs CS |
| RPMsg send → SGI → Linux 唤醒 | ~500 µs |
| daemon 解析 RSP + 填 IPC | ~100 µs |
| **单命令端到端** | **~7 ms** |
| 周期 250 ms 利用率 | < 3% |

### 4.4 主机端状态机（aqua_rpmsgd 主循环，伪码）

```
启动:
    if (探测 /sys/class/remoteproc/remoteproc0 存在 &&
        /lib/firmware/openamp_spi_core0.elf 存在):
        启动 remoteproc → 等待 /dev/rpmsg0 出现（最多 5 s）
        模式 = "v2 OpenAMP"
        rpmsg_open()
    else:
        卸载 spidev overlay 反向 apply（如果 user 装过）
        spidev_open(/dev/spidev0.0)
        模式 = "v1 fallback"

    listen_socket(/tmp/aqua_spi.sock)
    SYS_HELLO + SYS_PING （5 s 内重试至 STATUS=OK）

主循环（poll, 唤醒源 = 周期定时 / 客户端连接 / 信号）:
    if 周期到:
        send_cmd(SENSOR, POLL_ALL, ...) → 缓存 SensorData
    if 客户端连接:
        accept → ipc_handle() → close
    if 信号:
        rpmsg_close() / spidev_close()
        退出

send_cmd(dev, cmd, payload, len, flags):     # 后端无关
    spi_pack_cmd(frame_64B, ++seq, dev, cmd, payload, len, flags)
    if 模式 == "v2":
        write(rpmsg_fd, frame_64B, 64)
        read(rpmsg_fd, rsp_64B, 64)          # 阻塞等裸机核回包
    else:
        ioctl(spidev_fd, SPI_IOC_MESSAGE, &xfer{tx=frame_64B, rx=null})
        nanosleep(5 ms)
        ioctl(spidev_fd, SPI_IOC_MESSAGE, &xfer{tx=NOP, rx=rsp_64B})
    spi_validate_frame(rsp_64B) → 返回业务结果
```

### 4.5 裸机核端状态机（aqua_dispatch_loop，伪码）

```
init:
    init_system()
    aqua_spi_init()
    platform_create_proc(slave_priv, kick_dev)
    platform_setup_src_table()
    platform_setup_share_mems()
    rpdev = platform_create_rpmsg_vdev()
    rpmsg_create_ept(&lept, rpdev, "aqua-spi", 0, RPMSG_ADDR_ANY,
                     aqua_rpmsg_cb, aqua_rpmsg_unbind)

aqua_rpmsg_cb(ept, data, len, src, priv):
    if (len != 64) return RPMSG_SUCCESS    # 协议不识别，忽略
    memcpy(g_cmd_frame, data, 64)
    aqua_spi_xfer_64(g_cmd_frame, g_dummy_rx)  # 第一次：发 CMD，丢 MISO
    fsleep_microsec(5000)                       # 5 ms 让 RA6E2 装 RSP
    spi_pack_cmd(g_nop_frame, 0xFF, SPI_DEV_SYSTEM, SPI_CMD_SYS_NOP, NULL, 0, 0)
    aqua_spi_xfer_64(g_nop_frame, g_rsp_frame) # 第二次：发 NOP，取 RSP
    rpmsg_send(ept, g_rsp_frame, 64)            # 透传回 Linux
    return RPMSG_SUCCESS

main_loop:
    while !shutdown_req:
        platform_poll(remoteproc)              # 收 IPI、跑回调
        if rproc_get_stop_flag(): break
    rpmsg_destroy_ept(&lept)
    platform_cleanup(remoteproc)
    FPsciCpuOff()
```

> **关键：** 整个裸机核固件不识别 DEV/CMD/STATUS 任何字段。它只看 `len == 64`，看其他都是黑盒。这就是"任何业务命令新增不需要重烧裸机固件"的来源。

---

## 5. 双端代码框架

### 5.1 v2 目录结构（增量，不破坏 v1）

```
hardware/phytiumpi/spi_com/
├── 任务_功能实现_SPI通信.md           # 不变
├── 设计_SPI通信工程方案.md            # v1（spidev 直驱版本，已落地）
├── 设计_SPI通信工程方案_v2_OpenAMP版.md   # 本文档
├── 设计_SPI通信工程方案_v0_RPMsg版.md.bak # 历史早期方案
├── README.md                          # 顶部增加 v1/v2 切换章节
├── Makefile                           # 增 openamp_core / native rpmsgd 目标
│
├── include/                           # 协议层（完全复用，0 修改）
│   └── spi_protocol.h
│
├── linux/                             # Linux 端
│   ├── libaqua_spi/
│   │   ├── spi_codec.{h,c}            # 复用
│   │   └── aqua_ipc.h                 # 复用
│   ├── aqua_spid.c                    # v1 spidev daemon（保留作 fallback）
│   ├── aqua_rpmsgd.c                  # v2 rpmsg daemon（新增）
│   ├── aqua_backend.h                 # 新增：抽象 spi_send_cmd() 后端接口
│   ├── aqua_backend_spidev.c          # v1 后端实现
│   ├── aqua_backend_rpmsg.c           # v2 后端实现
│   └── aqua_spi_cli.c                 # 完全复用，0 修改
│
├── tests/test_spi_protocol.c          # 完全复用
│
├── ra6e2_patch/                       # 完全复用，RA6E2 端 0 修改
│
├── overlay/                           # v1 spidev overlay（v2 部署时必须 remove）
│
└── openamp_core/                      # ★ v2 新增：飞腾派裸机核工程
    ├── README.md                      # 编译/部署/调试步骤
    ├── main.c                         # 仅调 aqua_spi_slave_run()
    ├── makefile                       # 复用 SDK 标准 makefile
    ├── ft_openamp.ld                  # 复用 SDK 链接脚本
    ├── sdkconfig                      # 默认走 PHYTIUMPI aarch64 配置
    ├── Kconfig
    ├── configs/
    │   └── pe2204_aarch64_phytiumpi_aquaspi_core0.config
    │      （基于 SDK 自带 pe2204_aarch64_phytiumpi_openamp_core0.config 改）
    ├── common/
    │   ├── memory_layout.h            # 与 SDK 例程一致：0xC000_0000 共享区
    │   ├── openamp_configs.h
    │   └── libmetal_configs.h
    ├── inc/
    │   └── aqua_spi_slave.h
    └── src/
        ├── aqua_spi_slave.c           # RPMsg endpoint + dispatch loop
        ├── aqua_spi_master.c          # FSPIM Master 初始化 + 64B 单帧收发
        └── aqua_spi_master.h
```

### 5.2 共享协议头（0 修改，完全复用 v1）

参考 v1 设计文档 §5.3。`spi_protocol.h` / `spi_codec.h`/`.c` 在 v2 下原封不动 include，**裸机核也直接用同一份 `include/spi_protocol.h`**（通过 makefile 加 `-I`）。

### 5.3 Linux 端：抽象后端接口

新增 `linux/aqua_backend.h`：

```c
#ifndef AQUA_BACKEND_H
#define AQUA_BACKEND_H

#include <stdint.h>
#include "spi_protocol.h"

typedef enum
{
    AQUA_BACKEND_NONE = 0,
    AQUA_BACKEND_RPMSG,    /* v2: /dev/rpmsg0 */
    AQUA_BACKEND_SPIDEV,   /* v1: /dev/spidev0.0 */
} aqua_backend_kind_t;

typedef struct aqua_backend_ops
{
    int  (*open)(void *ctx);
    int  (*close)(void *ctx);

    /* 发一帧 64 B CMD，阻塞收一帧 64 B RSP；返回 0 = OK */
    int  (*xfer)(void *ctx,
                 const uint8_t cmd_frame[SPI_FRAME_LEN],
                 uint8_t       rsp_frame[SPI_FRAME_LEN]);

    aqua_backend_kind_t kind;
    const char         *name;
} aqua_backend_ops_t;

extern const aqua_backend_ops_t aqua_backend_rpmsg;
extern const aqua_backend_ops_t aqua_backend_spidev;

/* 自动检测：优先 rpmsg；探测失败回退 spidev；返回 NULL 表示彻底无后端 */
const aqua_backend_ops_t *aqua_backend_autoselect(void **out_ctx);

#endif
```

实现思路：

- `aqua_backend_spidev.c`：把现有 `aqua_spid.c` 里 spidev 相关代码抽出来，包成 `xfer()` 内部做 CMD + NOP_READ
- `aqua_backend_rpmsg.c`：用 `/dev/rpmsg_ctrl0` + `RPMSG_CREATE_EPT_IOCTL` + `/dev/rpmsg0` 收发，**单 write 一次 64 B + 单 read 一次 64 B**（裸机核负责双 SPI 事务）
- `aqua_rpmsgd.c` 与 `aqua_spid.c`：精简到只剩主循环 + IPC，所有 SPI 收发走 `aqua_backend_ops_t`

### 5.4 v2 后端实现（`aqua_backend_rpmsg.c` 关键片段）

```c
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/rpmsg.h>
#include "aqua_backend.h"

#define AQUA_RPMSG_SERVICE   "aqua-spi"
#define AQUA_RPMSG_CTRL      "/dev/rpmsg_ctrl0"
#define AQUA_RPMSG_DEV       "/dev/rpmsg0"

typedef struct
{
    int ctrl_fd;
    int rpmsg_fd;
} aqua_rpmsg_ctx_t;

static int rpmsg_open(void *_ctx)
{
    aqua_rpmsg_ctx_t *ctx = _ctx;
    struct rpmsg_endpoint_info ept = {0};

    ctx->ctrl_fd = open(AQUA_RPMSG_CTRL, O_RDWR);
    if (ctx->ctrl_fd < 0) return -1;

    snprintf(ept.name, sizeof(ept.name), "%s", AQUA_RPMSG_SERVICE);
    ept.src = 0;
    ept.dst = 0xFFFFFFFF;
    if (ioctl(ctx->ctrl_fd, RPMSG_CREATE_EPT_IOCTL, &ept) < 0)
        goto err_ctrl;

    ctx->rpmsg_fd = open(AQUA_RPMSG_DEV, O_RDWR);
    if (ctx->rpmsg_fd < 0) goto err_ctrl;
    return 0;

err_ctrl:
    close(ctx->ctrl_fd);
    return -1;
}

static int rpmsg_xfer(void *_ctx,
                      const uint8_t cmd[SPI_FRAME_LEN],
                      uint8_t       rsp[SPI_FRAME_LEN])
{
    aqua_rpmsg_ctx_t *ctx = _ctx;
    ssize_t n;

    n = write(ctx->rpmsg_fd, cmd, SPI_FRAME_LEN);
    if (n != SPI_FRAME_LEN) return -1;

    n = read(ctx->rpmsg_fd, rsp, SPI_FRAME_LEN);   /* 阻塞，等裸机核回 */
    if (n != SPI_FRAME_LEN) return -2;
    return 0;
}

const aqua_backend_ops_t aqua_backend_rpmsg = {
    .open  = rpmsg_open,
    .close = rpmsg_close,
    .xfer  = rpmsg_xfer,
    .kind  = AQUA_BACKEND_RPMSG,
    .name  = "rpmsg(openamp)",
};
```

### 5.5 裸机核端：`openamp_core/src/aqua_spi_slave.c` 关键片段

骨架来自 SDK `example/system/amp/openamp_for_linux/src/slaver_00_example.c`，**保留资源表 / 共享内存 / kick driver 全部不动**，只替换 endpoint 回调与服务名：

```c
#include <stdio.h>
#include <openamp/open_amp.h>
#include <metal/alloc.h>
#include <metal/sleep.h>
#include "platform_info.h"
#include "rpmsg_service.h"
#include "rsc_table.h"
#include "fcache.h"
#include "fdebug.h"
#include "fpsci.h"
#include "helper.h"
#include "openamp_configs.h"
#include "libmetal_configs.h"
#include "spi_protocol.h"          /* ← AquaGarden 共享头 */
#include "aqua_spi_master.h"

#define AQUA_TAG               "AQUA_SPI"
#define AQUA_RPMSG_SERVICE     "aqua-spi"
#define AQUA_RA6E2_PREP_US     5000U     /* 5 ms 让 RA6E2 装好 RSP */

static volatile int s_shutdown = 0;

/* 与 SDK 模板一致的资源表 / kick driver / slave_priv （省略，复用 SDK 例程） */
static struct remote_resource_table __resource s_rsc __attribute__((used)) = {
    1, NUM_TABLE_ENTRIES, {0,0},
    { offsetof(struct remote_resource_table, rpmsg_vdev), },
    { RSC_VDEV, VIRTIO_ID_RPMSG_, VDEV_NOTIFYID, RPMSG_IPU_C0_FEATURES,
      0, 0, 0, NUM_VRINGS, {0,0}, },
    { SLAVE00_TX_VRING_ADDR, VRING_ALIGN, SLAVE00_VRING_NUM, 1, 0 },
    { SLAVE00_RX_VRING_ADDR, VRING_ALIGN, SLAVE00_VRING_NUM, 2, 0 },
};
/* ... metal_device kick_driver / remoteproc_priv slave_priv 同 SDK 例程 ... */

/* 预先打包好的 NOP CMD 帧，避免每次回调都重新 CRC */
static uint8_t s_nop_frame[SPI_FRAME_LEN];
static uint8_t s_rsp_frame[SPI_FRAME_LEN];
static uint8_t s_dummy_rx[SPI_FRAME_LEN];

static void aqua_pack_nop(void)
{
    spi_pack_cmd(s_nop_frame,
                 SPI_SEQ_IDLE,         /* 0xFF */
                 SPI_DEV_SYSTEM,
                 SPI_CMD_SYS_NOP,
                 NULL, 0, 0);
}

static int aqua_rpmsg_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
                         uint32_t src, void *priv)
{
    int rc;
    (void)priv;

    if (len != SPI_FRAME_LEN)
    {
        FT_DEBUG_PRINT_W(AQUA_TAG, "drop pkt len=%zu", len);
        return RPMSG_SUCCESS;
    }
    ept->dest_addr = src;

    /* 第 1 次事务：发 CMD，丢 MISO（上一帧 RSP 已经被取过） */
    rc = aqua_spi_xfer_64((const uint8_t *)data, s_dummy_rx);
    if (rc) goto err;

    /* 让 RA6E2 ISR + 任务唤醒 + memcpy 准备好 RSP */
    metal_sleep_usec(AQUA_RA6E2_PREP_US);

    /* 第 2 次事务：发 NOP，取 RSP */
    rc = aqua_spi_xfer_64(s_nop_frame, s_rsp_frame);
    if (rc) goto err;

    /* 透传回 Linux，不解析任何字段 */
    rpmsg_send(ept, s_rsp_frame, SPI_FRAME_LEN);
    return RPMSG_SUCCESS;

err:
    /* SPI 出错时也要给 Linux 回个东西，否则它会一直阻塞在 read() */
    spi_pack_rsp(s_rsp_frame,
                 SPI_SEQ_IDLE, SPI_STATUS_BUSY,
                 SPI_RSP_TYPE_ACK, NULL, 0, 0, 0);
    rpmsg_send(ept, s_rsp_frame, SPI_FRAME_LEN);
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
    if (aqua_spi_init()) return -1;
    aqua_pack_nop();

    if (!platform_create_proc(&rproc, &slave_priv, &kick_driver)) return -1;
    rproc.rsc_table = &s_rsc;
    if (platform_setup_src_table(&rproc, rproc.rsc_table)) return -1;
    if (platform_setup_share_mems(&rproc)) return -1;
    rpdev = platform_create_rpmsg_vdev(&rproc, 0, VIRTIO_DEV_DEVICE, NULL, NULL);
    if (!rpdev) return -1;

    if (rpmsg_create_ept(&lept, rpdev, AQUA_RPMSG_SERVICE,
                         0, RPMSG_ADDR_ANY,
                         aqua_rpmsg_cb, aqua_rpmsg_unbind)) return -1;

    while (!s_shutdown && !rproc_get_stop_flag())
        platform_poll(&rproc);

    rpmsg_destroy_ept(&lept);
    platform_release_rpmsg_vdev(rpdev, &rproc);
    platform_cleanup(&rproc);
    FPsciCpuOff();
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

| 端 | 处理 |
|----|------|
| Linux daemon 收到 RSP CRC 错 | `stats.rx_crc_err_count++`；不重传命令；连续 5 次错 → 触发后端 `reset()`（v2 = remoteproc restart；v1 = close+open spidev） |
| 裸机核收到非 64 B 长度 RPMsg 包 | 丢弃 + 日志，不发 SPI |
| 裸机核 SPI 控制器返回错误 | 给 Linux 回 STATUS=BUSY 的伪 RSP（防 Linux 阻塞），同时 `stats.spi_err++` |
| RA6E2 收到 CMD CRC 错 | 不执行命令；返回 `STATUS_CRC_ERR`；置 `g_alarm_flags |= COMM_RX_ERROR` |

### 6.2 超时

| 场景 | 阈值 | 行为 |
|------|------|------|
| Linux：`read(rpmsg_fd)` | 100 ms（用 `poll()` 设超时） | 视为裸机核挂死，触发 remoteproc restart 自愈 |
| Linux：客户端 SEND_CMD 等响应 | 默认 200 ms | 单线程同步执行，几乎不会超时 |
| 裸机核：`FSpimTransferPollFifo` | SDK 内部超时 | 错误返回 → 装 STATUS_BUSY 伪 RSP 回 Linux |
| RA6E2：`xSemaphoreTake(spi_done_sem)` | 1000 ms | close + open `g_com_spi` 重置 SPI Slave |

### 6.3 裸机核 OpenAMP 异常

| 场景 | 处理 |
|------|------|
| RPMsg endpoint unbind（Linux 关 daemon） | 设 `s_shutdown = 1`，跳出主循环，`platform_cleanup()` + `FPsciCpuOff()`，等 Linux remoteproc 重新 start |
| RPMsg `rpmsg_send` 失败 | 日志一行，不重试（业务进程会因超时重发） |
| `platform_create_rpmsg_vdev` 失败 | 启动失败，远程核 panic 退出，remoteproc state 变 crashed，Linux daemon 自动 fallback 到 v1 |

### 6.4 未知设备 / 命令 / SPI 错位

完全复用 v1 §6.3 / §6.4 行为。**裸机核对此完全无感知**——所有 STATUS 都由 RA6E2 决定，由 Linux daemon 解析。

### 6.5 状态码统一表

完全复用 v1 §6.5（`spi_status_t`）。

### 6.6 日志通道

| 端 | 通道 |
|----|------|
| Linux daemon | `journalctl -u aqua-rpmsgd -f` |
| 裸机核 | UART1 串口（飞腾派 `ttyAMA1`），115200 8N1。SDK 默认 `CONFIG_DEFAULT_DEBUG_PRINT_UART1=y` 已选 |
| RA6E2 | 板载 USB-CDC，与 v1 一致 |

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

```c
const aqua_backend_ops_t *aqua_backend_autoselect(void **out_ctx)
{
    /* 优先 v2 OpenAMP */
    if (access("/dev/rpmsg_ctrl0", R_OK | W_OK) == 0 &&
        access("/dev/rpmsg0", R_OK | W_OK) == 0)
    {
        static aqua_rpmsg_ctx_t ctx;
        if (aqua_backend_rpmsg.open(&ctx) == 0)
        {
            *out_ctx = &ctx;
            return &aqua_backend_rpmsg;
        }
    }
    /* 退回 v1 spidev */
    if (access("/dev/spidev0.0", R_OK | W_OK) == 0)
    {
        static aqua_spidev_ctx_t ctx;
        if (aqua_backend_spidev.open(&ctx) == 0)
        {
            *out_ctx = &ctx;
            return &aqua_backend_spidev;
        }
    }
    return NULL;
}
```

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

`/etc/systemd/system/aqua-spi-controller.service`（互斥锁）：

```ini
[Unit]
Description=AquaGarden SPI bus access (mutex)
ConditionPathExists=/dev/spidev0.0|/dev/rpmsg0
Conflicts=aqua-spidev-overlay.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/true

[Install]
WantedBy=multi-user.target
```

| 部署模式 | 启用的 service |
|---------|-------------|
| **v2 OpenAMP 优先** | `aqua-openamp-load.service`（启动前用 remoteproc 加载固件） + `aqua-rpmsgd.service` + **不启用** `aqua-spidev-overlay.service` |
| **v1 fallback** | `aqua-spidev-overlay.service`（挂 overlay） + `aqua-spid.service` + **不启用** `aqua-openamp-load.service` |

### 8.4 切换脚本

`overlay/switch_to_v2.sh`（新增）：

```bash
#!/usr/bin/env bash
set -e
sudo systemctl stop aqua-spid 2>/dev/null || true
sudo systemctl disable aqua-spidev-overlay 2>/dev/null || true
sudo ./install_spidev_overlay.sh remove
sudo systemctl enable --now aqua-openamp-load.service
sudo systemctl enable --now aqua-rpmsgd.service
journalctl -u aqua-rpmsgd -f
```

`overlay/switch_to_v1.sh`：

```bash
#!/usr/bin/env bash
set -e
sudo systemctl stop aqua-rpmsgd 2>/dev/null || true
sudo systemctl disable aqua-openamp-load 2>/dev/null || true
sudo bash -c 'echo stop > /sys/class/remoteproc/remoteproc0/state' || true
sudo ./install_spidev_overlay.sh apply
sudo systemctl enable --now aqua-spidev-overlay
sudo systemctl enable --now aqua-spid
journalctl -u aqua-spid -f
```

---

## 9. 飞腾 Standalone SDK 关键 API 对照表

> SDK 路径：`~/phytium-standalone-sdk`（用户本机）
> SDK 版本：仓库当前 HEAD（含 `openamp_for_linux` v1.3+，2025-02 更新的 start/stop framework）

### 9.1 OpenAMP / RPMsg

| API | 头文件 | 用途 | 在本工程位置 |
|-----|-------|------|------------|
| `platform_create_proc` | `helper.h` | 创建 remoteproc 实例 | `aqua_spi_slave_run()` |
| `platform_setup_src_table` | `helper.h` | 注册 resource table | 同上 |
| `platform_setup_share_mems` | `helper.h` | 申请共享内存 | 同上 |
| `platform_create_rpmsg_vdev` | `helper.h` | 创建 RPMsg virtio device | 同上 |
| `rpmsg_create_ept` | `openamp/rpmsg.h` | 注册 endpoint + name service | 同上，service = `"aqua-spi"` |
| `rpmsg_send` | `openamp/rpmsg.h` | 发包到对端 | `aqua_rpmsg_cb()` |
| `platform_poll` | `helper.h` | 主循环驱动（处理 IPI / virtio） | 主循环 |
| `rproc_get_stop_flag` | `helper.h` | 检查是否被 Linux remoteproc stop | 主循环 |

### 9.2 SPI Master (FSPIM)

| API | 头文件 | 用途 |
|-----|-------|------|
| `FSpimLookupConfig(FSPI0_ID)` | `fspim.h` | 取默认配置（base_addr/irq） |
| `FSpimCfgInitialize` | `fspim.h` | 初始化实例 |
| `FSpimSetChipSelection(spim, on)` | `fspim.h` | 主动控 CS（E2000 系列） |
| `FSpimTransferPollFifo(spim, tx, rx, len)` | `fspim.h` | 阻塞收发，本工程主用 |
| `FSpimDeInitialize` | `fspim.h` | 去初始化 |
| `FSpimSetOption(spim, FSPIM_FREQUENCY_OPTION, hz)` | `fspim.h` | 运行时改速率 |

### 9.3 IO 引脚 mux

| API | 头文件 | 用途 |
|-----|-------|------|
| `FIOMuxInit()` | `fio_mux.h` | 初始化 IOPad 控制器 |
| `FIOPadSetSpimMux(FSPI0_ID)` | `fio_mux.h` | 一次性配好 SCK/MOSI/MISO/CSN0 复用为 SPI 功能（板级已实现） |

### 9.4 关键 SDK kconfig（v2 必启）

| KConfig | 取值 | 作用 |
|---------|------|------|
| `CONFIG_PHYTIUMPI_FIREFLY_BOARD` | `y` | 选板 |
| `CONFIG_TARGET_PE2204` | `y` | 选 SoC |
| `CONFIG_ARCH_ARMV8_AARCH64` | `y` | 与 Linux 5.10 一致；4.19 内核需切 aarch32 |
| `CONFIG_INTERRUPT_ROLE_SLAVE` | `y` | 远程核做 SGI 接收方 |
| `CONFIG_USE_AMP=y` + `CONFIG_USE_LIBMETAL=y` + `CONFIG_USE_OPENAMP=y` | `y` | 启用 OpenAMP |
| `CONFIG_USE_OPENAMP_IPI=y` | `y` | 用 SGI IPI 触发（不是轮询） |
| `CONFIG_SKIP_SHBUF_IO_WRITE=y` | `y` | Linux 主核管 vring；裸机不写自己的 shbuf 区描述 |
| `CONFIG_USE_MASTER_VRING_DEFINE=y` | `y` | vring 地址由 Linux 决定 |
| `CONFIG_USE_CACHE_COHERENCY=y` | `y` | 共享内存走 normal cacheable，依赖 Linux 在 reserved-memory 配 cacheable |
| `CONFIG_USE_SPI=y` + `CONFIG_USE_FSPIM=y` | `y` | **新增**：启用 FSPIM 驱动 |
| `CONFIG_USE_IOMUX=y` + `CONFIG_ENABLE_IOPAD=y` | `y` | 配 SPI 引脚复用 |
| `CONFIG_IMAGE_LOAD_ADDRESS=0xb0100000` | `0xb0100000` | 与 SDK 模板对齐；与 SLAVE00_SHARE_MEM_ADDR (0xC000_0000) 不冲突 |

---

## 10. 部署步骤

### 10.1 飞腾派 Linux 内核要求

| Kconfig | 状态 | 备注 |
|---------|------|------|
| `CONFIG_REMOTEPROC=y` | 必须 | 提供 `/sys/class/remoteproc/` |
| `CONFIG_PHYTIUM_REMOTEPROC=y` | 必须 | 飞腾远程核驱动（buildroot `openamp_*.config` 已含） |
| `CONFIG_RPMSG=y` + `CONFIG_RPMSG_VIRTIO=y` + `CONFIG_RPMSG_CHAR=y` | 必须 | 提供 `/dev/rpmsg_ctrl0` `/dev/rpmsg0` |
| `CONFIG_OF_RESERVED_MEM=y` | 必须 | 远程核内存窗口 |

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

```bash
# 飞腾派或装了 aarch64 工具链的开发机：
cd hardware/phytiumpi/spi_com/openamp_core/

# 一次性：把 SDK 路径告诉构建系统
export STANDALONE_SDK_ROOT=~/phytium-standalone-sdk

make load_kconfig LOAD_CONFIG_NAME=pe2204_aarch64_phytiumpi_aquaspi_core0.config
make clean
make image                                 # 输出 openamp_spi_core0.elf
```

### 10.4 部署到飞腾派

```bash
# 拷固件
scp openamp_spi_core0.elf user@phytium:/tmp/
ssh user@phytium 'sudo mv /tmp/openamp_spi_core0.elf /lib/firmware/'

# 装 daemon 和 service（首次）
scp build/native/aqua_rpmsgd user@phytium:/tmp/
scp systemd/aqua-rpmsgd.service systemd/aqua-openamp-load.service user@phytium:/tmp/
ssh user@phytium <<'EOF'
sudo mv /tmp/aqua_rpmsgd /usr/local/bin/
sudo mv /tmp/aqua-rpmsgd.service /tmp/aqua-openamp-load.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo bash -c 'overlay/switch_to_v2.sh'    # 卸 spidev overlay + 启 v2
EOF
```

### 10.5 启停远程核（手工调试用）

```bash
# 加载固件并启动
sudo bash -c 'echo openamp_spi_core0.elf > /sys/class/remoteproc/remoteproc0/firmware'
sudo bash -c 'echo start > /sys/class/remoteproc/remoteproc0/state'

# 应在 dmesg 看到：
#   remoteproc remoteproc0: powering up phytium-remoteproc
#   remoteproc remoteproc0: Booting fw image openamp_spi_core0.elf, size XXXX
#   virtio_rpmsg_bus virtio0: creating channel aqua-spi addr 0x0

# UART1 (115200 8N1) 应输出：
#   AquaGarden OpenAMP SPI core, build ...
#   [I] AQUA_SPI    Successfully created rpmsg endpoint.

# 停
sudo bash -c 'echo stop > /sys/class/remoteproc/remoteproc0/state'
```

### 10.6 联调流程

```bash
# 1. 启 v2 daemon（前台 + verbose）
sudo /usr/local/bin/aqua_rpmsgd -f -v

# 期待：
#   [INF] 后端探测: rpmsg(openamp) ✓
#   [INF] RA6E2 上线 uptime=1234 ms

# 2. 另开终端，用现有 CLI（无需任何改动）
aqua_spi_cli ping
aqua_spi_cli sys ping
aqua_spi_cli sensor poll
sudo aqua_spi_cli pump start 80
aqua_spi_cli stats
```

---

## 11. 决策记录（v2）

> 在 v1 已有的 9 个决策（Q1..Q9）基础上追加。

| ID | 问题 | 选择 | 理由 |
|----|------|------|------|
| Q10 | 是否真要按任务文档实现 OpenAMP？ | **是**（用户决定） | 任务文档明确要求 OpenAMP 路径；即便 v1 已工作也要给出符合任务定义的 v2 |
| Q11 | v1 是否废弃？ | **保留为 fallback** | 已稳定工作；v2 发布初期可能有未知问题；运行时自动检测后端 |
| Q12 | 远程核运行时 | **纯裸机（baremetal）** | 任务文档原话；FreeRTOS 引入额外调度复杂度，无收益（裸机核任务单一） |
| Q13 | 远程核加载方式 | **Linux remoteproc**（不是 U-Boot bootelf） | 用户选；运行时可热启停，便于调试与升级；与 SDK `openamp_for_linux` 例程方向一致 |
| Q14 | RPMsg 包大小 vs SPI 帧 | **1 RPMsg 包 = 1 SPI 帧 (64 B)** | RPMsg vring buffer 默认 512 B 远超 64 B，永不分片；语义清晰 |
| Q15 | CMD + NOP_READ 两次事务在哪一端发？ | **裸机核内部完成** | 一次 RPMsg 往返抵两次 SPI 事务，对 Linux 透明；避免 RPMsg 延迟翻倍 |
| Q16 | 裸机核是否解析协议字段 | **不解析，纯透传** | 业务命令新增时无需重烧裸机固件；与"任意业务进程通过共享 daemon 访问"目标一致 |
| Q17 | FSPIM 传输方式 | **POLL（首发）** | 64 B / 1 MHz 仅 0.5 ms，poll 阻塞代价可忽略；裸机核反正没别的事干。后续 INTERRUPT/DMA 升级路径预留 |
| Q18 | RPMsg endpoint 服务名 | **`"aqua-spi"`** | 与 SDK 例程默认服务名不冲突；命名清晰 |
| Q19 | v1/v2 切换粒度 | **每次部署选其一**（互斥） | 同一 SPI0 控制器不能同时被 spidev 内核驱动和裸机核占用；用 systemd Conflicts 强制互斥 |
| Q20 | Linux 端代码组织 | **抽 `aqua_backend_ops_t` 接口** | v1/v2 daemon 共享 95% 代码；上层 IPC + CLI 完全不动 |

---

## 附录 A：内存布局

| 范围 | 用途 | 来源 |
|------|------|------|
| `0x8000_0000 ~ 0xAFFF_FFFF` | Linux 主核内核 / 用户态（512 MB-） | 内核默认 |
| **`0xB010_0000 ~ 0xB0FF_FFFF`** | **裸机固件 image 区**（约 15 MB） | `CONFIG_IMAGE_LOAD_ADDRESS=0xb0100000`（与 SDK PHYTIUMPI 模板对齐） |
| **`0xC000_0000 ~ 0xC0FF_FFFF`** | **OpenAMP 共享内存区**（16 MB） | `SLAVE00_SHARE_MEM_ADDR`（与 SDK 例程对齐） |
| `0xC022_4000` | Kick / IPI mailbox 寄存器影子区 | `SLAVE00_KICK_IO_ADDR` |
| `0x2803_A000` | FSPI0 控制器寄存器（裸机核独占） | `FSPI0_BASE_ADDR` |
| `0x0000_002F`（SGI 编号） | Linux ↔ 裸机核 IPI | `SLAVE_00_SGI` (SDK 默认) |

> 实际地址必须与飞腾派 device-tree reserved-memory 一致，详见 §10.2。

---

## 附录 B：参考文件索引

| 文件 | 用途 |
|------|------|
| `任务_功能实现_SPI通信.md` | 顶层任务定义（v1/v2 共同遵循） |
| `设计_SPI通信工程方案.md` | v1 spidev 直驱方案（已落地） |
| **本文档** | v2 OpenAMP 方案 |
| `设计_SPI通信工程方案_v0_RPMsg版.md.bak` | 早期 v0 RPMsg 设想（与 v2 思路相近，但裸机核细节未对齐 SDK） |
| `~/phytium-standalone-sdk/example/system/amp/openamp_for_linux/` | **直接对标的 SDK 例程**——v2 远程核基于此改造 |
| `~/phytium-standalone-sdk/example/peripherals/spi/src/spim_polled_loopback_mode_example.c` | FSPIM polling 模式参考代码 |
| `~/phytium-standalone-sdk/board/phytiumpi_firefly/fio_mux.c` | 板级 SPI IOPad 配置实现 |
| `~/phytium-standalone-sdk/soc/pe220x/fparameters_comm.h` | FSPI0..3 基地址 / IRQ |
| `~/phytium-standalone-sdk/example/system/amp/README.md` | 多元异构（MSDF）部署框架说明 |
| `hardware/phytiumpi/openamp/demo/rpmsg-demo-single.c` | Linux 端 RPMsg 客户端参考实现 |
| `hardware/phytiumpi/spi0_scope_demo/spi_scope_demo.c` | SPI 物理链路验证（v1 也用） |
| `hardware/demo_wyr/FreeRTOS drive/data_merge/ra_gen/Communicate_Task.c` | RA6E2 SPI Slave + DMAC 现有 FSP 配置（v1/v2 共用） |

---

## 附录 C：v1 → v2 路径切换检查表（运维 Cheat Sheet）

切到 v2 前确认：

- [ ] `~/phytium-standalone-sdk` 已就位且能编 `openamp_for_linux` 例程
- [ ] 飞腾派内核 `zcat /proc/config.gz | grep -E 'REMOTEPROC|RPMSG'` 全 `=y`
- [ ] DTB / overlay 已 reserve `0xB010_0000` + `0xC000_0000` 两段内存
- [ ] `/lib/firmware/openamp_spi_core0.elf` 已部署
- [ ] **spidev overlay 已 remove**（`overlay/install_spidev_overlay.sh status` 应为 not-applied）
- [ ] `aqua_rpmsgd` 已编译并部署
- [ ] `aqua-spid.service` 已 stop+disable
- [ ] `aqua-rpmsgd.service` 和 `aqua-openamp-load.service` 已 enable

切回 v1 时反过来跑 `overlay/switch_to_v1.sh` 即可。

---

> **下一步实现顺序建议（不在本文档范围内，本文档仅为设计交付）：**
>
> 1. 抽 `aqua_backend.h` 接口，把现有 v1 daemon 拆成 `aqua_backend_spidev.c` + 新 `aqua_rpmsgd.c`（可以先不接 v2，跑通"抽象后的 v1"作为回归基线）
> 2. `openamp_core/` 复制 SDK `openamp_for_linux` 例程，改服务名 + 加 `aqua_spi_master.c`
> 3. 飞腾派端 buildroot 重编 OpenAMP 内核（如未启用），写 reserved-memory overlay
> 4. 联调：先 SDK 原版 echo 例程跑通 → 再换成 aqua-spi 例程跑通（用 stub 不接 RA6E2，loopback MOSI=MISO）→ 最后接 RA6E2 全链路验证
> 5. 写 systemd 编排 + `switch_to_v1/v2.sh`
> 6. 文档：在 `README.md` 顶部加 v1/v2 切换章节，把本文档链接放醒目位置
