<template>
  <div class="robot-page">
    <div class="quick-panel">
      <div class="quick-header">
        <div>
          <h3><i class="fas fa-robot"></i> 机械臂快捷操作</h3>
          <p>常用任务和滑轨移动放在顶部，执行前请确认相机画面和 Bridge 状态。</p>
        </div>
        <span :class="['quick-status', connected ? 'online' : 'offline']">{{ connected ? 'Bridge 在线' : 'Bridge 离线' }}</span>
      </div>

      <div class="primary-task-grid">
        <button class="primary-task-btn task-loosen" :disabled="busy" @click="sendTask('loosen')">
          <i class="fas fa-seedling"></i>
          <span>松土</span>
        </button>
        <button class="primary-task-btn task-feed" :disabled="busy" @click="sendTask('feed')">
          <i class="fas fa-utensils"></i>
          <span>喂食</span>
        </button>
        <button class="primary-task-btn task-prune" :disabled="busy" @click="sendTask('prune')">
          <i class="fas fa-cut"></i>
          <span>裁剪黄色叶子</span>
        </button>
        <button class="primary-task-btn task-stop" @click="stopTask">
          <i class="fas fa-hand-paper"></i>
          <span>停止</span>
        </button>
      </div>

      <div class="rail-panel rail-panel-top">
        <div class="rail-title">
          <span><i class="fas fa-arrows-alt-h"></i> 滑轨移动（0-4000）</span>
          <b>当前位置：{{ railPositionText }}</b>
        </div>
        <input v-model.number="railTarget" class="rail-range" type="range" min="0" max="4000" step="10" />
        <div class="rail-row">
          <button class="rail-btn" @click="setRailTarget(0)">回到 0</button>
          <input v-model.number="railTarget" class="rail-input" type="number" min="0" max="4000" step="10" />
          <button class="rail-btn" @click="setRailTarget(4000)">到 4000</button>
          <button class="rail-btn rail-btn-primary" :disabled="busy" @click="moveRail">移动滑轨</button>
        </div>
      </div>
    </div>

    <!-- 左侧面板：摄像头 + 控制按钮 -->
    <div class="robot-left">
      <!-- 摄像头区域 -->
      <div class="camera-panel">
        <div class="panel-header">
          <span><i class="fas fa-video"></i> 机械臂视角</span>
          <div class="camera-header-actions">
            <button type="button" :class="['mode-btn', cameraMode === 'rgb' ? 'active' : '']" @click="setCameraMode('rgb')">RGB</button>
            <button type="button" :class="['mode-btn', cameraMode === 'depth' ? 'active' : '']" @click="setCameraMode('depth')">深度图</button>
            <span class="badge-live"><span class="live-dot"></span>Live</span>
          </div>
        </div>
        <div class="camera-body">
          <img :key="camSrc" :src="camSrc" alt="Robot Camera" class="camera-img" @load="cameraError = ''" @error="cameraError = '机械臂相机画面加载失败，请检查树莓派 Bridge 视频流'" />
          <div v-if="cameraError" class="camera-error">
            <i class="fas fa-video-slash"></i>
            <span>{{ cameraError }}</span>
            <small>当前地址：{{ camSrc }}</small>
          </div>
          <div class="cam-overlay-info">
            <span>{{ cameraMode === 'rgb' ? 'RGB' : 'Depth' }}</span>
            <span>J1:{{ servoAngles[0] }}</span>
            <span>J2:{{ servoAngles[1] }}</span>
            <span>J3:{{ servoAngles[2] }}</span>
          </div>
        </div>
      </div>

      <!-- 控制区域 -->
      <div class="control-panel">
        <div class="panel-header">
          <span><i class="fas fa-crosshairs"></i> 绝对位置控制</span>
          <span class="kbd-hint">已禁用点按增量移动，仅保留绝对位置下发</span>
        </div>

        <div class="control-body">
          <div class="move-disabled-box">
            <i class="fas fa-ban"></i>
            <span>方向点按移动已关闭</span>
            <small>请使用上方滑轨绝对位置控制（0-4000）</small>
          </div>

          <!-- 位置显示 -->
          <div class="pos-bar">
            <span class="pos-item"><b>X</b> {{ posX }}</span>
            <span class="pos-item"><b>Y</b> {{ posY }}</span>
            <span class="pos-item"><b>Z</b> {{ posZ }}</span>
          </div>

        </div>
      </div>
    </div>

    <!-- 右侧面板：终端状态 -->
    <div class="robot-right">
      <div class="terminal-panel">
        <div class="panel-header terminal-header">
          <span><i class="fas fa-terminal"></i> 机械臂状态终端</span>
          <div class="terminal-status-row">
            <span :class="['status-dot', connected ? 'dot-connected' : 'dot-disconnected']"></span>
            <span class="status-text">{{ connected ? '已连接' : '断开' }}</span>
          </div>
        </div>

        <!-- 舵机角度可视化 -->
        <div class="servo-section">
          <div class="servo-title">舵机角度</div>
          <div class="servo-list">
            <div v-for="(angle, i) in servoAngles" :key="i" class="servo-row">
              <span class="servo-label">J{{ i + 1 }}</span>
              <div class="servo-bar-bg">
                <div class="servo-bar-fill" :style="{ width: (angle / 180 * 100) + '%' }"></div>
              </div>
              <span class="servo-val">{{ angle }}</span>
            </div>
          </div>
        </div>

        <!-- 当前任务 & 信息 -->
        <div class="info-section">
          <div class="info-row">
            <span class="info-label"><i class="fas fa-tasks"></i> 当前任务</span>
            <span class="info-val">{{ currentTask }}</span>
          </div>
          <div class="info-row">
            <span class="info-label"><i class="fas fa-project-diagram"></i> 当前阶段</span>
            <span class="info-val">{{ phase }}</span>
          </div>
          <div class="info-row">
            <span class="info-label"><i class="fas fa-arrows-alt-h"></i> 滑轨位置</span>
            <span class="info-val">{{ railPositionText }}</span>
          </div>
          <div class="info-row">
            <span class="info-label"><i class="fas fa-camera"></i> 相机</span>
            <span class="info-val">RGB:{{ cameraState.hasRgb ? 'OK' : '--' }} / D:{{ cameraState.hasDepth ? 'OK' : '--' }}</span>
          </div>
          <div class="info-row">
            <span class="info-label"><i class="fas fa-clock"></i> 运行时长</span>
            <span class="info-val">{{ uptime }}</span>
          </div>
          <div class="info-row">
            <span class="info-label"><i class="fas fa-map-marker-alt"></i> 末端位置</span>
            <span class="info-val">X:{{ posX }} Y:{{ posY }} Z:{{ posZ }}</span>
          </div>
          <div v-if="lastError" class="info-row info-row-error">
            <span class="info-label"><i class="fas fa-exclamation-triangle"></i> 错误</span>
            <span class="info-val">{{ lastError }}</span>
          </div>
        </div>

        <!-- 日志终端 -->
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
import { useRouter } from 'vue-router'
import { apiUrl, authHeaders, logout } from '@/api/http'

