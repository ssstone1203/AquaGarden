PhytiumPi ↔ RA6E2 SPI 通信工程设计方案 (v1)



适用项目：AquaGarden
上位机：飞腾派 Linux（用户态守护进程 aqua_spid 直接持有 spidev）
下位机：RA6E2 + FreeRTOS + Renesas FSP（SPI Slave）
物理层：4 线 SPI（SCK / MOSI / MISO / CS），全双工，64 字节定长帧

本方案是与原 v0 (RPMsg + 裸机核透传) 的重写版本。
旧文件保存在 设计_SPI通信工程方案_v0_RPMsg版.md.bak 仅供回溯。



0. 系统架构

┌────────────────────────────────────────────────────────────┐
│ 飞腾派 PhytiumPi（aarch64 Linux）                           │
│                                                            │
│   ┌────────────────────┐   ┌────────────────────────────┐  │
│   │ aqua_spi_cli       │   │ 业务后端（Web/UI/Python）   │  │
│   │ 命令行调试工具       │   │ 任意进程                    │  │
│   └─────────┬──────────┘   └────────────┬───────────────┘  │
│             │ Unix Domain Socket        │                  │
│             │ /tmp/aqua_spi.sock        │                  │
│             └────────────┬──────────────┘                  │
│                          ▼                                 │
│   ┌──────────────────────────────────────────────────────┐ │
│   │ aqua_spid（守护进程，单线程）                         │ │
│   │ - 独占 /dev/spidev0.0 (1 MHz, Mode 1, 8-bit)          │ │
│   │ - 周期 250 ms 自动 SENSOR_POLL_ALL → 缓存最近快照     │ │
│   │ - 处理 IPC：SEND_CMD / GET_SNAPSHOT / GET_STATS       │ │
│   │ - 维护通信统计、连续错误自愈                           │ │
│   └──────────────────────────────────────────────────────┘ │
└──────────────────┼─────────────────────────────────────────┘
                   │ SPI Bus (Mode 1, 1 MHz, 8-bit, MSB)
                   │ 64B CMD ↔ 64B RSP（CMD + NOP_READ 两次事务）
┌──────────────────┴─────────────────────────────────────────┐
│ RA6E2（FreeRTOS + FSP）                                     │
│   ┌────────────────────────────┐                           │
│   │ Communicate_Task            │                           │
│   │ - g_com_spi (SPI1 Slave)    │                           │
│   │ - 双 DMAC（TX:0 / RX:1）     │                           │
│   │ - app_dispatch 按 DEV+CMD   │                           │
│   │   分发到 PUMP/SENSOR/...    │                           │
│   │ - app_build_sensor_payload  │                           │
│   └────────────────────────────┘                           │
└────────────────────────────────────────────────────────────┘

职责分层：







层



跑什么



是否解析业务命令



可热更新





业务进程 / CLI



任意 Linux 进程，连 socket 即可



是（最终决策者）



✅ 重启即可





aqua_spid 守护进程



单线程主循环，独占 spidev



仅做 CMD 帧打包 + RSP 帧解包，不理解语义



✅ systemd 重启





RA6E2 Communicate_Task



FreeRTOS 任务，最高优先级



是（最终执行 + 状态机）



⚠️ 需重新烧录



1. SPI 硬件配置







配置项



选择



理由





SPI 模式



Mode 1（CPOL=0, CPHA=1）



RA6E2 FSP 的 SPI Slave 不支持 CPHA=0，R_SPI_Open() 会返回 FSP_ERR_UNSUPPORTED；因此主机 spidev 必须显式设置 SPI_CPHA，与 RA6E2 SPI_CLK_PHASE_EDGE_EVEN 对齐。





通信速率



首发 1 MHz，可调到 4 MHz



64 字节单事务在 1 MHz 下仅 0.5 ms，相对 250 ms 周期空载 < 1%；飞腾派排针无屏蔽，1 MHz 给信号完整性留够裕度；RA6E2 SPI1 + DMAC 在 4 MHz 内完全跟得上。





数据位宽



8 bit



