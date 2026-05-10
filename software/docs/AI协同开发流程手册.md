# AI 协同开发流程手册

本文档面向 AquaGarden 项目的开发者，说明如何使用 **Claude Code** + **Codex** 双 AI 协同完成前后端需求的完整操作流程。

---

## 目录

1. [角色分工](#1-角色分工)
2. [完整操作流程图](#2-完整操作流程图)
3. [阶段一：需求分析 & 任务规划（Claude Code）](#3-阶段一需求分析--任务规划claude-code)
4. [阶段二：代码实现（Codex）](#4-阶段二代码实现codex)
5. [阶段三：代码审查（Claude Code）](#5-阶段三代码审查claude-code)
6. [阶段四：迭代修复](#6-阶段四迭代修复)
7. [阶段五：收尾 & 提交](#7-阶段五收尾--提交)
8. [Skill 使用速查表](#8-skill-使用速查表)
9. [典型场景示例](#9-典型场景示例)
10. [常见问题 FAQ](#10-常见问题-faq)

---

## 1. 角色分工

```
┌─────────────────────────────────────────────────────────┐
│                      你（人类开发者）                      │
│   提出需求 → 审查结果 → 决策是否合并 → 手动测试验证         │
└──────────┬──────────────────────────────┬───────────────┘
           │                              │
    提出需求给 Claude              把 Codex 的代码拿回来审查
           │                              │
┌──────────▼──────────┐     ┌─────────────▼──────────────┐
│     Claude Code      │     │          Codex             │
│  （你的 AI 参谋）      │────▶│    （你的 AI 码农）           │
│                      │     │                            │
│  ● 理解需求意图        │     │  ● 读取 ai-tasks/ 任务文件   │
│  ● 分析代码上下文       │     │  ● 逐步骤实现代码编辑        │
│  ● 编写任务规格         │     │  ● 勾选 checklist 自检      │
│  ● 审查 Codex 成果     │     │  ● 运行测试 & 修复失败用例    │
│  ● 生成审查报告         │     │  ● 提交 git commit          │
└──────────────────────┘     └────────────────────────────┘
```

**核心原则**：Claude Code 动脑（规划 + 审查），Codex 动手（写代码）。你不直接让 Claude Code 改代码，而是让它写好"施工图纸"，交给 Codex 去施工。

---

## 2. 完整操作流程图

```
你向 Claude Code 提出需求
         │
         ▼
┌─────────────────────┐
│ 阶段一：Claude 分析   │  使用 Skill: /codex-task-writer
│ 生成任务文件到        │  产出: ai-tasks/20260510-001-*
│ ai-tasks/            │  (spec.md / plan.md / checklist.md)
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│ 你审查任务文件        │  打开 spec.md 和 plan.md
│ 拍板确认或修改范围     │  确认无误后告诉 Codex 开始
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│ 阶段二：Codex 实施    │  Codex 读取 ai-tasks/ 文件
│ 编辑代码 + 运行测试    │  逐步骤修改项目源文件
│ 自检 checklist        │  git commit 提交
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│ 阶段三：Claude 审查   │  使用 Skill: /codex-reviewer
│ 对照 spec 逐项核验     │  产出: 结构化审查报告
│ 检查 git diff         │  (pass / needs changes)
└────────┬────────────┘
         │
    ┌────┴────┐
    │         │
  pass    needs changes
    │         │
    ▼         ▼
  进入      ┌─────────────────────┐
 阶段五     │ 阶段四：迭代修复       │
           │ Claude 给出修复指令    │
           │ Codex 根据指令修改     │──── 回到阶段三
           └─────────────────────┘
```

---

## 3. 阶段一：需求分析 & 任务规划（Claude Code）

### 3.1 向 Claude Code 提出需求

在 Claude Code 会话中，用自然语言描述你要做的改动。**好的需求描述遵循以下模板**：

```
我要做一个 <前端/后端/桥接层> 的改动：

背景：<为什么要改？现在有什么问题？>

目标：<改完以后是什么效果？一句话说清楚>

涉及页面/接口：<具体在哪里？比如 DashboardView.vue 的传感器面板、/api/sensors 接口>

特别注意：<有哪些不能动的东西？有哪些边界情况？>
```

**反面示例**（太模糊）：
> "帮我加一个水温报警功能"

**正面示例**（Codex 能直接施工）：
> 我要加一个后端水温报警功能：
> - 背景：当前 SystemStateService 只存最新传感器快照，没有任何异常检测
> - 目标：当 waterTemp > 30°C 时自动创建一个 Alert 记录并推送到前端 WebSocket
> - 涉及文件：SystemStateService.java、新增 Alert entity、LogWebSocketHandler.java
> - 特别注意：阈值要可配置（放在 application.properties）、不要修改 SensorController

### 3.2 调用 codex-task-writer 技能

输入 `/codex-task-writer` 或直接说：

> "帮我把这个需求写成 Codex 任务文件"

Claude Code 会自动：
1. 分析你的需求
2. 读取项目相关源码（Controller、Service、Entity 等）
3. 在 `ai-tasks/` 下生成 4 个文件：

```
ai-tasks/
├── active.md                  ← 更新为当前任务 ID
├── 20260510-001-spec.md       ← 规格：背景、目标、验收标准
├── 20260510-001-plan.md       ← 实现计划：分步骤、含文件路径
└── 20260510-001-checklist.md  ← 复选框清单：Codex 自检用
```

### 3.3 你审查并确认任务文件

**这是最关键的人为把关环节**。打开生成的 `spec.md` 和 `plan.md`，检查：

| 检查项 | 为什么重要 |
|---|---|
| 验收标准是否正确完整？ | 决定了 Codex 做到什么程度算"做完" |
| 涉及文件列表是否遗漏？ | 漏了文件 Codex 就不会去改 |
| "绝不能修改"列表是否覆盖了关键禁区？ | 防止 Codex 越界（如改 pom.xml、安全配置） |
| 实施步骤顺序是否合理？ | Entity → Service → Controller 的顺序不能乱 |

如果发现问题，直接对 Claude Code 说"第3步应该先改 DTO 再改 Controller"等修正意见，Claude 会更新任务文件。

---

## 4. 阶段二：代码实现（Codex）

### 4.1 启动 Codex 并交代任务

切换到 Codex 会话，告诉它：

> "读取 ai-tasks/ 目录下的当前活跃任务，按 plan.md 的步骤实现，完成后自检 checklist"

Codex 会：
1. 读取 `ai-tasks/active.md` → 找到当前任务 ID
2. 读取 `spec.md` → 理解要做什么、不能做什么
3. 读取 `plan.md` → 按步骤逐个文件编辑
4. 读取 `checklist.md` → 完成后逐项勾选
5. 执行 `mvn test` 或 `npm run build` 验证
6. `git add` + `git commit` 提交代码

### 4.2 你监控 Codex 进度

Codex 实现过程中，你应该：
- 观察它是否按 plan.md 的顺序执行（如果跳步骤，提醒它）
- 如果 Codex 卡住或犯错，可以在 Codex 会话中直接纠正
- Codex 提交 commit 后，先用 `git diff HEAD~1` 快速浏览改动

### 4.3 Codex 完成后的自检清单

Codex 应该在 checklist.md 中完成以下自检：

```markdown
- [ ] 所有验收标准已实现
- [ ] 没有提交密钥或 API Token
- [ ] 没有修改计划外的文件
- [ ] 测试通过（mvn test / npm run build）
- [ ] git commit message 描述了本次变更
```

---

## 5. 阶段三：代码审查（Claude Code）

### 5.1 调用 codex-reviewer 技能

回到 Claude Code 会话，输入：

```
/codex-reviewer
```

或者直接说：

> "审查一下 Codex 最新提交的代码"

### 5.2 Claude Code 的审查流程

Claude Code 会自动：
1. 读取 `ai-tasks/active.md` → 找到当前任务
2. 读取 spec / plan / checklist
3. 执行 `git diff` / `git diff --cached` 获取所有变更
4. 逐文件对照 spec 的验收标准
5. 检查是否有计划外文件被修改
6. 检查是否遗漏测试
7. 输出结构化审查报告

### 5.3 审查报告格式

```markdown
## 审查报告: 20260510-001

### 总体结论: needs changes

### 需求覆盖
- [✅] 1. waterTemp > 30°C 时创建 Alert 记录
- [✅] 2. Alert 通过 WebSocket 推送
- [⚠️] 3. 阈值应从 application.properties 读取 — 当前写死为 30
- [❌] 4. 缺少 Alert 的单元测试

### 发现的问题
1. **SystemStateService.java:45** — 阈值硬编码，应注入 @Value("${alert.temp-threshold}")
2. **Alert.java:12** — 缺少 createdAt 字段

### 缺失测试
- AlertServiceTest.java 不存在，应覆盖：正常创建 / 阈值边界 / 重复告警去重

### 高风险变更
- 无

### 给 Codex 的下一步指令
"在 SystemStateService.java 中将硬编码阈值改为读取 application.properties 的 alert.temp-threshold 配置项，并在 test 目录下新增 AlertServiceTest，覆盖正常告警和边界情况。"
```

---

## 6. 阶段四：迭代修复

如果审查结论是 `needs changes`：

1. **把审查报告的"给 Codex 的下一步指令"发给 Codex**
2. Codex 修改后重新 commit
3. 回到 Claude Code，再次 `/codex-reviewer`
4. 直到审查结论为 `pass`

典型迭代次数：1-2 轮。如果超过 3 轮，说明 spec 写得不够清楚，应回到阶段一重新规划。

---

## 7. 阶段五：收尾 & 提交

### 7.1 审查通过后

1. **手动功能测试**：启动后端 / 前端，实际点击验证
2. **清理 active.md**：将 `active.md` 中的任务 ID 改回 `NONE`
3. **更新文档**（如果有架构变化）：更新 CLAUDE.md 中的后端/前端结构
4. **合并分支**：`git checkout develop && git merge <feature-branch>`

### 7.2 善用 Superpowers 做全流程

如果是大需求，可以在 Claude Code 中使用 Superpowers 技能链：

```
/superpowers:brainstorm           → 头脑风暴，细化需求
/superpowers:write-plan           → 编写完整实施计划
/codex-task-writer                → 转成 Codex 任务文件
（交给 Codex 实现）
/codex-reviewer                   → 审查 Codex 的代码
```

---

## 8. Skill 使用速查表

### Claude Code 端（你操作的）

| 命令 | 什么时候用 | 产出 |
|---|---|---|
| `/codex-task-writer` | 拿到需求后第一步 | ai-tasks/ 下的 4 个任务文件 |
| `/codex-reviewer` | Codex 提交代码后 | 结构化审查报告 |
| `/superpowers:brainstorm` | 需求不清晰时 | 需求细化、边界梳理 |
| `/superpowers:write-plan` | 大功能需要整体设计时 | 分阶段实施计划 |
| `/superpowers:execute-plan` | Claude Code 自己也要写一些代码时 | 代码实现 |
| `/testing-best-practices` | 写测试用例时 | 符合 AAA 模式的测试 |
| `/security-practices` | 涉及认证/授权/SQL/输入验证时 | 安全审查 |
| `/frontend-design` | 做前端 UI 时自动激活 | 高质量 UI 代码 |
| `/planning-with-files:plan` | 超长任务防止上下文丢失 | 持久化进度文件 |

### Codex 端（Codex 操作的）

| 指令 | 什么时候用 |
|---|---|
| "读取 ai-tasks/ 实现当前活跃任务" | 阶段二开始 |
| "[粘贴审查报告中的修复指令]" | 阶段四迭代修复 |
| "运行 mvn test 确保全部通过" | 提交前自检 |

---

## 9. 典型场景示例

### 场景 A：后端新增一个 REST 接口

```
你的操作:
  Step 1 → Claude Code: "/codex-task-writer
           我要在 AquaController 里新增一个 /api/aqua/feeding 接口，
           接收 { duration: number } 参数，调用 AquaBridgeService 下发给 Phytium Pi。
           返回 { status, message } 。不要改 AquaBridgeService 的核心逻辑，
           只做转发。"

  Step 2 → 审查生成的 spec.md 和 plan.md，确认无误

  Step 3 → Codex: "读取 ai-tasks/，实现当前活跃任务"

  Step 4 → Claude Code: "/codex-reviewer"

  Step 5 → 手动用 curl 测试接口

  Step 6 → 清理 active.md 为 NONE
```

### 场景 B：前端修改 Dashboard 页面布局

```
你的操作:
  Step 1 → Claude Code: "/codex-task-writer
           在 DashboardView.vue 中把传感器卡片从横向排列改为 2x2 网格，
           每张卡片显示传感器名称、当前值、单位、最后更新时间。
           不要改数据获取逻辑（WebSocket 已经是好的），只改 template 和 style。"

  Step 2 → 审查生成的 spec.md 和 plan.md

  Step 3 → Codex: "读取 ai-tasks/，实现当前活跃任务"

  Step 4 → Claude Code: "/codex-reviewer"

  Step 5 → npm run dev 启动前端，浏览器确认效果

  Step 6 → 清理 active.md 为 NONE
```

### 场景 C：跨层改动（前端 + 后端 + 桥接层）

```
你的操作:
  Step 1 → Claude Code: "/codex-task-writer
           需求涉及三层改动，请拆成多个独立任务"

  Claude Code 会生成:
    ai-tasks/20260510-001-spec.md  (后端: 新增 /api/pump/control)
    ai-tasks/20260510-002-spec.md  (Python 桥接层: aqua_bridge.py 新增 pump 端点)
    ai-tasks/20260510-003-spec.md  (前端: CameraView.vue 新增水泵开关按钮)

  Step 2 → 确认依赖顺序: 001 → 002 → 003

  Step 3 → 每次只 active 一个任务，完成一个再开下一个
    active.md → 001 → Codex 实现 → Claude 审查 → pass
    active.md → 002 → Codex 实现 → Claude 审查 → pass
    active.md → 003 → Codex 实现 → Claude 审查 → pass
```

---

## 10. 常见问题 FAQ

### Q1: Claude Code 可以直接改代码吗？

可以。但推荐做法是 Claude Code 规划 + 审查，Codex 施工。原因：
- Claude Code 的上下文窗口更大，更适合分析全局影响
- Codex 擅长执行精确的编辑指令
- 两人分工避免了"自己审查自己写的代码"的问题

如果是非常简单的改动（改一行配置、修正一个拼写），直接让 Claude Code 改就行，不需要走完整流程。

### Q2: 什么时候走完整流程，什么时候简化？

| 改动规模 | 流程 |
|---|---|
| 单文件、单函数、< 10 行 | Claude Code 直接改 |
| 2-5 个文件、新增/修改功能 | 走 Codex 完整流程 |
| 6+ 个文件、跨层改动 | 拆分多个 task + 走完整流程 |

### Q3: Codex 改错了怎么办？

1. 在 Claude Code 中 `/codex-reviewer` 获取详细问题清单
2. 把审查报告中的"给 Codex 的下一步指令"发给 Codex
3. Codex 根据指令修改后重新 commit
4. 再次审查

如果反复出错，说明 spec.md / plan.md 不够清晰，让 Claude Code 重新生成。

### Q4: 多个需求可以并行吗？

不可以同时。`active.md` 设计为只记录一个活跃任务。完成一个任务（审查通过）后再激活下一个。这样做的好处：
- 每次 git diff 范围明确，审查效率高
- 避免多个 Codex 会话互相覆盖代码

### Q5: 为什么我的 spec 总是需要返工？

常见原因：
- 需求描述太模糊，Claude Code 猜错了意图 → 用 3.1 节的模板
- 没有读取现有代码，计划与真实结构不符 → 在 `/codex-task-writer` 之前先让 Claude 读相关文件
- 验收标准没写清楚边界情况 → spec 中加一个"边界条件"段落

### Q6: 如何查看当前安装了哪些技能？

在 Claude Code 会话中输入：
```
/ls
```

### Q7: 手动写了一个 skill，怎么让它生效？

技能放在 `.claude/skills/<skill-name>/SKILL.md`，Claude Code 2.1.0+ 支持热重载，无需重启。文件格式参见 `.claude/skills/codex-task-writer/SKILL.md`。

### Q8: 前端自动激活的 frontend-design 太激进怎么办？

那是 frontend-design 技能的设计意图——强制不生成千篇一律的 AI 风格 UI。如果你对效果不满意，可以：
1. 在需求中明确描述你想要的视觉风格（如"简洁白底蓝调，类似 Ant Design"）
2. 或者在 `.claude/skills/` 下创建一个项目级配置覆盖它

---

## 快速参考卡片

```
┌────────────────────────────────────────────────────────────┐
│               AI 协同开发 一句话口诀                          │
├────────────────────────────────────────────────────────────┤
│                                                            │
│   来了需求 → /codex-task-writer → 审查 spec                  │
│                                                            │
│   spec 没问题 → 告诉 Codex 开工 → Codex 写代码                │
│                                                            │
│   Codex 提交了 → /codex-reviewer → 看审查报告                 │
│                                                            │
│   有问题 → 把修复指令丢给 Codex → 改完再审查                   │
│                                                            │
│   没问题 → 手动测一下 → 清理 active.md → 收工                  │
│                                                            │
│   大需求 → /superpowers:brainstorm → 拆成多个小 task          │
│                                                            │
│   碰安全代码 → /security-practices                           │
│                                                            │
│   写测试用例 → /testing-best-practices                       │
│                                                            │
└────────────────────────────────────────────────────────────┘
```
