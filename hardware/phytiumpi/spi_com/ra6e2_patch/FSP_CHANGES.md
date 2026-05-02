# RA6E2 工程改造步骤（接入 SPI 通信协议）

> 目标工程：`hardware/demo_wyr/FreeRTOS drive/data_merge/`
> 操作环境：Renesas e² studio + FSP Configurator

完成本节所有步骤后，重新编译烧录即可与飞腾派 daemon 联调。

---

## 1. 拷贝/同步源文件

把 `ra6e2_patch/` 下的 4 个文件复制到 e2studio 工程的 `src/` 目录：

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

## 2. FSP Configurator 改动

打开工程的 `configuration.xml` (e2studio 双击即进入 FSP Configurator)，
切到 **Stacks** 标签页：

### 2.1 修改 `g_com_spi` 的 DMAC 字宽

定位到 SPI1 (`g_com_spi`) 子节点 `Transfer (r_dmac) SPI1 RX (DMAC1)`：

| 属性 | 旧值 | 新值 |
|------|------|------|
| Properties → Source/Destination Transfer Size | **2 Bytes** | **1 Byte** |

同样修改 `Transfer (r_dmac) SPI1 TX (DMAC0)`：
| Properties → Source/Destination Transfer Size | **2 Bytes** | **1 Byte** |

> **理由**：协议是按字节流设计的（8-bit），如果 DMAC 仍按 2-Byte 搬运，
> 会出现"奇偶字节合并"的偏移错误，导致 CRC 永远校验失败。

修改后点 **Generate Project Content**。验证 `ra_gen/Communicate_Task.c`
中两处 `transfer_settings_word_b.size` 已变为 **`TRANSFER_SIZE_1_BYTE`**。

### 2.2 确认 SPI 模式仍为 Mode 0 + Slave

`g_com_spi` 节点 Properties：

| 属性 | 期望值 |
|------|--------|
| Operating Mode | **Slave** |
| Clock Phase    | **Data Sampling on Odd Edge** (CPHA = 0) |
| Clock Polarity | **Low when idle** (CPOL = 0) |
| Bit Order      | **MSB First** |
| Mode Fault Error | Disable |

这些应当已经是当前值，仅作核对。

### 2.3 确认中断优先级

| 中断 | IPL | 备注 |
|------|-----|------|
| SPI1 TEI (transfer end) | 12 | 必须 ≥ `configMAX_SYSCALL_INTERRUPT_PRIORITY`（ISR 中调 FreeRTOS API） |
| SPI1 ERI (error)        | 12 | 同上 |
| DMAC0 / DMAC1 INT       | 8  | 默认即可 |

### 2.4 任务堆栈与优先级

`Communicate_Task` 默认堆栈 1024 字节，**保持不变**。

任务优先级在 `Communicate_Task_entry.c` 的 `vTaskPrioritySet(NULL, 3u);`
改为 3（高于其他传感任务），**无需在 FSP 里改**。

---

## 3. 引脚映射核对

| 信号 | RA6E2 引脚 | 飞腾派排针 | 注意 |
|------|-----------|----------|------|
| SCK  | P102 (RSPCK1) | SPI0_SCK  | 接 ≤ 15 cm |
| MOSI | P101 (MOSI1)  | SPI0_MOSI | 主→从 |
| MISO | P100 (MISO1)  | SPI0_MISO | 从→主 |
| CS   | P103 (SSL10)  | SPI0_CSN0 | 低有效 |
| GND  | GND           | GND       | **必须共地** |

短接线 + 共地是**信号完整性的下限**。

---

## 4. 删除/保留旧代码

- **删除**：原 `Communicate_Task_entry.c` 中的 `host_uart0_*`、
  `host_parse_downlink_stream`、`host_apply_command` (扁平 CMD 版)、
  `host_build_uplink_frame` (UART 帧版) 已被新版整体替换，无需手动删除。
- **保留**：SCI0 模块 (`g_com_uart0`) 仍在 FSP 配置里，可以用作 `printf`
  调试日志通道。如不需要，也可在 FSP 里禁用以释放引脚。

---

## 5. 编译/下载/验证

1. e2studio 中 Project → Build All（应零错误零告警）。
2. J-Link / E2 烧录，复位运行。
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