协议按字节流设计，避免 16/32-bit 模式下的 endian 混乱；与 spi_codec.c 字节级 helper 天然契合。





DMA / 中断



RA6E2：双 DMAC + EOT 中断；飞腾：spidev 内部完成中断



从机无法控制时钟，CPU 轮询读 RDR 必丢字节，必须 DMAC。EOT 时由信号量唤醒任务，避免逐字节中断风暴。





CS 管理



spidev 自动管理（每次 SPI_IOC_MESSAGE 一次 CS 边沿）



CS 上升沿是 RA6E2 DMAC 的"帧结束 + 复位"信号，是抗错位最关键的一招。





字节序



小端 (LE)



RA6E2 (Cortex-M) 与飞腾派 (ARM-A) 默认皆 LE；不做转换。





位序



MSB First



RA6E2 已配置 SPI_BIT_ORDER_MSB_FIRST；行业默认。

1.1 RA6E2 现有 FSP 配置（节选）

const spi_cfg_t g_com_spi_cfg = {
    .channel        = 1,                         /* SPI1 */
    .operating_mode = SPI_MODE_SLAVE,
    .clk_phase      = SPI_CLK_PHASE_EDGE_EVEN,   /* CPHA=1 */
    .clk_polarity   = SPI_CLK_POLARITY_LOW,      /* CPOL=0 */
    .bit_order      = SPI_BIT_ORDER_MSB_FIRST,
    .p_transfer_tx  = &g_com_spi_tx,             /* DMAC0 */
    .p_transfer_rx  = &g_com_spi_rx,             /* DMAC1 */
    .p_callback     = Com_SPI_Callback,
};



⚠️ 必须改一处 FSP 配置：当前 DMAC 的 transfer_settings_word_b.size = TRANSFER_SIZE_2_BYTE，新方案按 8-bit 字节流，TX/RX 双 DMAC 都改为 TRANSFER_SIZE_1_BYTE，否则会出现"奇偶字节合并"的偏移错误。详见 ra6e2_patch/FSP_CHANGES.md。

1.2 引脚映射







信号



飞腾派排针



RA6E2





SCK



SPI0_SCK



P102 (RSPCK1)





MOSI



SPI0_MOSI



P101 (MOSI1)





MISO



SPI0_MISO



P100 (MISO1)





CS



SPI0_CSN0



P103 (SSL10)





GND



任一 GND



GND

接线 < 15 cm，共地。



2. SPI 通信协议设计

2.1 设计目标





全双工对齐：MOSI/MISO 同长度同帧结构对齐。



抗错位：64 字节定长 + 每帧 CS 上升沿复位 + CRC16 三重防御。



抗粘包：定长帧根本上避免；额外用 SOF 双重保险。



可扩展：保留版本字段、保留区、命令空间二级（DEV + CMD）。

2.2 帧总长 = 64 字节（定长）





一帧覆盖最大 CMD payload (16 B) 与最大 RSP payload (48 B)；



是 cache line 整数倍，DMAC 搬运对齐良好；



1 MHz 下单帧 0.5 ms，对 250 ms 周期完全无压力。

2.3 主 → 从帧（MOSI，CMD Frame）

偏移  长度  字段           说明
─────────────────────────────────────────────────────────────
 0    1    SOF0 = 0xA5
 1    1    SOF1 = 0x5A
 2    1    VER  = 0x01    协议版本
 3    1    SEQ            帧序列号 0..255 循环，用于响应配对
 4    1    DEV            设备类别 (spi_dev_t)
 5    1    CMD            命令 ID
 6    1    LEN            payload 字节数 (0..16)
 7    1    FLAGS          bit0=NEED_RSP, bit1..7=保留
 8   16    PAYLOAD        命令参数；不足部分填 0
24   38    RESERVED       保留区，必须填 0
62    2    CRC16          对前 62 字节做 CRC16/Modbus，小端
─────────────────────────────────────────────────────────────
共   64    字节

2.4 从 → 主帧（MISO，RSP Frame）

