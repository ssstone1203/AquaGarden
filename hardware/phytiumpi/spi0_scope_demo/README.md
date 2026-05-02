# SPI0 示波器 Demo（飞腾派 + spidev）

在 **`/dev/spidev0.0`**（SPI0、CS0）上周期性发送固定字节，SCK 默认约 **100 kHz**，突发之间有间隙，便于示波器触发、数 bit。

## 编译（在飞腾派本机）

```bash
cd AquaGarden/hardware/phytiumpi/spi0_scope_demo
make
```

## 运行

设备节点一般为 `root` 独占，需 **`sudo`**：

```bash
sudo ./spi_scope_demo
```

默认：100 kHz、每帧 8 字节（`A5 5A FF 00 33 CC AA 55`）、帧间隔 40 ms、无限循环。按 `Ctrl+C` 结束。

### 参数

| 参数 | 含义 |
|------|------|
| `-d` | spidev 路径，默认 `/dev/spidev0.0` |
| `-s` | SPI 时钟（Hz），例如 `-s 200000` |
| `-g` | 每次发送后的间隙（毫秒），调大后波形在时间上拉得更开 |
| `-c` | 发送次数；`0`（默认）表示不停 |
| `-m` | SPI 模式 0–3；**默认 0 时不发 mode ioctl**（飞腾 `spidev` + GPIO CS 时发 `WR_MODE` 易触发 `EINVAL`） |

## 说明（飞腾派 Invalid argument）

部分飞腾 SPI 驱动在 `spidev` 里对 **`SPI_IOC_WR_MODE` / `SPI_IOC_WR_MODE32`** 会与内核片选逻辑组合出 **`SPI_CS_HIGH`**，而控制器 **`mode_bits` 不含该位**，`spi_setup` 会返回 **EINVAL**。本程序在 **`-m 0`（默认）** 下**不再调用** mode 相关 ioctl，只设字长与速率。
| `-h` | 帮助 |

示例：只发 20 次、200 kHz、间隔 100 ms：

```bash
sudo ./spi_scope_demo -s 200000 -g 100 -c 20
```

## 示波器探头

- **GND**：与飞腾派共地。
- **SCK**：SPI 时钟。
- **MOSI**：主出从入；单看发送字节应主要为 **MOSI** 上跳变。
- **CS（SPI0_CSN0）**：每帧一次片选，便于触发。

若与 RA6E2 直连，注意电平与接线；单独测线时未接从机也可能仍有边沿（视硬件上拉/负载而定）。

## 依赖

内核启用 **`spidev`**，设备树中 SPI0 已挂用户态节点（与本仓库飞腾派设备树修改一致）。
