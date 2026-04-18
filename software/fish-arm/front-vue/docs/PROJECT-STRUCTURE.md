# AquaGarden 前端（Vue 3 + Vite）项目结构与说明

本文档说明 `front-vue` 工程的目录职责、主要模块功能以及运行与构建所需环境。

初学者建议配合阅读：**[VUE3-入门与实操指南.md](./VUE3-入门与实操指南.md)**（核心概念、启动链、路由与实操任务）。

---

## 一、运行环境

| 依赖 | 版本要求 | 说明 |
|------|----------|------|
| **Node.js** | 18 LTS 或 20 LTS（推荐） | 与 Vite 8、Vue 3.5 生态兼容 |
| **npm** | 随 Node 安装（或 pnpm/yarn，需自行对照锁文件） | 本仓库默认使用 **npm** 与 `package-lock.json` |
| **浏览器** | 现代 Chromium / Firefox / Safari / Edge | 开发调试与生产访问 |

**与后端联调：**

- 开发时默认假设 Spring Boot 监听 **`http://localhost:8090`**。
- `vite.config.js` 中配置了 **`server.proxy`**：将前端的 **`/api`**、**`/ws`** 代理到该地址，避免跨域与手写完整域名。
- WebSocket 在开发环境下由 `src/api/http.js` 中的 **`wsLogsUrl()`** 指向 **`ws://localhost:8090/ws/logs`**（与 Vite 开发服务器端口不同属正常情况）。

**可选环境变量（`.env` / `.env.production`）：**

| 变量 | 含义 |
|------|------|
| **`VITE_API_BASE`** | API 根地址；不设时开发环境用空字符串走同源代理，生产可设为 `https://your-api.example.com` |
| **`VITE_WS_BASE`** | WebSocket 根（不含路径）；不设则按 `http.js` 内逻辑拼接 |

---

## 二、仓库根目录一览

```
front-vue/
├── index.html                 # HTML 入口：挂载 #app、全局 title、外链字体等
├── vite.config.js             # Vite 配置：Vue 插件、路径别名 @、开发服务器与代理
├── package.json               # 项目元数据、脚本、依赖声明
├── package-lock.json          # npm 锁定版本（建议提交仓库）
├── .gitignore                 # Git 忽略规则（通常含 node_modules、dist）
├── public/                    # 公共静态资源（不经打包处理，按原路径访问）
├── dist/                      # 生产构建输出（npm run build 生成，勿手改源码）
├── node_modules/              # npm 安装的依赖包（勿提交、勿手改）
├── docs/
│   └── PROJECT-STRUCTURE.md   # 本说明文档
└── src/                       # 源代码主目录
    ├── main.js                # JS 入口：创建 Vue 应用、注册路由、引入全局样式
    ├── App.vue                # 根组件：仅承载 <router-view />
    ├── api/                   # 与后端通信的封装
    ├── assets/                # 需经构建处理的资源（图片、全局 CSS 等）
    ├── components/            # 可复用布局/通用组件
    ├── router/                # 路由定义与导航守卫
    └── views/                 # 各页面级视图组件
```

---

## 三、根目录文件

### `index.html`

- **作用**：Vite 应用的 HTML 壳。
- **功能**：引入 `src/main.js`、设置页面语言为中文、配置 viewport、外链 Font Awesome 等；**`<div id="app">`** 为 Vue 挂载点。

### `vite.config.js`

- **作用**：Vite 构建与开发服务器配置。
- **功能**：
  - 注册 **`@vitejs/plugin-vue`** 以编译 `.vue` 单文件组件。
  - **`resolve.alias['@']`** 指向 **`src`**，便于 `import xxx from '@/...'`。
  - **`server.port`**：开发服务器端口（默认 5173）。
  - **`server.proxy`**：将 **`/api`**、**`/ws`** 代理到后端，便于本地联调。

### `package.json`

- **作用**：npm 项目清单。
- **功能**：声明依赖（**vue**、**vue-router** 等）、脚本 **`dev`** / **`build`** / **`preview`**。

---

## 四、`public/`（公共静态目录）

- **作用**：不参与模块打包的文件，构建时**原样复制**到 `dist/` 根路径。
- **典型内容**：`favicon.svg`、无需 import 的固定资源。
- **注意**：不要放仅被 `import` 引用的业务资源（应放 `src/assets`），否则无法享受哈希文件名与优化。

---

## 五、`src/`（源代码）

### 5.1 `src/main.js`

- **作用**：前端应用入口脚本。
- **功能**：`createApp(App)`、**`use(router)`**、挂载 **`#app`**；全局引入 **`./assets/styles/main.css`**（全站布局与侧栏等样式）。

### 5.2 `src/App.vue`

- **作用**：根组件。
- **功能**：模板中仅 **`<router-view />`**，由路由决定渲染登录页、注册页或带侧栏的布局子页面。

### 5.3 `src/api/http.js`

