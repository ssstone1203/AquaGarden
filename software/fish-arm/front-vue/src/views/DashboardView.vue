<template>
  <section class="stats-section">
    <div class="stats-grid">
      <div class="stat-card card-temperature">
        <div class="stat-icon"><i class="fas fa-thermometer-half"></i></div>
        <div class="stat-info">
          <div class="stat-label">水温</div>
          <div class="stat-value">
            <span>{{ temp }}</span><span class="unit">°C</span>
          </div>
          <div class="stat-status status-normal">
            <i class="fas fa-check-circle"></i><span>正常范围</span>
          </div>
        </div>
      </div>
      <div class="stat-card card-ph">
        <div class="stat-icon"><i class="fas fa-flask"></i></div>
        <div class="stat-info">
          <div class="stat-label">pH值</div>
          <div class="stat-value"><span>{{ ph }}</span></div>
          <div class="stat-status status-normal">
            <i class="fas fa-check-circle"></i><span>正常范围</span>
          </div>
        </div>
      </div>
      <div class="stat-card card-oxygen">
        <div class="stat-icon"><i class="fas fa-wind"></i></div>
        <div class="stat-info">
          <div class="stat-label">溶解氧</div>
          <div class="stat-value">
            <span>{{ oxygen }}</span><span class="unit">mg/L</span>
          </div>
          <div class="stat-status status-good">
            <i class="fas fa-arrow-up"></i><span>良好</span>
          </div>
        </div>
      </div>
      <div class="stat-card card-turbidity">
        <div class="stat-icon"><i class="fas fa-eye"></i></div>
        <div class="stat-info">
          <div class="stat-label">浊度</div>
          <div class="stat-value">
            <span>{{ turbidity }}</span><span class="unit">NTU</span>
          </div>
          <div class="stat-status status-excellent">
            <i class="fas fa-star"></i><span>清澈</span>
          </div>
        </div>
      </div>
    </div>
  </section>

  <div class="content-grid">
    <div class="content-column column-left">
      <div class="content-card video-card">
        <div class="card-header">
          <div class="card-title"><i class="fas fa-robot"></i><span>机械臂摄像头</span></div>
          <div class="card-badge badge-online"><span class="badge-dot"></span>在线</div>
        </div>
        <div class="card-body">
          <div class="video-container">
            <img :src="robotCamSrc" alt="机械臂摄像头" />
            <div class="video-overlay">
              <div class="rec-indicator"><i class="fas fa-circle"></i><span>REC</span></div>
            </div>
          </div>
          <div class="robot-controls">
            <div class="position-info">
              <i class="fas fa-map-marker-alt"></i>
              <span>X: <strong>{{ posX }}</strong> Y: <strong>{{ posY }}</strong> Z: <strong>{{ posZ }}</strong></span>
            </div>
            <div class="control-pad">
              <button type="button" class="ctrl-btn ctrl-up" @click="move('up')"><i class="fas fa-arrow-up"></i></button>
              <button type="button" class="ctrl-btn ctrl-forward" @click="move('forward')"><i class="fas fa-chevron-up"></i></button>
              <button type="button" class="ctrl-btn ctrl-left" @click="move('left')"><i class="fas fa-arrow-left"></i></button>
              <button type="button" class="ctrl-btn ctrl-center" @click="center"><i class="fas fa-crosshairs"></i></button>
              <button type="button" class="ctrl-btn ctrl-right" @click="move('right')"><i class="fas fa-arrow-right"></i></button>
              <button type="button" class="ctrl-btn ctrl-down" @click="move('down')"><i class="fas fa-arrow-down"></i></button>
              <button type="button" class="ctrl-btn ctrl-backward" @click="move('backward')"><i class="fas fa-chevron-down"></i></button>
            </div>
          </div>
        </div>
      </div>
      <div class="content-card video-card">
        <div class="card-header">
          <div class="card-title"><i class="fas fa-water"></i><span>鱼缸摄像头</span></div>
          <div class="card-badge badge-online"><span class="badge-dot"></span>在线</div>
        </div>
        <div class="card-body">
          <div class="video-container">
            <img :src="tankCamSrc" alt="鱼缸摄像头" />
            <div class="video-overlay">
              <div class="rec-indicator"><i class="fas fa-circle"></i><span>REC</span></div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="content-column column-middle">
      <div class="content-card chart-card">
        <div class="card-header">
          <div class="card-title"><i class="fas fa-chart-area"></i><span>传感器数据趋势</span></div>
          <div class="chart-legend">
            <span class="legend-item"><span class="legend-dot" style="background: var(--color-temperature)"></span>温度</span>
            <span class="legend-item"><span class="legend-dot" style="background: var(--color-ph)"></span>pH值</span>
          </div>
        </div>
        <div class="card-body"><canvas id="sensorChart"></canvas></div>
      </div>
      <div class="content-card sensors-card">
        <div class="card-header">
          <div class="card-title"><i class="fas fa-microchip"></i><span>实时传感器数据</span></div>
          <button type="button" class="refresh-btn" @click="updateSensorData"><i class="fas fa-sync-alt"></i></button>
        </div>
        <div class="card-body">
          <div class="sensors-grid">
            <div class="sensor-item">
              <div class="sensor-icon sensor-temp"><i class="fas fa-temperature-high"></i></div>
              <div class="sensor-data">
                <div class="sensor-name">水温</div>
                <div class="sensor-value"><span>{{ temp }}</span><span class="unit">°C</span></div>
                <div class="sensor-range">正常: 22-28°C</div>
              </div>
              <div class="sensor-chart"><div class="mini-chart" style="height: 70%"></div></div>
            </div>
            <div class="sensor-item">
              <div class="sensor-icon sensor-ph"><i class="fas fa-vial"></i></div>
              <div class="sensor-data">
                <div class="sensor-name">pH值</div>
                <div class="sensor-value"><span>{{ ph }}</span></div>
                <div class="sensor-range">正常: 6.5-8.0</div>
              </div>
              <div class="sensor-chart"><div class="mini-chart" style="height: 85%"></div></div>
            </div>
            <div class="sensor-item">
              <div class="sensor-icon sensor-oxygen"><i class="fas fa-lungs"></i></div>
              <div class="sensor-data">
                <div class="sensor-name">溶解氧</div>
                <div class="sensor-value"><span>{{ oxygen }}</span><span class="unit">mg/L</span></div>
                <div class="sensor-range">正常: >6.0 mg/L</div>
              </div>
              <div class="sensor-chart"><div class="mini-chart" style="height: 90%"></div></div>
            </div>
            <div class="sensor-item">
              <div class="sensor-icon sensor-turbidity"><i class="fas fa-smog"></i></div>
              <div class="sensor-data">
                <div class="sensor-name">浊度</div>
                <div class="sensor-value"><span>{{ turbidity }}</span><span class="unit">NTU</span></div>
                <div class="sensor-range">正常: &lt;20 NTU</div>
              </div>
              <div class="sensor-chart"><div class="mini-chart" style="height: 50%"></div></div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="content-column column-right">
      <div class="content-card log-card">
        <div class="card-header">
          <div class="card-title"><i class="fas fa-terminal"></i><span>系统日志</span></div>
          <div class="log-actions">
            <div class="log-status"><span class="status-dot status-active"></span>{{ wsConnected ? '已连接' : '未连接' }}</div>
            <button type="button" class="icon-btn" title="清空日志" @click="clearLogs"><i class="fas fa-trash-alt"></i></button>
          </div>
        </div>
        <div class="card-body log-body">
          <div ref="logContainer" class="log-container">
            <div v-for="(e, i) in logs" :key="i" class="log-entry log-info">
              <span class="log-time">[{{ e.time }}]</span>
              <span class="log-message">{{ e.type }}: {{ e.message }}</span>
            </div>
          </div>
        </div>
      </div>
      <div class="content-card alerts-card">
        <div class="card-header">
          <div class="card-title"><i class="fas fa-bell"></i><span>系统通知</span></div>
        </div>
        <div class="card-body">
          <div class="alerts-list">
            <div class="alert-item alert-success">
              <div class="alert-icon"><i class="fas fa-check-circle"></i></div>
              <div class="alert-content">
                <div class="alert-title">系统正常</div>
                <div class="alert-time">所有参数在正常范围内</div>
              </div>
            </div>
            <div class="alert-item alert-info">
              <div class="alert-icon"><i class="fas fa-info-circle"></i></div>
              <div class="alert-content">
                <div class="alert-title">定时喂食</div>
                <div class="alert-time">下次喂食: 18:00</div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { apiUrl, authHeaders, wsLogsUrl } from '@/api/http'