const router = useRouter()

const posX = ref(0)
const posY = ref(0)
const posZ = ref(0)
const servoAngles = ref([90, 45, 120, 60, 90, 30])
const connected = ref(true)
const currentTask = ref('待命')
const phase = ref('idle')
const busy = ref(false)
const lastError = ref('')
const uptime = ref('00:00:00')
const logs = ref([])
const logContainer = ref(null)
const cameraMode = ref('rgb')
const cameraError = ref('')
const cameraReloadKey = ref(Date.now())
const railTarget = ref(0)
const railPosition = ref(null)
const cameraState = reactive({ hasRgb: false, hasDepth: false, ageSec: null })
const servoPulse = ref({ 1: 220, 2: 489, 3: 130, 4: 842, 5: 836, 6: 509 })
const camSrc = computed(() => apiUrl(`/api/aqua/video/${cameraMode.value}`) + `?t=${cameraReloadKey.value}`)
const railPositionText = computed(() => railPosition.value == null ? '--' : String(railPosition.value))

let statusTimer = null

function addLog(msg, type = 'info') {
  const time = new Date().toLocaleTimeString('zh-CN')
  logs.value.push({ time, msg, type })
  if (logs.value.length > 100) logs.value.shift()
  nextTick(() => {
    if (logContainer.value) logContainer.value.scrollTop = logContainer.value.scrollHeight
  })
}

function clearLogs() { logs.value = [] }

function setCameraMode(mode) {
  if (cameraMode.value === mode) return
  cameraMode.value = mode
  cameraReloadKey.value = Date.now()
  cameraError.value = ''
  addLog(`切换机械臂相机：${mode === 'rgb' ? 'RGB' : '深度图'}`, 'system')
}

function setRailTarget(value) {
  railTarget.value = Math.max(0, Math.min(4000, Number(value) || 0))
}

async function armHome() {
  addLog('请求机械臂回初始位姿', 'system')
  try {
    const r = await fetch(apiUrl('/api/aqua/arm/home'), {
      method: 'POST',
      headers: authHeaders(),
    })
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      throw new Error(d.message || `回初始位姿失败 HTTP ${r.status}`)
    }
    posX.value = 0
    posY.value = 0
    posZ.value = 0
    fetchStatus()
  } catch (e) {
    addLog(e.message || '机械臂回初始位姿失败', 'error')
  }
}

