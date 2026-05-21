<template>
  <div class="video-page">
    <div class="video-grid">
      <div class="video-card">
        <div class="video-header">
          <h3><i class="fas fa-robot"></i> 机械臂摄像头</h3>
          <div class="video-actions">
            <button type="button" class="video-btn" title="全屏" @click="toggleFullscreen('robot')">
              <i class="fas fa-expand"></i>
            </button>
            <button type="button" class="video-btn" title="刷新流" @click="bustRobot">
              <i class="fas fa-sync-alt"></i>
            </button>
          </div>
        </div>
        <div id="robotVideo" class="video-container">
          <img
            :src="robotSrcBusted"
            alt="机械臂摄像头"
            decoding="async"
            fetchpriority="high"
            @load="robotImgError = ''"
            @error="handleRobotImgError"
          />
          <div class="video-overlay">
            <div class="video-info-top">
              <div :class="['rec-indicator', robotHealthy ? '' : 'offline']"><div class="rec-dot"></div>{{ robotHealthy ? 'LIVE' : 'OFFLINE' }}</div>
              <div class="camera-name">CAM-01</div>
            </div>
            <div class="video-info-bottom">
              <div class="video-stat"><i class="fas fa-robot"></i>机械臂 RGB</div>
              <div class="video-stat"><i class="fas fa-tachometer-alt"></i>{{ robotHealthy ? 'MJPEG' : '等待 Bridge' }}</div>
              <div v-if="robotStatusHint" class="video-stat video-stat-warn"><i class="fas fa-info-circle"></i>状态待确认</div>
            </div>
          </div>
          <div v-if="robotMessage" class="video-diagnostic">
            <i class="fas fa-video-slash"></i>
            <span>{{ robotMessage }}</span>
          </div>
        </div>
      </div>
      <div class="video-card">
        <div class="video-header">
          <h3><i class="fas fa-water"></i> 鱼缸摄像头（含检测框叠加）</h3>
          <div class="video-actions">
            <button type="button" class="video-btn" title="全屏" @click="toggleFullscreen('tank')">
              <i class="fas fa-expand"></i>
            </button>
            <button type="button" class="video-btn" title="刷新流" @click="bustTank">
              <i class="fas fa-sync-alt"></i>
            </button>
          </div>
        </div>
        <div id="tankVideo" class="video-container">
          <img
            :src="tankSrcBusted"
            alt="鱼缸摄像头"
            decoding="async"
            fetchpriority="low"
            @load="tankImgError = ''"
            @error="tankImgError = '鱼缸视频流加载失败'"
          />
          <div class="video-overlay">
            <div class="video-info-top">
              <div :class="['rec-indicator', tankHealthy ? '' : 'offline']"><div class="rec-dot"></div>{{ tankHealthy ? 'LIVE' : 'WAIT' }}</div>
              <div class="camera-name">CAM-02</div>
            </div>
            <div class="video-info-bottom">
              <div class="video-stat"><i class="fas fa-project-diagram"></i>检测由后端叠加</div>
              <div class="video-stat"><i class="fas fa-tachometer-alt"></i>{{ tankHealthy ? 'MJPEG' : '等待帧' }}</div>
            </div>
          </div>
          <div v-if="tankMessage" class="video-diagnostic">
            <i class="fas fa-plug"></i>
            <span>{{ tankMessage }}</span>
          </div>
        </div>
      </div>
    </div>

    <div class="timelapse-panel">
      <h3><i class="fas fa-clock"></i> 延时摄影（鱼缸快照 → WebM）</h3>
      <p class="timelapse-desc">
        以 <code>/api/video/tank/snapshot</code> 定时抓帧写入画布并录制，减轻与实时 MJPEG 并行的解码压力。
        开始预览后请保持本标签页可见直至导出完成。
      </p>
      <div class="timelapse-controls">
        <label class="tl-label">间隔
          <select v-model.number="tlIntervalMs" class="tl-select" :disabled="tlRunning">
            <option :value="500">0.5 s</option>
            <option :value="1000">1 s</option>
            <option :value="2000">2 s</option>
            <option :value="5000">5 s</option>
          </select>
        </label>
        <button type="button" class="tl-btn tl-btn-primary" :disabled="tlRunning" @click="startTimelapse">
          开始采集并录制
        </button>
        <button type="button" class="tl-btn" :disabled="!tlRunning" @click="stopTimelapse">
          停止并下载
        </button>
      </div>
      <div class="timelapse-preview-wrap">
        <canvas ref="tlCanvas" class="timelapse-canvas" width="640" height="480" />
        <span v-if="tlStatus" class="tl-status">{{ tlStatus }}</span>
      </div>
    </div>

    <div class="det-hint-panel">
      <h3><i class="fas fa-vector-square"></i> 鱼类检测框（YOLO）</h3>
      <p>
        推荐：运行 <code>software/fish-arm/serial_bridge.py</code> 时不要加 <code>--no-yolo</code>，默认会加载仓库内
        <code>model/yolo_fish/runs/yolo11n_fish/weights/best.pt</code>，每帧推理后向
        <code>POST {{ detectionUrl }}</code> 上报框，本页「鱼缸摄像头」MJPEG 由后端自动叠加绿框。
      </p>
      <p class="det-small">
        也可自写进程按相同 JSON 格式 POST；坐标 <code>x,y,width,height</code> 为相对宽高的 0~1；需先通过桥接
        <code>/api/video/tank/ingest</code> 或 USB 抓流持续推 JPEG，否则画面会停留在等待状态。
      </p>
      <pre class="det-json">{{ detectionExample }}</pre>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, reactive, ref } from 'vue'