偏移  长度  字段           说明
─────────────────────────────────────────────────────────────
 0    1    SOF0 = 0x5A    与 CMD 镜像，便于眼看抓包
 1    1    SOF1 = 0xA5
 2    1    VER  = 0x01
 3    1    ACK_SEQ        被确认的命令 SEQ；0xFF=Idle
 4    1    STATUS         spi_status_t（0=OK）
 5    1    RSP_TYPE       0=Ack, 1=SensorData, 2=Status, 3=Version
 6    1    LEN            payload 实际有效字节数 (0..48)
 7    1    FLAGS          bit0=DATA_READY, bit1=ALARM, bit2=READY_GPIO(预留)
 8   48    PAYLOAD        响应数据；不足部分填 0
56    4    UPTIME_MS      从机已运行时间 (u32 LE)，用于诊断
60    2    RESERVED       保留区
62    2    CRC16          对前 62 字节做 CRC16/Modbus，小端
─────────────────────────────────────────────────────────────
共   64    字节

2.5 通信模型 ── CMD + NOP_READ 两次事务

Q3 决策：放弃流水线响应，改为对所有命令统一发两次 SPI 事务：

事务 1 (CMD)        : MOSI = CMD[N]    MISO = 上一帧 RSP（丢弃）
                       间隔 5 ms（让 RA6E2 在 ISR 中装好 RSP）
事务 2 (NOP_READ)   : MOSI = NOP        MISO = RSP[N] (本帧的真正响应)

代价是每条命令多花 0.5 ms + 5 ms = ~5.5 ms；收益是：





主机不需要管"流水线 SEQ 错位"



调试时一眼就能看出"这次 MISO 是哪条 CMD 的回包"



上电首帧不需要特殊处理 Idle 响应



v1 暂未启用 READY GPIO，靠 5 ms 帧间隔保证 RA6E2 装好 RSP（实测 RA6E2 在 ISR + 任务唤醒 + memcpy 全流程通常 < 200 µs）。如果实测 5 ms 不够，把 daemon 的 -g 参数调大即可，无需协议改动。预留协议位 SPI_RSP_FLAG_READY_GPIO 用于将来启用。

2.6 CRC 算法

CRC-16/Modbus，多项式 0xA001（反向 0x8005），初值 0xFFFF，无最终 XOR，结果小端写入帧尾。实现见 linux/libaqua_spi/spi_codec.c::spi_crc16_modbus，与 RA6E2 原 host_crc16_modbus 字节级一致。

PC 端单元测试 (tests/test_spi_protocol.c) 用 Modbus 标准向量 "123456789" → 0x4B37 自动验证。

2.7 端序

所有多字节字段（u16/u32/i16/i32）一律小端。



3. 命令定义表（与 RA6E2 实际语义对齐）



定义源：include/spi_protocol.h。所有 DEV/CMD 编号都来自该头文件，禁止在文档里另立。

3.1 设备类别 (DEV)







DEV ID



名称



说明





0x00



SPI_DEV_SYSTEM



系统级：心跳、版本、复位告警、HELLO





0x01



SPI_DEV_PUMP



水泵





0x02



SPI_DEV_SENSOR



各类传感器（温湿度、水温、土壤、压力、WQS）





0x03



SPI_DEV_LINKAGE



联动控制（阈值、规则）





0x04..0xEF



预留



后续新设备





0xF0..0xFF



厂商自定义



OEM 扩展

3.2 命令表

3.2.1 系统类（DEV = 0x00）







CMD



名称



参数 (PAYLOAD)



描述



响应





0x00



SYS_NOP



无



空操作；用于把上一帧 RSP 取回



ACK





0x01



SYS_PING



无



心跳



ACK





0x02



SYS_GET_VERSION



无



请求从机版本



VERSION (4 B)





0x03



SYS_GET_UPTIME



无



uptime 已在 RSP 固定字段



ACK





0x04



SYS_RESET_ALARM



无



清除从机锁存告警位



ACK





0x05



SYS_HELLO



无



主机重启通知（清统计）



ACK

3.2.2 水泵类（DEV = 0x01）







CMD



名称



参数