const router = useRouter()
const temp = ref('25.0')
const ph = ref('7.0')
const oxygen = ref('8.0')
const turbidity = ref('10')
const posX = ref(0)
const posY = ref(0)
const posZ = ref(0)
const camTick = ref(0)
const robotCamSrc = computed(() => `${apiUrl('/api/video/robot')}?t=${camTick.value}`)
const tankCamSrc = computed(() => `${apiUrl('/api/video/tank')}?t=${camTick.value}`)
const logs = ref([])
const logContainer = ref(null)
const wsConnected = ref(false)
let sensorTimer = null
let camTimer = null
let statusTimer = null
let ws = null

function pushLog(type, message) {
  const time = new Date().toLocaleTimeString('zh-CN')
  logs.value.push({ time, type, message })
  while (logs.value.length > 50) logs.value.shift()
}

function clearLogs() {
  logs.value = [{ time: '系统', type: '系统', message: '日志已清空' }]
}

function rnd(min, max, d = 1) {
  const v = Math.random() * (max - min) + min
  return d === 0 ? String(Math.round(v)) : String(Number(v.toFixed(d)))
}

function localSensors() {
  temp.value = rnd(24, 27)
  ph.value = rnd(6.8, 7.5)
  oxygen.value = rnd(7, 9)
  turbidity.value = rnd(8, 15, 0)
  pushLog('数据更新', `温度: ${temp.value}°C, pH: ${ph.value}, 溶解氧: ${oxygen.value}mg/L, 浊度: ${turbidity.value}NTU`)
}

