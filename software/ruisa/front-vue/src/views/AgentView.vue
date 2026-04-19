<template>
  <div class="ruisa-root">
    <div class="toast-container">
      <div
        v-for="t in toasts"
        :key="t.id"
        class="toast"
        :class="t.type"
      >
        {{ t.msg }}
      </div>
    </div>

    <div
      class="camera-modal"
      :class="{ open: cameraOpen }"
      :aria-hidden="!cameraOpen"
    >
      <div class="camera-modal-backdrop" @click="closeCameraPreview" />
      <div class="camera-modal-card">
        <div class="camera-modal-head">
          <span>📷 机械臂摄像头实时画面</span>
          <button type="button" class="camera-modal-close" @click="closeCameraPreview">关闭</button>
        </div>
        <div class="camera-modal-body">
          <img
            :src="cameraImgSrc"
            alt="摄像头画面加载中…"
            width="1280"
            height="720"
            decoding="async"
          >
          <p class="camera-modal-hint">
            画面由运行后端的机器从 USB 摄像头读取，与分拣 / 人脸 / 拍题任务同源；任务结束后自动关闭。
          </p>
        </div>
      </div>
    </div>

    <header class="header">
      <div class="header-left">
        <div class="logo">🤖</div>
        <div>
          <div class="title">小臂 · AI 机械臂辅学</div>
          <div class="subtitle">Web 端对齐 agent/agent.py 与 agent/tasks.py</div>
        </div>
      </div>
      <div class="header-right">
        <a class="link-docs" href="/docs" target="_blank" rel="noopener">API 文档</a>
        <div class="status-pill">
          <span class="status-dot" :class="{ on: connected }" />
          <span>{{ statusText }}</span>
        </div>
      </div>
    </header>

    <div class="main">
      <div class="col-chat">
        <div class="panel-title panel-title--row">
          <span>对话</span>
          <button
            type="button"
            class="agent-debug-toggle"
            :class="{ active: dbgActive }"
            title="等同 DEBUG=1 python agent.py"
            @click="toggleDebugMode"
          >
            {{ dbgToggleLabel }}
          </button>
        </div>
        <div ref="messagesEl" class="agent-messages">
          <div
            v-for="(m, i) in messages"
            :key="i"
            class="msg"
            :class="m.role"
          >
            {{ m.text }}
            <div class="msg-time">{{ m.time }}</div>
          </div>
          <div v-if="typing" id="typingInd" class="msg assistant typing">正在思考…</div>
        </div>
        <div class="quick-actions">
          <button
            v-for="q in quickActions"
            :key="q.text"
            type="button"
            class="quick-btn"
            @click="sendQuick(q.text)"
          >
            {{ q.label }}
          </button>
        </div>
        <div class="agent-debug-bar" :class="{ show: debugBarShow }">
          <span class="agent-debug-hint">{{ debugHint }}</span>
          <button
            type="button"
            class="agent-debug-wake-btn"
            :disabled="wakeBtnDisabled"
            @click="sendDebugWake"
          >
            模拟唤醒
          </button>
        </div>
        <div class="agent-task-indicator" :class="{ running: taskRunning }">
          <div class="agent-task-spinner" />
          <span>{{ taskText }}</span>
        </div>
        <div class="agent-input">
          <button
            type="button"
            class="agent-mic-btn"
            :class="{ listening: micListening }"
            title="点击开始语音输入（浏览器语音识别，需麦克风权限；Chrome/Edge 推荐）"
            aria-label="语音输入"
            @click="toggleVoiceInput"
          >
            🎤
          </button>
          <input
            v-model="inputText"
            type="text"
            placeholder="输入自然语言指令，或点左侧麦克风说话…"
            autocomplete="off"
            @keydown="onAgentInput"
          >
          <button type="button" class="agent-send-btn" @click="sendMessage">▶</button>
        </div>
      </div>

      <div class="col-side">
        <div class="term-wrap">
          <div class="panel-title">任务标准输出（tasks.py print，由后端捕获）</div>
          <div class="term-bar">
            <div class="term-dots">
              <span class="tdot r" /><span class="tdot y" /><span class="tdot g" />
            </div>
            <div class="term-title">ruisa/backend/agent — python agent.py（Web 桥接）</div>
          </div>
          <div ref="termEl" class="term-body">
            <div v-for="(line, i) in termLines" :key="i" class="log-line">
              <span class="t">{{ line.t }}</span><span :class="line.cls">[{{ line.level }}]</span>
              {{ line.msg }}
            </div>
          </div>
        </div>
        <div class="flow-panel">
          <div class="panel-title">agent.py 状态机 · 五项任务（tasks.py）</div>
          <div class="flow-scroll">
            <div class="flow-step">
              <strong>1 主循环（agent.py）</strong>
              初始姿态 go_home → 等待唤醒（麦克风或 DEBUG 下 Enter）→ <code>_continuous_session</code> 内 <code>dialogue.listen()</code>；Web 端可用输入框或麦克风按钮（浏览器语音识别 zh-CN）。摄像头由 <code>tasks._open_camera</code> 按 <code>config.CAMERA_INDEX</code> 打开。
            </div>
            <div class="flow-step">
              <strong>2 解析与分发</strong>
              <code>dialogue.parse(text)</code> → <code>(task_name, params)</code> → 与 <code>agent.py</code> 相同的 <code>_dispatch</code> 语义，直接调用下方 <code>task_*</code>。
            </div>
            <div class="flow-step">
              <strong>3 结束会话</strong>
              <code>dialogue.should_end_session</code>（如「拜拜」）→ 告别语 → 本机 <code>arm.go_home()</code>。
            </div>
            <div class="task-grid">
              <div class="task-card"><b>task_clamp</b>颜色识别与分拣；摄像头 OpenCV + 示教 MAP；空格确认 / Q 取消。</div>
              <div class="task-card"><b>task_led</b>智能台灯；preset off/low/medium/high；off 不移动手臂。</div>
              <div class="task-card"><b>task_face</b>Haar 人脸追踪；超时或 Q 退出。</div>
              <div class="task-card"><b>task_answer</b>观测位拍照 + 多模态解题 + TTS；需 DASHSCOPE_API_KEY。</div>
              <div class="task-card"><b>task_action</b>回放 actions/*.json 关键帧。</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, nextTick, onMounted, onUnmounted, ref } from 'vue'
import { AgentWS } from '@/modules/agentWs.js'

const AGENT_DEBUG_KEY = 'ruisa_agent_debug_keyboard'

const messagesEl = ref(null)
const termEl = ref(null)
const connected = ref(false)
const statusText = ref('连接中…')
const messages = ref([])
const typing = ref(false)
const inputText = ref('')
const termLines = ref([])
const cameraOpen = ref(false)
const cameraImgSrc = ref('')
const micListening = ref(false)
const taskRunning = ref(false)
const taskText = ref('正在执行任务…')
const toasts = ref([])
let toastId = 0

const dbgActive = ref(false)
const dbgToggleLabel = computed(() => (dbgActive.value ? '键盘DEBUG·开' : '键盘DEBUG·关'))

const debugBarShow = computed(() => AgentWS.debugKeyboard)
const debugHint = computed(() =>
  AgentWS.debugKeyboard && AgentWS.needsAgentWake
    ? '请先「模拟唤醒」或输入框留空按 Enter（等同 agent.py DEBUG 按 Enter）'
    : '已唤醒，可发指令。',
)
const wakeBtnDisabled = computed(() => !AgentWS.isConnected() || !AgentWS.needsAgentWake || !AgentWS.debugKeyboard)

const quickActions = [
  { label: '🎨 task_clamp', text: '帮我分拣积木' },
  { label: '💡 task_led', text: '打开台灯' },
  { label: '👁️ task_face', text: '看看我在不在' },
  { label: '📐 task_answer', text: '帮我看这道题' },
  { label: '🎭 task_action', text: '打个招呼' },
]

let voiceRecognition = null

function showToast(msg, type = 'error') {
  const id = ++toastId
  toasts.value = [...toasts.value, { id, msg, type }]
  setTimeout(() => {
    toasts.value = toasts.value.filter((t) => t.id !== id)
  }, 4000)
}

function formatTime(d) {
  return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`
}

function appendTerm(level, msg) {
  const t = new Date()
  const cls = level === 'ERROR' ? 'e' : level === 'SUCCESS' ? 's' : 'i'
  termLines.value = [...termLines.value, { t: formatTime(t), level, cls, msg }]
  nextTick(() => {
    const el = termEl.value
    if (el) el.scrollTop = el.scrollHeight
  })
}

function addMsg(role, text) {
  messages.value = [...messages.value, { role, text, time: formatTime(new Date()) }]
  nextTick(() => {
    const el = messagesEl.value
    if (el) el.scrollTop = el.scrollHeight
  })
}

function hideTyping() {
  typing.value = false
}

function showTyping() {
  typing.value = true
  nextTick(() => {
    const el = messagesEl.value
    if (el) el.scrollTop = el.scrollHeight
  })
}

function openCameraPreview(mjpegPath) {
  const path = mjpegPath || '/api/v1/camera/mjpeg'
  cameraImgSrc.value = `${path}?t=${Date.now()}`
  cameraOpen.value = true
}

function closeCameraPreview() {
  cameraImgSrc.value = ''
  cameraOpen.value = false
}

function setMicListening(on) {
  micListening.value = on
}

function toggleVoiceInput() {
  if (!AgentWS.isConnected()) {
    showToast('未连接服务器')
    return
  }
  if (AgentWS.debugKeyboard && AgentWS.needsAgentWake) {
    showToast('DEBUG：请先模拟唤醒')
    return
  }
  const Rec = window.SpeechRecognition || window.webkitSpeechRecognition
  if (!Rec) {
    showToast('当前浏览器不支持语音识别，请使用 Chrome 或 Edge（需安全来源：localhost 或 HTTPS）')
    return
  }
  if (voiceRecognition) {
    try {
      voiceRecognition.abort()
    } catch {
      /* ignore */
    }
    voiceRecognition = null
    setMicListening(false)
    return
  }
  const rec = new Rec()
  voiceRecognition = rec
  rec.lang = 'zh-CN'
  rec.interimResults = false
  rec.maxAlternatives = 1
  rec.continuous = false

  rec.onstart = () => setMicListening(true)

  rec.onend = () => {
    voiceRecognition = null
    setMicListening(false)
  }

  rec.onerror = (ev) => {
    voiceRecognition = null
    setMicListening(false)
    if (ev.error === 'not-allowed') {
      showToast('麦克风权限被拒绝，请在浏览器地址栏旁允许麦克风')
    } else if (ev.error === 'no-speech') {
      showToast('未检测到语音，请再试一次')
    } else if (ev.error !== 'aborted') {
      showToast('语音识别：' + (ev.error || '失败'))
    }
  }

  rec.onresult = (ev) => {
    let text = ''
    for (let i = ev.resultIndex; i < ev.results.length; i++) {
      text += ev.results[i][0].transcript
    }
    text = (text || '').trim()
    if (text) {
      inputText.value = text
      addMsg('user', text)
      showTyping()
      AgentWS.sendMessage(text)
    } else {
      showToast('未识别到有效内容')
    }
  }

  try {
    rec.start()
  } catch (e) {
    voiceRecognition = null
    setMicListening(false)
    showToast('无法启动语音识别：' + (e.message || String(e)))
  }
}

