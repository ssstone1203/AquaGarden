<template>
  <div class="robot-page">
    <div class="robot-left">
      <section class="control-panel arm-panel">
        <div class="panel-header control-panel-header">
          <div>
            <span><i class="fas fa-robot"></i> 机械臂与滑轨</span>
            <small>喂食、裁剪、松土和滑轨绝对位置控制</small>
          </div>
          <span :class="['quick-status', connected ? 'online' : 'offline']">{{ connected ? 'Bridge 在线' : 'Bridge 离线' }}</span>
        </div>

        <div class="arm-actions">
          <button class="primary-task-btn task-feed" :disabled="busy || railPending" @click="sendTask('feed')">
            <i class="fas fa-utensils"></i>
            <span>喂食</span>
          </button>
          <button class="primary-task-btn task-prune" :disabled="busy || railPending" @click="sendTask('prune')">
            <i class="fas fa-cut"></i>
            <span>裁剪黄色叶子</span>
          </button>
          <button class="primary-task-btn task-loosen" :disabled="busy || railPending" @click="sendTask('loosen')">
            <i class="fas fa-seedling"></i>
            <span>松土</span>
          </button>
          <button class="primary-task-btn task-stop" @click="stopTask">
            <i class="fas fa-hand-paper"></i>
            <span>停止</span>
          </button>
        </div>

        <div class="rail-control-block">
          <div class="rail-title">
            <span><i class="fas fa-arrows-alt-h"></i> 滑轨移动（0-4000）</span>
            <b>当前位置：{{ railPositionText }}</b>
          </div>
          <div class="rail-row rail-row-direct">
            <button class="rail-btn" @click="setRailTarget(0)">回到 0</button>
            <input v-model.number="railTarget" class="rail-input rail-input-large" type="number" min="0" max="4000" step="10" />
            <button class="rail-btn" @click="setRailTarget(4000)">到 4000</button>
            <button class="rail-btn rail-btn-primary" :disabled="busy || railPending" @click="moveRail">移动滑轨</button>
          </div>
        </div>
      </section>

      <section class="control-panel pump-panel">
        <div class="panel-header control-panel-header">
          <div>
            <span><i class="fas fa-tint"></i> 水泵控制</span>
            <small>手动启停、PWM 调节和自动模式</small>
          </div>
          <b class="pump-readout">{{ pumpPwm }}%</b>
        </div>
        <div class="pump-body">
          <input v-model.number="pumpPwm" class="pump-range" type="range" min="0" max="100" step="1" />
          <div class="rail-row">
            <input v-model.number="pumpPwm" class="rail-input rail-input-large" type="number" min="0" max="100" step="1" />
            <button class="rail-btn rail-btn-primary" :disabled="pumpBusy" @click="pumpStart">开泵</button>
            <button class="rail-btn" :disabled="pumpBusy" @click="pumpApplyPwm">更新 PWM</button>
            <button class="rail-btn" :disabled="pumpBusy" @click="pumpAuto">自动模式</button>
            <button class="rail-btn rail-btn-danger" :disabled="pumpBusy" @click="pumpStop">关泵</button>
          </div>
        </div>
      </section>
    </div>

    <div class="robot-right">
      <div class="terminal-panel">
        <div class="panel-header terminal-header">
          <span><i class="fas fa-terminal"></i> 终端运行状态</span>
          <div class="terminal-status-row">
            <span :class="['status-dot', connected ? 'dot-connected' : 'dot-disconnected']"></span>
            <span class="status-text">{{ connected ? '已连接' : '断开' }}</span>
          </div>
        </div>

        <div class="terminal-status-grid">
          <div><span>当前任务</span><b>{{ currentTask }}</b></div>
          <div><span>阶段</span><b>{{ phase }}</b></div>
          <div><span>滑轨</span><b>{{ railPositionText }}</b></div>
          <div><span>运行</span><b>{{ uptime }}</b></div>
          <div><span>RGB</span><b>{{ cameraState.hasRgb ? 'OK' : '--' }}</b></div>
          <div><span>Depth</span><b>{{ cameraState.hasDepth ? 'OK' : '--' }}</b></div>
          <div v-if="lastError" class="info-row info-row-error">
            <span class="info-label"><i class="fas fa-exclamation-triangle"></i> 错误</span>
            <span class="info-val">{{ lastError }}</span>
          </div>
        </div>

        <div class="log-section">
          <div class="log-section-title">
            <i class="fas fa-stream"></i> 运行日志
            <button class="clear-btn" @click="clearLogs">清空</button>
          </div>
          <div ref="logContainer" class="log-terminal">
            <div v-for="(entry, i) in logs" :key="i" :class="['log-line', 'log-' + entry.type]">
              <span class="log-ts">{{ entry.time }}</span>
              <span class="log-badge" :class="'badge-' + entry.type">{{ entry.type }}</span>
              <span class="log-msg">{{ entry.msg }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, nextTick, onMounted, onUnmounted, reactive, ref } from 'vue'