import { apiUrl, authHeaders } from '@/api/http'

import '@/assets/styles/cameras-page.css'

const robotKey = ref(0)
const tankKey = ref(0)
const robotImgError = ref('')
const tankImgError = ref('')
const aquaStatus = reactive({ connected: false, hasRgb: false, rgbError: '', lastError: '' })
const tankStatus = reactive({ hasFrame: false, seq: 0, bytes: 0, updatedAt: 0, detectionCount: 0 })
const robotUseDirectBridge = ref(false)

const robotDirectBridgeUrl = computed(() => {
  const base = import.meta.env.VITE_AQUA_BRIDGE_BASE || 'http://10.116.177.50:18080'
  return `${base.replace(/\/$/, '')}/video/rgb.mjpg`
})
const robotSrcBusted = computed(() => {
  const src = robotUseDirectBridge.value ? robotDirectBridgeUrl.value : apiUrl('/api/aqua/video/rgb')
  return `${src}?v=${robotKey.value}`
})
const tankSrcBusted = computed(() => `${apiUrl('/api/video/tank')}?v=${tankKey.value}`)

const detectionUrl = apiUrl('/api/video/tank/detections')
const detectionExample = `{
  "detections": [
    { "label": "fish", "x": 0.12, "y": 0.2, "width": 0.35, "height": 0.28, "score": 0.91 }
  ]
}`

const robotStreamOk = computed(() => !robotImgError.value)
const robotHealthy = computed(() => robotStreamOk.value && (!aquaStatus.connected || aquaStatus.hasRgb))
const tankHealthy = computed(() => tankStatus.hasFrame && !tankImgError.value)

const robotMessage = computed(() => {
  if (robotImgError.value) return `${robotImgError.value}：请检查 Spring Boot 后端和 /api/aqua/video/rgb`
  return ''
})

const robotStatusHint = computed(() => {
  if (robotImgError.value) return ''
  if (!aquaStatus.connected) return aquaStatus.lastError || '树莓派 Bridge 未连接；请确认树莓派服务已启动，且 application.properties 中 aquagarden.bridge.base-url 地址正确'
  if (!aquaStatus.hasRgb) return aquaStatus.rgbError || '树莓派 Bridge 返回 RGB 摄像头未就绪；请检查树莓派相机接口状态'
  return ''
})

const tankMessage = computed(() => {
  if (tankImgError.value) return `${tankImgError.value}：请检查 Spring Boot 后端 /api/video/tank`
  if (!tankStatus.hasFrame) return '后端还没有收到鱼缸 JPEG 帧；请运行 serial_bridge.py，并确认 --tank-camera-index 与摄像头实际编号一致'
  return ''
})

