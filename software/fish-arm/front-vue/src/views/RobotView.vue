<template>
  <div class="robot-page">
    <div class="robot-left">
      <section class="control-panel arm-panel">
        <div class="panel-header control-panel-header">
          <div>
            <span><i class="fas fa-robot"></i> 机械臂与滑轨</span>
            <small>喂食、裁剪、松土和滑轨绝对位置控制</small>
          </div>
          <span :class="['quick-status', connected ? 'online' : 'offline']">{{ hardwareStatusText }}</span>
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
            <span><i class="fas fa-arrows-alt-h"></i> 滑轨移动（0-5200）</span>
            <b>当前位置：{{ railPositionText }}</b>
          </div>
          <div class="rail-row rail-row-direct">
            <button class="rail-btn" @click="setRailTarget(0)">回到 0</button>
            <input
              v-model.number="railTarget"
              class="rail-input rail-input-large"
              type="number"
              min="0"
              max="5200"
              step="10"
              @focus="railEditing = true"
              @input="markRailTargetTouched"
              @blur="finishRailEditing"
            />
            <button class="rail-btn" @click="setRailTarget(5200)">到 5200</button>
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
          <div class="pump-readout-wrap">
            <span>{{ pumpModeText }}</span>
            <b class="pump-readout">{{ pumpPwm }}%</b>
          </div>
        </div>
        <div class="pump-body">
          <div class="pump-meter">
            <input
              v-model.number="pumpPwm"
              class="pump-range"
              type="range"
              min="0"
              max="100"
              step="1"
              @input="markPumpPwmTouched"
            />
            <div class="pump-scale">
              <span>0</span>
              <span>50</span>
              <span>100</span>
            </div>
          </div>
          <div class="rail-row pump-command-row">
            <input
              v-model.number="pumpPwm"
              class="rail-input rail-input-large"
              type="number"
              min="0"
              max="100"
              step="1"
              @input="markPumpPwmTouched"
              @blur="finishPumpPwmEditing"
            />
            <button class="rail-btn rail-btn-primary" :disabled="pumpBusy" @click="pumpStart">开泵</button>
            <button class="rail-btn" :disabled="pumpBusy" @click="pumpApplyPwm">更新 PWM</button>
            <button class="rail-btn" :disabled="pumpBusy" @click="pumpAuto">自动模式</button>
            <button class="rail-btn rail-btn-danger" :disabled="pumpBusy" @click="pumpStop">关泵</button>
          </div>
          <div class="pump-note">
            <span><i class="fas fa-usb"></i> {{ isDirectSerial ? '后端直连 MCU 串口' : '下游 HTTP Bridge 控制' }}</span>
            <span v-if="pumpBusy">指令发送中</span>
          </div>
        </div>
      </section>

      <section
        class="control-panel usb-light-card"
        :class="{ 'is-fault': usbLightFault, 'is-active': usbLightCurrentMode != null && usbLightCurrentMode !== 26 }"
      >
        <div class="usb-light-header">
          <div class="usb-light-identity">
            <span :class="['usb-light-beacon', usbLightBeaconTone, usbLightPulseClass]" aria-hidden="true">
              <i class="fas fa-bell"></i>
            </span>
            <span>
              <strong>USB 报警灯</strong>
              <small>USB HS · CH340 声光控制</small>
            </span>
          </div>
          <span
            :class="['usb-light-status', { ready: usbLightReady, fault: usbLightFault, busy: usbLightBusy }]"
          >
            {{ usbLightStatusText }}
          </span>
        </div>

        <div class="usb-light-body">
          <div class="usb-light-current">
            <span>当前模式</span>
            <strong>{{ usbLightCurrentLabel }}</strong>
            <small>{{ usbLightCurrentDescription }}</small>
          </div>

          <div class="usb-light-controls">
            <label for="usb-light-mode">报警模式</label>
            <div class="usb-light-command-row">
              <select
                id="usb-light-mode"
                v-model.number="usbLightSelectedMode"
                class="usb-light-select"
                :disabled="usbLightBusy"
                @change="usbLightSelectionTouched = true"
              >
                <optgroup v-for="group in USB_LIGHT_MODE_GROUPS" :key="group.group" :label="group.group">
                  <option v-for="option in group.modes" :key="option.mode" :value="option.mode">
                    {{ option.label }}
                  </option>
                </optgroup>
              </select>
              <button
                type="button"
                class="usb-light-off-btn"
                title="关闭灯光与喇叭"
                aria-label="关闭灯光与喇叭"
                :disabled="usbLightBusy"
                @click="applyUsbLightMode(26)"
              >
                <i class="fas fa-power-off"></i>
              </button>
              <button
                type="button"
                class="usb-light-apply-btn"
                :disabled="usbLightBusy"
                @click="applyUsbLightMode()"
              >
                <i :class="usbLightBusy ? 'fas fa-spinner fa-spin' : 'fas fa-paper-plane'"></i>
                {{ usbLightBusy ? '确认中' : '应用模式' }}
              </button>
            </div>
            <small>{{ usbLightSelectedDescription }}</small>
          </div>
        </div>
      </section>

      <button
        type="button"
        :class="[
          'control-panel',
          'atomizer-card',
          { 'is-on': atomizerState === true, 'is-fault': atomizerFault },
        ]"
        :aria-pressed="atomizerState === true"
        :aria-label="atomizerState === true ? '关闭雾化器' : '开启雾化器'"
        :disabled="atomizerBusy"
        @click="toggleAtomizer"
      >
        <span class="atomizer-icon" aria-hidden="true">
          <i :class="atomizerBusy ? 'fas fa-spinner fa-spin' : 'fas fa-cloud'"></i>
        </span>
        <span class="atomizer-copy">
          <strong>雾化器</strong>
          <small>{{ atomizerDetailText }}</small>
        </span>
        <span class="atomizer-meta">
          <b>{{ atomizerStatusText }}</b>
          <span class="atomizer-switch" aria-hidden="true">
            <span></span>
          </span>
        </span>
      </button>

    </div>

    <div class="robot-right">
      <div class="terminal-panel">
        <div class="panel-header terminal-header">
          <span><i class="fas fa-terminal"></i> 终端运行状态</span>
          <div class="terminal-status-row">
            <span :class="['status-dot', connected ? 'dot-connected' : 'dot-disconnected']"></span>
            <span class="status-text">{{ connected ? '链路正常' : '链路异常' }}</span>
          </div>
        </div>

        <div class="terminal-status-grid">
          <div><span>当前任务</span><b>{{ currentTask }}</b></div>
          <div><span>链路</span><b>{{ phaseText }}</b></div>
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
import { apiFetch } from '@/api/http'
import { clampControlValue, shouldAdoptReportedSetpoint } from '@/utils/controlSetpoints'
import { USB_LIGHT_MODE_GROUPS, getUsbLightMode } from '@/utils/usbLightModes'