import { apiUrl, authHeaders } from '@/api/http'

const connected = ref(true)
const currentTask = ref('待命')
const phase = ref('idle')
const busy = ref(false)
const lastError = ref('')
const uptime = ref('00:00:00')
const logs = ref([])
const logContainer = ref(null)
const railTarget = ref(0)
const railPosition = ref(null)
const cameraState = reactive({ hasRgb: false, hasDepth: false, ageSec: null })
const pumpPwm = ref(80)
const pumpBusy = ref(false)

/** 滑轨移动请求进行中（与 busy 分离，避免与服务端 busy 不同步时连点） */
const railPending = ref(false)

/** 控制类请求超时（毫秒） */
const RAIL_HTTP_MS = 90_000
const TASK_HTTP_MS = 120_000
const STATUS_HTTP_MS = 15_000
const RAIL_DEBOUNCE_MS = 350

function withTimeout(ms) {
  const ctrl = new AbortController()
  const tid = setTimeout(() => ctrl.abort(), ms)
  return { signal: ctrl.signal, cancel: () => clearTimeout(tid) }
}

async function fetchWithTimeout(url, init, timeoutMs) {
  const { signal, cancel } = withTimeout(timeoutMs)
  try {
    return await fetch(url, { ...init, signal })
  } catch (e) {
    if (e?.name === 'AbortError') throw new Error('请求超时，请检查 Bridge 与网络')
    throw e
  } finally {
    cancel()
  }
}
const railPositionText = computed(() => railPosition.value == null ? '--' : String(railPosition.value))

let statusTimer = null
let railDebounceTimer = null
let statusInFlight = false
let statusPollMs = 1500

function addLog(msg, type = 'info') {
  const time = new Date().toLocaleTimeString('zh-CN')
  logs.value.push({ time, msg, type })
  if (logs.value.length > 100) logs.value.shift()
  nextTick(() => {
    if (logContainer.value) logContainer.value.scrollTop = logContainer.value.scrollHeight
  })
}

function clearLogs() { logs.value = [] }

function setRailTarget(value) {
  railTarget.value = Math.max(0, Math.min(4000, Number(value) || 0))
}

function setPumpPwm(value) {
  pumpPwm.value = Math.max(0, Math.min(100, Number(value) || 0))
}

async function callPumpApi(path, payload, successMsg) {
  if (pumpBusy.value) return
  setPumpPwm(pumpPwm.value)
  pumpBusy.value = true
  try {
    const body = payload ? JSON.stringify(payload) : undefined
    const r = await fetchWithTimeout(
      apiUrl(path),
      {
        method: 'POST',
        headers: { ...authHeaders(), 'Content-Type': 'application/json' },
        body,
      },
      TASK_HTTP_MS,
    )
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      throw new Error(d.message || `水泵控制失败 HTTP ${r.status}`)
    }
    addLog(successMsg, 'task')
  } catch (e) {
    addLog(e.message || '水泵控制失败', 'error')
  } finally {
    pumpBusy.value = false
  }
}

async function pumpStart() {
  await callPumpApi('/api/aqua/pump/start', { pwm: pumpPwm.value }, `开泵，PWM=${pumpPwm.value}%`)
}

async function pumpApplyPwm() {
  await callPumpApi('/api/aqua/pump/pwm', { pwm: pumpPwm.value }, `更新水泵 PWM=${pumpPwm.value}%`)
}

async function pumpStop() {
  await callPumpApi('/api/aqua/pump/stop', {}, '关泵')
}

async function pumpAuto() {
  await callPumpApi('/api/aqua/pump/auto', {}, '切换为水泵自动模式')
}

async function sendTask(taskName) {
  if (busy.value || railPending.value) return
  const labels = { feed: '自动喂食', loosen: '松土', prune: '裁剪黄色叶子' }
  addLog(`触发任务: ${labels[taskName] ?? taskName}`, 'task')
  currentTask.value = labels[taskName] ?? taskName
  busy.value = true
  try {
    const r = await fetchWithTimeout(
      apiUrl(`/api/aqua/tasks/${taskName}`),
      { method: 'POST', headers: authHeaders() },
      TASK_HTTP_MS,
    )
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      throw new Error(d.message || `任务启动失败 HTTP ${r.status}`)
    }
    addLog(`任务已提交: ${labels[taskName] ?? taskName}`, 'task')
    fetchStatus()
  } catch (e) {
    busy.value = false
    addLog(e.message || '任务启动失败', 'error')
  }
}

