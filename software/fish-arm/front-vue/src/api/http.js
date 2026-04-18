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
