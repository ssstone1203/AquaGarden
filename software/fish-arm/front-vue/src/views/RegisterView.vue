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
  <div class="register-container">
    <div class="register-left">
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
      <div class="register-footer"><p>&copy; 2024 AquaGarden. 保留所有权利.</p></div>
    </div>
    <div class="register-right">
      <div class="register-card">
        <div class="register-header">
          <h2>创建账号</h2>
          <p>注册一个新账户以访问控制系统</p>
        </div>
        <form class="register-form" @submit.prevent="submit">
          <div class="form-group">
            <label for="username"><i class="fas fa-user"></i>用户名</label>
            <div class="input-wrapper">
              <i class="fas fa-user"></i>
              <input id="username" v-model="username" type="text" placeholder="请输入用户名（3-50个字符）" required minlength="3" maxlength="50" />
            </div>
          </div>
          <div class="form-group">
            <label for="email"><i class="fas fa-envelope"></i>邮箱（可选）</label>
            <div class="input-wrapper">
              <i class="fas fa-envelope"></i>
              <input id="email" v-model="email" type="email" placeholder="请输入邮箱地址" />
            </div>
          </div>
          <div class="form-group">
            <label for="password"><i class="fas fa-lock"></i>密码</label>
            <div class="input-wrapper">
              <i class="fas fa-lock"></i>
              <input
                id="password"
                v-model="password"
                :type="showPwd ? 'text' : 'password'"
                placeholder="请输入密码（至少6个字符）"
                required
                minlength="6"
                @input="updateStrength"
              />
              <button type="button" class="toggle-password" @click="showPwd = !showPwd">
                <i :class="showPwd ? 'fas fa-eye-slash' : 'fas fa-eye'"></i>
              </button>
            </div>
            <div class="password-strength">
              <div class="strength-bar">
                <div class="strength-fill" :class="strengthClass"></div>
              </div>
              <div class="strength-text">{{ strengthText }}</div>
            </div>
          </div>
          <div class="form-group">
            <label for="confirmPassword"><i class="fas fa-lock"></i>确认密码</label>
            <div class="input-wrapper">
              <i class="fas fa-lock"></i>
              <input
                id="confirmPassword"
                v-model="confirmPassword"
                :type="showPwd2 ? 'text' : 'password'"
                placeholder="请再次输入密码"
                required
              />
              <button type="button" class="toggle-password" @click="showPwd2 = !showPwd2">
                <i :class="showPwd2 ? 'fas fa-eye-slash' : 'fas fa-eye'"></i>
              </button>
            </div>
          </div>
          <div class="form-options">
            <label class="checkbox-label">
              <input v-model="agree" type="checkbox" required />
              <span>我同意服务条款和隐私政策</span>
            </label>
          </div>
          <div class="error-message" :class="{ show: !!error }">{{ error }}</div>
          <button type="submit" class="register-button" :disabled="loading">
            <span>{{ loading ? '注册中…' : '注册账号' }}</span>
            <i class="fas fa-user-plus"></i>
          </button>
        </form>
        <div class="divider"><span>或</span></div>
        <div class="login-link">已有账号？ <router-link to="/login">立即登录</router-link></div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, ref } from 'vue'
import { useRouter } from 'vue-router'
import { apiUrl } from '@/api/http'

import '@/assets/styles/login.css'
import '@/assets/styles/register-page.css'

const router = useRouter()
const username = ref('')
const email = ref('')
const password = ref('')
const confirmPassword = ref('')
const agree = ref(false)
const showPwd = ref(false)
const showPwd2 = ref(false)
const error = ref('')
const loading = ref(false)
const strengthLevel = ref(0)

const strengthClass = computed(() => {
  if (strengthLevel.value <= 1) return strengthLevel.value ? 'weak' : ''
  if (strengthLevel.value === 2) return 'medium'
  return 'strong'
})

const strengthText = computed(() => {
  if (!password.value) return ''
  if (strengthLevel.value <= 1) return '密码强度：弱'
  if (strengthLevel.value === 2) return '密码强度：中'
  return '密码强度：强'
})

function updateStrength() {
  const p = password.value
  if (!p) {
    strengthLevel.value = 0
    return
  }
  let s = 0
  if (p.length >= 6) s += 1
  if (p.length >= 10) s += 1
  if (/[a-z]/.test(p) && /[A-Z]/.test(p)) s += 1
  if (/\d/.test(p)) s += 1
  if (/[^a-zA-Z0-9]/.test(p)) s += 1
  if (s <= 2) strengthLevel.value = 1
  else if (s <= 3) strengthLevel.value = 2
  else strengthLevel.value = 3
}

async function submit() {
  error.value = ''
  if (username.value.trim().length < 3) {
    error.value = '用户名至少需要3个字符'
    return
  }
  if (password.value.length < 6) {
    error.value = '密码至少需要6个字符'
    return
  }
  if (password.value !== confirmPassword.value) {
    error.value = '两次输入的密码不一致'
    return
  }
  if (!agree.value) {
    error.value = '请同意服务条款和隐私政策'
    return
  }
  loading.value = true
  try {
    const body = {
      username: username.value.trim(),
      password: password.value,
      email: email.value.trim() || null,
    }
    const r = await fetch(apiUrl('/api/register'), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    })
    const data = await r.json().catch(() => ({}))
    if (r.ok) {
      router.push({ name: 'login' })
    } else {
      error.value = data.detail || '注册失败，请重试'
    }
  } catch {
    error.value = '网络错误，请检查服务器是否运行'
  } finally {
    loading.value = false
  }
}
</script>
