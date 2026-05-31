<template>
  <aside class="sidebar">
    <div class="sidebar-brand">
      <div class="brand-icon"><i class="fas fa-fish"></i></div>
      <span class="brand-name">AQUATECH</span>
    </div>
    <nav class="sidebar-nav">
      <router-link
        to="/"
        class="nav-item"
        :class="{ active: currentRoute.name === 'dashboard' }"
      >
        <div class="nav-icon"><i class="fas fa-th-large"></i></div>
        <div class="nav-labels">
          <div class="nav-en">Dashboard</div>
          <div class="nav-zh">仪表板</div>
        </div>
      </router-link>
      <router-link to="/robot" class="nav-item" active-class="active">
        <div class="nav-icon"><i class="fas fa-sliders-h"></i></div>
        <div class="nav-labels">
          <div class="nav-en">Control</div>
          <div class="nav-zh">系统控制</div>
        </div>
      </router-link>
      <router-link to="/history" class="nav-item" active-class="active">
        <div class="nav-icon"><i class="fas fa-history"></i></div>
        <div class="nav-labels">
          <div class="nav-en">History</div>
          <div class="nav-zh">历史记录</div>
        </div>
      </router-link>
    </nav>
  </aside>

  <main class="main-content">
    <header class="top-header">
      <div class="header-left">
        <h1>{{ pageTitle }}</h1>
        <p class="subtitle">{{ pageSubtitle }}</p>
      </div>
      <div class="header-right">
        <div class="date-badge">
          <i class="fas fa-calendar-alt"></i>
          <span>{{ dateText }}</span>
        </div>
        <button type="button" class="header-icon-btn" title="用户" @click="logout">
          <i class="fas fa-user-circle"></i>
        </button>
      </div>
    </header>

    <div class="page-content">
      <router-view v-slot="{ Component, route }">
        <keep-alive include="DashboardView">
          <component
            :is="Component"
            v-if="route.meta.keepAlive"
            :key="route.name"
          />
        </keep-alive>
        <component
          :is="Component"
          v-if="!route.meta.keepAlive"
          :key="route.name"
        />
      </router-view>
    </div>

    <footer class="status-bar">
      <div class="status-item">
        <span class="status-led"></span>
        <span>Status: <strong class="text-connected">CONNECTED</strong></span>
        <span class="status-cn">连接正常</span>
      </div>
      <div class="status-item">
        Active Units: Filter, Light, Feeder
      </div>
      <div class="status-item">
        System Time: <strong>{{ timeText }}</strong>
      </div>
    </footer>
  </main>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'

const router = useRouter()
const currentRoute = useRoute()
const timeText = ref('--:--:--')
const dateText = ref('')

const routeTitles = {
  dashboard: { title: 'Smart Aquarium Dashboard', subtitle: '智能水族箱仪表板' },
  robot: { title: 'System Control', subtitle: '系统控制台' },
  history: { title: 'History Records', subtitle: '历史数据记录' },
}

const pageTitle = computed(() => {
  const name = currentRoute.name
  return routeTitles[name]?.title || currentRoute.meta.title || 'Smart Aquarium Dashboard'
})
const pageSubtitle = computed(() => {
  const name = currentRoute.name
  return routeTitles[name]?.subtitle || currentRoute.meta.subtitle || '智能水族箱仪表板'
})

let tick = null

function updateTime() {
  const now = new Date()
  timeText.value = now.toLocaleTimeString('en-US', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  })
  dateText.value = now.toLocaleDateString('en-US', {
    month: 'short',
    day: 'numeric',
    year: 'numeric',
  }) + ', ' + now.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false })
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
})

onUnmounted(() => {
  if (tick) clearInterval(tick)
})
</script>