- **作用**：与后端地址、鉴权头、WebSocket URL 相关的工具函数。
- **功能**：
  - **`apiUrl(path)`**：拼接 API 路径（结合 `VITE_API_BASE`）。
  - **`authHeaders()`**：带 **`Authorization: Bearer <token>`** 与 JSON Content-Type。
  - **`wsLogsUrl()`**：根据是否开发环境、是否配置 `VITE_WS_BASE` 返回日志 WebSocket 完整 URL。

### 5.4 `src/assets/`（静态资源）

| 子路径 | 作用 |
|--------|------|
| **`assets/styles/main.css`** | 从旧版静态站迁移的全局主样式（侧栏、主内容区、仪表板卡片等）。 |
| **`assets/styles/login.css`** | 登录/注册页背景与表单区域样式。 |
| **`assets/styles/register-page.css`** | 注册页额外布局与表单样式。 |
| **`assets/styles/*-page.css`** | 各业务页独立样式：`cameras-page`、`robot-page`、`history-page`、`alerts-page`、`settings-page` 等。 |
| **其他图片/svg** | Vite 脚手架遗留或占位资源；可按需清理或替换。 |

### 5.5 `src/components/`

| 文件 | 作用 |
|------|------|
| **`AppLayout.vue`** | 已登录后的公共骨架：左侧导航、顶栏标题/副标题、演示与服务模式切换、退出登录；中间区域为 **`<router-view />`** 渲染子页面。 |
| **`HelloWorld.vue`** | Vite 默认示例组件；**当前路由未使用**，可删除以保持仓库整洁。 |

### 5.6 `src/router/index.js`

- **作用**：Vue Router 单页路由配置。
- **功能**：
  - 定义 **`/login`**、**`/register`** 与 **`/`** 下子路由（仪表板、摄像头、历史、机械臂、警报、设置）。
  - **`beforeEach` 导航守卫**：无 `localStorage.token` 时跳转登录；已登录访问登录/注册可重定向首页（可按需调整）。

### 5.7 `src/views/`（页面视图）

每个文件对应一个功能界面，与旧版 HTML 页面一一对应。

| 文件 | 作用 |
|------|------|
| **`LoginView.vue`** | 登录表单、调用 **`POST /api/login`**、记住用户名。 |
| **`RegisterView.vue`** | 注册表单、密码强度展示、调用 **`POST /api/register`**。 |
| **`DashboardView.vue`** | 仪表板：传感器轮询、双路视频、控制盘、系统日志与 WebSocket、键盘快捷键等。 |
| **`CamerasView.vue`** | 视频监控：双画面 MJPEG、录制/截图 UI（演示逻辑）。 |
| **`HistoryView.vue`** | 历史数据：筛选、表格分页、柱状趋势（前端模拟数据）。 |
| **`RobotView.vue`** | 机械臂控制：方向键/API、摇杆拖拽、预设位、操作日志。 |
| **`AlertsView.vue`** | 警报阈值与通知开关（前端演示，保存为 `alert`）。 |
| **`SettingsView.vue`** | 系统设置各区块（演示按钮与开关）。 |

### 5.8 `src/style.css`（若存在）

- **作用**：Vite 模板默认全局样式。
- **说明**：若 **`main.js` 未引用** 则不影响构建；与 **`main.css`** 重复时可删除引用或合并，避免样式冲突。

---

## 六、`dist/`（生产构建输出）

- **作用**：执行 **`npm run build`** 后生成，用于部署到 Nginx、静态托管或嵌入后端。
- **内容**：压缩后的 JS/CSS、`index.html`、从 `public` 复制的资源。
- **注意**：不要手工编辑 `dist/`；修改应始终在 **`src/`** 进行后重新构建。

---

## 七、`node_modules/`（依赖目录）

- **作用**：存放 `package.json` 声明的依赖包及传递依赖。
- **功能**：供 Vite 与 Node 在 `dev`/`build` 时解析模块。
- **注意**：体积大，**勿提交 Git**；新环境执行 **`npm install`** 即可恢复。

---

## 八、常用命令

```bash
# 在项目根 front-vue 下执行

# 安装依赖（首次或 package.json 变更后）
npm install

# 开发服务器（热更新）
npm run dev

# 生产构建
npm run build

# 本地预览构建结果（先 build）
npm run preview
```

---

## 九、与后端协作关系

| 场景 | 说明 |
|------|------|
| **开发** | 浏览器访问 Vite 端口（如 5173）；`/api`、`/ws` 由 Vite 代理到 Spring Boot **8090**。 |
| **生产** | 若前后端不同域，需配置 CORS、或将 `VITE_API_BASE` / `VITE_WS_BASE` 指向真实 API 与 WS 地址；或由网关统一反代。 |

若后端修改端口，请同步修改 **`vite.config.js`** 中的 **`proxy.target`**，或改用环境变量统一配置。
