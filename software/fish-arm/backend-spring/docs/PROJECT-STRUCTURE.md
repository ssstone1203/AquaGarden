# AquaGarden 后端（Spring Boot）项目结构与说明

本文档说明 `backend-spring` 工程的目录职责、主要模块功能以及运行所需环境。

初学者建议配合阅读：**[SPRING-BOOT-入门与实操指南.md](./SPRING-BOOT-入门与实操指南.md)**（IoC、请求链路、Security、JPA 与实操任务）。

---

## 一、运行环境

| 依赖 | 版本要求 | 说明 |
|------|----------|------|
| **JDK** | 17 及以上 | Spring Boot 3.x 要求 Java 17+ |
| **Maven** | 3.8+（推荐 3.9+） | 用于依赖管理与构建；也可用 IDE 内置 Maven |
| **操作系统** | Windows / Linux / macOS | 无特殊限制 |
| **可选** | 网络（首次构建） | `mvn` 需从中央仓库下载依赖 |

**运行时自动生成：**

- 工作目录下的 **`aquagarden.db`**：SQLite 数据库文件（路径由 `spring.datasource.url` 配置）。
- **`target/`**：Maven 编译与打包输出目录，可加入 `.gitignore`（若尚未忽略）。

**端口：**

- 默认 **`8090`**（`application.properties` 中 `server.port`）。若端口被占用，可改为其他端口，并同步修改前端代理或 `VITE_API_BASE`。

---

## 二、仓库根目录一览

```
backend-spring/
├── pom.xml                          # Maven 项目定义：依赖、插件、Java 版本等
├── docs/
│   └── PROJECT-STRUCTURE.md          # 本说明文档
├── src/
│   └── main/
│       ├── java/com/aquagarden/    # 业务与框架代码（Java）
│       └── resources/              # 配置文件与静态资源（非 Java）
└── target/                          # Maven 构建产物（编译后的 .class、jar 等，勿手改）
```

---

## 三、`pom.xml`（项目根）

- **作用**：Maven 工程描述文件。
- **功能**：
  - 声明父工程 `spring-boot-starter-parent`（统一版本与插件）。
  - 引入 **Web、JPA、Security、WebSocket、Validation** 等 Starter。
  - 引入 **SQLite JDBC**、**Hibernate SQLite 方言**、**JJWT** 等依赖。
  - 配置 **`spring-boot-maven-plugin`**，支持 `mvn spring-boot:run` 与打可执行 jar。

---

## 四、`src/main/java`（源代码根）

包根目录为 **`com.aquagarden`**，按职责划分子包。

### 4.1 `com.aquagarden`（根包）

| 文件 | 作用 |
|------|------|
| **`AquaGardenApplication.java`** | Spring Boot 入口类，包含 `main` 方法；启用 **`@EnableScheduling`** 以支持定时任务（如 WebSocket 心跳）。 |

---

### 4.2 `com.aquagarden.config`（配置类）

| 文件 | 作用 |
|------|------|
| **`SecurityConfig.java`** | Spring Security：无 Session、JWT 过滤器链、CORS、放行登录/注册/视频/WebSocket 等路径。 |
| **`WebSocketConfig.java`** | 注册 WebSocket 端点 **`/ws/logs`**，绑定日志推送处理器。 |
| **`WebSocketHeartbeat.java`** | 定时向所有 WebSocket 连接发送心跳类消息。 |
| **`DataInitializer.java`** | 应用启动时若不存在 `admin` 用户则创建默认管理员（与旧版演示账号一致）。 |

---

### 4.3 `com.aquagarden.entity`（JPA 实体）

| 文件 | 作用 |
|------|------|
| **`User.java`** | 用户表 **`users`** 映射：用户名、邮箱、密码哈希、角色、创建时间等。 |

---

### 4.4 `com.aquagarden.repo`（数据访问）

| 文件 | 作用 |
|------|------|
| **`UserRepository.java`** | Spring Data JPA 仓库：按用户名查询、判断用户名/邮箱是否存在等。 |

---

### 4.5 `com.aquagarden.dto`（数据传输对象）