描述



响应





0x01



PUMP_START



[0]=power% (可选)



启泵



ACK





0x02



PUMP_STOP



无



停泵



ACK





0x03



PUMP_SET_PWM



[0]=power%



设 PWM



ACK





0x04



PUMP_SET_AUTO



无



切自动



ACK





0x05



PUMP_SET_MANUAL



[0]=enable, [1]=power%



设手动



ACK





0x10



PUMP_SET_CYCLE_CFG



11 字节 (见下)



配置循环灌溉



ACK





0x11



PUMP_SET_CYCLE_CTRL



[0]=en, [1]=start, [2..5]=total_count(LE)



控制循环灌溉



ACK





0x20



PUMP_GET_STATUS



无



请求水泵状态



STATUS

PUMP_SET_CYCLE_CFG 11 字节布局：

[0]   enable(0/1)
[1]   start(0/1)
[2]   power %
[3..4] run_ms      (u16 LE)
[5..6] stop_ms     (u16 LE)
[7..8] interval_ms (u16 LE)
[9..10] total_count(u16 LE)

3.2.3 传感器类（DEV = 0x02）







CMD



名称



描述



响应





0x10



SENSOR_POLL_ALL



核心命令：全量数据快照



SENSOR_DATA





0x11



SENSOR_POLL_AIR



仅请求温湿度（v1 实现仍返回全量，主机自取）



SENSOR_DATA





0x12



SENSOR_POLL_WATER



同上



SENSOR_DATA





0x13



SENSOR_POLL_SOIL



同上



SENSOR_DATA





0x14



SENSOR_POLL_PRESSURE



同上



SENSOR_DATA



v1 简化：所有 POLL 都返回完整 48 B SensorData，让主机按需取字段。后续如需精简带宽再分裂返回。

3.2.4 联动类（DEV = 0x03）







CMD



名称



参数



描述





0x01



LINK_SET_SOIL_CFG



[0]=th%, [1]=hys%



土壤阈值





0x02



LINK_SET_RULE



4 B (见下)



联动规则

LINK_SET_RULE 4 字节：

[0]   bit0=enable_soil, bit1=enable_water_temp, bit2=enable_wqi
[1..2] water_temp_high_x10 (i16 LE, 单位 0.1 °C)
[3]   wqi_low_threshold (0..100)

3.3 SENSOR_DATA PAYLOAD（48 字节）

字段顺序与原 host_build_uplink_frame 完全对齐，只是去掉了 UART 包头。
完整定义见 include/spi_protocol.h 的 SPI_SENS_OFF_* 常量。

偏移  长度  字段                  类型
──────────────────────────────────────────────────
 0    4    timestamp_ms          u32 LE   (ms)
 4    2    air_temp_x10          i16 LE   (0.1 °C)
 6    2    air_humidity_x10      i16 LE   (0.1 %RH)
 8    2    water_temp_x10        i16 LE   (0.1 °C)
10    1    soil_moisture_pct     u8       (%)
11    1    wqi                   u8       (0..100)
12    1    pump_power_pct        u8       (%)
13    1    need_watering         u8       (0/1)
14    2    pressure_kg_0_x100    u16 LE   (0.01 kg)
16    2    pressure_kg_1_x100    u16 LE
18    2    pressure_kg_2_x100    u16 LE
20    4    alarm_flags           u32 LE   (位掩码)
24    2    air_retry_count       u16 LE
26    2    wqs_retry_count       u16 LE
28    2    uwt_retry_count       u16 LE
30    1    pump_cycle_enable     u8
31    1    pump_cycle_start      u8
32    1    pump_cycle_active     u8
33    1    pump_cycle_state      u8
34    1    pump_cycle_power_pct  u8
35    2    pump_cycle_done_count u16 LE
37   11    RESERVED              填 0
──────────────────────────────────────────────────
共   48    字节



4. 通信流程时序

4.1 总体节拍





主机周期：250 ms（与 RA6E2 现有 Communicate_Task 节拍对齐）



请求-响应：每条命令 = 2 次 SPI 事务（CMD + NOP_READ）



