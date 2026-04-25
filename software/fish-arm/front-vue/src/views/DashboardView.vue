<template>
  <div class="dashboard">
    <!-- Two live video feeds -->
    <div class="video-row">
      <div class="video-card">
        <div class="video-card-header">
          <span class="video-title">Robot Arm Camera <span class="video-title-cn">(机械臂摄像头)</span></span>
          <button type="button" class="menu-btn"><i class="fas fa-ellipsis-h"></i></button>
        </div>
        <div class="video-body">
          <div class="video-area">
            <img :src="robotCameraSrc" alt="Robot Arm Camera" />
            <div class="vbadge vbadge-live"><i class="fas fa-circle"></i> Live</div>
            <div class="vbadge vbadge-cam"><i class="fas fa-video"></i></div>
            <div class="vbadge vbadge-res">RGB</div>
          </div>
          <div class="video-controls">
            <div class="vctrl-left">
              <button type="button" class="vctrl-btn"><i class="fas fa-play"></i></button>
              <button type="button" class="vctrl-btn"><i class="fas fa-pause"></i></button>
              <button type="button" class="vctrl-btn"><i class="fas fa-stop"></i></button>
            </div>
            <div class="vctrl-right">
              <button type="button" class="vctrl-btn"><i class="fas fa-history"></i></button>
              <button type="button" class="vctrl-btn"><i class="fas fa-cog"></i></button>
            </div>
          </div>
        </div>
      </div>

      <div class="video-card">
        <div class="video-card-header">
          <span class="video-title">Tank Camera <span class="video-title-cn">(鱼缸摄像头)</span></span>
          <button type="button" class="menu-btn"><i class="fas fa-ellipsis-h"></i></button>
        </div>
        <div class="video-body">
          <div class="video-area">
            <img :src="tankCameraSrc" alt="Tank Camera" />
            <div class="vbadge vbadge-live"><i class="fas fa-circle"></i> Live</div>
            <div class="vbadge vbadge-cam"><i class="fas fa-video"></i></div>
            <div class="vbadge vbadge-res">{{ tankStatus.hasFrame ? 'USB 实时' : '等待鱼缸帧' }}</div>
            <div v-if="!tankStatus.hasFrame" class="video-waiting">
              <i class="fas fa-plug"></i>
              <span>后端还没有收到鱼缸摄像头帧</span>
            </div>
          </div>
          <div class="video-controls">
            <div class="vctrl-left">
              <button type="button" class="vctrl-btn"><i class="fas fa-play"></i></button>
              <button type="button" class="vctrl-btn"><i class="fas fa-pause"></i></button>
              <button type="button" class="vctrl-btn"><i class="fas fa-stop"></i></button>
            </div>
            <div class="vctrl-right">
              <button type="button" class="vctrl-btn"><i class="fas fa-history"></i></button>
              <button type="button" class="vctrl-btn"><i class="fas fa-cog"></i></button>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Five sensor metric cards -->
    <div class="sensor-row">
      <!-- Water Temperature (DS18B20) -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('waterTemp')">
        <div class="sensor-card-header">
          <span class="sensor-label">Water Temp.</span>
          <span class="sensor-icon-btn sensor-icon-temp"><i class="fas fa-thermometer-half"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ waterTemp }}</span>
          <span class="sensor-unit">°C</span>
        </div>
        <div class="sensor-value-sub">/{{ waterTempF }}°F</div>
        <div class="sensor-subtitle">水温 · DS18B20</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(waterTempHistory, 15, 35)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-normal"></span>
          <span class="sensor-status-text">Normal</span>
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>

      <!-- Air Temperature (SHT30) -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('airTemp')">
        <div class="sensor-card-header">
          <span class="sensor-label">Air Temp.</span>
          <span class="sensor-icon-btn sensor-icon-ph"><i class="fas fa-sun"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ airTemp }}</span>
          <span class="sensor-unit">°C</span>
        </div>
        <div class="sensor-subtitle">空气温度 · SHT30</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(airTempHistory, 5, 60)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-optimal"></span>
          <span class="sensor-status-text">Normal</span>
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>

      <!-- Air Humidity (SHT30) -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('airHumidity')">
        <div class="sensor-card-header">
          <span class="sensor-label">Air Humidity</span>
          <span class="sensor-icon-btn sensor-icon-turbidity"><i class="fas fa-cloud"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ airHumidity }}</span>
          <span class="sensor-unit">%RH</span>
        </div>
        <div class="sensor-subtitle">空气湿度 · SHT30</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(airHumidityHistory, 0, 100)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-normal"></span>
          <span class="sensor-status-text">Normal</span>
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>

      <!-- Water Quality Index (WQM11S) -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('wqi')">
        <div class="sensor-card-header">
          <span class="sensor-label">Water Quality</span>
          <span class="sensor-icon-btn sensor-icon-oxygen"><i class="fas fa-tachometer-alt"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ wqi }}</span>
          <span class="sensor-unit"> / 100</span>
        </div>
        <div class="sensor-subtitle">水质综合指数 · WQM11S</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(wqiHistory, 0, 100)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-normal"></span>
          <span class="sensor-status-text">Good</span>
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>

      <!-- Soil Moisture (ADC) -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('moisture')">
        <div class="sensor-card-header">
          <span class="sensor-label">Soil Moisture</span>
          <span class="sensor-icon-btn sensor-icon-moisture"><i class="fas fa-tint"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ soilMoisture }}</span>
          <span class="sensor-unit">%</span>
        </div>
        <div class="sensor-subtitle">土壤湿度 · ADC</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(moistureHistory, 0, 100)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-wet"></span>
          <span class="sensor-status-text">Wet</span>
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>
    </div>

    <!-- Sensor Detail Modal -->
    <div v-if="detailModal.show" class="sensor-modal-overlay" @click.self="closeDetail">
      <div class="sensor-modal">
        <div class="sensor-modal-header">
          <div class="sensor-modal-title">
            <span :class="['sensor-modal-icon', detailModal.iconClass]"><i :class="detailModal.icon"></i></span>
            <div>
              <h2>{{ detailModal.title }}</h2>
              <p>{{ detailModal.subtitle }}</p>
            </div>
          </div>
          <button class="sensor-modal-close" @click="closeDetail"><i class="fas fa-times"></i></button>
        </div>

        <div class="sensor-modal-stats">
          <div class="stat-card">
            <span class="stat-label">当前值</span>
            <span class="stat-value">{{ detailModal.current }}<small>{{ detailModal.unit }}</small></span>
          </div>
          <div class="stat-card">
            <span class="stat-label">最小值</span>
            <span class="stat-value stat-min">{{ detailModal.min }}<small>{{ detailModal.unit }}</small></span>
          </div>
          <div class="stat-card">
            <span class="stat-label">最大值</span>
            <span class="stat-value stat-max">{{ detailModal.max }}<small>{{ detailModal.unit }}</small></span>
          </div>
          <div class="stat-card">
            <span class="stat-label">平均值</span>
            <span class="stat-value stat-avg">{{ detailModal.avg }}<small>{{ detailModal.unit }}</small></span>
          </div>
        </div>

        <div class="sensor-modal-charts">
          <!-- Line Chart -->
          <div class="modal-chart-block">
            <h3><i class="fas fa-chart-line"></i> 实时趋势（最近 {{ detailModal.history.length }} 次采样）</h3>
            <div class="modal-line-chart">
              <svg viewBox="0 0 400 120" preserveAspectRatio="none" class="modal-svg">
                <defs>
                  <linearGradient :id="'grad-' + detailModal.key" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0%" :stop-color="detailModal.color" stop-opacity="0.3"/>
                    <stop offset="100%" :stop-color="detailModal.color" stop-opacity="0"/>
                  </linearGradient>
                </defs>
                <polygon
                  :points="areaPoints(detailModal.history, detailModal.rangeMin, detailModal.rangeMax)"
                  :fill="'url(#grad-' + detailModal.key + ')'"
                />
                <polyline
                  :points="modalLinePoints(detailModal.history, detailModal.rangeMin, detailModal.rangeMax)"
                  fill="none"
                  :stroke="detailModal.color"
                  stroke-width="2"
                  stroke-linejoin="round"
                />
                <line x1="0" y1="110" x2="400" y2="110" stroke="#e5e7eb" stroke-width="1"/>
                <text x="4" y="108" font-size="9" fill="#9ca3af">{{ detailModal.rangeMin }}{{ detailModal.unit }}</text>
                <text x="4" y="14" font-size="9" fill="#9ca3af">{{ detailModal.rangeMax }}{{ detailModal.unit }}</text>
              </svg>
            </div>
          </div>

          <!-- Bar Chart -->
          <div class="modal-chart-block">
            <h3><i class="fas fa-chart-bar"></i> 近期采样分布</h3>
            <div class="modal-bar-chart">
              <div
                v-for="(v, i) in detailModal.history.slice(-12)"
                :key="i"
                class="modal-bar-wrap"
              >
                <div
                  class="modal-bar"
                  :style="{
                    height: barPct(v, detailModal.rangeMin, detailModal.rangeMax) + '%',
                    background: detailModal.color
                  }"
                  :title="v + detailModal.unit"
                ></div>
                <span class="modal-bar-label">{{ i + 1 }}</span>
              </div>
            </div>
          </div>
        </div>

        <div class="sensor-modal-footer">
          <span class="modal-update-time">最近更新：{{ new Date().toLocaleTimeString('zh-CN') }}</span>
          <span :class="['modal-status-badge', detailModal.statusClass]">{{ detailModal.statusText }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { apiUrl, authHeaders, logout, wsLogsUrl } from '@/api/http'

const router = useRouter()

// 5 个传感器（与 MCU 上行帧字段一一对应）
const waterTemp    = ref('20.0')   // DS18B20 水温 (°C)
const airTemp      = ref('26.0')   // SHT30 空气温度 (°C)
const airHumidity  = ref('55.0')   // SHT30 空气湿度 (%RH)
const wqi          = ref('72')     // WQM11S 水质综合指数 (0-100)
const soilMoisture = ref('62')     // ADC 土壤湿度 (%)

const waterTempF = computed(() => (parseFloat(waterTemp.value) * 9 / 5 + 32).toFixed(1))

const robotCameraSrc = apiUrl('/api/aqua/video/rgb')
const tankCameraSrc = apiUrl('/api/video/tank')
const tankStatus = reactive({ hasFrame: false, seq: 0, bytes: 0, updatedAt: 0 })

const HISTORY_LEN = 20

const waterTempHistory   = ref(Array.from({ length: HISTORY_LEN }, () => 20 + Math.random() * 2 - 1))
const airTempHistory     = ref(Array.from({ length: HISTORY_LEN }, () => 26 + Math.random() * 4 - 2))
const airHumidityHistory = ref(Array.from({ length: HISTORY_LEN }, () => 55 + Math.random() * 10 - 5))
const wqiHistory         = ref(Array.from({ length: HISTORY_LEN }, () => 72 + Math.random() * 12 - 6))
const moistureHistory    = ref(Array.from({ length: HISTORY_LEN }, () => 62 + Math.random() * 10 - 5))

function pushHistory(arr, val) {
  arr.value.push(parseFloat(val))
  if (arr.value.length > HISTORY_LEN) arr.value.shift()
}

function sparkPoints(history, min, max) {
  const vals = history.value
  if (!vals || vals.length < 2) return ''
  const w = 100
  const h = 32
  const pad = 3
  const range = max - min || 1
  return vals
    .map((v, i) => {
      const x = (i / (vals.length - 1)) * (w - 2 * pad) + pad
      const y = h - pad - ((v - min) / range) * (h - 2 * pad)
      return `${x.toFixed(1)},${Math.max(pad, Math.min(h - pad, y)).toFixed(1)}`
    })
    .join(' ')
}

function rnd(min, max, d = 1) {
  const v = Math.random() * (max - min) + min
  return d === 0 ? String(Math.round(v)) : String(Number(v.toFixed(d)))
}

function localSensors() {
  waterTemp.value    = rnd(23, 26)
  airTemp.value      = rnd(24, 30)
  airHumidity.value  = rnd(50, 70)
  wqi.value          = rnd(65, 85, 0)
  soilMoisture.value = rnd(55, 75, 0)
  pushHistory(waterTempHistory,   waterTemp.value)
  pushHistory(airTempHistory,     airTemp.value)
  pushHistory(airHumidityHistory, airHumidity.value)
  pushHistory(wqiHistory,         wqi.value)
  pushHistory(moistureHistory,    soilMoisture.value)
}

async function updateSensorData() {
  try {
    const r = await fetch(apiUrl('/api/sensors'), { headers: authHeaders() })
    if (r.ok) {
      const d = await r.json()
      waterTemp.value    = String(d.water_temp    ?? waterTemp.value)
      airTemp.value      = String(d.air_temp      ?? airTemp.value)
      airHumidity.value  = String(d.air_humidity  ?? airHumidity.value)
      wqi.value          = String(d.wqi           ?? wqi.value)
      soilMoisture.value = String(d.soil_moisture ?? soilMoisture.value)
      pushHistory(waterTempHistory,   waterTemp.value)
      pushHistory(airTempHistory,     airTemp.value)
      pushHistory(airHumidityHistory, airHumidity.value)
      pushHistory(wqiHistory,         wqi.value)
      pushHistory(moistureHistory,    soilMoisture.value)
    } else if (r.status === 401) {
      setTimeout(() => logout(router), 2000)
    }
  } catch {
    localSensors()
  }
}

async function updateVideoStatus() {
  try {
    const r = await fetch(apiUrl('/api/video/tank/status'))
    if (!r.ok) return
    const d = await r.json()
    tankStatus.hasFrame = Boolean(d.hasFrame)
    tankStatus.seq = Number(d.seq ?? 0)
    tankStatus.bytes = Number(d.bytes ?? 0)
    tankStatus.updatedAt = Number(d.updatedAt ?? 0)
  } catch {
    tankStatus.hasFrame = false
  }
}

let ws = null
let sensorTimer = null
let videoStatusTimer = null

// ---- Sensor detail modal ----
// 每项参数与 MCU 传感器规格一一对应：
//   waterTemp   DS18B20   -55~125°C，显示范围 15~35°C
//   airTemp     SHT30     -40~125°C，推荐工作 5~60°C
//   airHumidity SHT30     0~100%RH，推荐 20~80%
//   wqi         WQM11S    0~100（综合水质评分）
//   moisture    ADC       0~100%
const SENSOR_META = {
  waterTemp:   { title: '水温',          subtitle: 'Water Temperature (DS18B20)', unit: '°C',   icon: 'fas fa-thermometer-half',  iconClass: 'sensor-icon-temp',     color: '#f59e0b', rangeMin: 15,  rangeMax: 35,  statusText: '正常', statusClass: 'modal-status-normal' },
  airTemp:     { title: '空气温度',       subtitle: 'Air Temperature (SHT30)',     unit: '°C',   icon: 'fas fa-sun',               iconClass: 'sensor-icon-ph',       color: '#8b5cf6', rangeMin: 5,   rangeMax: 60,  statusText: '正常', statusClass: 'modal-status-optimal' },
  airHumidity: { title: '空气湿度',       subtitle: 'Air Humidity (SHT30)',        unit: '%RH',  icon: 'fas fa-cloud',             iconClass: 'sensor-icon-turbidity', color: '#06b6d4', rangeMin: 0,   rangeMax: 100, statusText: '正常', statusClass: 'modal-status-normal' },
  wqi:         { title: '水质综合指数',    subtitle: 'Water Quality Index (WQM11S)',unit: ' 分',  icon: 'fas fa-tachometer-alt',    iconClass: 'sensor-icon-oxygen',   color: '#10b981', rangeMin: 0,   rangeMax: 100, statusText: '良好', statusClass: 'modal-status-normal' },
  moisture:    { title: '土壤湿度',       subtitle: 'Soil Moisture (ADC)',         unit: '%',    icon: 'fas fa-tint',              iconClass: 'sensor-icon-moisture', color: '#3b82f6', rangeMin: 0,   rangeMax: 100, statusText: '湿润', statusClass: 'modal-status-wet' },
}

const historyMap = computed(() => ({
  waterTemp:   waterTempHistory.value,
  airTemp:     airTempHistory.value,
  airHumidity: airHumidityHistory.value,
  wqi:         wqiHistory.value,
  moisture:    moistureHistory.value,
}))

const currentMap = computed(() => ({
  waterTemp:   waterTemp.value,
  airTemp:     airTemp.value,
  airHumidity: airHumidity.value,
  wqi:         wqi.value,
  moisture:    soilMoisture.value,
}))

const detailModal = reactive({ show: false, key: '' })

function openDetail(key) {
  const meta = SENSOR_META[key]
  const hist = historyMap.value[key] ?? []
  const nums = hist.map(Number).filter(n => !isNaN(n))
  Object.assign(detailModal, {
    show: true, key,
    ...meta,
    history: hist,
    current: currentMap.value[key],
    min: nums.length ? Math.min(...nums).toFixed(2) : '--',
    max: nums.length ? Math.max(...nums).toFixed(2) : '--',
    avg: nums.length ? (nums.reduce((a, b) => a + b, 0) / nums.length).toFixed(2) : '--',
  })
}

function closeDetail() { detailModal.show = false }

function modalLinePoints(history, min, max) {
  const vals = history.map(Number)
  if (vals.length < 2) return ''
  const W = 400, H = 110, pad = 5
  const range = max - min || 1
  return vals.map((v, i) => {
    const x = (i / (vals.length - 1)) * (W - 2 * pad) + pad
    const y = H - pad - ((v - min) / range) * (H - 2 * pad)
    return `${x.toFixed(1)},${Math.max(pad, Math.min(H - pad, y)).toFixed(1)}`
  }).join(' ')
}

function areaPoints(history, min, max) {
  const line = modalLinePoints(history, min, max)
  if (!line) return ''
  const vals = history.map(Number)
  const W = 400, H = 110, pad = 5
  const firstX = pad
  const lastX = (W - 2 * pad) + pad
  return `${firstX},${H} ${line} ${lastX},${H}`
}

function barPct(v, min, max) {
  const range = max - min || 1
  return Math.max(5, Math.min(100, ((Number(v) - min) / range) * 100))
}

function applySnapshot(d) {
  if (d.water_temp    != null) { waterTemp.value    = String(d.water_temp);    pushHistory(waterTempHistory,   d.water_temp) }
  if (d.air_temp      != null) { airTemp.value      = String(d.air_temp);      pushHistory(airTempHistory,     d.air_temp) }
  if (d.air_humidity  != null) { airHumidity.value  = String(d.air_humidity);  pushHistory(airHumidityHistory, d.air_humidity) }
  if (d.wqi           != null) { wqi.value          = String(d.wqi);           pushHistory(wqiHistory,         d.wqi) }
  if (d.soil_moisture != null) { soilMoisture.value = String(d.soil_moisture); pushHistory(moistureHistory,    d.soil_moisture) }
}

function connectWs() {
  try {
    ws = new WebSocket(wsLogsUrl())
    ws.onmessage = (event) => {
      try {
        const d = JSON.parse(event.data)
        if (d.type === 'sensor_data') applySnapshot(d)
      } catch { /* ignore malformed messages */ }
    }
    ws.onerror = () => {}
    ws.onclose = () => {
      // 断线后 5 秒重连
      setTimeout(connectWs, 5000)
    }
  } catch {
    /* ignore */
  }
}

onMounted(() => {
  updateSensorData()
  updateVideoStatus()
  sensorTimer = setInterval(updateSensorData, 5000)
  videoStatusTimer = setInterval(updateVideoStatus, 2000)
  connectWs()
})

onUnmounted(() => {
  if (sensorTimer) clearInterval(sensorTimer)
  if (videoStatusTimer) clearInterval(videoStatusTimer)
  if (ws) ws.close()
})
</script>

<style scoped>
.sensor-card-clickable { cursor: pointer; transition: transform 0.15s, box-shadow 0.15s; }
.sensor-card-clickable:hover { transform: translateY(-3px); box-shadow: 0 8px 24px rgba(139,92,246,0.18); }

.video-waiting {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  color: rgba(255,255,255,0.85);
  background: rgba(15,23,42,0.42);
  font-size: 13px;
  font-weight: 600;
  pointer-events: none;
}

.video-waiting i {
  font-size: 22px;
}

/* Modal overlay */
.sensor-modal-overlay {
  position: fixed; inset: 0; background: rgba(0,0,0,0.55); z-index: 1000;
  display: flex; align-items: center; justify-content: center; padding: 20px;
}
.sensor-modal {
  background: var(--bg-card, #1e1e2e); border-radius: 16px; width: 100%; max-width: 680px;
  box-shadow: 0 24px 60px rgba(0,0,0,0.4); overflow: hidden;
  animation: modalIn 0.2s ease;
}
@keyframes modalIn { from { opacity:0; transform:scale(0.94) } to { opacity:1; transform:scale(1) } }

.sensor-modal-header {
  display: flex; align-items: center; justify-content: space-between;
  padding: 20px 24px; border-bottom: 1px solid rgba(255,255,255,0.08);
}
.sensor-modal-title { display: flex; align-items: center; gap: 14px; }
.sensor-modal-title h2 { font-size: 18px; font-weight: 700; color: var(--text-primary, #fff); margin: 0; }
.sensor-modal-title p { font-size: 12px; color: var(--text-secondary, #9ca3af); margin: 4px 0 0; }
.sensor-modal-icon {
  width: 44px; height: 44px; border-radius: 12px;
  display: flex; align-items: center; justify-content: center; font-size: 18px; color: #fff;
}
.sensor-modal-close {
  background: none; border: none; color: var(--text-secondary, #9ca3af);
  font-size: 18px; cursor: pointer; padding: 6px; border-radius: 8px;
  transition: color 0.2s;
}
.sensor-modal-close:hover { color: var(--text-primary, #fff); }

/* Stats row */
.sensor-modal-stats {
  display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; padding: 20px 24px;
}
.stat-card {
  background: var(--bg-main, #0f0f1a); border-radius: 10px; padding: 14px;
  display: flex; flex-direction: column; gap: 4px;
}
.stat-label { font-size: 11px; color: var(--text-secondary, #9ca3af); }
.stat-value { font-size: 20px; font-weight: 700; color: var(--text-primary, #fff); }
.stat-value small { font-size: 11px; font-weight: 400; margin-left: 2px; color: var(--text-secondary, #9ca3af); }
.stat-min { color: #06b6d4; }
.stat-max { color: #f59e0b; }
.stat-avg { color: #8b5cf6; }

/* Charts */
.sensor-modal-charts { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; padding: 0 24px 20px; }
.modal-chart-block h3 {
  font-size: 13px; color: var(--text-secondary, #9ca3af); margin: 0 0 10px;
  display: flex; align-items: center; gap: 6px;
}
.modal-chart-block h3 i { color: var(--primary-color, #8b5cf6); }

.modal-line-chart { background: var(--bg-main, #0f0f1a); border-radius: 10px; padding: 10px; }
.modal-svg { width: 100%; height: 120px; display: block; }

.modal-bar-chart {
  background: var(--bg-main, #0f0f1a); border-radius: 10px; padding: 10px;
  display: flex; align-items: flex-end; gap: 4px; height: 140px;
}
.modal-bar-wrap { flex: 1; display: flex; flex-direction: column; align-items: center; gap: 4px; height: 100%; justify-content: flex-end; }
.modal-bar { width: 100%; min-height: 4px; border-radius: 3px 3px 0 0; transition: height 0.3s; opacity: 0.85; }
.modal-bar:hover { opacity: 1; }
.modal-bar-label { font-size: 9px; color: var(--text-secondary, #9ca3af); }

/* Footer */
.sensor-modal-footer {
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 24px; border-top: 1px solid rgba(255,255,255,0.06);
  font-size: 12px; color: var(--text-secondary, #9ca3af);
}
.modal-status-badge {
  padding: 3px 10px; border-radius: 20px; font-weight: 600; font-size: 12px;
}
.modal-status-normal  { background: rgba(16,185,129,0.15); color: #10b981; }
.modal-status-optimal { background: rgba(139,92,246,0.15); color: #8b5cf6; }
.modal-status-low     { background: rgba(6,182,212,0.15); color: #06b6d4; }
.modal-status-wet     { background: rgba(59,130,246,0.15); color: #3b82f6; }

@media (max-width: 600px) {
  .sensor-modal-stats { grid-template-columns: repeat(2,1fr); }
  .sensor-modal-charts { grid-template-columns: 1fr; }
}
</style>
