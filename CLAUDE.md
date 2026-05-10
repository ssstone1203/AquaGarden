# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AquaGarden is a smart aquarium monitoring and control system. It integrates MCU-driven sensors (temperature, humidity, water quality, soil moisture, pressure), mechanical arm control, pump control, RGB camera streaming, and AI-based fish detection (YOLO). The system spans embedded firmware, an ARM64 Linux SBC (Phytium Pi), a Spring Boot backend, and a Vue 3 frontend.

## Architecture & Data Flow

```
MCU (Renesas RA, COM20 115200 baud)
  → UART binary frames (30-byte payload, CRC-16, 250ms interval)
    → serial_bridge.py (Python, on Windows/Phytium Pi)
      → POST /api/sensors/ingest (JSON)
        → Spring Boot :8090
          ├── SystemStateService (in-memory latest snapshot)
          ├── LogWebSocketHandler → WebSocket /ws/logs → Vue DashboardView (real-time)
          ├── SQLite (aquagarden.db, 30-day retention)
          └── GET /api/sensors (5s polling fallback for frontend)

Phytium Pi (separate Linux SBC)
  → aqua_bridge.py (Flask :18080) → aqua_spi_cli → SPI hardware (pump, etc.)
    → Spring Boot AquaBridgeService proxies /api/aqua/* to this bridge
```

**Key directories:**
- `hardware/` — Embedded firmware (Renesas RA MCU with Keil MDK/e2studio, Phytium Pi Linux)
- `software/fish-arm/` — Main application: Spring Boot backend + Vue 3 frontend + Python bridges
- `software/ruisa/` — Separate "Ruisa" project (arm control + AI agent), not the current focus
- `model/` — 3D models (fishtank, robot arm), YOLO weights for fish detection
- `docs/` — Chinese documentation, module datasheets, purchasing records
- `tools/` — Dev utility scripts, Huawei coding standards PDF

## Commands

### Backend (Spring Boot)
```bash
cd software/fish-arm/backend-spring
mvn spring-boot:run          # Start on port 8090
mvn test                     # Run tests
```

### Frontend (Vue 3 + Vite)
```bash
cd software/fish-arm/front-vue
npm install                  # Install dependencies
npm run dev                  # Dev server on :5173, proxies /api & /ws to :8090
npm run build                # Production build to dist/
```

### Python Bridges
```bash
# Serial bridge (MCU UART → backend)
cd software/fish-arm
pip install pyserial requests opencv-python ultralytics
python serial_bridge.py --port COM20 --backend http://localhost:8090

# Device bridge (runs on Phytium Pi, controls hardware via aqua_spi_cli)
python aqua_bridge.py   # Flask on :18080, requires aqua_spi_cli on PATH
```

### Hardware
- **Renesas RA MCU**: Open in Keil MDK (`hardware/arm/arm.uvprojx`) or Renesas e2studio
- **Phytium Pi**: Buildroot-based Linux; see `hardware/phytiumpi/README.md`

## Backend Structure (Spring Boot 3.3.5, Java 17, port 8090)