独立 ACK：无。RSP 帧的 STATUS 字段就是 ACK，0=OK。

4.2 单命令时序（1 MHz）

时间轴 →

CS_n────┐                                     ┌────CS_n+1───
        │  ←—— 64B SPI 事务 (~512 µs) ——→     │
        │                                     │  ←——— 5 ms 间隔 ———→  ┌——
MOSI    │ [CMD_n: 64B]                        │                       [NOP: 64B]
MISO    │ [上一帧滞后响应（丢弃）]              │                       [RSP_n: 64B]
                                              │
                              RA6E2 EOT ISR → 任务唤醒 → app_dispatch
                              → 装好 s_tx_frame ← 必须在 5 ms 内完成

4.3 时间预算







阶段



时间





CS 建立 (Tcss)



~2 µs（spidev 自动）





64 字节 SCK



64 × 8 / 1 MHz = 512 µs





CS 保持 (Tcsh)



~1 µs





CMD 与 NOP_READ 间隔



5 ms（daemon 可调）





单命令总耗



~6 ms





周期 250 ms 利用率



< 3%

4.4 主机状态机（在 daemon 内）

启动:
    spi_open()
    SYS_HELLO + SYS_PING （5 s 内重试至 STATUS=OK）

主循环（poll, 唤醒源 = 周期定时 / 客户端连接 / 信号）:
    if 周期到:
        发 SENSOR_POLL_ALL → 缓存 SensorData
    if 客户端连接:
        accept → 同步 ipc_handle() → close
    if 信号:
        退出

ipc_handle() 内部 SEND_CMD 路径:
    spi_send_cmd(dev, cmd, payload, len, flags)
        → spi_pack_cmd(CMD)  → ioctl SPI_IOC_MESSAGE  (丢弃 MISO)
        → nanosleep(5 ms)
        → spi_pack_cmd(NOP)  → ioctl SPI_IOC_MESSAGE  (取 RSP)
        → spi_validate_frame(RSP) → 填 IPC 响应



5. 双端代码组织

5.1 目录结构

hardware/phytiumpi/spi_com/
├── 任务_功能实现_SPI通信.md         # 任务说明（不变）
├── 设计_SPI通信工程方案.md          # 本文档
├── 设计_SPI通信工程方案_v0_RPMsg版.md.bak
├── README.md                        # 端到端联调指南
├── Makefile                         # 单测 + 交叉编译入口
│
├── include/
│   └── spi_protocol.h               # 共享协议头（唯一定义源）
│
├── linux/
│   ├── libaqua_spi/
│   │   ├── spi_codec.h              # CRC + 帧打包/校验
│   │   ├── spi_codec.c
│   │   └── aqua_ipc.h               # daemon ↔ 客户端 IPC 协议
│   ├── aqua_spid.c                  # 守护进程
│   └── aqua_spi_cli.c               # 命令行工具
│
├── tests/
│   └── test_spi_protocol.c          # PC 单元测试（无需硬件）
│
└── ra6e2_patch/
    ├── FSP_CHANGES.md               # FSP Configurator 改动步骤
    ├── sync_from_canonical.sh       # 从 include/ 与 linux/libaqua_spi/ 同步
    ├── spi_protocol.h               # = ../include/spi_protocol.h（同步副本）
    ├── spi_codec.h
    ├── spi_codec.c
    └── Communicate_Task_entry.c     # RA6E2 端任务实现（替换 UART 版）

5.2 编译

# 在 Linux 主机或飞腾派上：
cd hardware/phytiumpi/spi_com

make test           # PC 跑单元测试
make linux          # 交叉编译 aarch64（aqua_spid + aqua_spi_cli）
make linux-native   # 飞腾派本机编译

5.3 关键源文件签名

linux/libaqua_spi/spi_codec.h

uint16_t spi_crc16_modbus(const uint8_t *data, uint16_t len);

int spi_pack_cmd(uint8_t *frame, uint8_t seq, uint8_t dev, uint8_t cmd,
                 const uint8_t *payload, uint8_t len, uint8_t flags);

