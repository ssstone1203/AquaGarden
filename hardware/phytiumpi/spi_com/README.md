# AquaGarden SPI 通信工程

飞腾派(Master) ↔ RA6E2(Slave) 4 线 SPI 通信完整实现。

> **设计文档**：`设计_SPI通信工程方案.md`
> **任务定义**：`任务_功能实现_SPI通信.md`
> **RA6E2 工程改造步骤**：`ra6e2_patch/FSP_CHANGES.md`

---

## 目录

```
spi_com/
├── include/spi_protocol.h            # 共享协议头（唯一定义源）
├── linux/
│   ├── libaqua_spi/
│   │   ├── spi_codec.{h,c}            # CRC + 帧打包/校验
│   │   └── aqua_ipc.h                 # daemon ↔ 客户端 IPC 协议
│   ├── aqua_spid.c                    # 守护进程（独占 spidev）
│   └── aqua_spi_cli.c                 # 命令行工具
├── tests/test_spi_protocol.c          # PC 端单元测试
├── ra6e2_patch/                       # RA6E2 工程要复制的文件
│   ├── spi_protocol.h                 # 同步副本
│   ├── spi_codec.{h,c}
│   ├── Communicate_Task_entry.c       # 替换原 UART 版本
│   ├── sync_from_canonical.sh         # 一键同步脚本
│   └── FSP_CHANGES.md                 # FSP Configurator 改动指南
└── Makefile
```

---

## 快速上手

### 1) 本机验证协议层（无需任何硬件）

```bash
cd hardware/phytiumpi/spi_com
make test
```

应输出 6 行 `[OK]` + `*** 所有 SPI 协议层单元测试通过 ***`。

### 2) 编译飞腾派可执行

**方式 A：直接在飞腾派上本机编译**（推荐，零 glibc 兼容性问题）

```bash
# 飞腾派上：
cd ~/AquaGarden/hardware/phytiumpi/spi_com
make linux-native                               # 输出 build/native/
sudo cp build/native/aqua_spid build/native/aqua_spi_cli /usr/local/bin/
```

**方式 B：在 x86 开发机上交叉编译再传过去**

```bash
# 开发机上：
sudo apt install gcc-aarch64-linux-gnu          # 一次性
make linux                                      # 输出 build/linux/
scp build/linux/aqua_spi* user@<飞腾派IP>:/tmp/

# 飞腾派上：
sudo mv /tmp/aqua_spi* /usr/local/bin/
```

> ⚠️ 不要把开发机交叉编译产物提交进 git 然后在飞腾派 git pull。glibc 版本差异可能导致加载失败，**始终在飞腾派上用方式 A 重新编译**最稳妥。

### 3) RA6E2 端

按 [`ra6e2_patch/FSP_CHANGES.md`](ra6e2_patch/FSP_CHANGES.md) 操作：

1. 拷贝 4 个文件到 e2studio 工程的 `src/` 目录
2. 在 FSP Configurator 把 SPI1 双 DMAC 字宽改 1 Byte
3. Generate Project Content → 编译 → 烧录

### 4) 接线（详见设计文档 §1.2）

| 信号 | 飞腾派 | RA6E2 | 备注 |
|-----|--------|-------|------|
| SCK  | SPI0_SCK  | P102 | < 15 cm |
| MOSI | SPI0_MOSI | P101 | |
| MISO | SPI0_MISO | P100 | |
| CS   | SPI0_CSN0 | P103 | |
| GND  | GND       | GND  | **必须共地** |

### 5) 启动 daemon + 调试

在飞腾派上：

```bash
# 前台 + 详细日志（调试用）
sudo /usr/local/bin/aqua_spid -f -v

# 期待输出：
# [INF] 已打开 /dev/spidev0.0 @ 1000000 Hz, 8-bit, Mode 0
# [INF] 监听 /tmp/aqua_spi.sock
# [INF] aqua_spid 启动：周期=250 ms，间隔=5000 µs
# [INF] RA6E2 上线，uptime=1234 ms
```

另开终端：

```bash
# 检查 daemon 在跑
aqua_spi_cli ping

# 心跳 RA6E2
aqua_spi_cli sys ping

# 拉一份实时传感器数据
aqua_spi_cli sensor poll

# 读 daemon 缓存的最新快照（不走 SPI）
aqua_spi_cli snapshot

# 启动水泵 80%
sudo aqua_spi_cli pump start 80

# 停泵
sudo aqua_spi_cli pump stop

# 设置土壤阈值 35% / 滞回 5%
sudo aqua_spi_cli link soil 35 5

# 看通信统计（CRC 错误数量等）
aqua_spi_cli stats
```

CLI 完整子命令：`aqua_spi_cli --help`

### 6) systemd 部署（可选）

`/etc/systemd/system/aqua-spid.service`:

```ini
[Unit]
Description=AquaGarden SPI Daemon
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/aqua_spid
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now aqua-spid
journalctl -u aqua-spid -f      # 看日志
```

---

## 故障排查

| 现象 | 检查 |
|------|------|
| `打开 /dev/spidev0.0 失败` | dmesg 看 spi-phytium 是否加载；`ls /dev/spidev*` |
| 启动后 `RA6E2 在 5 秒内未上线` | 接线/共地；示波器看 SCK 有无；RA6E2 烧录是否成功 |
| `stats` 显示 `rx_crc_err` 持续增加 | 90% 是 DMAC 字宽未改 1 Byte（见 FSP_CHANGES.md §2.1）；其次是布线干扰 |
| `rx_sof_err` 增加 | SPI 错位；通常下一次 CS 边沿会自愈，但若持续→改速率到 500 kHz 再排查 |
| `consecutive_err` 反复触发 spi_reset | 物理层不稳；缩短接线、加去耦、检查电源 |

---

## 协议演进与版本

- **当前版本**：v1（`SPI_PROTO_VERSION = 0x01`）
- **修改协议的唯一入口**：`include/spi_protocol.h`
- **同步到 RA6E2**：`bash ra6e2_patch/sync_from_canonical.sh`
- **不兼容更改**：必须升级 `SPI_PROTO_VERSION`，并在 RA6E2 `app_dispatch` 保留旧 VER 的解析路径

详见 `设计_SPI通信工程方案.md` §7。