async function sendTask(taskName) {
  const labels = { feed: '自动喂食', loosen: '松土', prune: '裁剪黄色叶子' }
  addLog(`触发任务: ${labels[taskName] ?? taskName}`, 'task')
  currentTask.value = labels[taskName] ?? taskName
  busy.value = true
  try {
    const r = await fetch(apiUrl(`/api/aqua/tasks/${taskName}`), {
      method: 'POST',
      headers: authHeaders(),
    })
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
    const r = await fetch(apiUrl('/api/aqua/tasks/stop'), {
      method: 'POST',
      headers: authHeaders(),
    })
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      throw new Error(d.message || `停止失败 HTTP ${r.status}`)
    }
    currentTask.value = '停止中'
    phase.value = 'stopping'
  } catch (e) {
    addLog(e.message || '停止任务失败', 'error')
  }
}

async function moveRail() {
  setRailTarget(railTarget.value)
  addLog(`滑轨移动到 ${railTarget.value}`, 'robot')
  try {
    let r = await fetch(apiUrl('/api/aqua/rail/position'), {
      method: 'POST',
      headers: authHeaders(),
      body: JSON.stringify({ position: railTarget.value }),
    })
    // 兼容旧后端：仅实现了 /api/aqua/rail/move
    if (r.status === 404) {
      r = await fetch(apiUrl('/api/aqua/rail/move'), {
        method: 'POST',
        headers: authHeaders(),
        body: JSON.stringify({ position: railTarget.value }),
      })
    }
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      throw new Error(d.message || `滑轨移动失败 HTTP ${r.status}`)
    }
    railPosition.value = railTarget.value
    fetchStatus()
  } catch (e) {
    addLog(e.message || '滑轨移动失败', 'error')
  }
}

async function fetchStatus() {
  try {
    const r = await fetch(apiUrl('/api/aqua/status'), { headers: authHeaders() })
    if (r.ok) {
      const d = await r.json()
      const pulses = d.servoPulse
      if (pulses && typeof pulses === 'object') {
        servoPulse.value = { ...servoPulse.value, ...pulses }
        servoAngles.value = [1, 2, 3, 4, 5, 6].map(i => Number(pulses[String(i)] ?? servoAngles.value[i - 1]))
      } else if (Array.isArray(d.servoAngles)) {
        servoAngles.value = d.servoAngles
      }
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
    }
  } catch { connected.value = false }
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
  statusTimer = setInterval(fetchStatus, 1500)
})

onUnmounted(() => {
  if (statusTimer) clearInterval(statusTimer)
})
</script>

<style scoped>
.robot-page {
  display: grid;
  grid-template-columns: 1fr 380px;
  gap: 20px;
  grid-template-areas:
    "quick quick"
    "left right";
  height: calc(100vh - 120px);
  min-height: 600px;
}

.quick-panel {
  grid-area: quick;
  background: var(--bg-card);
  border-radius: 14px;
  box-shadow: var(--shadow-md);
  padding: 16px 18px;
  display: grid;
  grid-template-columns: minmax(360px, 1fr) minmax(360px, 1.1fr);
  gap: 16px;
  align-items: stretch;
}

.quick-header {
  grid-column: 1 / -1;
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 12px;
}

.quick-header h3 {
  margin: 0 0 4px;
  font-size: 18px;
  color: var(--text-primary);
}

.quick-header h3 i {
  color: var(--primary-color);
  margin-right: 8px;
}

.quick-header p {
  margin: 0;
  font-size: 12px;
  color: var(--text-secondary);
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

.primary-task-grid {
  display: grid;
  grid-template-columns: repeat(4, minmax(110px, 1fr));
  gap: 10px;
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
}

.camera-panel, .control-panel, .terminal-panel {
  background: var(--bg-card);
  border-radius: 14px;
  box-shadow: var(--shadow-md);
  overflow: hidden;
}

.camera-error {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  padding: 20px;
  text-align: center;
  color: rgba(255,255,255,0.9);
  background: rgba(15,23,42,0.78);
  font-size: 13px;
  z-index: 2;
}

.camera-error i {
  font-size: 24px;
  color: #f87171;
}

.camera-error small {
  color: rgba(255,255,255,0.62);
  word-break: break-all;
  font-family: monospace;
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

.camera-header-actions {
  display: flex;
  align-items: center;
  gap: 8px;
}

.mode-btn {
  border: 1px solid rgba(139,92,246,0.25);
  background: var(--bg-main);
  color: var(--text-secondary);
  border-radius: 999px;
  padding: 4px 10px;
  font-size: 11px;
  cursor: pointer;
  transition: all 0.18s;
}

.mode-btn.active,
.mode-btn:hover {
  background: var(--primary-color);
  color: #fff;
  border-color: var(--primary-color);
}

.badge-live {
  display: flex; align-items: center; gap: 5px;
  background: rgba(239,68,68,0.12); color: #ef4444;
  padding: 3px 8px; border-radius: 20px; font-size: 11px; font-weight: 600;
}
.live-dot {
  width: 6px; height: 6px; border-radius: 50%; background: #ef4444;
  animation: pulse 1.4s infinite;
}
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.3} }

