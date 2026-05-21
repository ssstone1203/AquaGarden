<template>
  <div class="dashboard">
    <!-- Two live video feeds -->
    <div ref="dashboardVideoRow" class="video-row">
      <div class="video-card">
        <div class="video-card-header">
          <span class="video-title">Robot Arm Camera <span class="video-title-cn">(机械臂摄像头)</span></span>
          <div class="camera-mode-switch">
            <button type="button" :class="['camera-mode-btn', robotCameraMode === 'rgb' ? 'active' : '']" @click="setRobotCameraMode('rgb')">RGB</button>
            <button type="button" :class="['camera-mode-btn', robotCameraMode === 'depth' ? 'active' : '']" @click="setRobotCameraMode('depth')">深度图</button>
          </div>
        </div>
        <div class="video-body">
          <div class="video-area">
            <img
              :key="robotCameraImgKey"
              :src="showLiveVideos ? robotCameraSrc : ''"
              alt="Robot Arm Camera"
              decoding="async"
              fetchpriority="high"
              @load="robotVideoError = ''"
              @error="handleRobotVideoError"
            />
            <div class="vbadge vbadge-live"><i class="fas fa-circle"></i> {{ robotVideoReady ? 'Live' : 'Wait' }}</div>
            <div class="vbadge vbadge-cam"><i class="fas fa-video"></i></div>
            <div class="vbadge vbadge-res">{{ robotVideoReady ? robotCameraLabel : 'Bridge' }}</div>
            <div v-if="robotVideoMessage" class="video-waiting">
              <i class="fas fa-video-slash"></i>
              <span>{{ robotVideoMessage }}</span>
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
            <img
              :src="showLiveVideos ? tankCameraSrc : ''"
              alt="Tank Camera"
              decoding="async"
              fetchpriority="low"
              @load="tankVideoError = ''"
              @error="tankVideoError = '鱼缸视频流加载失败'"
            />
            <div class="vbadge vbadge-live"><i class="fas fa-circle"></i> {{ tankVideoReady ? 'Live' : 'Wait' }}</div>
            <div class="vbadge vbadge-cam"><i class="fas fa-video"></i></div>
            <div class="vbadge vbadge-res">{{ tankStatus.hasFrame ? 'USB 实时' : '等待鱼缸帧' }}{{ tankStatus.detectionCount ? ` · ${tankStatus.detectionCount} 检测` : '' }}</div>
            <div v-if="tankVideoMessage" class="video-waiting">
              <i class="fas fa-plug"></i>
              <span>{{ tankVideoMessage }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div
      v-if="sensorBannerMessage"
      class="sensor-banner"
      role="status"
    >
      <i class="fas fa-plug"></i>
      <span>{{ sensorBannerMessage }}</span>
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
        <div class="sensor-value-sub">{{ waterTempFSub }}</div>
        <div class="sensor-subtitle">水温 · DS18B20</div>
        <div class="sparkline-wrap">
          <svg class="sparkline" viewBox="0 0 100 32" preserveAspectRatio="none">
            <polyline :points="sparkPoints(waterTempHistory, 15, 35)" class="sparkline-line" />
          </svg>
        </div>
        <div class="sensor-footer">
          <span class="sensor-status-dot" :class="sensorPollError ? 'dot-warn' : 'dot-normal'"></span>
          <span class="sensor-status-text">{{ sensorFooterStatus }}</span>
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
          <span class="sensor-status-dot" :class="sensorPollError ? 'dot-warn' : 'dot-optimal'"></span>
          <span class="sensor-status-text">{{ sensorFooterStatus }}</span>
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
          <span class="sensor-status-dot" :class="sensorPollError ? 'dot-warn' : 'dot-normal'"></span>
          <span class="sensor-status-text">{{ sensorFooterStatus }}</span>
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
          <span class="sensor-status-dot" :class="sensorPollError ? 'dot-warn' : 'dot-normal'"></span>
          <span class="sensor-status-text">{{ sensorFooterStatus }}</span>
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
          <span class="sensor-status-dot" :class="sensorPollError ? 'dot-warn' : 'dot-wet'"></span>
          <span class="sensor-status-text">{{ sensorFooterStatus }}</span>
          <span class="sensor-period">点击查看详情</span>
        </div>
      </div>
    </div>

    <div class="ai-panel">
      <div class="ai-panel-header">
        <h3><i class="fas fa-brain"></i> 生态数据智能分析</h3>
        <div class="ai-header-actions">
          <button
            v-if="aiInsight.analysis"
            type="button"
            class="ai-secondary-btn"
            title="复制正文"
            @click="copyAiAnalysis"
          >
            <i class="fas fa-copy"></i> 复制
          </button>
          <button type="button" class="ai-action-btn" :disabled="aiInsight.loading" @click="runAiAnalysis">
            {{ aiInsight.loading ? '分析中…' : 'Claude 综合分析' }}
          </button>
        </div>
      </div>
      <p class="ai-hint">
        基于当前传感器快照调用后端配置的大模型（默认 Anthropic Claude；未设置 <code>ANTHROPIC_API_KEY</code> 时自动走规则回退）。<br />
        快捷键 <kbd>P</kbd> 触发水泵脉冲（需树莓派 Bridge 实现 <code>POST /api/control/pump</code>）。
      </p>

      <div v-if="aiInsight.loading" class="ai-skeleton" aria-busy="true">
        <div class="ai-skel-line"></div>
        <div class="ai-skel-line ai-skel-short"></div>
        <div class="ai-skel-line"></div>
      </div>

      <div v-else-if="aiInsight.error" class="ai-error-box" role="alert">
        <i class="fas fa-exclamation-circle"></i>
        <span>{{ aiInsight.error }}</span>
      </div>

      <template v-else>
        <div v-if="aiLlmBanner.title" :class="aiLlmBanner.cls" role="status">
          <i :class="aiLlmBanner.icon"></i>
          <div class="ai-llm-banner-inner">
            <strong>{{ aiLlmBanner.title }}</strong>
            <p v-if="aiLlmBanner.showDetail && aiInsight.llmMessage" class="ai-llm-banner-msg">{{ aiInsight.llmMessage }}</p>
          </div>
        </div>

        <div v-if="aiInsight.analysis || aiInsight.source" class="ai-result-card" :class="{ 'ai-result-card-shift': aiLlmBanner.title }">
          <div class="ai-result-toolbar">
            <span v-if="aiSourceBadge.label" :class="['ai-source-badge', aiSourceBadge.cls]">{{ aiSourceBadge.label }}</span>
            <code v-if="aiInsight.model" class="ai-model-pill">{{ aiInsight.model }}</code>
          </div>
          <div class="ai-result-body">{{ aiInsight.analysis }}</div>
        </div>
      </template>
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
import { computed, nextTick, onMounted, onUnmounted, reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { apiUrl, authHeaders, logout, wsLogsUrl } from '@/api/http'

const router = useRouter()

/** 占位；成功拉取或 WebSocket 推送后再写入真实读数 */
const PLACEHOLDER = '--'

const waterTemp    = ref(PLACEHOLDER) // DS18B20 水温 (°C)
const airTemp      = ref(PLACEHOLDER) // SHT30 空气温度 (°C)
const airHumidity  = ref(PLACEHOLDER) // SHT30 空气湿度 (%RH)
const wqi          = ref(PLACEHOLDER) // WQM11S 水质综合指数 (0-100)
const soilMoisture = ref(PLACEHOLDER) // ADC 土壤湿度 (%)

const waterTempF = computed(() => {
  const n = Number(waterTemp.value)
  if (Number.isNaN(n)) return PLACEHOLDER
  return (n * 9 / 5 + 32).toFixed(1)
})

const waterTempFSub = computed(() => {
  if (waterTemp.value === PLACEHOLDER) return '/ -- °F'
  return `/${waterTempF.value}°F`
})

const robotUseDirectBridge = ref(false)
const robotCameraMode = ref('rgb')
const robotCameraKey = ref(Date.now())
const robotDirectBridgeUrl = computed(() => {
  const base = import.meta.env.VITE_AQUA_BRIDGE_BASE || 'http://10.116.177.50:18080'
  return `${base.replace(/\/$/, '')}/video/${robotCameraMode.value}.mjpg`
})
const robotCameraSrc = computed(() => {
  const src = robotUseDirectBridge.value ? robotDirectBridgeUrl.value : apiUrl(`/api/aqua/video/${robotCameraMode.value}`)
  return `${src}${src.includes('?') ? '&' : '?'}v=${robotCameraKey.value}`
})
const robotCameraImgKey = computed(() => `${robotCameraMode.value}-${robotCameraKey.value}`)
const robotCameraLabel = computed(() => robotCameraMode.value === 'depth' ? 'Depth' : 'RGB')
const tankCameraSrc = apiUrl('/api/video/tank')
const tankStatus = reactive({ hasFrame: false, seq: 0, bytes: 0, updatedAt: 0, detectionCount: 0 })
const aquaStatus = reactive({ connected: false, hasRgb: false, hasDepth: false, rgbError: '', depthError: '', lastError: '' })
const robotVideoError = ref('')
const tankVideoError = ref('')

const dashboardVideoRow = ref(null)
const showLiveVideos = ref(true)

const robotVideoReady = computed(() => {
  if (robotVideoError.value) return false
  if (!aquaStatus.connected) return true
  return robotCameraMode.value === 'depth' ? aquaStatus.hasDepth : aquaStatus.hasRgb
})
const tankVideoReady = computed(() => tankStatus.hasFrame && !tankVideoError.value)
const robotVideoMessage = computed(() => {
  if (!showLiveVideos.value) return ''
  if (robotVideoError.value) return `${robotVideoError.value}，请检查 /api/aqua/video/${robotCameraMode.value}`
  return ''
})
const tankVideoMessage = computed(() => {
  if (!showLiveVideos.value) return ''
  if (tankVideoError.value) return `${tankVideoError.value}，请检查 /api/video/tank`
  if (!tankStatus.hasFrame) return '后端还没有收到鱼缸摄像头帧'
  return ''
})

function handleRobotVideoError() {
  if (!robotUseDirectBridge.value) {
    robotUseDirectBridge.value = true
    return
  }
  robotVideoError.value = '机械臂视频流加载失败'
}

function setRobotCameraMode(mode) {
  if (!['rgb', 'depth'].includes(mode) || robotCameraMode.value === mode) return
  robotCameraMode.value = mode
  robotCameraKey.value = Date.now()
  robotUseDirectBridge.value = false
  robotVideoError.value = ''
  updateVideoStatus()
}

/** 与后端 SystemStateService.DEMO_SNAPSHOT 一致，用于首次请求失败时的可读默认展示 */
const STABLE_DEFAULTS = {
  water_temp: 24.5,
  air_temp: 26.0,
  air_humidity: 58.0,
  wqi: 76.0,
  soil_moisture: 62.0,
}

const sensorDataSource = ref('unknown')
let sensorsEverSucceeded = false

const aiInsight = reactive({
  loading: false,
  error: '',
  source: '',
  provider: '',
  model: '',
  analysis: '',
  llmOk: null,
  llmStatus: '',
  llmMessage: '',
})

const aiLlmBanner = computed(() => {
  const st = aiInsight.llmStatus
  if (st === 'ok') {
    return {
      cls: 'ai-llm-banner ai-llm-banner-ok',
      icon: 'fas fa-check-circle',
      title: '大模型 API：调用成功',
      showDetail: Boolean(aiInsight.llmMessage),
    }
  }
  if (st === 'error') {
    return {
      cls: 'ai-llm-banner ai-llm-banner-err',
      icon: 'fas fa-plug',
      title: '大模型 API：调用失败',
      showDetail: true,
    }
  }
  if (st === 'skipped') {
    return {
      cls: 'ai-llm-banner ai-llm-banner-skip',
      icon: 'fas fa-info-circle',
      title: '大模型 API：未调用',
      showDetail: true,
    }
  }
  return { cls: '', icon: '', title: '', showDetail: false }
})

const aiSourceBadge = computed(() => {
  if (aiInsight.source === 'llm') {
    if (aiInsight.provider === 'anthropic') return { label: 'Claude', cls: 'badge-claude' }
    if (aiInsight.provider === 'openai') return { label: 'OpenAI', cls: 'badge-openai' }
    return { label: 'LLM', cls: 'badge-llm' }
  }
  if (aiInsight.source === 'fallback') return { label: '规则回退', cls: 'badge-fallback' }
  return { label: '', cls: '' }
})

const HISTORY_LEN = 20

const waterTempHistory   = ref([])
const airTempHistory     = ref([])
const airHumidityHistory = ref([])
const wqiHistory         = ref([])
const moistureHistory    = ref([])

/** 最近一次 `/api/sensors` 轮询是否失败（不改动当前显示值与曲线数据源） */
const sensorPollError = ref(false)

function valueLooksLive(s) {
  if (s == null || s === PLACEHOLDER) return false
  const n = Number(s)
  return !Number.isNaN(n)
}

const sensorHasLiveReading = computed(() =>
  valueLooksLive(waterTemp.value)
  || valueLooksLive(airTemp.value)
  || valueLooksLive(airHumidity.value)
  || valueLooksLive(wqi.value)
  || valueLooksLive(soilMoisture.value))

const sensorBannerMessage = computed(() => {
  if (!sensorHasLiveReading.value) {
    return '尚未收到有效传感器数据；请检查设备与网关连接，以下为界面占位或本地基线。'
  }
  if (sensorDataSource.value === 'demo') {
    return '当前无实时传感器数据，正在显示后端模拟基线；收到串口实时数据后会自动切换。'
  }
  if (sensorPollError.value) {
    return '最近一次 HTTP 拉取失败；以下为上次成功读数。WebSocket 仍可能推送更新。'
  }
  return ''
})

const sensorFooterStatus = computed(() => {
  if (!sensorHasLiveReading.value) return '暂无数据'
  if (sensorDataSource.value === 'demo') return '模拟数据'
  if (sensorPollError.value) return 'HTTP 暂未更新（已保留读数）'
  return '实时'
})

function applyStableDisplayDefaults() {
  sensorDataSource.value = 'demo'
  waterTemp.value = String(STABLE_DEFAULTS.water_temp)
  airTemp.value = String(STABLE_DEFAULTS.air_temp)
  airHumidity.value = String(STABLE_DEFAULTS.air_humidity)
  wqi.value = String(STABLE_DEFAULTS.wqi)
  soilMoisture.value = String(STABLE_DEFAULTS.soil_moisture)
  pushHistory(waterTempHistory, STABLE_DEFAULTS.water_temp)
  pushHistory(airTempHistory, STABLE_DEFAULTS.air_temp)
  pushHistory(airHumidityHistory, STABLE_DEFAULTS.air_humidity)
  pushHistory(wqiHistory, STABLE_DEFAULTS.wqi)
  pushHistory(moistureHistory, STABLE_DEFAULTS.soil_moisture)
}

function pushHistory(arr, val) {
  const n = parseFloat(val)
  if (!Number.isFinite(n)) return
  arr.value.push(n)
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

async function updateSensorData() {
  try {
    const r = await fetch(apiUrl('/api/sensors'), { headers: authHeaders() })
    if (r.ok) {
      sensorsEverSucceeded = true
      sensorPollError.value = false
      const d = await r.json()
      if (d.source) sensorDataSource.value = d.source
      if (d.water_temp != null) {
        waterTemp.value = String(d.water_temp)
        pushHistory(waterTempHistory, d.water_temp)
      }
      if (d.air_temp != null) {
        airTemp.value = String(d.air_temp)
        pushHistory(airTempHistory, d.air_temp)
      }
      if (d.air_humidity != null) {
        airHumidity.value = String(d.air_humidity)
        pushHistory(airHumidityHistory, d.air_humidity)
      }
      if (d.wqi != null) {
        wqi.value = String(d.wqi)
        pushHistory(wqiHistory, d.wqi)
      }
      if (d.soil_moisture != null) {
        soilMoisture.value = String(d.soil_moisture)
        pushHistory(moistureHistory, d.soil_moisture)
      }
      return
    } else {
      sensorPollError.value = true
      if (!sensorsEverSucceeded) applyStableDisplayDefaults()
    }
  } catch {
    sensorPollError.value = true
    if (!sensorsEverSucceeded) applyStableDisplayDefaults()
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
    tankStatus.detectionCount = Number(d.detectionCount ?? 0)
  } catch {
    tankStatus.hasFrame = false
    tankStatus.detectionCount = 0
  }

  try {
    const r = await fetch(apiUrl('/api/aqua/status'), { headers: authHeaders() })
    if (!r.ok) throw new Error(`HTTP ${r.status}`)
    const d = await r.json()
    const camera = d.camera || {}
    aquaStatus.connected = Boolean(d.connected ?? d.ok)
    aquaStatus.hasRgb = Boolean(camera.hasRgb)
    aquaStatus.hasDepth = Boolean(camera.hasDepth)
    aquaStatus.rgbError = camera.rgb?.lastError ? String(camera.rgb.lastError) : ''
    aquaStatus.depthError = camera.depth?.lastError ? String(camera.depth.lastError) : ''
    aquaStatus.lastError = d.lastError ? String(d.lastError) : ''
  } catch (e) {
    aquaStatus.connected = false
    aquaStatus.hasRgb = false
    aquaStatus.hasDepth = false
    aquaStatus.lastError = `无法获取 Aqua Bridge 状态：${e.message || e}`
  }
}

let ws = null
let sensorTimer = null
let videoStatusTimer = null
let videoObserver = null
let observedVideoEl = null

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
  sensorsEverSucceeded = true
  sensorPollError.value = false
  sensorDataSource.value = d.source || 'hardware'
  if (d.water_temp != null) {
    waterTemp.value = String(d.water_temp)
    pushHistory(waterTempHistory, d.water_temp)
  }
  if (d.air_temp != null) {
    airTemp.value = String(d.air_temp)
    pushHistory(airTempHistory, d.air_temp)
  }
  if (d.air_humidity != null) {
    airHumidity.value = String(d.air_humidity)
    pushHistory(airHumidityHistory, d.air_humidity)
  }
  if (d.wqi != null) {
    wqi.value = String(d.wqi)
    pushHistory(wqiHistory, d.wqi)
  }
  if (d.soil_moisture != null) {
    soilMoisture.value = String(d.soil_moisture)
    pushHistory(moistureHistory, d.soil_moisture)
  }
}

async function runAiAnalysis() {
  aiInsight.loading = true
  aiInsight.error = ''
  aiInsight.analysis = ''
  aiInsight.source = ''
  aiInsight.provider = ''
  aiInsight.model = ''
  aiInsight.llmOk = null
  aiInsight.llmStatus = ''
  aiInsight.llmMessage = ''
  try {
    const r = await fetch(apiUrl('/api/ai/ecosystem-analysis'), { method: 'POST', headers: authHeaders() })
    if (r.status === 401) {
      setTimeout(() => logout(router), 500)
      return
    }
    const d = await r.json().catch(() => ({}))
    if (!r.ok) {
      aiInsight.error = (d && d.message) ? String(d.message) : `请求失败（HTTP ${r.status}）`
      return
    }

    aiInsight.llmOk = typeof d.llmOk === 'boolean' ? d.llmOk : (d.source === 'llm')
    aiInsight.llmStatus = d.llmStatus != null && d.llmStatus !== ''
      ? String(d.llmStatus)
      : (d.source === 'llm' ? 'ok' : 'skipped')
    aiInsight.llmMessage = d.llmMessage != null ? String(d.llmMessage) : ''

    aiInsight.source = d.source || ''
    aiInsight.provider = d.provider || ''
    aiInsight.model = d.model || ''
    aiInsight.analysis = d.analysis != null ? String(d.analysis) : ''
    if (!aiInsight.analysis && !aiInsight.source) {
      aiInsight.error = '返回内容为空'
    }
  } catch (e) {
    aiInsight.error = '分析请求失败：' + (e.message || e)
  } finally {
    aiInsight.loading = false
  }
}

async function copyAiAnalysis() {
  const t = aiInsight.analysis
  if (!t || !navigator.clipboard?.writeText) return
  try {
    await navigator.clipboard.writeText(t)
  } catch { /* ignore */ }
}

async function triggerPump() {
  try {
    const r = await fetch(apiUrl('/api/control/pump'), {
      method: 'POST',
      headers: authHeaders(),
      body: JSON.stringify({ seconds: 8 }),
    })
    if (r.status === 401) {
      setTimeout(() => logout(router), 500)
      return
    }
    const d = await r.json().catch(() => ({}))
    window.alert(d.ok ? `水泵指令已提交（约 ${d.seconds ?? '?'} 秒）` : (d.message || '水泵指令未完成'))
  } catch (e) {
    window.alert('水泵请求失败：' + (e.message || e))
  }
}

function onPumpHotkey(e) {
  const t = e.target
  const tag = t && t.tagName
  if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT' || t.isContentEditable) return
  if (e.code === 'KeyP' && !e.ctrlKey && !e.metaKey && !e.altKey) {
    e.preventDefault()
    triggerPump()
  }
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
  videoStatusTimer = setInterval(updateVideoStatus, 2500)
  connectWs()
  window.addEventListener('keydown', onPumpHotkey)
  nextTick(() => {
    observedVideoEl = dashboardVideoRow.value
    if (observedVideoEl && typeof IntersectionObserver !== 'undefined') {
      videoObserver = new IntersectionObserver(
        (entries) => {
          const en = entries[0]
          showLiveVideos.value = en ? en.isIntersecting : true
        },
        { root: null, rootMargin: '100px', threshold: 0.03 },
      )
      videoObserver.observe(observedVideoEl)
    }
  })
})

onUnmounted(() => {
  window.removeEventListener('keydown', onPumpHotkey)
  if (videoObserver && observedVideoEl) videoObserver.unobserve(observedVideoEl)
  if (videoObserver) videoObserver.disconnect()
  videoObserver = null
  observedVideoEl = null
  if (sensorTimer) clearInterval(sensorTimer)
  if (videoStatusTimer) clearInterval(videoStatusTimer)
  if (ws) ws.close()
})
</script>

<style scoped>
.ai-panel {
  margin: 0 0 20px;
  padding: 18px 20px;
  border-radius: 14px;
  background: var(--bg-card, #1e1e2e);
  border: 1px solid rgba(139, 92, 246, 0.22);
  box-shadow: var(--shadow-md, 0 8px 28px rgba(0,0,0,0.2));
}
.ai-panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  flex-wrap: wrap;
}
.ai-header-actions {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}
.ai-secondary-btn {
  border: 1px solid rgba(139, 92, 246, 0.45);
  border-radius: 10px;
  padding: 8px 14px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  color: var(--text-primary, #e2e8f0);
  background: rgba(99, 102, 241, 0.12);
  transition: background 0.18s, border-color 0.18s;
}
.ai-secondary-btn:hover {
  background: rgba(99, 102, 241, 0.2);
  border-color: rgba(167, 139, 250, 0.55);
}
.ai-panel-header h3 {
  margin: 0;
  font-size: 17px;
  color: var(--text-primary, #fff);
}
.ai-panel-header h3 i { color: var(--primary-color, #8b5cf6); margin-right: 8px; }
.ai-action-btn {
  border: none;
  border-radius: 10px;
  padding: 8px 16px;
  font-size: 13px;
  font-weight: 700;
  cursor: pointer;
  color: #fff;
  background: linear-gradient(135deg, #8b5cf6, #6366f1);
  transition: filter 0.18s, opacity 0.18s;
}
.ai-action-btn:hover:not(:disabled) { filter: brightness(1.06); }
.ai-action-btn:disabled { opacity: 0.55; cursor: not-allowed; }
.ai-hint {
  margin: 12px 0 0;
  font-size: 12px;
  line-height: 1.55;
  color: var(--text-secondary, #9ca3af);
}
.ai-hint kbd {
  display: inline-block;
  padding: 1px 6px;
  border-radius: 4px;
  background: rgba(255,255,255,0.08);
  font-size: 11px;
}
.ai-hint code { font-size: 11px; color: #a5f3fc; }
.ai-skeleton {
  margin-top: 16px;
  padding: 18px;
  border-radius: 12px;
  background: rgba(15, 23, 42, 0.55);
  border: 1px solid rgba(139, 92, 246, 0.12);
}
.ai-skel-line {
  height: 12px;
  border-radius: 6px;
  background: linear-gradient(90deg, rgba(99,102,241,0.08), rgba(139,92,246,0.22), rgba(99,102,241,0.08));
  background-size: 200% 100%;
  animation: aiSkelShine 1.2s ease-in-out infinite;
  margin-bottom: 12px;
}
.ai-skel-line:last-child { margin-bottom: 0; }
.ai-skel-short { width: 55%; }
@keyframes aiSkelShine {
  0% { background-position: 100% 0; }
  100% { background-position: -100% 0; }
}
.ai-error-box {
  margin-top: 16px;
  display: flex;
  align-items: flex-start;
  gap: 10px;
  padding: 14px 16px;
  border-radius: 12px;
  background: rgba(239, 68, 68, 0.1);
  border: 1px solid rgba(248, 113, 113, 0.35);
  color: #fecaca;
  font-size: 13px;
  line-height: 1.5;
}
.ai-error-box i {
  flex: none;
  margin-top: 2px;
  color: #f87171;
}
.ai-llm-banner {
  margin-top: 16px;
  display: flex;
  align-items: flex-start;
  gap: 12px;
  padding: 14px 16px;
  border-radius: 12px;
  font-size: 13px;
  line-height: 1.5;
}
.ai-llm-banner > i:first-child {
  flex: none;
  margin-top: 2px;
  font-size: 18px;
}
.ai-llm-banner-inner strong {
  display: block;
  font-size: 14px;
  margin-bottom: 4px;
}
.ai-llm-banner-msg {
  margin: 0;
  font-size: 12px;
  line-height: 1.55;
  opacity: 0.92;
}
.ai-llm-banner-ok {
  background: rgba(16, 185, 129, 0.12);
  border: 1px solid rgba(52, 211, 153, 0.35);
  color: #a7f3d0;
}
.ai-llm-banner-ok > i:first-child { color: #34d399; }

.ai-llm-banner-skip {
  background: rgba(148, 163, 184, 0.1);
  border: 1px solid rgba(148, 163, 184, 0.3);
  color: #cbd5e1;
}
.ai-llm-banner-skip > i:first-child { color: #94a3b8; }

.ai-llm-banner-err {
  background: rgba(245, 158, 11, 0.1);
  border: 1px solid rgba(251, 191, 36, 0.42);
  color: #fde68a;
}
.ai-llm-banner-err > i:first-child { color: #fbbf24; }

.ai-result-card-shift {
  margin-top: 12px;
}
.ai-result-card {
  margin-top: 16px;
  padding: 0;
  border-radius: 14px;
  overflow: hidden;
  border: 1px solid rgba(139, 92, 246, 0.2);
  background: linear-gradient(165deg, rgba(99,102,241,0.06) 0%, rgba(15,23,42,0.92) 48%);
}
.ai-result-toolbar {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px;
  padding: 12px 16px;
  border-bottom: 1px solid rgba(255,255,255,0.06);
  background: rgba(0,0,0,0.2);
}
.ai-source-badge {
  display: inline-flex;
  align-items: center;
  padding: 4px 10px;
  border-radius: 999px;
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.02em;
  text-transform: uppercase;
}
.badge-claude {
  background: linear-gradient(135deg, rgba(217,119,87,0.35), rgba(245,158,121,0.2));
  color: #ffefe8;
  border: 1px solid rgba(245,158,121,0.35);
}
.badge-openai {
  background: rgba(16, 163, 127, 0.2);
  color: #86efac;
  border: 1px solid rgba(16, 163, 127, 0.35);
}
.badge-llm {
  background: rgba(139, 92, 246, 0.22);
  color: #ddd6fe;
  border: 1px solid rgba(167, 139, 250, 0.35);
}
.badge-fallback {
  background: rgba(148, 163, 184, 0.15);
  color: #cbd5e1;
  border: 1px solid rgba(148, 163, 184, 0.3);
}
.ai-model-pill {
  font-size: 11px;
  padding: 3px 8px;
  border-radius: 6px;
  background: rgba(0,0,0,0.35);
  color: #bae6fd;
  border: 1px solid rgba(56, 189, 248, 0.2);
}
.ai-result-body {
  padding: 16px 18px 18px;
  font-size: 14px;
  line-height: 1.75;
  color: #f1f5f9;
  white-space: pre-wrap;
  word-break: break-word;
  max-height: 340px;
  overflow-y: auto;
}

.sensor-card-clickable { cursor: pointer; transition: transform 0.15s, box-shadow 0.15s; }
.sensor-card-clickable:hover { transform: translateY(-3px); box-shadow: 0 8px 24px rgba(139,92,246,0.18); }

.sensor-banner {
  display: flex;
  align-items: flex-start;
  gap: 10px;
  margin: 0 0 16px;
  padding: 11px 14px;
  border-radius: 10px;
  font-size: 13px;
  line-height: 1.45;
  color: #fef3c7;
  background: rgba(245, 158, 11, 0.14);
  border: 1px solid rgba(245, 158, 11, 0.35);
}
.sensor-banner i {
  flex: none;
  margin-top: 2px;
  color: #fbbf24;
}
.dot-warn {
  background: #fbbf24 !important;
  box-shadow: 0 0 6px rgba(251, 191, 36, 0.5);
}

.camera-mode-switch {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 3px;
  border-radius: 8px;
  background: rgba(15, 23, 42, 0.66);
  border: 1px solid rgba(255,255,255,0.08);
}

.camera-mode-btn {
  height: 26px;
  border: none;
  border-radius: 6px;
  padding: 0 10px;
  cursor: pointer;
  color: var(--text-secondary, #9ca3af);
  background: transparent;
  font-size: 12px;
  font-weight: 700;
}

.camera-mode-btn.active {
  color: #fff;
  background: #0ea5e9;
}

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