用于接收/返回 HTTP JSON，与 API 契约对齐。

| 文件 | 作用 |
|------|------|
| **`LoginRequest.java`** | 登录请求体：用户名、密码。 |
| **`RegisterRequest.java`** | 注册请求体：用户名、可选邮箱、密码。 |
| **`TokenResponse.java`** | 登录成功：`access_token`、`token_type`、`expires_in`。 |
| **`RobotControlRequest.java`** | 机械臂控制：`direction`。 |
| **`ModeRequest.java`** | 模式切换：`mode`（demo/service）。 |
| **`SensorSnapshot.java`** | 传感器读数快照（内部或接口用）。 |

---

### 4.6 `com.aquagarden.service`（业务服务）

| 文件 | 作用 |
|------|------|
| **`AuthService.java`** | 注册校验、密码加密、登录校验、签发 JWT。 |
| **`JwtService.java`** | JWT 生成、解析、校验（密钥与过期时间来自配置）。 |
| **`SystemStateService.java`** | 内存中的演示态：模式、机械臂坐标、传感器随机波动等。 |

---

### 4.7 `com.aquagarden.security`（安全组件）

| 文件 | 作用 |
|------|------|
| **`JwtAuthFilter.java`** | Servlet 过滤器：从 `Authorization: Bearer` 解析 JWT，校验后写入 `SecurityContext`；对放行路径直接跳过。 |

---

### 4.8 `com.aquagarden.web`（HTTP 控制器与全局异常）

| 文件 | 作用 |
|------|------|
| **`AuthController.java`** | **`POST /api/register`**、**`POST /api/login`**。 |
| **`UserController.java`** | **`GET /api/users`** 用户列表。 |
| **`SensorController.java`** | **`GET /api/sensors`** 传感器数据。 |
| **`RobotController.java`** | **`POST /api/robot/control`**。 |
| **`ModeController.java`** | **`GET/POST /api/mode`**。 |
| **`VideoController.java`** | **`GET /api/video/robot`**、**`/api/video/tank`** MJPEG 模拟视频流。 |
| **`GlobalExceptionHandler.java`** | 统一返回 **`{"detail":"..."}`** 等，与前端错误处理习惯一致。 |

---

### 4.9 `com.aquagarden.websocket`（WebSocket）

| 文件 | 作用 |
|------|------|
| **`LogWebSocketHandler.java`** | 管理连接、广播 JSON 日志、单连接欢迎消息；供控制类在业务事件时调用广播。 |

---

## 五、`src/main/resources`（资源目录）

| 文件/目录 | 作用 |
|-----------|------|
| **`application.properties`** | 服务端口、数据源 URL、JPA、JWT 密钥与过期时间、日志级别等。**生产环境务必修改 JWT 密钥与敏感项。** |

（当前工程未使用 `static/`、`templates/` 等目录；若将来由 Spring 托管前端打包产物，可将 Vue `dist` 放入 `static` 并配置 SPA 路由回退。）

---

## 六、`target/`（构建输出）

- **作用**：Maven 执行 `compile`、`package` 等命令时生成。
- **内容**：`.class` 文件、可执行 **`aquagarden-server-*.jar`**、复制后的 `application.properties` 等。
- **注意**：不要向版本库提交 `target/`（应在 `.gitignore` 中忽略）；可随时 `mvn clean` 删除后重新构建。

---

## 七、常用命令

```bash
# 在项目根 backend-spring 下执行

# 编译
mvn -DskipTests compile

# 运行（开发）
mvn spring-boot:run

# 打可执行 jar
mvn -DskipTests package
# 产物：target/aquagarden-server-1.0.0.jar
java -jar target/aquagarden-server-1.0.0.jar
```

---

## 八、与前端协作关系

- 默认提供 **REST JSON API**（`/api/...`）与 **WebSocket**（`/ws/logs`）。
- 开发模式下 Vue 通过 **反向代理** 将 `/api`、`/ws` 转发到本服务端口（见前端 `vite.config.js`）。
- 若后端修改端口，需同步修改前端代理或环境变量 **`VITE_API_BASE` / `VITE_WS_BASE`**。