const connected = ref(true)
const currentTask = ref('待命')
const phase = ref('idle')
const busy = ref(false)
const lastError = ref('')
const uptime = ref('00:00:00')
const logs = ref([])
const logContainer = ref(null)
const railTarget = ref(0)
const lastValidRailTarget = ref(0)
const railTargetTouched = ref(false)
const railPosition = ref(null)
const railEditing = ref(false)
const cameraState = reactive({ hasRgb: false, hasDepth: false, ageSec: null })
const pumpPwm = ref(80)
const lastValidPumpPwm = ref(80)
const pumpPwmTouched = ref(false)
const pumpBusy = ref(false)
const pumpManualOn = ref(false)
const atomizerState = ref(null)
const atomizerBusy = ref(false)
const atomizerFault = ref(false)
const usbLightCurrentMode = ref(null)
const usbLightSelectedMode = ref(19)
const usbLightReady = ref(false)
const usbLightBusy = ref(false)
const usbLightFault = ref(false)
const usbLightSelectionTouched = ref(false)
const taskPending = ref(false)

/** 滑轨移动请求进行中（与 busy 分离，避免与服务端 busy 不同步时连点） */
const railPending = ref(false)

/** 控制类请求超时（毫秒） */
const RAIL_HTTP_MS = 90_000
const TASK_HTTP_MS = 120_000
const STATUS_HTTP_MS = 15_000
const RAIL_DEBOUNCE_MS = 350
const TASK_SUBMIT_LOCK_MS = 900

