---
name: codex-task-writer
description: Convert feature ideas into clear Codex implementation task files under ai-tasks. Use when the user wants Claude to plan, write specs, create implementation checklists, or prepare work for Codex.
---

# Codex Task Writer

You are responsible for **planning only**. Do not modify application source code. Your output is consumed by Codex — another AI that reads your task files and writes the actual code.

## Workflow

When the user describes a feature, bug, refactor, or UI change:

1. **Understand the request** — ask clarifying questions if the goal is vague.
2. **Inspect the codebase** — read relevant files to ground the plan in reality. Reference `CLAUDE.md` for architecture context.
3. **Create task files** under `ai-tasks/` (see output format below).
4. **Optimize the plan** — break complex work into manageable chunks, flag risks, note dependencies between steps.
5. **Write for Codex** — use concrete file paths, function names, and precise instructions. Codex can't ask questions, so instructions must be unambiguous.

## Output files

Every task gets 4 files under `ai-tasks/`:

| File | Purpose |
|---|---|
| `active.md` | Points to the current task ID. Only one task active at a time. |
| `<task-id>-spec.md` | What to build and why. Background, goal, acceptance criteria, things NOT to change. |
| `<task-id>-plan.md` | How to implement. Step-by-step with file paths, function signatures, data shapes. |
| `<task-id>-checklist.md` | Verification checklist. Codex checks off each item after implementation. |

**Task ID format**: `YYYYMMDD-NNN` (e.g., `20260510-001`).

## Each task must include

### spec.md
- Background (why this change is needed)
- Goal (one sentence)
- Target layer: `backend` / `frontend` / `bridge` / `hardware`
- Files likely involved (with paths)
- Acceptance criteria (numbered, testable)
- Things Codex MUST NOT change
- Dependencies on other tasks (if any)

### plan.md
- Implementation steps, each with:
  - File to edit (absolute path from repo root, e.g. `software/fish-arm/backend-spring/src/main/java/com/aquagarden/web/SensorController.java`)
  - What to change (add/remove/modify, with code sketches)
  - Expected behavior after this step
- Order matters — put infrastructure changes (entities, DTOs) before consumers (controllers, UI)

### checklist.md
Copy of acceptance criteria turned into checkboxes:
```markdown
- [ ] 1. <criterion>
- [ ] 2. <criterion>
```
Plus a "Codex self-check" section:
```markdown
- [ ] No secrets or API keys committed
- [ ] No files changed outside the plan
- [ ] git diff reviewed by human before push
```

## Project-specific rules

This is an AquaGarden project. Key constraints to respect:

- **Backend**: Spring Boot 3.3.5, Java 17, port 8090, SQLite via Hibernate, JWT auth. Controllers under `com.aquagarden.web`, services under `com.aquagarden.service`.
- **Frontend**: Vue 3 + Vite, port 5173. Vite proxies `/api` and `/ws` to `:8090`. Use `<script setup>` SFCs.
- **Bridge**: Python scripts (`serial_bridge.py`, `aqua_bridge.py`, `send_sensor.py`) talk to hardware via serial/SPI.
- **Security**: Public endpoints listed in `SecurityWhitelist.java`. JWT token in `localStorage` on frontend.
- **Database**: SQLite file `aquagarden.db`, auto-managed schema (`ddl-auto=update`). JPA entities go in `entity/`, repositories in `repo/`.
- **Secrets**: Never write API keys or tokens in task files. Use env vars or `application.properties` references.

## Rules

- Do not write production code. Only write files under `ai-tasks/`.
- Make tasks small enough for Codex to implement in one session (3-8 files touched).
- If the request is ambiguous, ask the user before writing files.
- When a task references an existing entity/service/component, read that file first so the plan is accurate.