```
com.aquagarden
├── AquaGardenApplication.java    (entry point, @EnableScheduling)
├── config/
│   ├── SecurityConfig.java       (JWT stateless, CORS allow all, public endpoint whitelist)
│   ├── WebSocketConfig.java      (/ws/logs endpoint)
│   ├── WebSocketHeartbeat.java   (periodic ping via @Scheduled)
│   └── DataInitializer.java      (demo user seeding)
├── security/
│   ├── JwtAuthFilter.java        (extracts Bearer token from Authorization header)
│   └── SecurityWhitelist.java    (centralized public URI list, used by both filter & config)
├── web/
│   ├── SensorController.java     (GET /api/sensors, GET /api/sensors/history, POST /api/sensors/ingest, POST /api/sensor/upload)
│   ├── AquaController.java       (proxy to Phytium Pi Bridge: /api/aqua/status, /api/aqua/tasks/*, /api/aqua/arm/*, /api/aqua/pump/*, /api/aqua/video/*)
│   ├── AuthController.java       (/api/register, /api/login)
│   ├── UserController.java       (/api/users)
│   ├── RobotController.java      (/api/robot/*)
│   ├── ModeController.java       (/api/mode)
│   ├── VideoController.java      (/api/video/*)
│   ├── DeviceControlController.java
│   ├── AiAnalysisController.java
│   └── GlobalExceptionHandler.java
├── service/
│   ├── SystemStateService.java   (in-memory latest sensor snapshot, returns demo data when no hardware)
│   ├── AquaBridgeService.java    (HTTP proxy to Python aqua_bridge.py on Phytium Pi)
│   ├── JwtService.java           (JJWT 0.12.5)
│   ├── AuthService.java
│   ├── EcosystemLlmService.java  (Claude/OpenAI LLM for ecosystem analysis)
│   └── SensorReadingRetentionService.java (scheduled purge of old SQLite rows)
├── websocket/
│   └── LogWebSocketHandler.java  (broadcasts sensor_data, logs, heartbeats to all connected frontends)
├── entity/ — JPA entities (User, SensorReading)
├── repo/ — Spring Data repositories (UserRepository, SensorReadingRepository)
└── dto/ — Data transfer objects (SensorSnapshot is a record with waterTemp/airTemp/airHumidity/wqi/soilMoisture)
```

Database: **SQLite** via `sqlite-jdbc` + Hibernate community dialects. The DB file is `aquagarden.db` in the backend working directory. Schema is auto-managed (`ddl-auto=update`).

## Frontend Structure (Vue 3 + Vite + Vue Router)

```
src/
├── App.vue
├── main.js
├── router/index.js       (auth guard: checks localStorage token)
├── api/http.js           (axios-like wrapper around fetch with JWT interceptor)
├── components/
│   ├── AppLayout.vue     (sidebar nav layout for authenticated views)
│   └── HelloWorld.vue
└── views/
    ├── LoginView.vue     (guest-only)
    ├── RegisterView.vue  (guest-only)
    ├── DashboardView.vue (real-time sensor data via WebSocket + polling)
    ├── CamerasView.vue   (MJPEG video streams)
    ├── HistoryView.vue   (sensor history charts)
    └── RobotView.vue     (mechanical arm control)
```

Vite dev server proxies `/api` → `localhost:8090` and `/ws` → `localhost:8090` (with WebSocket support).

## Security

- Stateless JWT authentication (30-min expiration). Token stored in `localStorage` by frontend.
- Public endpoints (no auth required): `/api/register`, `/api/login`, `/api/sensors/ingest`, `/api/sensor/upload`, `/api/sensor/latest`, `/api/sensors/history`, `/api/video/**`, `/api/aqua/video/**`, `/ws/**`, `/api/debug/whoami`, `/api/robot/status`
- Device upload uses a separate token header (`X-Device-Token`) checked against `aquagarden.device-upload.token` in application.properties.
- **Secrets**: `.env` files are in `.gitignore` but the current `software/fish-arm/.env` contains an API key — do not commit it.

## Coding Conventions (from hardware/规范编写代码.md)

- File naming: `dev_<sensor>_driver.c/.h` for device drivers under `device/`, system-level code under `module/`
- Function prefix: e.g., `UTS_` for Underwater Temperature Sensor
- Header macros: all uppercase, e.g., `UTS_TEMP_MAX`
- Avoid `extern` across files; prefer function parameters (bare-metal) or queues (RTOS)
- Use `volatile` for local variables in ISR callbacks
- Protect shared functions with semaphores in RTOS
- Prefer device-specific data structs over generic names (e.g., `temp_sensor_data_t` over `temp_sensor_t`)

## Hardware Subsystems