async function stopTask() {
  addLog('请求停止当前任务', 'warn')
  try {
    const r = await fetchWithTimeout(
      apiUrl('/api/aqua/tasks/stop'),
      { method: 'POST', headers: authHeaders() },
      TASK_HTTP_MS,
    )
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      throw new Error(d.message || `停止失败 HTTP ${r.status}`)
    }
    currentTask.value = '停止中'
    phase.value = 'stopping'
    fetchStatus()
  } catch (e) {
    addLog(e.message || '停止任务失败', 'error')
  }
}

function moveRail() {
  if (busy.value || railPending.value) return
  clearTimeout(railDebounceTimer)
  railDebounceTimer = setTimeout(() => {
    railDebounceTimer = null
    executeMoveRail()
  }, RAIL_DEBOUNCE_MS)
}

async function executeMoveRail() {
  if (busy.value || railPending.value) return
  setRailTarget(railTarget.value)
  railPending.value = true
  addLog(`滑轨移动到 ${railTarget.value}`, 'robot')
  try {
    let r = await fetchWithTimeout(
      apiUrl('/api/aqua/rail/position'),
      {
        method: 'POST',
        headers: { ...authHeaders(), 'Content-Type': 'application/json' },
        body: JSON.stringify({ position: railTarget.value }),
      },
      RAIL_HTTP_MS,
    )
    if (r.status === 404) {
      r = await fetchWithTimeout(
        apiUrl('/api/aqua/rail/move'),
        {
          method: 'POST',
          headers: { ...authHeaders(), 'Content-Type': 'application/json' },
          body: JSON.stringify({ position: railTarget.value }),
        },
        RAIL_HTTP_MS,
      )
    }
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      throw new Error(d.message || `滑轨移动失败 HTTP ${r.status}`)
    }
    railPosition.value = railTarget.value
    fetchStatus()
  } catch (e) {
    addLog(e.message || '滑轨移动失败', 'error')
  } finally {
    railPending.value = false
  }
}

async function fetchStatus() {
  if (statusInFlight) return
  statusInFlight = true
  try {
    const r = await fetchWithTimeout(apiUrl('/api/aqua/status'), { headers: authHeaders() }, STATUS_HTTP_MS)
    if (r.ok) {
      const d = await r.json()
      connected.value = d.connected ?? d.ok ?? true
      busy.value = Boolean(d.busy)
      currentTask.value = d.currentTask ?? '待命'
      phase.value = d.phase ?? 'idle'
      railPosition.value = d.railPosition ?? railPosition.value
      railTarget.value = Number(railPosition.value ?? railTarget.value)
      lastError.value = d.lastError ?? ''
      if (d.uptimeSec != null) uptime.value = formatUptime(Number(d.uptimeSec))
      else uptime.value = d.uptime ?? uptime.value
      if (d.camera) {
        cameraState.hasRgb = Boolean(d.camera.hasRgb)
        cameraState.hasDepth = Boolean(d.camera.hasDepth)
        cameraState.ageSec = d.camera.ageSec ?? null
      }
      // Bridge 在手动模式下返回 UI 语义 pwm；自动模式为 null，避免轮询把滑块拽成 0 或与硬件反向值混淆
      if (d.pump && d.pump.manualOn === true && typeof d.pump.pwm === 'number') {
        setPumpPwm(d.pump.pwm)
      }
    }
  } catch { connected.value = false }
  finally { statusInFlight = false }
}

function restartStatusTimer() {
  if (statusTimer) clearInterval(statusTimer)
  statusTimer = setInterval(fetchStatus, statusPollMs)
}

function onPageVisibility() {
  statusPollMs = document.hidden ? 4500 : 1500
  restartStatusTimer()
  if (!document.hidden) fetchStatus()
}

function formatUptime(seconds) {
  const s = Math.max(0, Math.floor(seconds))
  const h = String(Math.floor(s / 3600)).padStart(2, '0')
  const m = String(Math.floor((s % 3600) / 60)).padStart(2, '0')
  const sec = String(s % 60).padStart(2, '0')
  return `${h}:${m}:${sec}`
}

