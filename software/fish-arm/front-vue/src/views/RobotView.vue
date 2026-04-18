<template>
  <div class="robot-page">
    <div class="control-panel">
      <div class="joystick-section">
        <div class="card-header">
          <div class="card-title"><i class="fas fa-gamepad"></i><span>方向控制</span></div>
          <div class="card-badge badge-online"><span class="badge-dot"></span>在线</div>
        </div>
        <div class="joystick-area" style="padding: 20px">
          <div class="joystick-container">
            <div class="joystick-base">
              <div
                ref="joystickHandle"
                class="joystick-handle"
                @mousedown.prevent="onDragStart"
              ></div>
            </div>
          </div>
          <div class="direction-buttons">
            <div class="dir-btn empty"></div>
            <button type="button" class="dir-btn" @click="controlRobot('forward')"><i class="fas fa-chevron-up"></i></button>
            <div class="dir-btn empty"></div>
            <button type="button" class="dir-btn" @click="controlRobot('left')"><i class="fas fa-arrow-left"></i></button>
            <button type="button" class="dir-btn" @click="centerPosition"><i class="fas fa-crosshairs"></i></button>
            <button type="button" class="dir-btn" @click="controlRobot('right')"><i class="fas fa-arrow-right"></i></button>
            <div class="dir-btn empty"></div>
            <button type="button" class="dir-btn" @click="controlRobot('backward')"><i class="fas fa-chevron-down"></i></button>
            <div class="dir-btn empty"></div>
          </div>
          <div style="display: flex; gap: 12px; width: 100%; margin-top: 8px">
            <button type="button" class="dir-btn" style="flex: 1; height: 50px" @click="controlRobot('up')">
              <i class="fas fa-arrow-up"></i><span style="margin-left: 8px">Z+</span>
            </button>
            <button type="button" class="dir-btn" style="flex: 1; height: 50px" @click="controlRobot('down')">
              <i class="fas fa-arrow-down"></i><span style="margin-left: 8px">Z-</span>
            </button>
          </div>
          <div class="current-position">
            <i class="fas fa-map-marker-alt"></i>
            <div class="position-values">
              <div class="pos-item"><span class="pos-label">X</span><span class="pos-value">{{ posX }}</span></div>
              <div class="pos-item"><span class="pos-label">Y</span><span class="pos-value">{{ posY }}</span></div>
              <div class="pos-item"><span class="pos-label">Z</span><span class="pos-value">{{ posZ }}</span></div>
            </div>
          </div>
        </div>
      </div>
      <div class="status-section">
        <div class="status-card">
          <h3><i class="fas fa-bookmark"></i> 预设位置</h3>
          <div class="preset-buttons">
            <button type="button" class="preset-btn" @click="moveToPreset('feed')"><i class="fas fa-utensils"></i><span>喂食位置</span></button>
            <button type="button" class="preset-btn" @click="moveToPreset('clean')"><i class="fas fa-broom"></i><span>清洁位置</span></button>
            <button type="button" class="preset-btn" @click="moveToPreset('monitor')"><i class="fas fa-eye"></i><span>监控位置</span></button>
            <button type="button" class="preset-btn" @click="moveToPreset('home')"><i class="fas fa-home"></i><span>原点位置</span></button>
          </div>
        </div>
        <div class="status-card">
          <h3><i class="fas fa-server"></i> 状态信息</h3>
          <div class="status-item"><span class="status-label">电机状态</span><span class="status-val online">● 运行正常</span></div>
          <div class="status-item"><span class="status-label">电池电量</span><span class="status-val">87%</span></div>
          <div class="status-item"><span class="status-label">运行时间</span><span class="status-val">3h 24m</span></div>
          <div class="status-item"><span class="status-label">任务队列</span><span class="status-val">2 个任务</span></div>
        </div>
        <button type="button" class="emergency-btn" @click="emergencyStop">
          <i class="fas fa-hand-paper"></i>紧急停止
        </button>
      </div>
    </div>
    <div class="operation-logs">
      <div class="card-header">
        <div class="card-title"><i class="fas fa-list"></i><span>操作日志</span></div>
      </div>
      <div class="log-list">
        <div v-for="(e, i) in opLogs" :key="i" class="log-entry">
          <span class="log-time">{{ e.t }}</span>
          <span class="log-content">{{ e.m }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted, onUnmounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { apiUrl, authHeaders } from '@/api/http'

import '@/assets/styles/robot-page.css'

const router = useRouter()
const posX = ref(0)
const posY = ref(0)
const posZ = ref(0)
const opLogs = ref([{ t: '15:12:30', m: '机械臂已连接就绪' }])
const joystickHandle = ref(null)
let dragging = false
let startX = 0
let startY = 0

function addLog(message) {
  const t = new Date().toLocaleTimeString('zh-CN')
  opLogs.value.unshift({ t, m: message })
  while (opLogs.value.length > 20) opLogs.value.pop()
}

async function controlRobot(direction) {
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
      addLog(`移动到位置 X:${posX.value} Y:${posY.value} Z:${posZ.value}`)
    } else if (r.status === 401) {
      addLog('登录已过期，请重新登录')
      setTimeout(() => router.push({ name: 'login' }), 2000)
    }
  } catch {
    simulateMovement(direction)
  }
}

