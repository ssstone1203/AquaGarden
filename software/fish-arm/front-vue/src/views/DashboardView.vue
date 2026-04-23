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
      <div class="sensor-card sensor-card-clickable" @click="openDetail('temp')">
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
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>

      <!-- pH Level -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('ph')">
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
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>

      <!-- Turbidity -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('turbidity')">
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
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>

      <!-- Dissolved Oxygen -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('oxygen')">
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
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>

      <!-- Soil Moisture -->
      <div class="sensor-card sensor-card-clickable" @click="openDetail('moisture')">
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

// ---- Sensor detail modal ----
const SENSOR_META = {
  temp:     { title: '水温', subtitle: 'Water Temperature', unit: '°C', icon: 'fas fa-thermometer-half', iconClass: 'sensor-icon-temp', color: '#f59e0b', rangeMin: 18, rangeMax: 32, statusText: '正常', statusClass: 'modal-status-normal' },
  ph:       { title: 'pH 酸碱度', subtitle: 'pH Level', unit: ' pH', icon: 'fas fa-wave-square', iconClass: 'sensor-icon-ph', color: '#8b5cf6', rangeMin: 6, rangeMax: 9, statusText: '最优', statusClass: 'modal-status-optimal' },
  turbidity:{ title: '浊度', subtitle: 'Turbidity', unit: ' NTU', icon: 'fas fa-eye-slash', iconClass: 'sensor-icon-turbidity', color: '#06b6d4', rangeMin: 0, rangeMax: 30, statusText: '低', statusClass: 'modal-status-low' },
  oxygen:   { title: '溶解氧', subtitle: 'Dissolved Oxygen', unit: ' mg/L', icon: 'fas fa-wind', iconClass: 'sensor-icon-oxygen', color: '#10b981', rangeMin: 5, rangeMax: 12, statusText: '正常', statusClass: 'modal-status-normal' },
  moisture: { title: '土壤湿度', subtitle: 'Soil Moisture', unit: '%', icon: 'fas fa-tint', iconClass: 'sensor-icon-moisture', color: '#3b82f6', rangeMin: 30, rangeMax: 100, statusText: '湿润', statusClass: 'modal-status-wet' },
}

const historyMap = computed(() => ({
  temp: tempHistory.value,
  ph: phHistory.value,
  turbidity: turbidityHistory.value,
  oxygen: oxygenHistory.value,
  moisture: moistureHistory.value,
}))

const currentMap = computed(() => ({
  temp: temp.value, ph: ph.value, turbidity: turbidity.value,
  oxygen: oxygen.value, moisture: soilMoisture.value,
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
  if (d.temperature != null) { temp.value = String(d.temperature); pushHistory(tempHistory, d.temperature) }
  if (d.ph != null)          { ph.value = String(d.ph);           pushHistory(phHistory, d.ph) }
  if (d.oxygen != null)      { oxygen.value = String(d.oxygen);   pushHistory(oxygenHistory, d.oxygen) }
  if (d.turbidity != null)   { turbidity.value = String(d.turbidity); pushHistory(turbidityHistory, d.turbidity) }
  if (d.soil_moisture != null) { soilMoisture.value = String(d.soil_moisture); pushHistory(moistureHistory, d.soil_moisture) }
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

<style scoped>
.sensor-card-clickable { cursor: pointer; transition: transform 0.15s, box-shadow 0.15s; }
.sensor-card-clickable:hover { transform: translateY(-3px); box-shadow: 0 8px 24px rgba(139,92,246,0.18); }

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
