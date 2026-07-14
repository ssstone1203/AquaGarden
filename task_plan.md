# 任务计划：COM4 传感器数据通路

## 目标
在 `software/fish-arm/backend-fastapi` 和 `hardware/ra8p1_wyr` 之间建立稳定数据通路，使 RA8P1 固件通过 USB 串口 COM4 将传感器数据送入 FastAPI 后端。

## 阶段
- [complete] 阶段 1：确认现有 FastAPI 结构与 RA8P1 串口协议
- [complete] 阶段 2：实现/修复 COM4 串口采集、解析、状态更新与接口暴露
- [complete] 阶段 3：补充运行说明并执行最小验证

## 决策
- 优先复用 `hardware/ra8p1_wyr/merge` 中已有 UART 二进制帧协议。
- 后端只接受经过帧头、长度、CRC、范围校验的数据，避免将串口噪声直接写入状态。
