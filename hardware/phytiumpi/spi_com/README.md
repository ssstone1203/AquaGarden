# AquaGarden SPI 通信工程

飞腾派(Master) ↔ RA6E2(Slave) 4 线 SPI 通信完整实现。

> **任务定义**：[`任务_功能实现_SPI通信.md`](任务_功能实现_SPI通信.md)
> **RA6E2 工程改造步骤**：[`ra6e2_patch/FSP_CHANGES.md`](ra6e2_patch/FSP_CHANGES.md)

---

## 0. 两套主机端实现并存

协议层 100 % 兼容，**RA6E2 端完全复用**，可在飞腾派上一键切换。

| | **v1 — Linux spidev 直驱** | **v2 — OpenAMP/RPMsg + 裸机从核** |
|---|---|---|
| 设计文档 | [`设计_SPI通信工程方案.md`](设计_SPI通信工程方案.md) | [`设计_SPI通信工程方案_v2_OpenAMP版.md`](设计_SPI通信工程方案_v2_OpenAMP版.md) |
| 与任务文档 §1 架构 | 不一致（决策 Q1 主动放弃 OpenAMP） | **严格对齐**任务文档 |
| Linux 主核 | `aqua_spid` 用户态守护 | `aqua_rpmsgd` 用户态守护（无 SPI 直接访问） |
| SPI 物理通道 | `/dev/spidev0.0` → 内核 spi-phytium → FSPI0 | RPMsg → 裸机核固件 → FSPIM → FSPI0 |
| 4 个 ARMv8 核 | Linux 全占 | Linux 占 3 个，第 4 个跑 OpenAMP 裸机固件 |
| 调试便利度 | 高（spidev 直接抓） | 中（多了 OpenAMP 层、远程核串口） |
| 业务客户端 | 走 `/tmp/aqua_spi.sock` 与 daemon IPC | **完全相同**（`aqua_spi_cli` 不需要重编） |
| 部署门槛 | 普通飞腾派固件即可 | 需要：reserved-memory overlay + remoteproc 内核 + 自编固件 |

设计上：
- **协议层** (`include/spi_protocol.h`、`linux/libaqua_spi/spi_codec.{h,c}`) — 三方共用
  （Linux daemon、PC 单测、飞腾派裸机核固件）。修协议**只改一处**。
- **SPI 后端** (`linux/aqua_backend.h` + `aqua_backend_{spidev,rpmsg}.c`) — 抽象成 ops 表，
  daemon 主循环 (`linux/aqua_daemon_core.c`) 不感知差异，运行时按 `-B {auto|rpmsg|spidev}` 选。
- **客户端 / IPC** (`linux/aqua_spi_cli.c`、`linux/libaqua_spi/aqua_ipc.h`) — 与 backend 无关。
- **systemd 互斥** — `aqua-spid.service` 与 `aqua-rpmsgd.service` 用 `Conflicts=` 强保证同一时刻只跑一个。

---

## 1. 目录

```
spi_com/
├── include/spi_protocol.h            协议唯一定义源
├── linux/
│   ├── libaqua_spi/
│   │   ├── spi_codec.{h,c}            CRC + 帧打包/校验（三方共享）
│   │   └── aqua_ipc.h                 daemon ↔ 客户端 IPC 协议
│   ├── aqua_backend.h                 SPI 后端抽象接口
│   ├── aqua_backend_spidev.c          v1 后端：/dev/spidev0.0
│   ├── aqua_backend_rpmsg.c           v2 后端：/dev/rpmsg0 + autoselect
│   ├── aqua_daemon_core.{h,c}         主循环 + IPC + stats（与后端无关）
│   ├── aqua_spid.c                    v1 入口（强制 spidev）
│   ├── aqua_rpmsgd.c                  v2 入口（默认 autoselect）
│   └── aqua_spi_cli.c                 命令行工具（与后端无关）
├── openamp_core/                      v2 飞腾派裸机从核固件
│   ├── main.c, src/, inc/, common/
│   ├── configs/pe2204_aarch64_phytiumpi_aquaspi_core0.config
│   ├── makefile / Kconfig / ft_openamp.ld
│   └── README.md                      固件构建/烧录/调试指南
├── deploy/                            v1+v2 systemd / overlay / 切换脚本
│   ├── overlay/phytium_pi_openamp.dts
│   ├── overlay/install_openamp_overlay.sh
│   ├── systemd/aqua-spid.service
│   ├── systemd/aqua-rpmsgd.service
│   ├── systemd/aqua-openamp-load.service
│   ├── scripts/switch_to_v1.sh
│   ├── scripts/switch_to_v2.sh
│   └── README.md                      部署细节
├── overlay/                           v1 spidev DT overlay（首次部署必跑）
│   ├── phytium_pi_spidev0.dts
│   └── install_spidev_overlay.sh
├── tests/test_spi_protocol.c          PC 端单元测试
├── ra6e2_patch/                       RA6E2 端要复制的文件 + FSP 改造步骤
└── Makefile
```