onMounted(() => {
  addLog('机械臂控制台已加载', 'system')
  addLog('已切换为绝对位置控制模式（禁用点按增量移动）', 'system')
  fetchStatus()
  restartStatusTimer()
  document.addEventListener('visibilitychange', onPageVisibility)
})

onUnmounted(() => {
  document.removeEventListener('visibilitychange', onPageVisibility)
  if (statusTimer) clearInterval(statusTimer)
  clearTimeout(railDebounceTimer)
})
</script>

<style scoped>
.robot-page {
  display: grid;
  grid-template-columns: minmax(520px, 1fr) 390px;
  gap: 20px;
  grid-template-areas: "left right";
  height: calc(100vh - 120px);
  min-height: 600px;
  align-items: stretch;
}

.quick-status {
  flex: none;
  padding: 5px 10px;
  border-radius: 999px;
  font-size: 12px;
  font-weight: 700;
}

.quick-status.online {
  color: #10b981;
  background: rgba(16,185,129,0.12);
}

.quick-status.offline {
  color: #ef4444;
  background: rgba(239,68,68,0.12);
}

.arm-actions {
  display: grid;
  grid-template-columns: repeat(4, minmax(110px, 1fr));
  gap: 12px;
  padding: 18px;
}

.primary-task-btn {
  min-height: 82px;
  border: none;
  border-radius: 14px;
  color: #fff;
  cursor: pointer;
  font-size: 14px;
  font-weight: 700;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  transition: transform 0.18s, filter 0.18s, opacity 0.18s;
  box-shadow: 0 12px 28px rgba(0,0,0,0.18);
}

.primary-task-btn i {
  font-size: 22px;
}

.primary-task-btn:hover {
  transform: translateY(-2px);
  filter: brightness(1.08);
}

.primary-task-btn:disabled {
  opacity: 0.45;
  cursor: not-allowed;
  transform: none;
}

