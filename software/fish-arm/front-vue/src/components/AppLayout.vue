<template>
  <aside class="sidebar">
    <div class="sidebar-header">
      <div class="logo">
        <i class="fas fa-fish"></i>
        <span>AquaGarden</span>
      </div>
    </div>
    <nav class="sidebar-nav">
      <router-link
        to="/"
        class="nav-item"
        :class="{ active: currentRoute.name === 'dashboard' }"
      >
        <i class="fas fa-tachometer-alt"></i><span>仪表板</span>
      </router-link>
      <router-link to="/cameras" class="nav-item" active-class="active">
        <i class="fas fa-video"></i><span>视频监控</span>
      </router-link>
      <router-link to="/history" class="nav-item" active-class="active">
        <i class="fas fa-chart-line"></i><span>历史数据</span>
      </router-link>
      <router-link to="/robot" class="nav-item" active-class="active">
        <i class="fas fa-robot"></i><span>机械臂控制</span>
      </router-link>
      <router-link to="/alerts" class="nav-item" active-class="active">
        <i class="fas fa-bell"></i><span>警报设置</span>
      </router-link>
      <router-link to="/settings" class="nav-item" active-class="active">
        <i class="fas fa-cog"></i><span>系统设置</span>
      </router-link>
    </nav>
    <div class="sidebar-footer">
      <div class="user-info">
        <i class="fas fa-user-circle"></i>
        <div class="user-details">
          <div class="user-name">{{ displayName }}</div>
          <div class="user-role">Admin</div>
        </div>
      </div>
      <button type="button" class="logout-btn" @click="logout">
        <i class="fas fa-sign-out-alt"></i>
      </button>
    </div>
  </aside>
  <main class="main-content">
    <header class="top-header">
      <div class="header-left">
        <h1>{{ pageTitle }}</h1>
        <p class="subtitle">{{ pageSubtitle }}</p>
      </div>
      <div class="header-right">
        <div class="mode-selector">
          <button
            type="button"
            class="mode-btn"
            :class="{ active: mode === 'demo' }"
            @click="setMode('demo')"
          >
            <i class="fas fa-eye"></i>演示模式
          </button>
          <button
            type="button"
            class="mode-btn"
            :class="{ active: mode === 'service' }"
            @click="setMode('service')"
          >
            <i class="fas fa-tools"></i>服务模式
          </button>
        </div>
        <div class="time-display">
          <i class="fas fa-clock"></i>
          <span>{{ timeText }}</span>
        </div>
      </div>
    </header>
    <router-view />
  </main>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { apiUrl, authHeaders } from '@/api/http'

const router = useRouter()
const currentRoute = useRoute()
const timeText = ref('--:--:--')
const mode = ref('demo')
const displayName = computed(() => localStorage.getItem('username') || '管理员')

const pageTitle = computed(() => currentRoute.meta.title || '控制面板')
const pageSubtitle = computed(() => currentRoute.meta.subtitle || '')

let tick = null
function updateTime() {
  timeText.value = new Date().toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  })
}

async function setMode(m) {
  mode.value = m
  try {
    await fetch(apiUrl('/api/mode'), {
      method: 'POST',
      headers: authHeaders(),
      body: JSON.stringify({ mode: m }),
    })
  } catch {
    /* 离线时仍保持 UI 切换 */
  }
}

function logout() {
  if (confirm('确定要退出系统吗？')) {
    localStorage.removeItem('token')
    router.push({ name: 'login' })
  }
}

onMounted(() => {
  updateTime()
  tick = setInterval(updateTime, 1000)
  fetch(apiUrl('/api/mode'), { headers: authHeaders() })
    .then((r) => (r.ok ? r.json() : null))
    .then((j) => {
      if (j?.mode) mode.value = j.mode
    })
    .catch(() => {})
})

onUnmounted(() => {
  if (tick) clearInterval(tick)
})
</script>