const tlCanvas = ref(null)
const tlIntervalMs = ref(2000)
const tlRunning = ref(false)
const tlStatus = ref('')

let tlLoopTimer = null
let mediaRecorder = null
let recordedChunks = []
let statusTimer = null

function bustRobot() {
  robotKey.value = Date.now()
  robotImgError.value = ''
  robotUseDirectBridge.value = false
  updateStatuses()
}

function handleRobotImgError() {
  if (!robotUseDirectBridge.value) {
    robotUseDirectBridge.value = true
    robotKey.value = Date.now()
    return
  }
  robotImgError.value = '机械臂视频流加载失败'
}

function bustTank() {
  tankKey.value = Date.now()
  tankImgError.value = ''
  updateStatuses()
}

function toggleFullscreen(which) {
  const el = document.getElementById(`${which}Video`)
  if (!el) return
  if (document.fullscreenElement) document.exitFullscreen()
  else el.requestFullscreen()
}

function pickMime() {
  const c = ['video/webm;codecs=vp9', 'video/webm;codecs=vp8', 'video/webm']
  for (const m of c) {
    if (MediaRecorder.isTypeSupported(m)) return m
  }
  return ''
}

async function drawSnapshotToCanvas() {
  const canvas = tlCanvas.value
  if (!canvas) return false
  const ctx = canvas.getContext('2d')
  if (!ctx) return false
  const r = await fetch(apiUrl('/api/video/tank/snapshot'))
  if (!r.ok) {
    tlStatus.value = '无可用鱼缸快照（请先运行串口桥上传 JPEG）'
    return false
  }
  const blob = await r.blob()
  const bmp = await createImageBitmap(blob)
  ctx.drawImage(bmp, 0, 0, canvas.width, canvas.height)
  bmp.close()
  return true
}

async function updateAquaStatus() {
  try {
    const r = await fetch(apiUrl('/api/aqua/status'), { headers: authHeaders() })
    if (!r.ok) throw new Error(`HTTP ${r.status}`)
    const d = await r.json()
    aquaStatus.connected = Boolean(d.connected ?? d.ok)
    aquaStatus.lastError = d.lastError ? String(d.lastError) : ''
    const camera = d.camera || {}
    aquaStatus.hasRgb = Boolean(camera.hasRgb)
    aquaStatus.rgbError = camera.rgb?.lastError ? String(camera.rgb.lastError) : ''
  } catch (e) {
    aquaStatus.connected = false
    aquaStatus.hasRgb = false
    aquaStatus.lastError = `无法获取 Aqua Bridge 状态：${e.message || e}`
  }
}

async function updateTankStatus() {
  try {
    const r = await fetch(apiUrl('/api/video/tank/status'))
    if (!r.ok) throw new Error(`HTTP ${r.status}`)
    const d = await r.json()
    tankStatus.hasFrame = Boolean(d.hasFrame)
    tankStatus.seq = Number(d.seq ?? 0)
    tankStatus.bytes = Number(d.bytes ?? 0)
    tankStatus.updatedAt = Number(d.updatedAt ?? 0)
    tankStatus.detectionCount = Number(d.detectionCount ?? 0)
  } catch {
    tankStatus.hasFrame = false
    tankStatus.bytes = 0
    tankStatus.detectionCount = 0
  }
}

function updateStatuses() {
  updateAquaStatus()
  updateTankStatus()
}

