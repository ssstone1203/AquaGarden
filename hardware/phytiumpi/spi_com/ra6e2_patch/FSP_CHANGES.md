# RA6E2 工程改造步骤（接入 SPI 通信协议）

> 目标工程：`hardware/demo_wyr/FreeRTOS drive/data_merge/`
> 操作环境：**Keil MDK (µVision, armclang/AC6) + Renesas RASC（Renesas Smart Configurator，独立版 FSP Configurator）**
> RASC 启动方式：双击工程根目录的 `rasc_launcher.bat`，它会用 `rasc_version.txt` 指定的版本打开 `configuration.xml`。

完成本节所有步骤后，在 Keil µVision 里点 **Build (F7)**，再用 J-Link/E2 烧录即可与飞腾派 daemon 联调。

---

## 0. 工作流总览

| 步骤 | 工具 | 修改的文件 |
|------|------|-----------|
| §1 同步源文件 | Shell / 资源管理器 | `src/*.{c,h}` |
| §2 配置改动 | RASC 或直接改 XML | `configuration.xml` + `ra_gen/Communicate_Task.c` + `ra_gen/pin_data.c` |
| §3 引脚核对 | RASC Pins 视图 | — |
| §4 Keil 工程登记 | µVision 或直接改 .uvprojx | `data_merge.uvprojx` |
| §5 编译烧录 | Keil + J-Link | — |

> **关键提示**：本仓库的提交里 `configuration.xml`、`ra_gen/Communicate_Task.c`、`ra_gen/pin_data.c`、`data_merge.uvprojx`、`src/*` 已**全部按本文档落地**。如果你只是 `git pull` 然后 Build 就能跑——**不必再做 §2.1 / §2.5**。
> 但如果你之后在 RASC 里点了一次 *Generate Project Content*，RASC 会用 XML 里的状态重生成 `ra_gen/`——XML 里我们也已经把 DMAC 字宽改成 1 Byte、把 P103 选成 SPI1.SSLB0，所以重生成后状态依然正确，不会丢。

---

## 1. 拷贝/同步源文件

把 `ra6e2_patch/` 下的 4 个文件复制到 µVision 工程的 `src/` 目录：

| 文件 | 目标位置 | 说明 |
|------|---------|------|
| `spi_protocol.h` | `src/spi_protocol.h` | 共享协议定义；**禁止手改**，永远从 `../include/spi_protocol.h` 同步 |
| `spi_codec.h`    | `src/spi_codec.h`    | 字节助手 + CRC + 帧打包/解析；**禁止手改** |
| `spi_codec.c`    | `src/spi_codec.c`    | 同上；**禁止手改** |
| `Communicate_Task_entry.c` | `src/Communicate_Task_entry.c` | **覆盖**原 UART 版本 |

如果以后协议升级，直接在 `spi_com/include/spi_protocol.h` 修改后运行
`bash ra6e2_patch/sync_from_canonical.sh`，再把 `ra6e2_patch/` 下变化的文件
复制到 `src/`。

---

## 2. RASC（FSP Configurator）改动

启动 RASC：

```
双击 hardware/demo_wyr/FreeRTOS drive/data_merge/rasc_launcher.bat
```

> 或者命令行：`rasc_launcher.bat configuration.xml`。
> 注意：由于本仓库已经把 `configuration.xml` 与 `ra_gen/` 都改好了，**你不需要在 RASC 里点 *Generate Project Content***。仅当你**手动改了 RASC 里任何字段**时才需要点一次重生成。

切到 **Stacks** 标签页：

### 2.1 修改 `g_com_spi` 的 DMAC 字宽

定位到 SPI1 (`g_com_spi`) 子节点 `Transfer (r_dmac) SPI1 RX (DMAC1)`：

| 属性 | 旧值 | 新值 |
|------|------|------|
| Properties → Source/Destination Transfer Size | **2 Bytes** | **1 Byte** |

同样修改 `Transfer (r_dmac) SPI1 TX (DMAC0)`：

| 属性 | 旧值 | 新值 |
|------|------|------|
| Properties → Source/Destination Transfer Size | **2 Bytes** | **1 Byte** |

