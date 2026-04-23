<template>
  <div class="robot-page">
    <!-- 左侧面板：摄像头 + 控制按钮 -->
    <div class="robot-left">
      <!-- 摄像头区域 -->
      <div class="camera-panel">
        <div class="panel-header">
          <span><i class="fas fa-video"></i> 机械臂视角</span>
          <span class="badge-live"><span class="live-dot"></span>Live</span>
        </div>
        <div class="camera-body">
          <img :src="camSrc" alt="Robot Camera" class="camera-img" />
          <div class="cam-overlay-info">
            <span>J1:{{ servoAngles[0] }}°</span>
            <span>J2:{{ servoAngles[1] }}°</span>
            <span>J3:{{ servoAngles[2] }}°</span>
          </div>
        </div>
      </div>

      <!-- 控制区域 -->
      <div class="control-panel">
        <div class="panel-header">
          <span><i class="fas fa-gamepad"></i> 方向控制</span>
          <span class="kbd-hint">键盘: A/D 左右 &nbsp; W/S 前后 &nbsp; Q/E Z轴</span>
        </div>

        <!-- 方向按钮 -->
        <div class="control-body">
          <div class="dir-grid">
            <div></div>
            <button class="dir-btn" @click="control('forward')" title="前进(W)"><i class="fas fa-chevron-up"></i></button>
            <div></div>
            <button class="dir-btn dir-btn-left" @click="control('left')" title="左移(A)">
              <i class="fas fa-arrow-left"></i><span>左</span>
            </button>
            <button class="dir-btn dir-btn-center" @click="centerRobot" title="复位(空格)">
              <i class="fas fa-crosshairs"></i>
            </button>
            <button class="dir-btn dir-btn-right" @click="control('right')" title="右移(D)">
              <span>右</span><i class="fas fa-arrow-right"></i>
            </button>
            <div></div>
            <button class="dir-btn" @click="control('backward')" title="后退(S)"><i class="fas fa-chevron-down"></i></button>
            <div></div>
          </div>

          <div class="z-btns">
            <button class="z-btn" @click="control('up')"><i class="fas fa-arrow-up"></i> Z+</button>
            <button class="z-btn" @click="control('down')"><i class="fas fa-arrow-down"></i> Z−</button>
          </div>

          <!-- 位置显示 -->
          <div class="pos-bar">
            <span class="pos-item"><b>X</b> {{ posX }}</span>
            <span class="pos-item"><b>Y</b> {{ posY }}</span>
            <span class="pos-item"><b>Z</b> {{ posZ }}</span>
          </div>

          <!-- 预设任务按钮 -->
          <div class="task-btns">
            <button class="task-btn" @click="sendTask('feed')"><i class="fas fa-utensils"></i> 喂食</button>
            <button class="task-btn" @click="sendTask('scoop')"><i class="fas fa-hand-sparkles"></i> 捞叶</button>
            <button class="task-btn" @click="sendTask('loosen')"><i class="fas fa-seedling"></i> 松土</button>
            <button class="task-btn task-btn-stop" @click="emergencyStop"><i class="fas fa-hand-paper"></i> 急停</button>
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
              <span class="servo-val">{{ angle }}°</span>
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
            <span class="info-label"><i class="fas fa-clock"></i> 运行时长</span>
            <span class="info-val">{{ uptime }}</span>
          </div>
          <div class="info-row">
            <span class="info-label"><i class="fas fa-map-marker-alt"></i> 末端位置</span>
            <span class="info-val">X:{{ posX }} Y:{{ posY }} Z:{{ posZ }}</span>
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
import { nextTick, onMounted, onUnmounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { apiUrl, authHeaders } from '@/api/http'

const router = useRouter()

const posX = ref(0)
const posY = ref(0)
const posZ = ref(0)
const servoAngles = ref([90, 45, 120, 60, 90, 30])
const connected = ref(true)
const currentTask = ref('待命')
const uptime = ref('00:00:00')
const logs = ref([])
const logContainer = ref(null)
const camTick = ref(0)
const camSrc = ref('')

let statusTimer = null
let camTimer = null

function addLog(msg, type = 'info') {
  const time = new Date().toLocaleTimeString('zh-CN')
  logs.value.push({ time, msg, type })
  if (logs.value.length > 100) logs.value.shift()
  nextTick(() => {
    if (logContainer.value) logContainer.value.scrollTop = logContainer.value.scrollHeight
  })
}

function clearLogs() { logs.value = [] }

async function control(direction) {
  try {
    const r = await fetch(apiUrl('/api/robot/control'), {
      method: 'POST',
      headers: authHeaders(),
      body: JSON.stringify({ direction }),
    })
    if (r.ok) {
      const d = await r.json()
      posX.value = d.position.x
      posY.value = d.position.y
      posZ.value = d.position.z
      addLog(`移动 ${direction} → X:${posX.value} Y:${posY.value} Z:${posZ.value}`, 'robot')
    } else if (r.status === 401) {
      addLog('登录已过期', 'error')
      setTimeout(() => router.push({ name: 'login' }), 2000)
    }
  } catch {
    simulateMovement(direction)
    addLog(`[离线] 模拟移动 ${direction}`, 'warn')
  }
}

function simulateMovement(direction) {
  const step = 1
  if (direction === 'forward')  posY.value = Math.min(10, posY.value + step)
  if (direction === 'backward') posY.value = Math.max(-10, posY.value - step)
  if (direction === 'left')     posX.value = Math.max(-10, posX.value - step)
  if (direction === 'right')    posX.value = Math.min(10, posX.value + step)
  if (direction === 'up')       posZ.value = Math.min(20, posZ.value + step)
  if (direction === 'down')     posZ.value = Math.max(0, posZ.value - step)
}

function centerRobot() {
  posX.value = 0; posY.value = 0; posZ.value = 0
  addLog('机械臂已复位到原点', 'system')
}

async function sendTask(taskName) {
  const labels = { feed: '自动喂食', scoop: '捞取落叶', loosen: '松土' }
  addLog(`触发任务: ${labels[taskName] ?? taskName}`, 'task')
  currentTask.value = labels[taskName] ?? taskName
  try {
    await fetch(apiUrl('/api/robot/task'), {
      method: 'POST',
      headers: authHeaders(),
      body: JSON.stringify({ task: taskName }),
    })
  } catch { /* offline */ }
  setTimeout(() => { currentTask.value = '待命'; addLog(`任务 ${labels[taskName]} 完成`, 'task') }, 4000)
}

function emergencyStop() {
  addLog('⚠ 紧急停止已触发！', 'error')
  currentTask.value = '已急停'
}

async function fetchStatus() {
  try {
    const r = await fetch(apiUrl('/api/robot/status'))
    if (r.ok) {
      const d = await r.json()
      if (Array.isArray(d.servoAngles)) servoAngles.value = d.servoAngles
      connected.value = d.connected ?? true
      currentTask.value = d.currentTask ?? '待命'
      uptime.value = d.uptime ?? uptime.value
    }
  } catch { connected.value = false }
}

function onKey(e) {
  const map = { ArrowUp:'forward', w:'forward', W:'forward', ArrowDown:'backward', s:'backward', S:'backward',
    ArrowLeft:'left', a:'left', A:'left', ArrowRight:'right', d:'right', D:'right',
    q:'up', Q:'up', e:'down', E:'down' }
  if (map[e.key]) control(map[e.key])
  if (e.key === ' ') { e.preventDefault(); centerRobot() }
}

onMounted(() => {
  window.addEventListener('keydown', onKey)
  addLog('机械臂控制台已加载', 'system')
  addLog('键盘: WASD 前后左右 | Q/E Z轴 | 空格 归零', 'system')
  fetchStatus()
  statusTimer = setInterval(fetchStatus, 1500)
  camTimer = setInterval(() => {
    camTick.value = Date.now()
    camSrc.value = apiUrl('/api/video/robot') + '?t=' + camTick.value
  }, 2000)
  camSrc.value = apiUrl('/api/video/robot')
})

onUnmounted(() => {
  window.removeEventListener('keydown', onKey)
  if (statusTimer) clearInterval(statusTimer)
  if (camTimer) clearInterval(camTimer)
})
</script>

<style scoped>
.robot-page {
  display: grid;
  grid-template-columns: 1fr 380px;
  gap: 20px;
  height: calc(100vh - 120px);
  min-height: 600px;
}

/* ---- Left Panel ---- */
.robot-left {
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

.z-btns { display: flex; gap: 10px; }
.z-btn {
  flex: 1; padding: 10px; background: var(--bg-main); color: var(--text-primary);
  border: none; border-radius: 10px; cursor: pointer; font-size: 13px;
  display: flex; align-items: center; justify-content: center; gap: 6px;
  transition: all 0.18s;
}
.z-btn:hover { background: var(--bg-gradient); color: #fff; }

.pos-bar {
  display: flex; justify-content: center; gap: 20px;
  background: var(--bg-main); border-radius: 10px; padding: 10px 16px;
  font-size: 15px; font-family: monospace;
}
.pos-item b { color: var(--primary-color); margin-right: 4px; }

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
.robot-right { min-width: 0; }
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
  .robot-page { grid-template-columns: 1fr; height: auto; }
  .terminal-panel { min-height: 500px; }
}
</style>
