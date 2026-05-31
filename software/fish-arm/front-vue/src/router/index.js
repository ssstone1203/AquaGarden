import { createRouter, createWebHistory } from 'vue-router'
import AppLayout from '@/components/AppLayout.vue'
import DashboardView from '@/views/DashboardView.vue'
import LoginView from '@/views/LoginView.vue'
import RegisterView from '@/views/RegisterView.vue'
import HistoryView from '@/views/HistoryView.vue'
import RobotView from '@/views/RobotView.vue'

const routes = [
  { path: '/login', name: 'login', component: LoginView, meta: { guest: true } },
  { path: '/register', name: 'register', component: RegisterView, meta: { guest: true } },
  {
    path: '/',
    component: AppLayout,
    meta: { requiresAuth: true },
    children: [
      { path: '', name: 'dashboard', component: DashboardView, meta: { nav: 'dashboard', title: '控制面板', subtitle: '实时监控您的水族箱状态', keepAlive: true } },
      { path: 'history', name: 'history', component: HistoryView, meta: { nav: 'history', title: '历史数据', subtitle: '查看和分析历史监控数据' } },
      { path: 'robot', name: 'robot', component: RobotView, meta: { nav: 'robot', title: '系统控制', subtitle: '机械臂任务、滑轨、水泵与终端状态' } },
    ],
  },
]

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes,
})

router.beforeEach((to, _from, next) => {
  const token = localStorage.getItem('token')
  if (to.matched.some((r) => r.meta.requiresAuth) && !token) {
    next({ name: 'login', query: { redirect: to.fullPath } })
    return
  }
  if (to.meta.guest && token) {
    next({ name: 'dashboard' })
    return
  }
  next()
})

export default router