function toggleDebugMode() {
  if (!AgentWS.isConnected()) {
    showToast('请先等待 WebSocket 连接')
    return
  }
  const next = !AgentWS.debugKeyboard
  AgentWS.sendSetDebugKeyboard(next)
  localStorage.setItem(AGENT_DEBUG_KEY, next ? '1' : '0')
  dbgActive.value = next
}

function sendMessage() {
  if (!AgentWS.isConnected()) {
    showToast('未连接服务器')
    return
  }
  if (AgentWS.debugKeyboard && AgentWS.needsAgentWake) {
    showToast('DEBUG：请先模拟唤醒')
    return
  }
  const text = (inputText.value || '').trim()
  if (!text) return
  inputText.value = ''
  addMsg('user', text)
  showTyping()
  AgentWS.sendMessage(text)
}

function sendQuick(text) {
  if (!AgentWS.isConnected()) return showToast('未连接')
  if (AgentWS.debugKeyboard && AgentWS.needsAgentWake) return showToast('DEBUG：请先模拟唤醒')
  addMsg('user', text)
  showTyping()
  AgentWS.sendMessage(text)
}

function sendDebugWake() {
  if (!AgentWS.isConnected()) return
  showTyping()
  AgentWS.sendDebugWake()
}

function onAgentInput(ev) {
  if (ev.key !== 'Enter' || ev.shiftKey) return
  ev.preventDefault()
  if (AgentWS.debugKeyboard && AgentWS.needsAgentWake) {
    if (!(inputText.value || '').trim()) sendDebugWake()
    else showToast('DEBUG：请先清空输入框并模拟唤醒')
    return
  }
  sendMessage()
}