async function startTimelapse() {
  const canvas = tlCanvas.value
  if (!canvas) return
  tlStatus.value = '初始化录制…'
  const mime = pickMime()
  if (!mime || !MediaRecorder.isTypeSupported(mime)) {
    tlStatus.value = '浏览器不支持 WebM 录制'
    return
  }
  const ok = await drawSnapshotToCanvas()
  if (!ok) return

  const stream = canvas.captureStream(30)
  recordedChunks = []
  mediaRecorder = new MediaRecorder(stream, { mimeType: mime })
  mediaRecorder.ondataavailable = (e) => {
    if (e.data.size > 0) recordedChunks.push(e.data)
  }
  mediaRecorder.onstop = () => {
    const blob = new Blob(recordedChunks, { type: mime })
    const a = document.createElement('a')
    a.href = URL.createObjectURL(blob)
    a.download = `tank_timelapse_${Date.now()}.webm`
    a.click()
    URL.revokeObjectURL(a.href)
    tlStatus.value = '已导出 WebM'
  }
  mediaRecorder.start(200)
  tlRunning.value = true

  tlLoopTimer = setInterval(async () => {
    const fine = await drawSnapshotToCanvas()
    if (!fine) stopTimelapse()
    else tlStatus.value = `录制中 · 间隔 ${tlIntervalMs.value / 1000}s`
  }, tlIntervalMs.value)
}

function stopTimelapse() {
  if (tlLoopTimer) {
    clearInterval(tlLoopTimer)
    tlLoopTimer = null
  }
  tlRunning.value = false
  if (mediaRecorder && mediaRecorder.state !== 'inactive') {
    mediaRecorder.stop()
  }
  mediaRecorder = null
}

onMounted(() => {
  updateStatuses()
  statusTimer = setInterval(updateStatuses, 2500)
})

onUnmounted(() => {
  if (tlLoopTimer) clearInterval(tlLoopTimer)
  if (statusTimer) clearInterval(statusTimer)
  if (mediaRecorder && mediaRecorder.state !== 'inactive') mediaRecorder.stop()
})
</script>

<style scoped>
.timelapse-panel {
  margin-top: 22px;
  padding: 18px 20px;
  border-radius: 14px;
  background: var(--bg-card);
  border: 1px solid rgba(255,255,255,0.06);
}
.timelapse-panel h3 {
  margin: 0 0 10px;
  font-size: 16px;
  color: var(--text-primary);
}
.timelapse-panel h3 i { color: var(--primary-color); margin-right: 8px; }
.timelapse-desc {
  margin: 0 0 14px;
  font-size: 12px;
  line-height: 1.55;
  color: var(--text-secondary);
}
.timelapse-desc code { font-size: 11px; color: #a5f3fc; }
.timelapse-controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 12px;
  margin-bottom: 14px;
}
.tl-label { font-size: 12px; color: var(--text-secondary); display: flex; align-items: center; gap: 8px; }
.tl-select {
  background: var(--bg-main);
  color: var(--text-primary);
  border: 1px solid rgba(255,255,255,0.1);
  border-radius: 8px;
  padding: 6px 10px;
}
.tl-btn {
  border: none;
  border-radius: 10px;
  padding: 8px 14px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  background: var(--bg-main);
  color: var(--text-secondary);
}
.tl-btn:disabled { opacity: 0.45; cursor: not-allowed; }
.tl-btn-primary {
  background: var(--primary-color);
  color: #fff;
}
.timelapse-preview-wrap {
  position: relative;
  display: inline-block;
  max-width: 100%;
}
.timelapse-canvas {
  width: 100%;
  max-width: 640px;
  height: auto;
  display: block;
  border-radius: 10px;
  background: #000;
}
.tl-status {
  position: absolute;
  left: 10px;
  bottom: 10px;
  padding: 4px 10px;
  border-radius: 8px;
  background: rgba(0,0,0,0.65);
  color: #e2e8f0;
  font-size: 12px;
}
.det-hint-panel {
  margin-top: 18px;
  padding: 16px 18px;
  border-radius: 14px;
  background: rgba(15, 23, 42, 0.55);
  border: 1px dashed rgba(139, 92, 246, 0.35);
  font-size: 13px;
  color: var(--text-secondary);
}
.det-hint-panel h3 {
  margin: 0 0 10px;
  font-size: 15px;
  color: var(--text-primary);
}
.det-hint-panel h3 i { margin-right: 8px; color: var(--primary-color); }
.det-json {
  margin: 10px 0;
  padding: 12px;
  border-radius: 8px;
  background: #0a0a14;
  color: #a5f3fc;
  font-size: 11px;
  overflow-x: auto;
}
.det-small { margin: 8px 0 0; font-size: 12px; }
.det-small code { color: #fbbf24; }
</style>
