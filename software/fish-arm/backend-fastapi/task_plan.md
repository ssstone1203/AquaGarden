# COM20 实时传感器链路实施计划

## 目标
实现并验证 `MCU -> COM20 -> FastAPI -> WebSocket -> front-vue` 实时传感器数据链路。

## 阶段
- [complete] 1. 审查现有串口协议、后端广播和前端消费逻辑
- [complete] 2. 编写失败测试，明确串口到 WebSocket 的行为契约
- [complete] 3. 实现 COM20 配置和线程安全的实时广播
- [complete] 4. 接入或修正 front-vue 实时显示与断线重连
- [complete] 5. 运行自动化测试和本机链路检查，更新文档
- [complete] 6. 以 `hardware/ra8p1/aquagarden` 为准审计实际上下行协议
- [complete] 7. 用真实帧编写解析与命令协议失败测试
- [complete] 8. 调整 FastAPI 串口解析、状态与控制协议
- [complete] 9. 使用 COM20 和 front-vue 完成端到端复测并更新文档
- [complete] 10. 审查现有 FastAPI 用户、注册、登录和 JWT 契约
- [complete] 11. 为管理员初始化和登录行为编写失败测试
- [complete] 12. 实现环境变量驱动的管理员初始化及认证加固
- [complete] 13. 在运行库创建管理员并完成真实登录/JWT验收
- [complete] 14. 审查雾化器固件协议、后端串口服务和系统控制界面
- [complete] 15. 编写雾化器命令、鉴权接口和状态确认测试
- [complete] 16. 实现 FastAPI `CMD 0x09` 下行与 telemetry 回读接口
- [complete] 17. 在 front-vue 系统控制页实现雾化器开关与状态反馈
- [complete] 18. 完成自动化、真实 COM20 和 Playwright 桌面/移动端验收
- [complete] 19. 实现树莓派与电脑 USB 双视频流
- [complete] 20. 将金鱼 YOLO 模型接入 USB 实时视频并叠加检测结果
- [complete] 21. 审计并验证 FastAPI 水泵、机械臂任务和滑轨控制
- [complete] 22. 合并树莓派机械臂状态与本机串口执行器状态并完成实机验收

## 验收条件
- 后端默认或项目配置使用 COM20、115200 baud，并可由环境变量覆盖。
- 串口断开后自动重连，不阻塞 FastAPI 请求处理。
- 每个有效传感器快照更新内存状态、按节流规则持久化并实时广播。
- WebSocket 提供明确的传感器端点，连接后立即收到当前快照，后续收到实时更新。
- 前端展示实时值并处理 WebSocket 断线重连。
- 自动化测试覆盖解析、广播和 WebSocket 公共契约。
- 雾化器开关仅接受布尔状态并要求 JWT，发送权威 `5A A5 02 09 state CRC16` 帧。
- 接口响应区分“命令已发送”和“上行 telemetry 已确认”，前端防止重复点击并显示实际状态。
- 系统控制界面在桌面与移动端无溢出、重叠，并能处理断线、401 和命令失败。

## 遇到的错误
| 错误 | 尝试次数 | 解决方案 |
|---|---:|---|
| Windows 沙箱无法并行创建多个 PowerShell 进程 | 1 | 改为顺序执行读取命令 |
| `Get-CimInstance Win32_SerialPort` 拒绝访问 | 1 | 使用 `python -m serial.tools.list_ports -v` 检测串口 |
| `rg` 对不存在的通配目录返回错误 | 1 | 后续只搜索已确认存在的 `../front-vue` |
| 实时测试首次收集失败：缺少 `sensor_message` | 1 | 预期的红灯测试；实现统一消息构造器 |
| 业务与计划合并补丁因计划状态已变化而中止 | 1 | 缩小补丁范围，业务代码与记录分开更新 |
| `compileall` 无法覆盖现有只读 `__pycache__` | 1 | 将 `PYTHONPYCACHEPREFIX` 指向系统临时目录后重跑 |
| COM20 首帧写 SQLite 时只读错误中断串口 | 1 | 隔离持久化失败，实时状态与 WebSocket 继续运行 |
| 首次真实 WebSocket 连续消息检查第二帧超时 | 1 | 启停后先等待 COM20 已连接且收到帧，再连接 WebSocket 分步验证 |
| 初次查找旧固件路径不存在 | 2 | 用户明确指定 `hardware/ra8p1/aquagarden`，后续仅以该目录为协议依据 |
| PowerShell 下对绝对路径使用 `*.md` 导致 `rg` 路径错误 | 1 | 改为显式读取权威目录内的 Markdown 文件 |
| 协议测试与任务记录合并补丁格式错误 | 1 | 拆分为测试代码和记录两个独立补丁 |
| 第二次多文件记录补丁分隔错误 | 1 | 继续保持业务测试与任务记录分别更新 |
| 数据库只读检查首次内嵌脚本转义失败 | 1 | 改为只读 SQLite URI 的多行脚本 |
| 新认证进程启动时 8090 被旧 PID 43052 占用 | 1 | 通过 netstat 定位真实监听进程，替换后重新验收 |
