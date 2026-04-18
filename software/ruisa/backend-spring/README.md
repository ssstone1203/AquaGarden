# Ruisa Spring Boot 网关

将浏览器对 `/api/v1/ws/agent/chat` 与 `/api/v1/camera/mjpeg` 的请求**透明代理**到本仓库 Python FastAPI（`software/ruisa/backend`），机械臂与 `agent/tasks.py` 逻辑仍在 Python 中执行，界面与协议不变。

## 启动顺序

1. Python：`cd software/ruisa/backend` → `uvicorn app.main:app --host 0.0.0.0 --port 8000`
2. Spring：`cd software/ruisa/backend-spring` → `mvn spring-boot:run`（默认 `8080`）
3. Vue：`cd software/ruisa/front-vue` → `npm install` → `npm run dev`（`5173`，已配置将 `/api`、`/docs` 等代理到 Spring）

配置：`src/main/resources/application.properties` 中 `ruisa.agent.base-url` 指向 Python 根地址。
