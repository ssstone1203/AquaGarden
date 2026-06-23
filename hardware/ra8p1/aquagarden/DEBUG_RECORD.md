# AquaGarden RA8P1 调试记录

> 日期：2026-06-24  
> 范围：`hardware/ra8p1/aquagarden/` RA8P1 FreeRTOS 固件  
> 主题：UART 上行调试、HardFault/INVSTATE 定位、任务架构调整

## 1. 初始问题

Keil Debug 运行后，程序反复停在 FSP startup 的不可恢复错误处理位置。随后确认属于 HardFault，进一步细分为 UsageFault 的 `INVSTATE`。

最初串口助手无法收到数据，后续在逐步规避 UART TX 中断和 FreeRTOS 通知路径后，串口助手可以收到多帧合法上行帧，例如：

```text
55 AA 02 00 1E ...
55 AA 02 01 1E ...
55 AA 02 02 1E ...
```

这说明 UART 物理发送、`55 AA VER=0x02` 上行组帧和 CRC 基本可工作。

## 2. 观察到的 Fault 位置

调试过程中先后停在以下自定义 trap：

- `aqua_debug_trap_invstate_uart_callback`
- `aqua_debug_trap_invstate_uart_send`
- `aqua_debug_trap_invstate_com_before_delay`
- `aqua_debug_trap_invstate_com_after_delay`
- `aqua_debug_trap_invstate_com_loop_top`
- `aqua_debug_trap_invstate_com_before_tx_counter`
- `aqua_debug_trap_invstate_com_before_pause`
- `aqua_debug_trap_invstate_uart_tend_wait`
- `aqua_debug_trap_invstate_sensor_water`

这些函数名代表“最后一次 debug mark 所在区域”，并不一定等同于真正 fault 的指令地址。后续已增强 `debug_probe`，记录 `stacked_pc`、`stacked_lr`、`stacked_xpsr`、`exc_return`、`msp`、`psp`、`control`、`primask`、`basepri`，用于后续精确判断异常返回帧是否损坏。

## 3. 已做定位与修复

### 3.1 UART TX 回调路径

最早 fault 出现在 `UART_EVENT_TX_COMPLETE` 回调附近。初步判断 FSP 的中断式 `g_uart0.p_api->write()` 会启用 TXI/TEI，并在 TEI ISR 中触发 `UART_EVENT_TX_COMPLETE`，该路径在当前工程中不稳定。

处理：

- 不再使用 FSP UART `write()` 做上行发送。
- 改为任务上下文中轮询 SCI 寄存器：
  - 等待 `CSR.TDRE`
  - 写 `TDR_BY`
  - 清 `CFCLR.TDREC`
  - 等待 `CSR.TEND`
- 显式关闭并清除 TXI/TEI 中断。

结果：

- UART 能输出合法上行帧。
- 发送链路阶段性跑通。

### 3.2 FreeRTOS semaphore 通知路径

`com_thread` 曾在主循环顶部附近随机停在 `com_loop_top`。当时 RX 完整帧通知使用：

- `xSemaphoreCreateBinaryStatic`
- `xSemaphoreGiveFromISR`
- `portYIELD_FROM_ISR`
- `xSemaphoreTake`

处理：

- 移除 RX semaphore。
- UART RX callback 只拷贝帧并置位 `volatile s_frame_pending`。
- 任务侧轮询 `s_frame_pending` 后处理下行帧。

结果：

- 不再停在 `com_loop_top`。
- 说明 semaphore/ISR 通知路径至少是一个触发点或放大因素。

### 3.3 `com_thread` 的 `vTaskDelay()` / tick API

`com_thread` 后续在 `vTaskDelay()` 前后、`xTaskGetTickCount()` 附近出现 `INVSTATE`。尝试过：

- 移除 `vTaskDelay(1ms)`，改用低优先级后台空循环。
- 移除 `xTaskGetTickCount()`，改用 loop 计数控制发送周期。
- 将 `com_thread` 优先级降为 1，`sensor_thread` 保持 2。

结果：

- 独立 `com_thread` 仍会随机在后台循环附近触发 `INVSTATE`。
- 该方案不适合作为当前阶段的稳定架构。

### 3.4 DS18B20 水温阻塞采样