function withTimeout(ms) {
  const ctrl = new AbortController()
  const tid = setTimeout(() => ctrl.abort(), ms)
  return { signal: ctrl.signal, cancel: () => clearTimeout(tid) }
}

async function fetchWithTimeout(url, init, timeoutMs) {
  const { signal, cancel } = withTimeout(timeoutMs)
  try {
    return await apiFetch(url, { ...init, signal })
  } catch (e) {
    if (e?.name === 'AbortError') throw new Error('请求超时，请检查硬件链路与后端服务')
    throw e
  } finally {
    cancel()
  }
}
const railPositionText = computed(() => railPosition.value == null ? '--' : String(railPosition.value))
const isDirectSerial = computed(() => phase.value === 'spring-serial' || phase.value === 'fastapi-serial')
const hardwareStatusText = computed(() => {
  if (!connected.value) return isDirectSerial.value ? '串口异常' : '链路离线'
  return isDirectSerial.value ? '串口直连' : 'Bridge 在线'
})
const phaseText = computed(() => {
  if (isDirectSerial.value) return 'MCU 串口'
  if (phase.value === 'disabled') return '未启用'
  if (phase.value === 'offline') return '离线'
  return phase.value || 'idle'
})
const pumpModeText = computed(() => pumpManualOn.value ? '手动' : '自动')
const atomizerStatusText = computed(() => {
  if (atomizerBusy.value) return '切换中'
  if (atomizerFault.value) return '设备告警'
  if (atomizerState.value == null) return '状态未知'
  return atomizerState.value ? '运行中' : '已关闭'
})
const atomizerDetailText = computed(() => {
  if (atomizerBusy.value) return '等待 MCU 上行状态确认'
  if (atomizerFault.value) return '硬件上报雾化器控制异常'
  if (atomizerState.value == null) return '等待 MCU 状态 · GPIO P402'
  return atomizerState.value ? '雾化输出已开启 · GPIO P402' : '雾化输出已停止 · GPIO P402'
})
const usbLightCurrentOption = computed(() => getUsbLightMode(usbLightCurrentMode.value))
const usbLightSelectedOption = computed(() => getUsbLightMode(usbLightSelectedMode.value))
const usbLightCurrentLabel = computed(() => usbLightCurrentOption.value?.label ?? '等待模式确认')
const usbLightCurrentDescription = computed(() => {
  if (usbLightFault.value) return 'USB 灯链路上报故障'
  if (!usbLightReady.value) return '尚未收到已确认的灯光模式'
  return usbLightCurrentOption.value?.description ?? '等待 MCU 遥测'
})
const usbLightSelectedDescription = computed(() => usbLightSelectedOption.value?.description ?? '请选择有效模式')
const usbLightStatusText = computed(() => {
  if (usbLightBusy.value) return '等待 MCU 确认'
  if (usbLightFault.value) return '设备告警'
  return usbLightReady.value ? '链路就绪' : '待初始化'
})
const usbLightBeaconTone = computed(() => `tone-${usbLightCurrentOption.value?.tone ?? 'off'}`)
const usbLightPulseClass = computed(() => {
  if (usbLightCurrentOption.value?.group === '慢闪') return 'pulse-slow'
  if (usbLightCurrentOption.value?.group === '快闪') return 'pulse-fast'
  return ''
})

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

function pumpSuccessLog(successMsg, data) {
  const detail = []
  const pwm = data?.pwmUi ?? data?.pwm ?? data?.pump?.pwm
  if (typeof pwm === 'number') detail.push(`PWM=${pwm}%`)
  if (typeof data?.connected === 'boolean') detail.push(data.connected ? '串口已连接' : '串口未连接')
  addLog(detail.length ? `${successMsg}（${detail.join('，')}）` : successMsg, 'task')
}

function setRailTarget(value, userInitiated = true) {
  const normalized = clampControlValue(value, 0, 5200, lastValidRailTarget.value)
  railTarget.value = normalized
  lastValidRailTarget.value = normalized
  if (userInitiated) railTargetTouched.value = true
}

function markRailTargetTouched() {
  railTargetTouched.value = true
  if (railTarget.value !== '' && railTarget.value != null) {
    lastValidRailTarget.value = clampControlValue(railTarget.value, 0, 5200, lastValidRailTarget.value)
  }
}

function finishRailEditing() {
  railEditing.value = false
  setRailTarget(railTarget.value)
}