> **理由**：协议是按字节流设计的（8-bit），如果 DMAC 仍按 2-Byte 搬运，
> 会出现"奇偶字节合并"的偏移错误，导致 CRC 永远校验失败。

修改后点 **Generate Project Content**。验证 `ra_gen/Communicate_Task.c`
中两处 `transfer_settings_word_b.size` 已变为 **`TRANSFER_SIZE_1_BYTE`**。

> 如果你不想启动 RASC，本仓库已直接把 `configuration.xml` 里那两处 `module.driver.transfer.size.size_2_byte` 改成 `size_1_byte`，并同步把 `ra_gen/Communicate_Task.c` 里两处 `TRANSFER_SIZE_2_BYTE` 改成 `TRANSFER_SIZE_1_BYTE`。Keil 直接 Build 就能用。

### 2.2 确认 SPI 模式为 Mode 1 + Slave

`g_com_spi` 节点 Properties：

| 属性 | 期望值 |
|------|--------|
| Operating Mode | **Slave** |
| Clock Phase    | **Data Sampling on Even Edge** (CPHA = 1) |
| Clock Polarity | **Low when idle** (CPOL = 0) |
| Bit Order      | **MSB First** |
| Mode Fault Error | Disable |
| SSL Select     | **SSL0**（驱动层选择 SSL 槽位 0；该槽位的物理引脚由 §2.5 决定） |

这些应当已经是当前值，仅作核对。RA6E2 FSP 的 `R_SPI_Open()` 在 Slave
模式下会拒绝 CPHA=0（`SPI_CLK_PHASE_EDGE_ODD`），否则 `g_com_spi.open()`
返回 `FSP_ERR_UNSUPPORTED`，通信任务会卡在 `configASSERT`，外部表现为主机
MOSI 正常但 MISO 不返回 `5A A5` 响应帧。

### 2.3 确认中断优先级

| 中断 | IPL | 备注 |
|------|-----|------|
| SPI1 TEI (transfer end) | 12 | 必须 ≥ `configMAX_SYSCALL_INTERRUPT_PRIORITY`（ISR 中调 FreeRTOS API） |
| SPI1 ERI (error)        | 12 | 同上 |
| DMAC0 / DMAC1 INT       | 8  | 默认即可 |

`SPI1 RXI / TXI` 在 DMAC 接管模式下保持 **Disabled**（生成代码里是 `BSP_IRQ_DISABLED`），不要改。

### 2.4 任务堆栈与优先级

`Communicate_Task` 默认堆栈 1024 字节，**保持不变**。

任务优先级在 `Communicate_Task_entry.c` 的 `vTaskPrioritySet(NULL, 3u);`
已自抬到 3（高于其他传感任务），**无需在 RASC 里改**（RASC 里仍是默认值 1）。

### 2.5 P103 引脚改作 SPI1 SSLB0（**新增，原文档遗漏**）

> RA6E2 BSP 里 SPI1 的 4 个 SSL 引脚命名为 `SSLB0..SSLB3`（不是 RA6M 系列的 `SSL10..SSL13`），
> 详见 `ra_cfg.txt` 第 118 行 `P103 ... "SPI1: SSLB0"`。

切到 **Pins** 标签页 → 在左侧 Pin Selection 里展开 `Ports` → `P1` → 选中 `P103`：

| 属性 | 期望值 |
|------|--------|
| Symbolic Name | （留空或随意） |
| Mode          | **Peripheral mode** |
| Pull up       | None |
| Drive Capacity | **Middle** |
| Output type   | CMOS |
| Input/Output  | （由外设决定） |
| Function (Module) | **SPI1: SSLB0** |

保存 → **Generate Project Content**。验证：

- `configuration.xml` 出现 `<configSetting altId="p103.spi1.sslb0" .../>` 与 `<configSetting altId="spi1.sslb0.p103" .../>`。
- `ra_gen/pin_data.c` 出现：

  ```c
  {
      .pin = BSP_IO_PORT_01_PIN_03,
      .pin_cfg = ((uint32_t) IOPORT_CFG_DRIVE_MID
                | (uint32_t) IOPORT_CFG_PERIPHERAL_PIN
                | (uint32_t) IOPORT_PERIPHERAL_SPI)
  },
  ```