function wireAgentWs() {
  AgentWS.on('onConnected', () => {
    connected.value = true
    statusText.value = '已连接'
    const want = localStorage.getItem(AGENT_DEBUG_KEY) === '1'
    if (want) AgentWS.sendSetDebugKeyboard(true)
    dbgActive.value = want
    appendTerm('INFO', 'WebSocket 已连接；意图解析与 agent/dialogue.py 一致，任务由 agent/tasks.py 执行。')
  })

  AgentWS.on('onDisconnected', () => {
    connected.value = false
    statusText.value = '未连接'
    hideTyping()
    closeCameraPreview()
    if (voiceRecognition) {
      try {
        voiceRecognition.abort()
      } catch {
        /* ignore */
      }
      voiceRecognition = null
      setMicListening(false)
    }
    appendTerm('ERROR', 'WebSocket 已断开')
  })

  AgentWS.on('onResponse', (data) => {
    hideTyping()
    if (data.response) addMsg('assistant', data.response)
    dbgActive.value = AgentWS.debugKeyboard
    if (data.shouldEnd) appendTerm('SUCCESS', '会话结束（拜拜）→ 机械臂归位')
  })

  AgentWS.on('onDebugMode', () => {
    dbgActive.value = AgentWS.debugKeyboard
  })

  AgentWS.on('onTaskStart', (d) => {
    taskRunning.value = true
    taskText.value = `执行中: ${d.task}`
    appendTerm('INFO', `task_start → ${d.task}`)
  })

  AgentWS.on('onTaskLog', (d) => {
    appendTerm('INFO', d.message || '')
  })

  AgentWS.on('onTaskComplete', (d) => {
    taskRunning.value = false
    appendTerm('SUCCESS', `task_complete → ${d.task}: ${JSON.stringify(d.result || {})}`)
    if (d.task === 'answer' && d.result && d.result.answer) {
      addMsg('assistant', '【题目解答】\n' + d.result.answer)
    }
  })

  AgentWS.on('onTaskError', (d) => {
    taskRunning.value = false
    appendTerm('ERROR', `${d.task}: ${d.error}`)
  })

  AgentWS.on('onCameraPreview', (d) => {
    if (d.show) openCameraPreview(d.mjpegPath)
    else closeCameraPreview()
  })
}

onMounted(() => {
  const greet =
    '你好呀！我是小臂，你的学习小助手~ 有什么需要帮忙的吗？\n可打字或点输入框左侧 🎤 用中文语音说话（Chrome/Edge）；视觉任务请配置机械臂 USB 摄像头（CAMERA_INDEX 等）。'
  addMsg('assistant', greet)
  dbgActive.value = localStorage.getItem(AGENT_DEBUG_KEY) === '1'
  wireAgentWs()
  AgentWS.connect().catch((e) => {
    connected.value = false
    statusText.value = '未连接'
    showToast('无法连接 WebSocket，请启动 Python（8000）与 Spring（8080）后执行 npm run dev')
    appendTerm('ERROR', String(e))
  })
})

onUnmounted(() => {
  AgentWS.disconnect()
  if (voiceRecognition) {
    try {
      voiceRecognition.abort()
    } catch {
      /* ignore */
    }
  }
})
</script>

<style scoped>
.ruisa-root {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 0;
}
</style>
