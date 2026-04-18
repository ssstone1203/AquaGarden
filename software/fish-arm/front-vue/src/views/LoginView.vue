<template>
  <div class="login-background">
    <div class="animated-bg">
      <div class="bubble bubble-1"></div>
      <div class="bubble bubble-2"></div>
      <div class="bubble bubble-3"></div>
      <div class="bubble bubble-4"></div>
      <div class="bubble bubble-5"></div>
    </div>
  </div>
  <div class="login-container">
    <div class="login-left">
      <div class="brand-section">
        <div class="logo-animation">
          <div class="logo-circle"><i class="fas fa-fish"></i></div>
          <div class="water-ripple"></div>
        </div>
        <h1 class="brand-title">AquaGarden</h1>
        <p class="brand-tagline">智能水族箱管理系统</p>
        <div class="features">
          <div class="feature-item">
            <div class="feature-icon"><i class="fas fa-video"></i></div>
            <div class="feature-text">
              <h3>双摄像头实时监控</h3>
              <p>高清视频流，随时掌握水族箱状态</p>
            </div>
          </div>
          <div class="feature-item">
            <div class="feature-icon"><i class="fas fa-chart-line"></i></div>
            <div class="feature-text">
              <h3>智能数据分析</h3>
              <p>实时监测水质，历史数据可视化</p>
            </div>
          </div>
          <div class="feature-item">
            <div class="feature-icon"><i class="fas fa-robot"></i></div>
            <div class="feature-text">
              <h3>自动化控制</h3>
              <p>智能机械臂，精准喂食与清洁</p>
            </div>
          </div>
        </div>
      </div>
    </div>
    <div class="login-right">
      <div class="login-card">
        <div class="login-header">
          <h2>欢迎回来</h2>
          <p>登录以访问您的控制面板</p>
        </div>
        <form class="login-form" @submit.prevent="submit">
          <div class="form-group">
            <label for="username"><i class="fas fa-user"></i>账号</label>
            <div class="input-wrapper">
              <input id="username" v-model="username" type="text" placeholder="请输入账号" required />
            </div>
          </div>
          <div class="form-group">
            <label for="password"><i class="fas fa-lock"></i>密码</label>
            <div class="input-wrapper">
              <input
                id="password"
                v-model="password"
                :type="showPwd ? 'text' : 'password'"
                placeholder="请输入密码"
                required
              />
              <button type="button" class="toggle-password" @click="showPwd = !showPwd">
                <i :class="showPwd ? 'fas fa-eye-slash' : 'fas fa-eye'"></i>
              </button>
            </div>
          </div>
          <div class="form-options">
            <label class="checkbox-label">
              <input v-model="remember" type="checkbox" />
              <span>记住我</span>
            </label>
            <a href="#" class="forgot-link" @click.prevent>忘记密码？</a>
          </div>
          <div class="error-message" :class="{ show: !!error }">{{ error }}</div>
          <button type="submit" class="login-button" :disabled="loading">
            <span>{{ loading ? '登录中…' : '登录系统' }}</span>
            <i class="fas fa-arrow-right"></i>
          </button>
        </form>
        <div class="demo-info">
          <div class="demo-header"><i class="fas fa-info-circle"></i><span>演示账号信息</span></div>
          <div class="demo-credentials">
            <div class="credential-item"><i class="fas fa-user-circle"></i><span>账号：<strong>admin</strong></span></div>
            <div class="credential-item"><i class="fas fa-key"></i><span>密码：<strong>admin123</strong></span></div>
          </div>
        </div>
      </div>
      <div class="login-footer">
        <p>&copy; 2024 AquaGarden. 保留所有权利.</p>
        <p style="margin-top: 10px">
          没有账号？<router-link to="/register" style="color: blue; text-decoration: underline">立即注册</router-link>
        </p>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { apiUrl } from '@/api/http'

import '@/assets/styles/login.css'

const router = useRouter()
const route = useRoute()
const username = ref('')
const password = ref('')
const remember = ref(false)
const showPwd = ref(false)
const error = ref('')
const loading = ref(false)

onMounted(() => {
  const u = localStorage.getItem('username')
  if (u) {
    username.value = u
    remember.value = true
  }
})

async function submit() {
  error.value = ''
  loading.value = true
  try {
    const r = await fetch(apiUrl('/api/login'), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: username.value, password: password.value }),
    })
    if (r.ok) {
      const d = await r.json()
      localStorage.setItem('token', d.access_token)
      if (remember.value) localStorage.setItem('username', username.value)
      else localStorage.removeItem('username')
      const redir = route.query.redirect
      router.push(typeof redir === 'string' && redir.startsWith('/') ? redir : '/')
    } else {
      let msg = '用户名或密码错误，请重试！'
      try {
        const j = await r.json()
        if (j.detail) msg = typeof j.detail === 'string' ? j.detail : msg
      } catch {
        /* */
      }
      error.value = msg
    }
  } catch {
    error.value = '网络错误，请确认后端服务已启动'
  } finally {
    loading.value = false
  }
}
</script>
