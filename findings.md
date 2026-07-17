# 发现与决策

## 需求
- 在 Vue 系统控制界面增加 USB 报警灯控制卡片。
- 在 Vue 仪表盘的数据传感器行增加水质显示卡片，逻辑与其他传感器一致。
- 将仪表盘 AI 问答通过 FastAPI 重构：使用后端内部 API Key、实时传感器上下文和多轮聊天历史与用户交流。

## 研究发现
- 目标固件目录为 `hardware/ra8p1/aquagarden`，包含 `dev_usb_light_driver.*`、通信协议和命令表。
- 目标前端为 `software/fish-arm/front-vue`，系统控制页是 `RobotView.vue`，仪表盘是 `DashboardView.vue`。
- 工作树已有与目标页面及后端 API 相关的未提交修改，后续必须增量兼容。
- 固件命令 `COM_CMD_USB_LIGHT_MODE` 的命令字为 `0x08`，负载为灯模式；合法模式为 `0..26`，其中 `26` 是关闭。
- 固件遥测包含 `tds_ntu`、`usb_light_mode` 和 `alarm_flags`；USB 灯未就绪时模式为 `0xFF`。
- 后端串口服务已经解析以上字段，并把 `tds_ntu` 映射为传感器状态中的 `wqi`。
- USB 灯驱动定义了 27 个模式帧：`0..23` 为三组灯效档位、`24..25` 为另外两种灯效、`26` 为关闭；固件只暴露数字模式，没有内置中文名称。
- 固件调用 `dev_usb_light_set_mode()` 后只有在 USB 设备 ready 且写入成功时才更新当前模式；因此前端应展示未就绪、故障和已确认模式，不能只做乐观切换。
- `RobotView.vue` 当前已有用户新增的雾化器控制卡片和轮询状态同步逻辑，USB 报警灯应按同一页面风格增量加入。
- 现有后端改动已经为雾化器提供状态确认模式，可作为 USB 灯端到端控制的实现参考。
- `DashboardView.vue` 的传感器行目前实际只有水温、空气温度、空气湿度、土壤湿度四张卡片，注释写的是 five；水质正好补齐第五张。
- 仪表盘传感器通过 `/api/sensors` 每 5 秒刷新，并通过 WebSocket `sensor_data` 快照即时更新；水质卡必须同时接入这两条路径、稳定默认值、历史曲线、详情弹窗元数据和“是否有有效数据”判断。
- 后端 `/api/aqua/status` 目前只顶层合并 `pump` 与 `atomizer`，没有 USB 灯顶层状态；也没有 USB 灯控制路由。
- 固件命令表提供完整模式名称：0/1/2 红黄绿常亮，3 红常亮+喇叭，4..7 白/青/紫/蓝常亮；8..15 对应慢闪；16..23 对应快闪；24 仅开喇叭，25 关喇叭，26 关灯关喇叭。
- 串口遥测每约 250 ms 上报一次，适合复用雾化器的 condition/revision 确认机制；USB 灯需要独立命令锁，避免与雾化器并发操作相互阻塞。
- 后端 `HardwareSerialService.status()` 已经组合设备状态对象，新增 `usbLight`（前端风格）或 `usb_light` 时应与现有 `atomizer` 顶层结构一致，并保留底层 `serial.telemetry`。
- 水质值是 TDS ADC 标准曲线换算后的浊度 NTU，不是 0..100 的综合质量评分；卡片单位和详情范围应明确使用 `NTU`。
- 后端已有雾化器 API 的认证、严格类型、串口帧和遥测确认测试，可按同一 Arrange-Act-Assert 结构覆盖 USB 灯。
- 前端使用 Node 内置测试运行器，现有纯函数测试位于 `front-vue/tests`；USB 灯 27 模式表适合抽成纯 JS 模块并验证完整性、关闭模式和标签。
- 全局传感器网格当前桌面为 4 列；为满足“同一行”应调整为 5 列，并在 1280px 以下回落为 3/2/1 列以避免卡片内容挤压。
- 雾化器测试已经覆盖“写帧后仅接受更新版本遥测”的并发确认方式，USB 灯测试将复用该可观察行为而不依赖私有实现细节。
- 2026-07-17 用户确认实际控制后端必须是 `software/fish-arm/backend-fastapi`，不走 Spring Boot。
- 直接请求 `http://127.0.0.1:8090/api/aqua/usb-light` 返回 Uvicorn 的 `404 {"detail":"Not Found"}`；说明 8090 当前是 FastAPI，但运行进程尚未加载新增路由。
- FastAPI 源码已经包含认证路由、COM `0x08` 下行帧、模式 0..26 校验和下一帧遥测确认逻辑；需要验证运行进程与源码版本一致并重启服务。
- `netstat` 确认 8090 的监听 PID 为 `62980`。
- PID 62980 是 `D:\soft\anaconda3\python.exe`，启动时间为 2026-07-16 23:07。
- 运行中 FastAPI 的 OpenAPI 包含 `/api/aqua/atomizer`，但不包含 `/api/aqua/usb-light`；当前源码明确包含 USB 灯路由，因此 404 根因是旧 Uvicorn 进程未重载新代码。
- 当前 FastAPI 默认启用硬件串口并使用 COM20/115200，前端 `/api/aqua/usb-light` 与源码路由路径一致。
- 旧 FastAPI PID 62980 已停止；新进程 PID 17068 已启动并监听 8090。
- 新运行 OpenAPI 已包含 `/api/aqua/usb-light`；无认证 POST 返回 401 而非 404，证明路由已加载且受认证保护。
- 新 FastAPI 在沙箱内以后台子进程启动后会随父进程退出；首次 `mode=26` 验证在登录前即连接失败，没有发送硬件命令。
- FastAPI 已改为沙箱外持久后台进程，当前监听 PID 为 33916。
- 实际串口状态为 COM20/115200、connected=true，并持续接收 MCU 上行帧。
- 实际发送安全模式 `26`（关灯关喇叭）成功：HTTP 200、`confirmed=true`，随后遥测 `usb_light_mode=26`、ready=true、无 `usb_light_fault`。
- 经前端 5173 Vite 代理调用 `/api/aqua/usb-light`，越界模式返回 422；同一代理读取 `/api/aqua/status` 得到 mode=26、ready=true、fault=false，确认前端请求路径已打通。
- FastAPI 完整测试 46 项通过，PID 33916 在最终检查时仍监听 8090，404 已消除。
- FastAPI 已存在认证路由 `/api/ai/ecosystem-analysis` 与 `/api/ai/chat`，Vue 仪表盘也已有多轮消息 UI；本阶段属于完善并重构现有链路，而不是从零新增界面。
- 当前 AI 服务把固件 `tds_ntu` 映射后的 `wqi` 错误描述为“水质综合指数 /100”，应改为“TDS 水质浊度 NTU”。
- AI 路由使用 `has_hardware_snapshot()`，会把任意历史硬件快照当成实时；应与传感器接口一致，按配置的新鲜度窗口选择实时快照或演示基线。
- 当前大模型异常会把截断后的异常文本返回前端，存在暴露上游 URL/内部细节的风险，应记录服务端日志并返回通用错误说明。
- `llm_api_key` 当前是普通 `str` 且只绑定 `ANTHROPIC_API_KEY`；应使用 `SecretStr` 并兼容统一内部变量 `AQUAGARDEN_LLM_API_KEY` 以及 Anthropic/OpenAI 常用环境变量。
- `.env.example` 当前没有 LLM 配置示例，需要补充空密钥占位，禁止提交真实密钥。
- Vue 已实现 Enter 发送、Shift+Enter 换行、最近 12 条历史、复制与清空；前端主要缺少“实时传感器/演示基线”的明确上下文状态。
- 当前运行 FastAPI OpenAPI 已包含 `/api/ai/chat` 与 `/api/ai/ecosystem-analysis`，说明路由已加载；本次需要更新服务契约和运行进程。
- 后端测试尚无 AI 专用文件，可新增独立 `test_ai_chat.py`，复用现有认证覆盖 fixture 并只 mock 外部模型边界。
- FastAPI 重构后的 AI 目标测试已通过：新鲜快照、NTU 语义、12 条历史、错误脱敏、SecretStr 和认证均受覆盖。
- 当前 `backend-fastapi/.env` 对应的模型配置已检测为 configured=true，provider=anthropic，model=kimi-for-coding；检查过程只输出布尔值和模型名，没有读取或打印密钥。
- README 只列出 AI 路由和回退说明，尚未说明内部密钥变量与传感器上下文契约。
- 更新后的 FastAPI PID 14028 已启动；真实 `/api/ai/chat` 能读取 hardware 快照（水温 20.2°C、TDS 126 NTU 等），但首次上游调用返回 error 并安全回退到规则结果。
- 前端响应中未出现上游 URL、密钥或异常正文，错误脱敏生效。