---

## 2. 快速验证（无硬件）

```bash
cd hardware/phytiumpi/spi_com
make test
```

应输出 6 行 `[OK]` + `*** 所有 SPI 协议层单元测试通过 ***`。

---

## 3. 编译

```bash
make linux           # 交叉编译 aarch64：v1 + v2 daemon + CLI
                     # 输出：build/linux/{aqua_spid, aqua_rpmsgd, aqua_spi_cli}

make linux-native    # 飞腾派本机编译
                     # 输出：build/native/...

make clean
make help
```

> **裸机核固件不在本 Makefile 内**，它依赖飞腾 standalone-sdk 工具链。
> 详见 [`openamp_core/README.md`](openamp_core/README.md)。

---

## 4. 接线

| 信号 | 飞腾派 | RA6E2 | 备注 |
|-----|--------|-------|------|
| SCK  | SPI0_SCK  | P102 | < 15 cm |
| MOSI | SPI0_MOSI | P101 | |
| MISO | SPI0_MISO | P100 | |
| CS   | SPI0_CSN0 | P103 | |
| GND  | GND       | GND  | **必须共地** |

---

## 5. 部署：选 v1 还是 v2 ?

| 场景 | 推荐 |
|------|------|
| 第一次接通，调示波器看波形 | **v1** |
| 业务集成、协议验证、demo 演示 | **v1**（足够稳，调试链短） |
| 任务文档审查/答辩、对齐 §1 系统架构 | **v2** |
| 4 核 SoC 资源隔离、Linux 死了 SPI 仍要工作（heartbeat/watchdog 场景） | **v2** |

> v1 ↔ v2 切换是**热切换**：随时一行命令切回去。

### 5.1 v1 路径

详细一次性安装在原历史版本里，本节给浓缩版：

```bash
# 飞腾派上：
make linux-native
sudo cp build/native/{aqua_spid,aqua_spi_cli} /opt/aqua/bin/
sudo cp deploy/systemd/aqua-spid.service /etc/systemd/system/
sudo systemctl daemon-reload

# 装 spidev overlay
sudo overlay/install_spidev_overlay.sh apply        # /dev/spidev0.0 出现

# 启动
sudo systemctl enable --now aqua-spid
journalctl -fu aqua-spid                            # 看到 "RA6E2 上线" 即可
```

### 5.2 v2 路径

```bash
# 1) 在带 phytium-standalone-sdk 的开发机上编出固件：
cd openamp_core
make config_pe2204_phytiumpi_aarch64
make all -j
scp openamp_spi_core0.elf root@<飞腾派IP>:/lib/firmware/

# 2) 飞腾派上一次性安装：
sudo cp build/native/{aqua_rpmsgd,aqua_spi_cli} /opt/aqua/bin/
sudo cp deploy/systemd/aqua-rpmsgd.service \
        deploy/systemd/aqua-openamp-load.service \
        /etc/systemd/system/
sudo systemctl daemon-reload

# 3) 切到 v2（自动停 v1 + 卸 spidev overlay + 挂 reserved-memory + start remoteproc + 起 daemon）
sudo deploy/scripts/switch_to_v2.sh
```

