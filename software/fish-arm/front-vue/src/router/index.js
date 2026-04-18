import { createRouter, createWebHistory } from 'vue-router'
import AppLayout from '@/components/AppLayout.vue'
import DashboardView from '@/views/DashboardView.vue'
import LoginView from '@/views/LoginView.vue'
import RegisterView from '@/views/RegisterView.vue'
import CamerasView from '@/views/CamerasView.vue'
import HistoryView from '@/views/HistoryView.vue'
import RobotView from '@/views/RobotView.vue'
import AlertsView from '@/views/AlertsView.vue'
import SettingsView from '@/views/SettingsView.vue'

const routes = [
  { path: '/login', name: 'login', component: LoginView, meta: { guest: true } },
  { path: '/register', name: 'register', component: RegisterView, meta: { guest: true } },
  {
    path: '/',
    component: AppLayout,
    meta: { requiresAuth: true },
    children: [
      { path: '', name: 'dashboard', component: DashboardView, meta: { nav: 'dashboard', title: '控制面板', subtitle: '实时监控您的水族箱状态' } },
      { path: 'cameras', name: 'cameras', component: CamerasView, meta: { nav: 'cameras', title: '视频监控', subtitle: '实时查看水族箱内部情况' } },
      { path: 'history', name: 'history', component: HistoryView, meta: { nav: 'history', title: '历史数据', subtitle: '查看和分析历史监控数据' } },
      { path: 'robot', name: 'robot', component: RobotView, meta: { nav: 'robot', title: '机械臂控制', subtitle: '实时控制机械臂进行各种操作' } },
      { path: 'alerts', name: 'alerts', component: AlertsView, meta: { nav: 'alerts', title: '警报设置', subtitle: '配置传感器阈值和报警通知' } },
      { path: 'settings', name: 'settings', component: SettingsView, meta: { nav: 'settings', title: '系统设置', subtitle: '管理系统配置和设备参数' } },
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
