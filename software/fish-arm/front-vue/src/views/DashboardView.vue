<template>
  <div class="dashboard">
    <!-- Two live video feeds -->
    <div class="video-row">
      <div class="video-card">
        <div class="video-card-header">
          <span class="video-title">Tank 1 Live Feed <span class="video-title-cn">(鱼缸1直播)</span></span>
          <button type="button" class="menu-btn"><i class="fas fa-ellipsis-h"></i></button>
        </div>
        <div class="video-body">
          <div class="video-area">
            <img :src="tank1Src" alt="Tank 1 Live Feed" />
            <div class="vbadge vbadge-live"><i class="fas fa-circle"></i> Live</div>
            <div class="vbadge vbadge-cam"><i class="fas fa-video"></i></div>
            <div class="vbadge vbadge-res">1080p</div>
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
          <span class="video-title">Tank 2 Live Feed <span class="video-title-cn">(鱼缸2直播)</span></span>
          <button type="button" class="menu-btn"><i class="fas fa-ellipsis-h"></i></button>
        </div>
        <div class="video-body">
          <div class="video-area">
            <img :src="tank2Src" alt="Tank 2 Live Feed" />
            <div class="vbadge vbadge-live"><i class="fas fa-circle"></i> Live</div>
            <div class="vbadge vbadge-cam"><i class="fas fa-video"></i></div>
            <div class="vbadge vbadge-res">1080p</div>
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
      <!-- Water Temperature -->
      <div class="sensor-card">
        <div class="sensor-card-header">
          <span class="sensor-label">Water Temp.</span>
          <span class="sensor-icon-btn sensor-icon-temp"><i class="fas fa-thermometer-half"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ temp }}</span>
          <span class="sensor-unit">°C</span>
        </div>
        <div class="sensor-value-sub">/{{ tempF }}°F</div>
        <div class="sensor-subtitle">水温</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(tempHistory, 22, 28)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-normal"></span>
          <span class="sensor-status-text">Normal</span>
          <span class="sensor-period">24h</span>
        </div>
      </div>

      <!-- pH Level -->
      <div class="sensor-card">
        <div class="sensor-card-header">
          <span class="sensor-label">pH Level</span>
          <span class="sensor-icon-btn sensor-icon-ph"><i class="fas fa-wave-square"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ ph }}</span>
          <span class="sensor-unit"> pH</span>
        </div>
        <div class="sensor-subtitle">酸碱度</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(phHistory, 6, 9)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-optimal"></span>
          <span class="sensor-status-text">Optimal</span>
          <span class="sensor-period">24h</span>
        </div>
      </div>

      <!-- Turbidity -->
      <div class="sensor-card">
        <div class="sensor-card-header">
          <span class="sensor-label">Turbidity</span>
          <span class="sensor-icon-btn sensor-icon-turbidity"><i class="fas fa-eye-slash"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ turbidity }}</span>
          <span class="sensor-unit"> NTU</span>
        </div>
        <div class="sensor-subtitle">浊度</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(turbidityHistory, 0, 30)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-low"></span>
          <span class="sensor-status-text">Low</span>
          <span class="sensor-period">24h</span>
        </div>
      </div>

      <!-- Dissolved Oxygen -->
      <div class="sensor-card">
        <div class="sensor-card-header">
          <span class="sensor-label">Dissolved Oxygen</span>
          <span class="sensor-icon-btn sensor-icon-oxygen"><i class="fas fa-wind"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ oxygen }}</span>
          <span class="sensor-unit"> mg/L</span>
        </div>
        <div class="sensor-subtitle">溶解氧</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(oxygenHistory, 5, 12)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-normal"></span>
          <span class="sensor-status-text">Normal</span>
          <span class="sensor-period">24h</span>
        </div>
      </div>

      <!-- Soil Moisture -->
      <div class="sensor-card">
        <div class="sensor-card-header">
          <span class="sensor-label">Soil Moisture</span>
          <span class="sensor-icon-btn sensor-icon-moisture"><i class="fas fa-tint"></i></span>
        </div>
        <div class="sensor-value-row">
          <span class="sensor-big">{{ soilMoisture }}</span>
          <span class="sensor-unit">%</span>
        </div>
        <div class="sensor-subtitle">土壤湿度</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(moistureHistory, 30, 100)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot dot-wet"></span>
          <span class="sensor-status-text">Wet</span>
          <span class="sensor-period">24h</span>
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