## 技术决策
| 决策 | 理由 |
|------|------|
| USB 灯控制等待 MCU 遥测确认后再更新 UI | 固件可能未就绪或写入失败，避免显示虚假成功 |
| 水质卡使用后端现有 `wqi` 字段 | 后端已将固件 `tds_ntu` 映射到该字段，与现有传感器 API 一致 |
| 水质卡补全所有现有传感器数据路径，而非仅加静态模板 | 保持 HTTP、WebSocket、历史曲线和详情弹窗行为一致 |
| USB 灯 API 使用 Pydantic 严格整数及 0..26 范围 | 防止布尔值、字符串和越界模式进入硬件边界 |
| USB 灯前端使用分组选项并保留快捷关闭 | 完整覆盖固件能力，同时保持控制页的操作密度 |
| 不改前端路径或切换 Spring，重启 FastAPI 使现有路由生效 | 用户确认实际操作由 FastAPI 完成，源码和前端路径已一致 |
| 保留现有安静、工作台式聊天面板，只增强状态和数据来源反馈 | 前端已具备完整交互，避免无关重设计 |
| API Key 使用 `SecretStr` 与环境变量 AliasChoices | 降低日志/repr 泄露风险，并兼容内部部署方式 |

## 遇到的问题
| 问题 | 解决方案 |
|------|---------|
| Windows 上 `rg` 的 `hardware/.../*.md` 参数触发路径语法错误 | 改用显式文件路径或目录级检索 |
| PowerShell 嵌套单区间数组被自动展开，导致 `[Math]::Min` 参数类型不匹配 | 改用显式 `Select-Object -Skip/-First` 读取固定区间 |
| 规划技能的 `session-catchup.py` 在本机 `.claude` 路径不存在 | 已直接读取三个规划文件恢复上下文 |
| WMI 查询 8090 进程命令行被系统拒绝访问 | 改用 `Get-Process`、HTTP Server 头和 OpenAPI 判断进程类型 |
| 沙箱内无法停止旧 FastAPI 进程 | 经用户授权后在沙箱外停止并重启 |
| 沙箱内启动的 FastAPI 后台子进程不持久 | 改为沙箱外隐藏后台进程启动 |
| PowerShell 使用 `$home` 临时变量与只读 `$HOME` 冲突 | 不重复该赋值；后续代理登录、USB 校验与状态读取均已成功 |
| 真实模型调用返回错误并走规则回退 | 正在以脱敏 HTTP 状态定位兼容地址/鉴权问题 |