.camera-body { position: relative; background: #000; aspect-ratio: 16/9; }
.camera-img { width: 100%; height: 100%; object-fit: cover; display: block; }
.cam-overlay-info {
  position: absolute; bottom: 8px; left: 8px;
  display: flex; gap: 6px;
}
.cam-overlay-info span {
  background: rgba(0,0,0,0.65); color: #a5f3fc;
  padding: 2px 7px; border-radius: 6px; font-size: 11px; font-family: monospace;
}

/* Control body */
.control-body {
  padding: 16px 20px;
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.dir-grid {
  display: grid;
  grid-template-columns: repeat(3, 56px);
  grid-template-rows: repeat(3, 56px);
  gap: 8px;
  justify-content: center;
}

.dir-grid-lr {
  grid-template-rows: 56px;
}

.move-disabled-box {
  border: 1px dashed rgba(139,92,246,0.4);
  background: rgba(139,92,246,0.08);
  border-radius: 12px;
  padding: 12px;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  color: var(--text-secondary);
  font-size: 13px;
}

.move-disabled-box i {
  color: #ef4444;
}

.move-disabled-box small {
  color: var(--text-secondary);
}

.dir-btn {
  border: none;
  background: var(--bg-main);
  color: var(--text-primary);
  border-radius: 12px;
  cursor: pointer;
  font-size: 18px;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 6px;
  transition: all 0.18s;
  box-shadow: var(--shadow-sm);
}
.dir-btn:hover { background: var(--bg-gradient); color: #fff; transform: scale(1.06); }
.dir-btn:active { transform: scale(0.94); }
.dir-btn-left, .dir-btn-right { font-size: 13px; font-weight: 600; }
.dir-btn-center { background: rgba(139,92,246,0.12); color: var(--primary-color); }

.pos-bar {
  display: flex; justify-content: center; gap: 20px;
  background: var(--bg-main); border-radius: 10px; padding: 10px 16px;
  font-size: 15px; font-family: monospace;
}
.pos-item b { color: var(--primary-color); margin-right: 4px; }

.rail-panel {
  background: var(--bg-main);
  border-radius: 12px;
  padding: 12px;
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.rail-panel-top {
  background: rgba(15,15,26,0.78);
}

.rail-title,
.rail-row {
  display: flex;
  align-items: center;
  gap: 8px;
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

.rail-range {
  width: 100%;
  accent-color: var(--primary-color);
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

.rail-btn:disabled,
.task-btn:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

.task-btns { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
.task-btn {
  padding: 9px 4px; background: var(--bg-main); color: var(--text-primary);
  border: none; border-radius: 10px; cursor: pointer; font-size: 12px;
  display: flex; align-items: center; justify-content: center; gap: 5px;
  transition: all 0.18s;
}
.task-btn:hover { background: var(--primary-color); color: #fff; }
.task-btn-stop { background: rgba(239,68,68,0.12); color: #ef4444; }
.task-btn-stop:hover { background: #ef4444; color: #fff; }

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

/* Servo section */
.servo-section { padding: 14px 18px; border-bottom: 1px solid rgba(255,255,255,0.06); }
.servo-title { font-size: 11px; color: var(--text-secondary); font-weight: 600; letter-spacing: 0.5px; margin-bottom: 10px; text-transform: uppercase; }
.servo-list { display: flex; flex-direction: column; gap: 7px; }
.servo-row { display: flex; align-items: center; gap: 8px; }
.servo-label { font-size: 12px; color: var(--primary-color); font-weight: 700; width: 24px; font-family: monospace; }
.servo-bar-bg { flex: 1; height: 6px; background: var(--bg-main); border-radius: 3px; overflow: hidden; }
.servo-bar-fill { height: 100%; background: linear-gradient(90deg, #8b5cf6, #06b6d4); border-radius: 3px; transition: width 0.4s; }
.servo-val { font-size: 11px; color: var(--text-secondary); font-family: monospace; width: 34px; text-align: right; }

/* Info section */
.info-section { padding: 12px 18px; border-bottom: 1px solid rgba(255,255,255,0.06); display: flex; flex-direction: column; gap: 8px; }
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
      "quick"
      "left"
      "right";
    height: auto;
  }
  .quick-panel { grid-template-columns: 1fr; }
  .primary-task-grid { grid-template-columns: repeat(2, minmax(120px, 1fr)); }
  .terminal-panel { min-height: 500px; }
}

@media (max-width: 640px) {
  .primary-task-grid { grid-template-columns: 1fr; }
  .rail-row { flex-wrap: wrap; }
  .rail-input { flex-basis: 100%; }
}
</style>