function simulateMovement(direction) {
  const step = 1
  switch (direction) {
    case 'forward':
      posY.value += step
      break
    case 'backward':
      posY.value -= step
      break
    case 'left':
      posX.value -= step
      break
    case 'right':
      posX.value += step
      break
    case 'up':
      posZ.value += step
      break
    case 'down':
      posZ.value -= step
      break
  }
  posX.value = Math.max(-10, Math.min(10, posX.value))
  posY.value = Math.max(-10, Math.min(10, posY.value))
  posZ.value = Math.max(0, Math.min(20, posZ.value))
  addLog(`移动到位置 X:${posX.value} Y:${posY.value} Z:${posZ.value}`)
}

function moveToPreset(position) {
  const presets = {
    feed: { x: 5, y: 5, z: 10 },
    clean: { x: -5, y: 5, z: 5 },
    monitor: { x: 0, y: 8, z: 12 },
    home: { x: 0, y: 0, z: 0 },
  }
  const p = presets[position]
  if (p) {
    posX.value = p.x
    posY.value = p.y
    posZ.value = p.z
    const label =
      position === 'feed' ? '喂食' : position === 'clean' ? '清洁' : position === 'monitor' ? '监控' : '原点'
    addLog(`移动到预设位置: ${label}`)
  }
}

function centerPosition() {
  posX.value = 0
  posY.value = 0
  posZ.value = 0
  addLog('机械臂已复位到原点')
}

function emergencyStop() {
  addLog('紧急停止已触发！')
  if (confirm('确定要执行紧急停止吗？')) addLog('机械臂已紧急停止')
}

function onDragStart(e) {
  dragging = true
  startX = e.clientX
  startY = e.clientY
  const el = joystickHandle.value
  if (el) el.style.cursor = 'grabbing'
}

function onMouseMove(e) {
  if (!dragging) return
  const el = joystickHandle.value
  if (!el) return
  const dx = e.clientX - startX
  const dy = e.clientY - startY
  const maxDist = 60
  const dist = Math.min(Math.sqrt(dx * dx + dy * dy), maxDist)
  const angle = Math.atan2(dy, dx)
  const newX = Math.cos(angle) * dist
  const newY = Math.sin(angle) * dist
  el.style.transform = `translate(calc(-50% + ${newX}px), calc(-50% + ${newY}px))`
  if (Math.abs(newX) > 30 || Math.abs(newY) > 30) {
    if (newX > 15 && Math.abs(newY) < 15) controlRobot('right')
    else if (newX < -15 && Math.abs(newY) < 15) controlRobot('left')
    else if (newY > 15 && Math.abs(newX) < 15) controlRobot('backward')
    else if (newY < -15 && Math.abs(newX) < 15) controlRobot('forward')
  }
}

function onMouseUp() {
  dragging = false
  const el = joystickHandle.value
  if (el) {
    el.style.transform = 'translate(-50%, -50%)'
    el.style.cursor = 'grab'
  }
}

function onKey(e) {
  switch (e.key) {
    case 'ArrowUp':
    case 'w':
    case 'W':
      controlRobot('forward')
      break
    case 'ArrowDown':
    case 's':
    case 'S':
      controlRobot('backward')
      break
    case 'ArrowLeft':
    case 'a':
    case 'A':
      controlRobot('left')
      break
    case 'ArrowRight':
    case 'd':
    case 'D':
      controlRobot('right')
      break
    case 'q':
    case 'Q':
      controlRobot('up')
      break
    case 'e':
    case 'E':
      controlRobot('down')
      break
    case ' ':
      e.preventDefault()
      centerPosition()
      break
  }
}

onMounted(() => {
  window.addEventListener('mousemove', onMouseMove)
  window.addEventListener('mouseup', onMouseUp)
  window.addEventListener('keydown', onKey)
  addLog('机械臂控制页面已加载')
  addLog('可使用键盘方向键或WASD控制机械臂')
  addLog('Q/E键控制Z轴，空格键归零')
})

onUnmounted(() => {
  window.removeEventListener('mousemove', onMouseMove)
  window.removeEventListener('mouseup', onMouseUp)
  window.removeEventListener('keydown', onKey)
})
</script>