function setPumpPwm(value) {
  const normalized = clampControlValue(value, 0, 100, lastValidPumpPwm.value)
  pumpPwm.value = normalized
  lastValidPumpPwm.value = normalized
}

function markPumpPwmTouched() {
  pumpPwmTouched.value = true
  if (pumpPwm.value !== '' && pumpPwm.value != null) {
    lastValidPumpPwm.value = clampControlValue(pumpPwm.value, 0, 100, lastValidPumpPwm.value)
  }
}

function finishPumpPwmEditing() {
  setPumpPwm(pumpPwm.value)
}

async function callPumpApi(path, payload, successMsg, syncPwmFromResponse = false) {
  if (pumpBusy.value) return
  pumpPwmTouched.value = true
  setPumpPwm(pumpPwm.value)
  pumpBusy.value = true
  try {
    const body = payload ? JSON.stringify(payload) : undefined
    const r = await fetchWithTimeout(
      path,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body,
      },
      TASK_HTTP_MS,
    )
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      const msg = d.message || d.error || (r.ok ? '水泵控制未完成' : `水泵接口返回 ${r.status}`)
      throw new Error(msg)
    }
    pumpSuccessLog(successMsg, d)
    if (d.pump) {
      pumpManualOn.value = Boolean(d.pump.manualOn)
    }
    if (syncPwmFromResponse) {
      const confirmedPwm = typeof d.pwmUi === 'number' ? d.pwmUi : d.pump?.pwm
      if (typeof confirmedPwm === 'number') setPumpPwm(confirmedPwm)
    }
  } catch (e) {
    addLog(e.message || '水泵控制失败', 'error')
  } finally {
    pumpBusy.value = false
  }
}

async function pumpStart() {
  setPumpPwm(pumpPwm.value)
  const pwm = pumpPwm.value
  await callPumpApi('/api/aqua/pump/start', { pwm }, `开泵，PWM=${pwm}%`, true)
}

async function pumpApplyPwm() {
  setPumpPwm(pumpPwm.value)
  const pwm = pumpPwm.value
  await callPumpApi('/api/aqua/pump/pwm', { pwm }, `更新水泵 PWM=${pwm}%`, true)
}

async function pumpStop() {
  await callPumpApi('/api/aqua/pump/stop', {}, '关泵')
}

async function pumpAuto() {
  await callPumpApi('/api/aqua/pump/auto', {}, '切换为水泵自动模式')
}

async function applyUsbLightMode(requestedMode = usbLightSelectedMode.value) {
  if (usbLightBusy.value) return
  const targetMode = getUsbLightMode(requestedMode)
  if (!targetMode) {
    addLog('USB 报警灯模式无效', 'error')
    return
  }

  usbLightBusy.value = true
  usbLightSelectedMode.value = targetMode.mode
  try {
    const r = await fetchWithTimeout(
      '/api/aqua/usb-light',
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: targetMode.mode }),
      },
      STATUS_HTTP_MS,
    )
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok !== true || d.confirmed !== true || d.mode !== targetMode.mode) {
      throw new Error(d.message || d.detail || 'USB 报警灯接口返回 ' + r.status)
    }
    usbLightCurrentMode.value = d.mode
    usbLightReady.value = true
    usbLightFault.value = false
    usbLightSelectionTouched.value = false
    addLog(`USB 报警灯已切换：${targetMode.label}（MCU 状态已确认）`, 'task')
  } catch (e) {
    addLog(e.message || 'USB 报警灯控制失败', 'error')
  } finally {
    usbLightBusy.value = false
    fetchStatus()
  }
}

async function toggleAtomizer() {
  if (atomizerBusy.value) return
  const targetState = atomizerState.value !== true
  atomizerBusy.value = true
  try {
    const r = await fetchWithTimeout(
      '/api/aqua/atomizer',
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ state: targetState }),
      },
      STATUS_HTTP_MS,
    )
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok !== true || d.confirmed !== true || d.state !== targetState) {
      const detail = typeof d.detail === 'string' ? d.detail : ''
      throw new Error(d.message || detail || '雾化器接口返回 ' + r.status)
    }
    atomizerState.value = d.state
    atomizerFault.value = false
    addLog('雾化器已' + (d.state ? '开启' : '关闭') + '（MCU 状态已确认）', 'task')
  } catch (e) {
    addLog(e.message || '雾化器控制失败', 'error')
  } finally {
    atomizerBusy.value = false
    fetchStatus()
  }
}