## 资源
- `hardware/ra8p1/aquagarden/device/dev_usb_light_driver.c`
- `hardware/ra8p1/aquagarden/通信协议.md`
- `software/fish-arm/front-vue/src/views/RobotView.vue`
- `software/fish-arm/front-vue/src/views/DashboardView.vue`

## 视觉/浏览器发现
- 前端开发服务已在 `http://127.0.0.1:5173` 返回 HTTP 200，准备检查桌面和移动布局。
- 1920px 仪表盘中五张传感器卡同排显示，水质卡文字、单位和图标无溢出。
- 系统控制页的 USB 灯卡片结构和 27 个模式选项完整，但首次检查发现缺少遥测时 `null` 被误解析为 mode 0；已增加边界测试并修复。
- 390px 控制页中 USB 灯卡片能正确纵向重排；同时发现现有水泵命令横向溢出、零宽侧栏子元素仍可见、状态栏文字过宽，已做小屏局部修正。
- 修正后 390px 控制页的水泵按钮为两列，侧栏完全隐藏，状态栏仅保留连接状态和系统时间；USB 灯与雾化器卡片均无重叠。
- 390px 仪表盘五张传感器卡逐行显示，水质卡和详情入口文本完整。

## FastAPI AI 运行结论（2026-07-17）
- AI 运行链路不依赖 Spring：Vue 仅调用 FastAPI 的 `POST /api/ai/chat`，FastAPI 自行读取新鲜 MCU 传感器快照并调用模型。
- Kimi Coding 的 Anthropic 兼容端点为 `https://api.kimi.com/coding/v1/messages`，默认应使用 `Authorization: Bearer`；直连 Anthropic 时可通过 `AQUAGARDEN_LLM_AUTH_MODE=x-api-key` 切换。
- 当前进程中的 `ANTHROPIC_AUTH_TOKEN` 对最小请求和实际聊天请求均返回 `401 authentication_error`，服务端明确提示凭据无效或已过期；这不是 FastAPI 路由、模型名或传感器上下文问题。
- 实际 FastAPI 聊天读取到硬件快照并安全回退：水温 20.1 °C、TDS 125 NTU、气温 21.3 °C、空气湿度 72.6%、土壤湿度 76%。响应与日志均未包含 API Key 或上游 URL。

---
*每执行2次查看/浏览器/搜索操作后更新此文件*