| Subsystem | MCU Peripheral | Sensor/Actuator |
|---|---|---|
| Water temperature | OneWire | DS18B20 |
| Air temperature/humidity | I2C | SHT30 |
| Water quality | UART | WQM11S (WQI 0-100) |
| Soil moisture | ADC | Capacitive soil sensor |
| Pressure ×3 | ADC | Pressure sensors (0-5 kg) |
| Pump | SPI → GPIO | DC pump (PWM) |
| RGB light | GPIO/PWM | 3-color LED |
| Mechanical arm | PWM ×6 | Servo motors (4 for arm, 2 for end-effector) |

## AI Agent Skills

This project uses Claude Code Skills — reusable behavior packs that teach AI agents specialized workflows. Skills are auto-discovered from `~/.agents/skills/` (global) and `.claude/skills/` (project). Codex also reads these skills; invoke them by name via `/skill-name`.

### Global Skills (all projects, `~/.agents/skills/`)

| Skill | Source | Purpose |
|---|---|---|
| **using-superpowers** | obra/superpowers (40.9K ⭐) | Structured dev lifecycle: brainstorm → write-plan → execute-plan → code-review → merge. Use for any feature work. Invoke with `/superpowers:brainstorm`, `/superpowers:write-plan`, `/superpowers:execute-plan`. |
| **planning-with-files-zh** | OthmanAdi/planning-with-files (13.4K ⭐) | Persistent task planning in Markdown (task_plan.md, findings.md, progress.md). Prevents context loss on long tasks. Chinese edition. Invoke with `/planning-with-files:plan`. |
| **frontend-design** | anthropics/skills (official) | Forces a deliberate aesthetic direction before writing UI code. Bans generic AI fonts/gradients. Activates automatically on frontend tasks. |
| **typescript-best-practices** | spardutti/claude-skills | TypeScript 5.x type design, generics, discriminated unions. Applied to Vue 3 frontend code. |
| **testing-best-practices** | spardutti/claude-skills | Arrange-Act-Assert, factory fixtures, test isolation, mock boundaries. Applied to both backend (JUnit 5) and frontend (Vitest). |
| **security-practices** | spardutti/claude-skills | OWASP Top 10 prevention: SQL injection, XSS, CSRF, JWT hardening, input validation. Applied to Spring Boot controllers and auth code. |
| **docker-best-practices** | spardutti/claude-skills | Multi-stage builds, layer caching, non-root user, security hardening. |
| **skill-creator** | Claude Code built-in | Meta-skill: guides you step-by-step to create a new custom skill. Invoke with `/skill-creator`. |

### Project Skills (`.claude/skills/`)

| Skill | Purpose |
|---|---|
| **codex-task-writer** | Converts feature requests into structured Codex task files under `ai-tasks/` (spec.md, plan.md, checklist.md). |
| **codex-reviewer** | Reviews Codex-implemented code against the active ai-task spec. Returns a structured pass/needs-changes/unclear report. |

### When Skills Activate

- **Automatic**: Claude/Codex detects the task (e.g., writing a Vue component → `frontend-design` + `typescript-best-practices`)
- **Manual**: Type `/skill-name` (e.g., `/codex-task-writer`, `/testing-best-practices`)
- **Superpowers chain**: `/superpowers:brainstorm` → `/superpowers:write-plan` → `/superpowers:execute-plan` → `/superpowers:code-review`

### Important
- Do NOT install more than 10-12 skills total — beyond that, trigger accuracy drops below 50%.
- All installed skills passed security review (Safe / Low Risk by skills.sh).
- Skills are agent-agnostic: they work with Claude Code, Codex, Cursor, and Gemini CLI.

## Note on `software/ruisa/`

This is a separate project (arm control + AI chat agent) with its own Spring Boot gateway (port 8080) that proxies to a Python FastAPI backend (port 8000) and a Vue frontend. When working on `fish-arm`, do not modify `ruisa/` unless explicitly asked.