系统一度停在 `sensor_water`。当时 DS18B20 驱动在 `dev_ds18b20_measure()` 内阻塞等待 750ms 温度转换。

处理：

- 将 DS18B20 改成非阻塞状态机：
  - 第一次调用发起转换，返回 `FSP_ERR_IN_USE`
  - 750ms 后的后续调用读取 scratchpad
- `app_sample_water_temp()` 对 `FSP_ERR_IN_USE` 不计入失败重试。

结果：

- 避免 `sensor_thread` 长时间阻塞。
- 水温采样更符合 FreeRTOS 周期任务模型。

### 3.5 当前阶段性稳定方案

最终为了停止独立 `com_thread` 带来的上下文切换问题，已将 COM 服务并入 `sensor_thread`：

- 新增 `com_app_init()`
- 新增 `com_app_process_10ms()`
- `sensor_app_entry()` 中：
  - 初始化时调用 `com_app_init()`
  - 每 10ms 调用 `com_app_process_10ms()`
- 生成的 `com_thread` 启动后调用 `vTaskSuspend(NULL)`，不再后台轮询。
- 上行发送改为每 25 个 10ms 周期发送一次，即约 250ms 一帧。

这是当前推荐保留的 bring-up 架构。

## 4. 关键结论

1. UART 协议与物理发送已验证可工作。
2. FSP UART 中断式 TX callback 路径在当前工程中不稳定，bring-up 阶段不建议继续使用。
3. `com_thread` 独立后台轮询会引入复杂的上下文切换、tick、ISR 通知交织，是当前 `INVSTATE` 的主要放大因素。
4. DS18B20 750ms 阻塞转换不适合直接放入 10ms 任务，应保持非阻塞。
5. 当前应优先保证系统稳定运行，再逐步恢复下行 RX、任务拆分、RTOS 通知等复杂度。

## 5. 后续建议

- 保留 `com_app_init()` / `com_app_process_10ms()` 单任务驱动 COM 的方案，直到全系统稳定。
- 保留 UART TX 轮询发送，暂不恢复 FSP `write()` + TXI/TEI。
- 暂不恢复 RX semaphore，继续使用 `volatile` pending 标志。
- 若后续仍发生 `INVSTATE`，优先记录并分析：
  - `g_aqua_debug_probe.stacked_pc`
  - `g_aqua_debug_probe.stacked_lr`
  - `g_aqua_debug_probe.stacked_xpsr`
  - `g_aqua_debug_probe.exc_return`
  - `g_aqua_debug_probe.psp`
  - `g_aqua_debug_probe.msp`
- 稳定后再清理大量临时 trap 函数，保留少量通用 fault 记录即可。

## 6. 对原需求“两任务”设计的架构评估

`需求.md` 中要求“实现 2 个（或已商讨确定的）FreeRTOS 任务”，验收标准也写了“任务数 ≤ 2”。这个方向本身没有错，但“固定拆成 sensor_app + com 两个长期运行任务”在本工程当前阶段不够稳妥。

我的判断：

- 对这个项目，**任务数 ≤ 2 是合理约束**，因为外设多、内存和调试资源有限，任务过多会增加栈、同步和竞态成本。
- 但不应坚持“COM 必须是独立常驻任务”。通信量很低，上行 250ms 一帧，下行命令也不高频，完全可以由一个 10ms 主服务任务驱动。
- 当前更适合的架构是：
  - 一个主周期任务：传感器状态机、控制逻辑、COM 组帧/发送、下行命令处理
  - 必要时保留一个专用任务给 USB Host 或摄像头等确实需要阻塞/协议栈轮询的模块
- DS18B20、SHT30、ADC、TDS、泵、雾化器、UART 都应尽量做成“短步骤状态机”，不要在任务内长时间阻塞。
- 需求里把“任务数”写成验收项可以保留，但建议改成“任务模型需经实测稳定确认”，不要把通信任务独立化作为硬约束。

简而言之：我会坚持“少任务”，但不会坚持“一定两个任务且 COM 独立任务”。对当前 RA8P1 工程，单主循环任务 + 非阻塞外设状态机，比两个任务加 ISR/semaphore 通知更适合 bring-up 和比赛交付。