async function sendTask(taskName) {
  if (busy.value || railPending.value || taskPending.value) return
  const labels = { feed: '自动喂食', loosen: '松土', prune: '裁剪黄色叶子' }
  addLog(`触发任务: ${labels[taskName] ?? taskName}`, 'task')
  currentTask.value = labels[taskName] ?? taskName
  busy.value = true
  taskPending.value = true

  setTimeout(() => {
    taskPending.value = false
  }, TASK_SUBMIT_LOCK_MS)

  ;(async () => {
    try {
    const r = await fetchWithTimeout(
      `/api/aqua/tasks/${taskName}`,
      { method: 'POST' },
      TASK_HTTP_MS,
    )
    const d = await r.json().catch(() => ({}))
    if (!r.ok || d.ok === false) {
      throw new Error(d.message || `任务启动失败 HTTP ${r.status}`)
    }
    addLog(`任务已提交: ${labels[taskName] ?? taskName}`, 'task')
    busy.value = Boolean(d.busy)
    if (!busy.value) currentTask.value = '待命'
    fetchStatus()
    } catch (e) {
      busy.value = false
      currentTask.value = '待命'
      addLog(e.message || '任务启动失败', 'error')
    }
  })()

  requestAnimationFrame(() => {
    fetchStatus()
  })
}