.task-loosen { background: linear-gradient(135deg, #10b981, #059669); }
.task-feed { background: linear-gradient(135deg, #f59e0b, #d97706); }
.task-prune { background: linear-gradient(135deg, #8b5cf6, #6d28d9); }
.task-stop { background: linear-gradient(135deg, #ef4444, #dc2626); }

/* ---- Left Panel ---- */
.robot-left {
  grid-area: left;
  display: flex;
  flex-direction: column;
  gap: 16px;
  min-width: 0;
  height:100%;
}

.control-panel, .terminal-panel {
  background: var(--bg-card);
  border-radius: 14px;
  box-shadow: var(--shadow-md);
  overflow: hidden;
}

.panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 18px;
  border-bottom: 1px solid rgba(255,255,255,0.06);
  font-size: 13px;
  font-weight: 600;
  color: var(--text-secondary);
}
.panel-header i { color: var(--primary-color); margin-right: 6px; }
.kbd-hint { font-size: 11px; color: var(--text-secondary); font-weight: 400; }

.control-panel-header {
  min-height: 62px;
}

.control-panel-header > div {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.control-panel-header small {
  color: var(--text-secondary);
  font-size: 12px;
  font-weight: 400;
}

.rail-control-block,
.pump-body {
  margin: 0 18px 18px;
  padding: 16px;
  border-radius: 12px;
  background: rgba(15,15,26,0.78);
  border: 1px solid rgba(255,255,255,0.06);
}

.rail-title,
.rail-row {
  display: flex;
  align-items: center;
  gap: 8px;
}

.rail-row-direct .rail-input {
  flex: 0 1 180px;
}

.rail-title {
  justify-content: space-between;
  font-size: 13px;
  color: var(--text-secondary);
}

.rail-title i {
  color: var(--primary-color);
  margin-right: 6px;
}

.rail-title b {
  color: var(--text-primary);
  font-family: monospace;
}

.pump-range {
  width: 100%;
  margin: 4px 0 14px;
  accent-color: #06b6d4;
}

.rail-input {
  min-width: 0;
  flex: 1;
  height: 34px;
  border: 1px solid rgba(255,255,255,0.08);
  background: var(--bg-card);
  color: var(--text-primary);
  border-radius: 8px;
  padding: 0 10px;
  font-family: monospace;
}

.rail-btn {
  height: 34px;
  border: none;
  background: var(--bg-card);
  color: var(--text-secondary);
  border-radius: 8px;
  padding: 0 10px;
  cursor: pointer;
  transition: all 0.18s;
}

.rail-btn:hover {
  background: rgba(139,92,246,0.15);
  color: var(--primary-color);
}

.rail-btn-primary {
  background: var(--primary-color);
  color: #fff;
}

.rail-btn-danger {
  background: rgba(239,68,68,0.16);
  color: #f87171;
}

.rail-btn-danger:hover {
  background: #ef4444;
  color: #fff;
}

.rail-input-large {
  height: 38px;
  font-size: 15px;
}

.rail-btn:disabled,
.task-btn:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

.pump-readout {
  font-family: monospace;
  color: #67e8f9;
  font-size: 20px;
}

/* ---- Right Panel: Terminal ---- */
.robot-right { grid-area: right; min-width: 0; }
.terminal-panel {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.terminal-header { background: rgba(15,15,26,0.8); }
.terminal-status-row { display: flex; align-items: center; gap: 6px; }
.status-dot { width: 8px; height: 8px; border-radius: 50%; }
.dot-connected { background: #10b981; box-shadow: 0 0 6px #10b981; }
.dot-disconnected { background: #ef4444; }
.status-text { font-size: 12px; color: var(--text-secondary); }

.terminal-status-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 10px;
  padding: 14px 18px;
  border-bottom: 1px solid rgba(255,255,255,0.06);
}

.terminal-status-grid > div {
  min-width: 0;
  padding: 10px;
  border-radius: 10px;
  background: rgba(15,15,26,0.72);
  border: 1px solid rgba(255,255,255,0.06);
  display: flex;
  flex-direction: column;
  gap: 5px;
}

.terminal-status-grid span {
  color: var(--text-secondary);
  font-size: 12px;
}

.terminal-status-grid b {
  color: var(--text-primary);
  font-family: monospace;
  font-size: 14px;
  overflow: hidden;
  text-overflow: ellipsis;
}

.info-row { display: flex; justify-content: space-between; align-items: center; font-size: 13px; }
.info-label { color: var(--text-secondary); display: flex; align-items: center; gap: 6px; }
.info-label i { color: var(--primary-color); font-size: 12px; }
.info-val { color: var(--text-primary); font-weight: 600; font-family: monospace; }
.info-row-error {
  align-items: flex-start;
  gap: 12px;
}
.info-row-error .info-val {
  color: #f87171;
  text-align: right;
  white-space: normal;
}

/* Log section */
.log-section { flex: 1; display: flex; flex-direction: column; overflow: hidden; }
.log-section-title {
  display: flex; align-items: center; gap: 6px;
  padding: 10px 18px 8px;
  font-size: 11px; color: var(--text-secondary); font-weight: 600; letter-spacing: 0.5px; text-transform: uppercase;
}
.clear-btn {
  margin-left: auto; background: none; border: 1px solid rgba(255,255,255,0.1);
  color: var(--text-secondary); padding: 2px 8px; border-radius: 6px; font-size: 11px; cursor: pointer;
}
.clear-btn:hover { color: var(--text-primary); }
.log-terminal {
  flex: 1;
  overflow-y: auto;
  padding: 4px 10px 10px;
  font-family: 'Courier New', monospace;
  font-size: 12px;
  background: #0a0a14;
}
.log-line { display: flex; align-items: flex-start; gap: 6px; padding: 3px 0; line-height: 1.5; }
.log-ts { color: #4b5563; min-width: 70px; font-size: 11px; }
.log-badge {
  padding: 1px 5px; border-radius: 4px; font-size: 10px; font-weight: 700; text-transform: uppercase;
  min-width: 36px; text-align: center;
}
.badge-info   { background: rgba(59,130,246,0.2); color: #60a5fa; }
.badge-robot  { background: rgba(139,92,246,0.2); color: #a78bfa; }
.badge-task   { background: rgba(16,185,129,0.2); color: #34d399; }
.badge-warn   { background: rgba(245,158,11,0.2); color: #fbbf24; }
.badge-error  { background: rgba(239,68,68,0.2); color: #f87171; }
.badge-system { background: rgba(156,163,175,0.2); color: #9ca3af; }
.log-msg { color: #d1d5db; flex: 1; word-break: break-all; }

@media (max-width: 1024px) {
  .robot-page {
    grid-template-columns: 1fr;
    grid-template-areas:
      "left"
      "right";
    height: auto;
  }
  .arm-actions { grid-template-columns: repeat(2, minmax(120px, 1fr)); }
  .terminal-panel { min-height: 500px; }
}

@media (max-width: 640px) {
  .arm-actions { grid-template-columns: 1fr; }
  .rail-row { flex-wrap: wrap; }
  .rail-input { flex-basis: 100%; }
}
</style>
