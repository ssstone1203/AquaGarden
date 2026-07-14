# 进度记录：COM4 数据通路

## 2026-06-26
- 已确认仓库目标目录存在：`software/fish-arm/backend-fastapi`、`hardware/ra8p1_wyr/merge`。
- 已确认后续应聚焦 FastAPI 后端和 RA8P1 merge 固件，不修改 `software/ruisa`。
- 已确认 RA8P1 merge 固件发送 `55 AA 01 seq len=30 payload crc16` 的 38 字节帧。
- 已将 FastAPI 串口默认配置改为启用 COM4，并增加串口状态接口与帧校验统计。
- 已通过 `python -m compileall` 检查 FastAPI 代码语法。
- 已用模拟 30 字节 payload 验证后端解析结果：水温 24.7、空气温度 26.1、湿度 58.2、水质 76、土壤湿度 63。
- 已通过 FastAPI TestClient 验证 `/api/mcu/serial/status` 默认返回 COM4/115200 且 `/api/sensors` 可用。
- 用户更新实际连接端口为 COM4，已将默认串口和文档改为 COM4。
- 已实测 COM4 可收到硬件原始数据，115200 下出现连续 `55 AA 02`、payload 长度 30 的帧。
- 已将后端串口解析从仅接受协议版本 `0x01` 调整为接受 `0x01/0x02`。
- `python run_fastapi.py` 前台启动可正常初始化 Uvicorn；当前工具环境下 `Start-Process` 后台启动会立即退出且没有日志，因此未保留长驻后台进程。