async function stopTask() {
  addLog('请求停止当前任务', 'warn')
  try {
    const r = await fetchWithTimeout(
      '/api/aqua/tasks/stop',
      { method: 'POST' },
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
      '/api/aqua/rail/position',
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ position: railTarget.value }),
      },
      RAIL_HTTP_MS,
    )
    if (r.status === 404) {
      r = await fetchWithTimeout(
        '/api/aqua/rail/move',
        {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
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
    const r = await fetchWithTimeout('/api/aqua/status', {}, STATUS_HTTP_MS)
    if (r.ok) {
      const d = await r.json()
      const nextTask = d.currentTask ?? '待命'
      const taskText = String(nextTask).trim().toLowerCase()
      const taskIsIdle = taskText === '' || taskText === 'idle' || taskText === '待命'
      connected.value = d.connected ?? d.ok ?? true
      busy.value = taskPending.value || (Boolean(d.busy) && !taskIsIdle)
      currentTask.value = busy.value ? nextTask : '待命'
      phase.value = d.phase ?? 'idle'
      if (d.railPosition != null) {
        railPosition.value = d.railPosition
        if (shouldAdoptReportedSetpoint({
          touched: railTargetTouched.value,
          pending: railEditing.value || railPending.value,
          reported: d.railPosition,
        })) {
          setRailTarget(d.railPosition, false)
        }
      }
      lastError.value = d.lastError ?? ''
      if (d.uptimeSec != null) uptime.value = formatUptime(Number(d.uptimeSec))
      else uptime.value = d.uptime ?? uptime.value
      if (d.camera) {
        cameraState.hasRgb = Boolean(d.camera.hasRgb)
        cameraState.hasDepth = Boolean(d.camera.hasDepth)
        cameraState.ageSec = d.camera.ageSec ?? null
      }
      if (d.pump) {
        pumpManualOn.value = Boolean(d.pump.manualOn)
        if (d.pump.manualOn === true && shouldAdoptReportedSetpoint({
          touched: pumpPwmTouched.value,
          pending: pumpBusy.value,
          reported: d.pump.pwm,
          acceptZero: false,
        })) {
          setPumpPwm(d.pump.pwm)
        }
      }
      const reportedAtomizerState = d.atomizer?.state ?? d.serial?.telemetry?.atomizer_state
      if (!atomizerBusy.value && typeof reportedAtomizerState === 'boolean') {
        atomizerState.value = reportedAtomizerState
      } else if (!atomizerBusy.value && (reportedAtomizerState === 0 || reportedAtomizerState === 1)) {
        atomizerState.value = reportedAtomizerState === 1
      }
      atomizerFault.value = Boolean(d.atomizer?.fault)
      const reportedUsbLight = d.usbLight ?? d.hardwareSerial?.usbLight
      const telemetryUsbLightMode = d.hardwareSerial?.serial?.telemetry?.usb_light_mode
      const reportedUsbLightMode = reportedUsbLight?.mode ?? telemetryUsbLightMode
      const parsedUsbLightMode = Number(reportedUsbLightMode)
      const reportedUsbLightOption = getUsbLightMode(parsedUsbLightMode)
      if (!usbLightBusy.value) {
        usbLightCurrentMode.value = reportedUsbLightOption?.mode ?? null
        usbLightReady.value = Boolean(reportedUsbLight?.ready ?? reportedUsbLightOption)
        usbLightFault.value = Boolean(reportedUsbLight?.fault)
        if (!usbLightSelectionTouched.value && reportedUsbLightOption) {
          usbLightSelectedMode.value = reportedUsbLightOption.mode
        }
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
  grid-template-columns: minmax(560px, 1fr) 420px;
  gap: 18px;
  grid-template-areas: "left right";
  height: calc(100vh - 120px);
  min-height: 600px;
  align-items: stretch;
}

.quick-status {
  flex: none;
  padding: 6px 11px;
  border-radius: 8px;
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
  gap: 10px;
  padding: 16px;
}

.primary-task-btn {
  min-height: 76px;
  border: none;
  border-radius: 8px;
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
  box-shadow: 0 10px 22px rgba(0,0,0,0.16);
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

.task-loosen { background: #0f9f6e; }
.task-feed { background: #d97706; }
.task-prune { background: #6d5bd0; }
.task-stop { background: #dc2626; }

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
  border-radius: 8px;
  border: 1px solid rgba(255,255,255,0.07);
  box-shadow: var(--shadow-md);
  overflow: hidden;
}

.panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 13px 16px;
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
  margin: 0 16px 16px;
  padding: 16px;
  border-radius: 8px;
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
  margin: 4px 0 6px;
  accent-color: #06b6d4;
}

.pump-meter {
  margin-bottom: 14px;
}

.pump-scale {
  display: flex;
  justify-content: space-between;
  padding: 0 2px;
  color: var(--text-secondary);
  font-size: 11px;
  font-family: monospace;
}

.pump-command-row {
  display: grid;
  grid-template-columns: minmax(72px, 90px) repeat(4, minmax(86px, 1fr));
}

.rail-input {
  min-width: 0;
  flex: 1;
  height: 34px;
  border: 1px solid rgba(255,255,255,0.08);
  background: var(--bg-card);
  color: var(--text-primary);
  border-radius: 7px;
  padding: 0 10px;
  font-family: monospace;
}

.rail-btn {
  height: 34px;
  border: none;
  background: var(--bg-card);
  color: var(--text-secondary);
  border-radius: 7px;
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

.pump-readout-wrap {
  display: flex;
  align-items: flex-end;
  gap: 10px;
}

.pump-readout-wrap span {
  color: var(--text-secondary);
  font-size: 12px;
}

.pump-note {
  margin-top: 12px;
  display: flex;
  justify-content: space-between;
  gap: 10px;
  color: var(--text-secondary);
  font-size: 12px;
}

.pump-note i {
  color: #67e8f9;
  margin-right: 6px;
}

.usb-light-card {
  padding: 0;
  border-color: rgba(245, 158, 11, 0.2);
  transition: border-color 0.18s, box-shadow 0.18s;
}

.usb-light-card.is-active {
  border-color: rgba(245, 158, 11, 0.38);
  box-shadow: 0 12px 28px rgba(245, 158, 11, 0.08);
}

.usb-light-card.is-fault {
  border-color: rgba(239, 68, 68, 0.48);
}

.usb-light-header {
  min-height: 68px;
  padding: 12px 16px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.06);
}

.usb-light-identity {
  display: flex;
  align-items: center;
  gap: 11px;
  min-width: 0;
}

.usb-light-identity > span:last-child {
  display: flex;
  flex-direction: column;
  gap: 3px;
  min-width: 0;
}

.usb-light-identity strong {
  color: var(--text-primary);
  font-size: 14px;
}

.usb-light-identity small,
.usb-light-controls small,
.usb-light-current small {
  color: var(--text-secondary);
  font-size: 11px;
  overflow-wrap: anywhere;
}

.usb-light-beacon {
  width: 42px;
  height: 42px;
  border-radius: 8px;
  display: grid;
  place-items: center;
  flex: none;
  color: #94a3b8;
  background: rgba(100, 116, 139, 0.14);
  border: 1px solid rgba(148, 163, 184, 0.12);
}

.usb-light-beacon.tone-red { color: #f87171; background: rgba(239, 68, 68, 0.15); }
.usb-light-beacon.tone-yellow { color: #facc15; background: rgba(234, 179, 8, 0.14); }
.usb-light-beacon.tone-green { color: #4ade80; background: rgba(34, 197, 94, 0.14); }
.usb-light-beacon.tone-white { color: #f8fafc; background: rgba(226, 232, 240, 0.14); }
.usb-light-beacon.tone-cyan { color: #22d3ee; background: rgba(6, 182, 212, 0.14); }
.usb-light-beacon.tone-purple { color: #c084fc; background: rgba(168, 85, 247, 0.14); }
.usb-light-beacon.tone-blue { color: #60a5fa; background: rgba(59, 130, 246, 0.14); }
.usb-light-beacon.tone-horn { color: #fb923c; background: rgba(249, 115, 22, 0.14); }

.usb-light-beacon.pulse-slow { animation: usb-light-pulse 1.4s steps(2, end) infinite; }
.usb-light-beacon.pulse-fast { animation: usb-light-pulse 0.55s steps(2, end) infinite; }

@keyframes usb-light-pulse {
  50% { opacity: 0.36; }
}

.usb-light-status {
  flex: none;
  min-height: 26px;
  padding: 5px 9px;
  border-radius: 6px;
  color: #94a3b8;
  background: rgba(100, 116, 139, 0.13);
  border: 1px solid rgba(148, 163, 184, 0.13);
  font-size: 11px;
  font-weight: 700;
}

.usb-light-status.ready { color: #4ade80; background: rgba(34, 197, 94, 0.11); }
.usb-light-status.fault { color: #f87171; background: rgba(239, 68, 68, 0.12); }
.usb-light-status.busy { color: #fbbf24; background: rgba(245, 158, 11, 0.12); }

.usb-light-body {
  display: grid;
  grid-template-columns: minmax(150px, 0.7fr) minmax(280px, 1.5fr);
  gap: 16px;
  padding: 16px;
}

.usb-light-current {
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 5px;
  min-width: 0;
  padding-right: 16px;
  border-right: 1px solid rgba(255, 255, 255, 0.07);
}

.usb-light-current > span,
.usb-light-controls label {
  color: var(--text-secondary);
  font-size: 11px;
  font-weight: 700;
}

.usb-light-current strong {
  color: var(--text-primary);
  font-size: 16px;
}

.usb-light-controls {
  display: flex;
  flex-direction: column;
  gap: 7px;
  min-width: 0;
}

.usb-light-command-row {
  display: grid;
  grid-template-columns: minmax(150px, 1fr) 38px minmax(106px, auto);
  gap: 8px;
}

.usb-light-select,
.usb-light-off-btn,
.usb-light-apply-btn {
  min-width: 0;
  height: 38px;
  border-radius: 7px;
  border: 1px solid rgba(255, 255, 255, 0.09);
  font: inherit;
}

.usb-light-select {
  padding: 0 10px;
  color: var(--text-primary);
  background: var(--bg-card);
}

.usb-light-off-btn,
.usb-light-apply-btn {
  cursor: pointer;
  transition: background 0.18s, color 0.18s, border-color 0.18s;
}

.usb-light-off-btn {
  color: #f87171;
  background: rgba(239, 68, 68, 0.12);
}

.usb-light-off-btn:hover:not(:disabled) { background: rgba(239, 68, 68, 0.24); }

.usb-light-apply-btn {
  padding: 0 13px;
  color: #111827;
  background: #fbbf24;
  border-color: #fbbf24;
  font-size: 12px;
  font-weight: 800;
}

.usb-light-apply-btn i { margin-right: 6px; }
.usb-light-apply-btn:hover:not(:disabled) { background: #f59e0b; border-color: #f59e0b; }
.usb-light-off-btn:disabled,
.usb-light-apply-btn:disabled,
.usb-light-select:disabled { cursor: wait; opacity: 0.58; }

.atomizer-card {
  position: relative;
  width: 100%;
  min-height: 112px;
  padding: 18px;
  display: grid;
  grid-template-columns: 54px minmax(0, 1fr) auto;
  align-items: center;
  gap: 14px;
  color: var(--text-primary);
  font: inherit;
  text-align: left;
  cursor: pointer;
  transition: border-color 0.18s, box-shadow 0.18s, transform 0.18s, background 0.18s;
}

.atomizer-card::before {
  content: '';
  position: absolute;
  inset: 0 auto 0 0;
  width: 4px;
  background: #64748b;
  transition: background 0.18s, box-shadow 0.18s;
}

.atomizer-card:hover:not(:disabled) {
  transform: translateY(-2px);
  border-color: rgba(34, 211, 238, 0.35);
  box-shadow: 0 12px 28px rgba(6, 182, 212, 0.12);
}

.atomizer-card:focus-visible {
  outline: 3px solid rgba(34, 211, 238, 0.32);
  outline-offset: 2px;
}

.atomizer-card:disabled {
  cursor: wait;
  opacity: 0.78;
}

.atomizer-card.is-on {
  background: rgba(8, 145, 178, 0.08);
  border-color: rgba(34, 211, 238, 0.28);
}

.atomizer-card.is-on::before {
  background: #22d3ee;
  box-shadow: 0 0 14px rgba(34, 211, 238, 0.55);
}

.atomizer-card.is-fault::before {
  background: #ef4444;
  box-shadow: 0 0 12px rgba(239, 68, 68, 0.45);
}

.atomizer-icon {
  width: 54px;
  height: 54px;
  border-radius: 8px;
  display: grid;
  place-items: center;
  background: rgba(100, 116, 139, 0.16);
  color: #94a3b8;
  font-size: 22px;
  transition: color 0.18s, background 0.18s, box-shadow 0.18s;
}

.atomizer-card.is-on .atomizer-icon {
  color: #67e8f9;
  background: rgba(6, 182, 212, 0.16);
  box-shadow: inset 0 0 0 1px rgba(103, 232, 249, 0.12);
}

.atomizer-card.is-fault .atomizer-icon {
  color: #f87171;
  background: rgba(239, 68, 68, 0.13);
}

.atomizer-copy,
.atomizer-meta {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.atomizer-copy {
  gap: 5px;
}

.atomizer-copy strong {
  font-size: 15px;
}

.atomizer-copy small {
  color: var(--text-secondary);
  font-size: 12px;
  overflow-wrap: anywhere;
}

.atomizer-meta {
  align-items: flex-end;
  gap: 8px;
}

.atomizer-meta b {
  color: #94a3b8;
  font-size: 12px;
}

.atomizer-card.is-on .atomizer-meta b {
  color: #67e8f9;
}

.atomizer-card.is-fault .atomizer-meta b {
  color: #f87171;
}

.atomizer-switch {
  width: 42px;
  height: 24px;
  padding: 3px;
  border-radius: 12px;
  background: #334155;
  transition: background 0.18s;
}

.atomizer-switch span {
  display: block;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: #cbd5e1;
  transition: transform 0.18s, background 0.18s;
}

.atomizer-card.is-on .atomizer-switch {
  background: #0891b2;
}

.atomizer-card.is-on .atomizer-switch span {
  background: #ecfeff;
  transform: translateX(18px);
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
  border-radius: 8px;
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
  background: #070b12;
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
  .pump-command-row { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  .pump-command-row .rail-input-large { grid-column: 1 / -1; width: 100%; }
  .pump-note { flex-direction: column; }
  .usb-light-header { align-items: flex-start; }
  .usb-light-body { grid-template-columns: 1fr; }
  .usb-light-current { padding: 0 0 12px; border-right: 0; border-bottom: 1px solid rgba(255,255,255,0.07); }
  .usb-light-command-row { grid-template-columns: minmax(0, 1fr) 38px; }
  .usb-light-apply-btn { grid-column: 1 / -1; }
  .atomizer-card {
    grid-template-columns: 46px minmax(0, 1fr) auto;
    gap: 10px;
    padding: 14px;
  }
  .atomizer-icon { width: 46px; height: 46px; }
}

</style>