int spi_pack_rsp(uint8_t *frame, uint8_t ack_seq, uint8_t status,
                 uint8_t type, const uint8_t *payload, uint8_t len,
                 uint8_t flags, uint32_t uptime_ms);

int spi_validate_frame(const uint8_t *frame, int is_cmd);

linux/aqua_spid.c 主循环（伪码）

spi_open(); listen_socket(); hello_handshake();
while (!quit) {
    poll([sock_fd, signal_fd], wait_until_next_poll_ms);
    if (sock 有连接) ipc_handle(accept());
    if (周期到)      periodic_poll_all();   // 内部 spi_send_cmd(SENSOR, POLL_ALL, ...)
}

ra6e2_patch/Communicate_Task_entry.c 主循环

spi.open();
spi_pack_rsp(s_tx_frame, IDLE);            // 装一帧 Idle 响应
for (;;) {
    spi.writeRead(s_tx_frame, s_rx_frame, 64, 8);   // 异步：装 DMAC
    xSemaphoreTake(spi_done_sem, 1000ms);           // 等 EOT 中断
    if (event != COMPLETE) continue;
    app_update_alarm_flags();
    app_dispatch(s_rx_frame, s_tx_frame);           // 解析 + 装下次 RSP
}

5.4 IPC 协议（daemon ↔ 任意客户端）

见 linux/libaqua_spi/aqua_ipc.h：





单 socket 单连接单请求单响应



请求 aqua_ipc_req_t：op + dev/cmd/payload/flags/timeout



响应 aqua_ipc_rsp_t：rc + status + payload + uptime + stats（按 op 解释）



操作码：SEND_CMD / GET_SNAPSHOT / GET_STATS / PING_DAEMON



6. 错误处理机制

6.1 校验失败







端



处理





主机收到 RSP CRC 错



stats.rx_crc_err_count++；不重传命令；连续 5 次错触发 spidev 重置





从机收到 CMD CRC 错