详细前置条件 / 内核 config 检查见 [`deploy/README.md`](deploy/README.md) 与
[`设计_SPI通信工程方案_v2_OpenAMP版.md §10 部署步骤`](设计_SPI通信工程方案_v2_OpenAMP版.md)。

### 5.3 v1 ↔ v2 切换

```bash
sudo deploy/scripts/switch_to_v1.sh         # 切到 v1
sudo deploy/scripts/switch_to_v2.sh         # 切到 v2

# 任意路径都用同一个 CLI
/opt/aqua/bin/aqua_spi_cli sys ping
/opt/aqua/bin/aqua_spi_cli stats
/opt/aqua/bin/aqua_spi_cli snapshot
```

---

## 6. RA6E2 端

按 [`ra6e2_patch/FSP_CHANGES.md`](ra6e2_patch/FSP_CHANGES.md) 操作（**v1/v2 共用同一份固件**）：

1. 拷贝 4 个文件到 e2studio 工程的 `src/` 目录
2. 在 FSP Configurator 把 SPI1 双 DMAC 字宽改 1 Byte
3. Generate Project Content → 编译 → 烧录

---

## 7. 客户端 CLI 用法（v1 / v2 通用）

```bash
aqua_spi_cli ping                # 检查 daemon 在不在
aqua_spi_cli sys ping            # 心跳 RA6E2
aqua_spi_cli sensor poll         # 走 SPI 实时拉一份传感器
aqua_spi_cli snapshot            # 读 daemon 缓存快照（不走 SPI）
aqua_spi_cli pump start 80       # 启泵 80%
aqua_spi_cli pump stop
aqua_spi_cli pump pwm 60
aqua_spi_cli link soil 35 5      # 阈值 35%，滞回 5%
aqua_spi_cli stats               # 通信统计（CRC 错、SOF 错、连续错…）
aqua_spi_cli raw 0x10 0x01       # 原始 dev/cmd 直发（高级）
```

完整子命令：`aqua_spi_cli --help`

---

## 8. 故障排查

| 现象 | 检查 |
|------|------|
| `打开 /dev/spidev0.0 失败` (v1) | 99% 是 spidev overlay 没装：`sudo overlay/install_spidev_overlay.sh status` |
| `RPMSG_CREATE_EPT_IOCTL 'aqua-spi' 失败` (v2) | 远程核没起来：`cat /sys/class/remoteproc/remoteproc0/state` 应为 `running` |
| 启动后 `RA6E2 在 5 秒内未上线` | 接线/共地；示波器看 SCK 有无；RA6E2 烧录是否成功 |
| `stats` 显示 `rx_crc_err` 持续增加 | 90% 是 RA6E2 DMAC 字宽未改 1 Byte（FSP_CHANGES.md §2.1）；其次是布线干扰 |
| `rx_sof_err` 增加 | SPI 错位；下次 CS 边沿会自愈，持续→改速率到 500 kHz |
| `consecutive_err` 反复触发 reset | 物理层不稳；缩短接线、加去耦、检查电源 |
| v1 切回后 `/dev/spidev0.0` 没出现 | 上一次 v2 没正确卸 overlay：`overlay/install_spidev_overlay.sh apply` |
| v2 切过去后 daemon 一直 timeout | 远程核启动了但 SPI 卡死 → 飞腾派 UART1 看裸机核日志 |

---

## 9. 协议演进与版本

- **当前版本**：v1（`SPI_PROTO_VERSION = 0x01`）
- **修改协议的唯一入口**：[`include/spi_protocol.h`](include/spi_protocol.h)
- **同步到 RA6E2**：`bash ra6e2_patch/sync_from_canonical.sh`
- **不兼容更改**：必须升级 `SPI_PROTO_VERSION`，并在 RA6E2 `app_dispatch` 保留旧 VER 的解析路径
- **裸机核固件不需要重编**：它纯字节透传，不解析协议字段

详见 [`设计_SPI通信工程方案.md` §7](设计_SPI通信工程方案.md) /
    [`设计_SPI通信工程方案_v2_OpenAMP版.md` §7](设计_SPI通信工程方案_v2_OpenAMP版.md)。