const temp = ref('26.5')
const ph = ref('7.2')
const oxygen = ref('8.1')
const turbidity = ref('1.5')
const soilMoisture = ref('68')

const tempF = computed(() => (parseFloat(temp.value) * 9 / 5 + 32).toFixed(1))

const camTick = ref(0)
const tank1Src = computed(() => `${apiUrl('/api/video/tank')}?t=${camTick.value}`)
const tank2Src = computed(() => `${apiUrl('/api/video/robot')}?t=${camTick.value}`)

const HISTORY_LEN = 20

const tempHistory = ref(Array.from({ length: HISTORY_LEN }, () => 25 + Math.random() - 0.5))
const phHistory = ref(Array.from({ length: HISTORY_LEN }, () => 7.0 + (Math.random() * 0.4 - 0.2)))
const oxygenHistory = ref(Array.from({ length: HISTORY_LEN }, () => 8.0 + (Math.random() * 0.6 - 0.3)))
const turbidityHistory = ref(Array.from({ length: HISTORY_LEN }, () => 1.5 + (Math.random() * 0.4 - 0.2)))
const moistureHistory = ref(Array.from({ length: HISTORY_LEN }, () => 65 + Math.random() * 6))

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
  temp.value = rnd(24, 27)
  ph.value = rnd(6.8, 7.5)
  oxygen.value = rnd(7, 9)
  turbidity.value = rnd(1.0, 3.0)
  soilMoisture.value = rnd(55, 80, 0)
  pushHistory(tempHistory, temp.value)
  pushHistory(phHistory, ph.value)
  pushHistory(oxygenHistory, oxygen.value)
  pushHistory(turbidityHistory, turbidity.value)
  pushHistory(moistureHistory, soilMoisture.value)
}

async function updateSensorData() {
  try {
    const r = await fetch(apiUrl('/api/sensors'), { headers: authHeaders() })
    if (r.ok) {
      const d = await r.json()
      temp.value = String(d.temperature ?? d.temp ?? temp.value)
      ph.value = String(d.ph ?? ph.value)
      oxygen.value = String(d.oxygen ?? oxygen.value)
      turbidity.value = String(d.turbidity ?? turbidity.value)
      soilMoisture.value = String(d.soil_moisture ?? d.soilMoisture ?? soilMoisture.value)
      pushHistory(tempHistory, temp.value)
      pushHistory(phHistory, ph.value)
      pushHistory(oxygenHistory, oxygen.value)
      pushHistory(turbidityHistory, turbidity.value)
      pushHistory(moistureHistory, soilMoisture.value)
    } else if (r.status === 401) {
      setTimeout(() => router.push({ name: 'login' }), 2000)
    }
  } catch {
    localSensors()
  }
}

let ws = null
let sensorTimer = null
let camTimer = null

function connectWs() {
  try {
    ws = new WebSocket(wsLogsUrl())
    ws.onmessage = () => {}
    ws.onerror = () => {}
  } catch {
    /* ignore */
  }
}

onMounted(() => {
  updateSensorData()
  sensorTimer = setInterval(updateSensorData, 5000)
  camTimer = setInterval(() => { camTick.value = Date.now() }, 2000)
  connectWs()
})

onUnmounted(() => {
  if (sensorTimer) clearInterval(sensorTimer)
  if (camTimer) clearInterval(camTimer)
  if (ws) ws.close()
})
</script>