> **理由**：Slave 模式下 SPI 外设依赖硬件 SSL 边沿来启动/结束事务。若 P103 留在
> GPIO 默认输入态，主机即使按时序拉低 CS，外设内部也无法识别到 SSLB0 的有效电平，
> `g_com_spi.p_api->writeRead()` 启动后会一直空转、收不到数据。

> 如果你不想启动 RASC，本仓库已直接把 `configuration.xml` 加上 P103.SSLB0 配置，并在 `ra_gen/pin_data.c` 加好 P103 项。Keil 直接 Build 就能用。

---

## 3. 引脚映射核对

| 信号 | RA6E2 引脚 | RA6E2 alt 功能 | 飞腾派排针 | 注意 |
|------|-----------|---------------|----------|------|
| SCK  | P102      | RSPCK1        | SPI0_SCK  | 接 ≤ 15 cm |
| MOSI | P101      | MOSI1         | SPI0_MOSI | 主→从 |
| MISO | P100      | MISO1         | SPI0_MISO | 从→主 |
| CS   | P103      | **SSLB0** (= SPI1 SSL 槽位 0) | SPI0_CSN0 | 低有效 |
| GND  | GND       | —             | GND       | **必须共地** |

短接线 + 共地是**信号完整性的下限**。

---

## 4. 删除/保留旧代码 + Keil 工程登记

### 4.1 旧代码

- **删除**：原 `Communicate_Task_entry.c` 中的 `host_uart0_*`、
  `host_parse_downlink_stream`、`host_apply_command` (扁平 CMD 版)、
  `host_build_uplink_frame` (UART 帧版) 已被新版整体替换，**整体覆盖即可，无需手动删除**。
- **保留**：SCI0 模块 (`g_com_uart0`) 仍在 FSP/RASC 配置里，作为日志通道。
  新版 `Communicate_Task_entry.c` 在任务启动时会调用 `app_log_uart_init()` 自动 open，
  并提供同步发送函数 `bool app_log_uart_write(const uint8_t *buf, uint16_t len)`，
  业务任意位置可用（≤ 5 ms / byte 阻塞，TDR 轮询，不依赖回调）。

### 4.2 Keil 工程登记

`data_merge.uvprojx` 默认只把 `src/Communicate_Task_entry.c` 列在源码组里。
新增的 `src/spi_codec.c` **必须**追加到 `:Renesas RA Smart Configurator:Common Sources` 组下，
否则 Keil 不会编译它，链接会报 `undefined symbol: spi_pack_rsp / spi_validate_frame / ...`。

| 文件 | FileType | 用途 |
|------|----------|------|
| `src/spi_codec.c` | 1（Source） | 必须加，参与编译 |
| `src/spi_codec.h` | 5（Header） | 加上方便 µVision 浏览，不参与编译 |
| `src/spi_protocol.h` | 5（Header） | 同上 |

> 仓库里这一项也已经替你改好（直接编辑了 `.uvprojx` 的 XML）。
> 如果你以后用 µVision GUI 右键源码组 *Add Existing Files* 增删了别的文件，
> 这里的修改不会被覆盖。

---

## 5. 编译/下载/验证

1. µVision 中 **Project → Build Target (F7)**，应零错误零告警。
2. J-Link / E2 烧录（µVision 的 Debug 按钮，或外部 J-Flash）。复位运行。
3. 飞腾派端启动 daemon：

   ```bash
   sudo ./aqua_spid -f -v
   ```
   应在 5 s 内看到 `RA6E2 上线，uptime=... ms`。
4. 另开终端：

   ```bash
   ./aqua_spi_cli sys ping        # 应返回 status=OK
   ./aqua_spi_cli sensor poll     # 应返回 SENSOR_DATA + 实测数值
   ./aqua_spi_cli stats           # tx/rx 计数应在累加
   ```

如出现 CRC_ERR 持续累积：

- 用示波器抓 SCK / MOSI / MISO / CS 看波形是否干净
- 把 daemon 速率调到 `-s 500000`（500 kHz）排除布线问题
- 确认 §2.1 的 DMAC 字宽改成了 1 Byte
- 确认 §2.5 的 P103 = SSLB0 已生效（dump `R_PFS->PORT[1].PIN[3]` 的 PSEL 字段应为 `0b00110` = SPI）
