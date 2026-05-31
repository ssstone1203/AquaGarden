/** 开发环境通过 Vite 代理访问后端；生产可设 VITE_API_BASE 为完整后端地址 */
export function apiUrl(path) {
  const base = import.meta.env.VITE_API_BASE ?? ''
  if (!path.startsWith('/')) {
    path = '/' + path
  }
  return `${base}${path}`
}

export function wsLogsUrl() {
  if (import.meta.env.VITE_WS_BASE) {
    return `${import.meta.env.VITE_WS_BASE.replace(/\/$/, '')}/ws/logs`
  }
  const proto = window.location.protocol === 'https:' ? 'wss' : 'ws'
  if (import.meta.env.DEV) {
    return `${proto}://localhost:8090/ws/logs`
  }
  return `${proto}://${window.location.host}/ws/logs`
}

export function authHeaders() {
  const token = localStorage.getItem('token')
  const h = { 'Content-Type': 'application/json' }
  if (token) {
    h.Authorization = `Bearer ${token}`
  }
  return h
}

export async function apiFetch(path, init = {}) {
  const headers = new Headers(init.headers ?? {})
  const token = localStorage.getItem('token')
  if (token && !headers.has('Authorization')) {
    headers.set('Authorization', `Bearer ${token}`)
  }

  const response = await fetch(apiUrl(path), { ...init, headers })
  if (response.status === 401 && await isJwtAuthFailure(response)) {
    localStorage.removeItem('token')
    const next = `${window.location.pathname}${window.location.search}${window.location.hash}`
    if (!window.location.pathname.startsWith('/login')) {
      window.location.assign(`/login?redirect=${encodeURIComponent(next)}`)
    }
  }
  return response
}

async function isJwtAuthFailure(response) {
  try {
    const body = await response.clone().json()
    return body?.detail === '无效的认证凭据' || body?.detail === '用户不存在'
  } catch {
    return false
  }
}

/**
 * 清除本地凭证并跳转到登录页。
 * 必须在调用 router.push('/login') 之前先调用此函数，否则路由守卫会因
 * localStorage 里的 token 尚未清除而立即把用户弹回 dashboard。
 */
export function logout(router) {
  localStorage.removeItem('token')
  router.push({ name: 'login' })
}