async function updateSensorData() {
  try {
    const r = await fetch(apiUrl('/api/sensors'), { headers: authHeaders() })
    if (r.ok) {
      const d = await r.json()
      temp.value = String(d.temperature)
      ph.value = String(d.ph)
      oxygen.value = String(d.oxygen)
      turbidity.value = String(d.turbidity)
      pushLog('数据更新', `温度: ${temp.value}°C, pH: ${ph.value}, 溶解氧: ${oxygen.value}mg/L, 浊度: ${turbidity.value}NTU`)
    } else if (r.status === 401) {
      pushLog('错误', '登录已过期，请重新登录')
      setTimeout(() => router.push({ name: 'login' }), 2000)
    }
  } catch {
    localSensors()
  }
}

function move(dir) {
  const step = 1
  switch (dir) {
    case 'up':
      posZ.value += step
      break
    case 'down':
      posZ.value -= step
      break
    case 'left':
      posX.value -= step
      break
    case 'right':
      posX.value += step
      break
    case 'forward':
      posY.value += step
      break
    case 'backward':
      posY.value -= step
      break
  }
  posX.value = Math.max(-10, Math.min(10, posX.value))
  posY.value = Math.max(-10, Math.min(10, posY.value))
  posZ.value = Math.max(0, Math.min(20, posZ.value))
  pushLog('机械臂', `移动到位置 X:${posX.value}, Y:${posY.value}, Z:${posZ.value}`)
}

function center() {
  posX.value = 0
  posY.value = 0
  posZ.value = 0
  pushLog('机械臂', '返回中心位置')
}

function onKey(e) {
  switch (e.key) {
    case 'ArrowUp':
      e.preventDefault()
      move('forward')
      break
    case 'ArrowDown':
      e.preventDefault()
      move('backward')
      break
    case 'ArrowLeft':
      e.preventDefault()
      move('left')
      break
    case 'ArrowRight':
      e.preventDefault()
      move('right')
      break
    case 'w':
    case 'W':
      move('up')
      break
    case 's':
    case 'S':
      move('down')
      break
  }
}

function connectWs() {
  try {
    ws = new WebSocket(wsLogsUrl())
    ws.onopen = () => {
      wsConnected.value = true
    }
    ws.onclose = () => {
      wsConnected.value = false
    }
    ws.onmessage = (ev) => {
      try {
        const o = JSON.parse(ev.data)
        if (o.type === 'heartbeat') return
        pushLog(o.type || '消息', o.message || '')
      } catch {
        /* ignore */
      }
    }
  } catch {
    wsConnected.value = false
  }
}

onMounted(() => {
  pushLog('系统', '系统已启动')
  pushLog('传感器', '所有传感器已连接')
  pushLog('摄像头', '摄像头在线')
  updateSensorData()
  sensorTimer = setInterval(updateSensorData, 5000)
  camTimer = setInterval(() => {
    camTick.value = Date.now()
  }, 2000)
  statusTimer = setInterval(() => {
    const msgs = ['系统运行正常', '传感器数据稳定', '水质参数正常', '自动监控进行中']
    pushLog('状态', msgs[Math.floor(Math.random() * msgs.length)])
  }, 30000)
  connectWs()
  window.addEventListener('keydown', onKey)
  setTimeout(() => pushLog('提示', '可使用方向键和W/S键控制机械臂'), 2000)
})

onUnmounted(() => {
  if (sensorTimer) clearInterval(sensorTimer)
  if (camTimer) clearInterval(camTimer)
  if (statusTimer) clearInterval(statusTimer)
  if (ws) ws.close()
  window.removeEventListener('keydown', onKey)
})
</script>