不执行命令；返回 STATUS_CRC_ERR；置 `g_alarm_flags

6.2 超时







场景



阈值



行为





主机：SPI_IOC_MESSAGE ioctl



内核默认



返回 -EIO；累加 spidev_io_err_count





从机：xSemaphoreTake(spi_done_sem)



1000 ms



主机长期空闲：close + open 重置 SPI Slave





主机：客户端 SEND_CMD 等响应



默认 100 ms



单线程同步执行，几乎不会超时

6.3 未知设备/命令

从机返回 STATUS_UNKNOWN_DEV / STATUS_UNKNOWN_CMD，主机据此判定固件是否需要升级。

6.4 SPI 错位 / 丢帧

定长 64 B + 每事务 CS 上升沿 = 天然防护。一旦错位，CRC 必失败，下次 CS 拉低时 RA6E2 DMAC 自动从字节 0 开始填，不会持续累积。

如果连续 N 次（默认 5）CRC 错：





主机：spi_reset() → close 后重开 spidev，等 10 ms 让 RA6E2 DMAC 复位



从机：超时 1 s 后 close + open g_com_spi

6.5 状态码统一表







值



名称



含义





0x00



STATUS_OK



成功





0x01



STATUS_CRC_ERR



CRC 校验失败





0x02



STATUS_UNKNOWN_DEV



设备类别未知





0x03



STATUS_UNKNOWN_CMD



命令 ID 未知





0x04



STATUS_BAD_PAYLOAD_LEN



payload 长度不符





0x05



STATUS_BUSY



资源忙





0x06



STATUS_VER_MISMATCH



CMD 帧 VER 不识别





0x07



STATUS_BAD_SOF



帧头不对，疑似 SPI 错位





0x80..0xFF



厂商扩展



留给业务层



调试提示：aqua_spi_cli sys ping 若出现 rc=-7，该值来自主机对 RSP 帧的 spi_validate_frame()，即上表 STATUS_BAD_SOF (0x07)，不是用 strerror(7) 读成的 Linux E2BIG。含义是 MISO 前两字节不是 5A A5：优先查接线/共地、RA6E2 固件是否在跑、以及 ra6e2_patch/FSP_CHANGES.md 里 TX/RX DMAC 按 1 字节搬运是否已落实。
另：标准 spidev 上 SPI_IOC_MESSAGE 成功时 ioctl 返回 0，应用侧应使用 ret < 0 判定失败；用「ret < 1」会把成功误判为失败。



7. 可扩展性设计

7.1 新增设备

按 §3.1 设备空间分配 ID：





在 include/spi_protocol.h 增 SPI_DEV_xxx 与对应 CMD 枚举



bash ra6e2_patch/sync_from_canonical.sh 同步到 RA6E2



RA6E2 app_dispatch() 增 case SPI_DEV_xxx



（可选）aqua_spi_cli.c 增子命令

7.2 新增命令

在已有 DEV 下追加 CMD ID（保持 ≤ 16 B payload），重复 §7.1 的 1/3/4 步。

7.3 预留字段







字段



位置



用途建议





CMD 帧 24..61 字节



38 B 保留



长 payload、批量配置、未来 OTA chunk





RSP 帧 60..61 字节



2 B 保留



未来加报警子码





RSP FLAGS bit2..7



6 位



bit2=READY_GPIO 已预留





VER 字段



1 B



协议演进；未识别 VER 应回 STATUS_VER_MISMATCH

7.4 v2 升级路径





payload > 48 B → 引入多帧 sequence（FLAGS bit3 = MORE）



实时事件（从机主动通知）→ 启用 READY_GPIO 中断（v1 已预留 FLAG 位）



加密 → PAYLOAD 区前加 4 B nonce + 4 B MAC，AES-CMAC

均可在 VER=0x02 下平滑过渡，v1 解析路径保留。



8. 决策记录（设计阶段）



实现前 9 个关键决策，留底备查。







ID



问题



选择



理由





Q1



飞腾派 SPI 物理层路径



A：Linux spidev 直驱



250 ms 周期下调度抖动无影响；不写裸机固件，调试快 10×





Q2



帧长



保持 64 B 定长



抗错位；DMAC 长度可固定





Q3



响应模型



CMD + NOP_READ 两次事务



牺牲 5 ms 换可读性 + 调试容易





Q4



RA6E2 准备时间保障



预留 READY GPIO 协议位 + 5 ms 帧间隔



v1 不接 GPIO，靠加大帧间隔；协议预留 bit2=READY_GPIO 给未来





Q5



SPI0 device tree



N/A（spidev 直驱无需改 dtb）



spidev0.0 默认就在





Q6



Linux 端架构



A：daemon 独占 spidev



多进程并发安全；可缓存快照





Q7



命令空间映射



A：重写 host_apply_command 接收 DEV+CMD



干净；可扩展性强





Q8



READY GPIO 启用



(b) 先预留协议位，初版纯靠帧间隔



硬件 GPIO 待用户确认；协议平滑预留





Q9



工程目录



分目录 include / linux / tests / ra6e2_patch



清晰、易维护



附录 A：物理链路验证

在 RA6E2 端固件未就绪前，可用 hardware/phytiumpi/spi0_scope_demo/spi_scope_demo.c 通过 spidev 直接验证布线：

sudo ./spi_scope_demo -s 1000000 -g 200 -c 5

预期：示波器在 SCK 上看到 5 次突发，每次 8 字节；RA6E2 端 Com_SPI_Callback 应触发 5 次。

附录 B：参考文件索引







文件



用途





hardware/phytiumpi/spi0_scope_demo/spi_scope_demo.c



spidev 物理链路验证工具，附录 A





hardware/demo_wyr/FreeRTOS drive/data_merge/ra_gen/Communicate_Task.c



RA6E2 SPI Slave + DMAC 现有 FSP 配置





hardware/demo_wyr/FreeRTOS drive/data_merge/src/sensor_fusion.h



RA6E2 全局传感器/水泵/告警变量声明





hardware/phytiumpi/spi_com/ra6e2_patch/FSP_CHANGES.md



RA6E2 工程改造步骤

