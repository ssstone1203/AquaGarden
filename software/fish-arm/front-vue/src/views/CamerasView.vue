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
            <button type="button" class="video-btn" title="截图" @click="takeScreenshot('robot')">
              <i class="fas fa-camera"></i>
            </button>
          </div>
        </div>
        <div id="robotVideo" class="video-container">
          <img :src="robotSrc" alt="机械臂摄像头" />
          <div class="video-overlay">
            <div class="video-info-top">
              <div class="rec-indicator"><div class="rec-dot"></div>REC</div>
              <div class="camera-name">CAM-01</div>
            </div>
            <div class="video-info-bottom">
              <div class="video-stat"><i class="fas fa-robot"></i>机械臂 RGB</div>
              <div class="video-stat"><i class="fas fa-tachometer-alt"></i>MJPEG</div>
            </div>
          </div>
        </div>
        <div class="video-controls-bar">
          <button type="button" class="control-btn" :class="{ record: recRobot }" @click="toggleRecording('robot')">
            <i class="fas fa-record-vinyl"></i>{{ recRobot ? '停止' : '录制' }}
          </button>
          <button type="button" class="control-btn" @click="takeScreenshot('robot')">
            <i class="fas fa-camera"></i>截图
          </button>
          <button type="button" class="control-btn" @click="stopRecording('robot')"><i class="fas fa-stop"></i>停止</button>
        </div>
      </div>
      <div class="video-card">
        <div class="video-header">
          <h3><i class="fas fa-water"></i> 鱼缸摄像头</h3>
          <div class="video-actions">
            <button type="button" class="video-btn" title="全屏" @click="toggleFullscreen('tank')">
              <i class="fas fa-expand"></i>
            </button>
            <button type="button" class="video-btn" title="截图" @click="takeScreenshot('tank')">
              <i class="fas fa-camera"></i>
            </button>
          </div>
        </div>
        <div id="tankVideo" class="video-container">
          <img :src="tankSrc" alt="鱼缸摄像头" />
          <div class="video-overlay">
            <div class="video-info-top">
              <div class="rec-indicator"><div class="rec-dot"></div>REC</div>
              <div class="camera-name">CAM-02</div>
            </div>
            <div class="video-info-bottom">
              <div class="video-stat"><i class="fas fa-plug"></i>USB 实时</div>
              <div class="video-stat"><i class="fas fa-tachometer-alt"></i>MJPEG</div>
            </div>
          </div>
        </div>
        <div class="video-controls-bar">
          <button type="button" class="control-btn" :class="{ record: recTank }" @click="toggleRecording('tank')">
            <i class="fas fa-record-vinyl"></i>{{ recTank ? '停止' : '录制' }}
          </button>
          <button type="button" class="control-btn" @click="takeScreenshot('tank')">
            <i class="fas fa-camera"></i>截图
          </button>
          <button type="button" class="control-btn" @click="stopRecording('tank')"><i class="fas fa-stop"></i>停止</button>
        </div>
      </div>
    </div>
    <div class="recording-section">
      <h3><i class="fas fa-film"></i> 录像列表</h3>
      <div class="recording-list">
        <div class="recording-item">
          <div class="recording-info">
            <div class="recording-icon"><i class="fas fa-video"></i></div>
            <div class="recording-details">
              <h4>robot_camera_20250316_143000.mp4</h4>
              <p>机械臂摄像头 • 10分钟 • 128MB</p>
            </div>
          </div>
          <div class="recording-actions">
            <button type="button" class="action-icon-btn" title="播放"><i class="fas fa-play"></i></button>
            <button type="button" class="action-icon-btn" title="下载"><i class="fas fa-download"></i></button>
            <button type="button" class="action-icon-btn danger" title="删除"><i class="fas fa-trash"></i></button>
          </div>
        </div>
        <div class="recording-item">
          <div class="recording-info">
            <div class="recording-icon"><i class="fas fa-video"></i></div>
            <div class="recording-details">
              <h4>tank_camera_20250316_140000.mp4</h4>
              <p>鱼缸摄像头 • 10分钟 • 135MB</p>
            </div>
          </div>
          <div class="recording-actions">
            <button type="button" class="action-icon-btn" title="播放"><i class="fas fa-play"></i></button>
            <button type="button" class="action-icon-btn" title="下载"><i class="fas fa-download"></i></button>
            <button type="button" class="action-icon-btn danger" title="删除"><i class="fas fa-trash"></i></button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { apiUrl } from '@/api/http'

import '@/assets/styles/cameras-page.css'

const robotSrc = apiUrl('/api/aqua/video/rgb')
const tankSrc = apiUrl('/api/video/tank')
const recRobot = ref(false)
const recTank = ref(false)

function toggleFullscreen(which) {
  const el = document.getElementById(`${which}Video`)
  if (!el) return
  if (document.fullscreenElement) document.exitFullscreen()
  else el.requestFullscreen()
}

function takeScreenshot(which) {
  window.alert(`${which === 'robot' ? '机械臂' : '鱼缸'}摄像头截图已保存`)
}

function toggleRecording(which) {
  if (which === 'robot') recRobot.value = !recRobot.value
  else recTank.value = !recTank.value
}

function stopRecording(which) {
  if (which === 'robot') recRobot.value = false
  else recTank.value = false
}
</script>
