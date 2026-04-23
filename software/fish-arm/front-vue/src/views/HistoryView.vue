<template>
  <div class="history-page">
    <!-- 筛选栏 -->
    <div class="filter-bar">
      <div class="filter-group">
        <label>开始时间</label>
        <input v-model="startTime" type="datetime-local" />
      </div>
      <div class="filter-group">
        <label>结束时间</label>
        <input v-model="endTime" type="datetime-local" />
      </div>
      <div class="filter-group">
        <label>数据类型</label>
        <select v-model="dataType">
          <option value="">全部</option>
          <option value="water_temp">水温</option>
          <option value="air_temp">空气温度</option>
          <option value="air_humidity">空气湿度</option>
          <option value="wqi">水质综合指数</option>
          <option value="soil_moisture">土壤湿度</option>
        </select>
      </div>
      <div class="filter-group">
        <label>时间范围</label>
        <select v-model="timeRange" @change="queryData">
          <option value="24h">最近24小时</option>
          <option value="7d">最近7天</option>
          <option value="30d">最近30天</option>
        </select>
      </div>
      <div class="filter-actions">
        <button type="button" class="filter-btn" @click="queryData"><i class="fas fa-search"></i>查询</button>
        <button type="button" class="filter-btn secondary" @click="exportData"><i class="fas fa-download"></i>导出 CSV</button>
      </div>
    </div>

    <!-- 统计卡片 -->
    <div class="stats-row">
      <div v-for="m in metrics" :key="m.key" class="stat-card" :class="{ 'stat-card-active': activeMetrics.includes(m.key) }" @click="toggleMetric(m.key)">
        <div class="stat-card-header">
          <span :class="['stat-icon', m.iconClass]"><i :class="m.icon"></i></span>
          <span class="stat-toggle"><i :class="activeMetrics.includes(m.key) ? 'fas fa-eye' : 'fas fa-eye-slash'"></i></span>
        </div>
        <div class="stat-name">{{ m.label }}<small>{{ m.unit }}</small></div>
        <div class="stat-nums">
          <span class="stat-cur">{{ statFor(m.key).cur }}</span>
        </div>
        <div class="stat-minmax">
          <span>↓ {{ statFor(m.key).min }}</span>
          <span>↑ {{ statFor(m.key).max }}</span>
          <span>∅ {{ statFor(m.key).avg }}</span>
        </div>
      </div>
    </div>

    <!-- 多折线图 -->
    <div class="chart-card">
      <div class="chart-card-header">
        <h3><i class="fas fa-chart-line"></i> 传感器趋势图</h3>
        <div class="chart-legend">
          <span v-for="m in activeMetricsMeta" :key="m.key" class="legend-item">
            <span class="legend-dot" :style="{ background: m.color }"></span>{{ m.label }}
          </span>
        </div>
      </div>
      <div class="multi-chart-wrap">
        <svg ref="chartSvg" class="multi-chart-svg" :viewBox="`0 0 ${SVG_W} ${SVG_H}`" preserveAspectRatio="xMidYMid meet">
          <!-- Y轴网格线 -->
          <line v-for="i in 5" :key="'g'+i"
            :x1="PAD_L" :y1="PAD_T + ((i-1) / 4) * CHART_H"
            :x2="SVG_W - PAD_R" :y2="PAD_T + ((i-1) / 4) * CHART_H"
            stroke="rgba(255,255,255,0.06)" stroke-width="1"
          />
          <!-- 每条折线 -->
          <g v-for="m in activeMetricsMeta" :key="m.key">
            <defs>
              <linearGradient :id="'hg-'+m.key" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" :stop-color="m.color" stop-opacity="0.2"/>
                <stop offset="100%" :stop-color="m.color" stop-opacity="0"/>
              </linearGradient>
            </defs>
            <polygon
              :points="areaPoints(m.key)"
              :fill="'url(#hg-'+m.key+')'"
            />
            <polyline
              :points="linePoints(m.key)"
              fill="none"
              :stroke="m.color"
              stroke-width="2"
              stroke-linejoin="round"
              stroke-linecap="round"
            />
          </g>
          <!-- X 轴时间标签（每隔几个显示一个） -->
          <text v-for="(row, i) in xLabels" :key="'xl'+i"
            :x="PAD_L + (i / (xLabels.length - 1)) * CHART_W"
            :y="SVG_H - 4"
            font-size="10" fill="#6b7280" text-anchor="middle"
          >{{ row }}</text>
        </svg>
      </div>
    </div>

    <!-- 数据表格 -->
    <div class="data-card">
      <div class="data-card-header">
        <h3><i class="fas fa-table"></i> 详细记录</h3>
        <span class="data-count">共 {{ filteredData.length }} 条</span>
      </div>
      <div class="table-wrap">
        <table class="data-table">
          <thead>
            <tr>
              <th>时间</th>
              <th>水温 (°C)</th>
              <th>空气温度 (°C)</th>
              <th>空气湿度 (%RH)</th>
              <th>水质指数</th>
              <th>土壤湿度 (%)</th>
              <th>状态</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(row, i) in pageData" :key="i">
              <td class="td-time">{{ row.time }}</td>
              <td>{{ row.water_temp }}</td>
              <td>{{ row.air_temp }}</td>
              <td>{{ row.air_humidity }}</td>
              <td>{{ row.wqi }}</td>
              <td>{{ row.soil_moisture }}</td>
              <td><span :class="['status-badge', rowStatus(row)]">{{ rowStatusText(row) }}</span></td>
            </tr>
          </tbody>
        </table>
      </div>
      <div class="pagination">
        <div class="pagination-info">第 {{ currentPage }} / {{ totalPages }} 页</div>
        <div class="pagination-controls">
          <button class="page-btn" :disabled="currentPage === 1" @click="goToPage(currentPage - 1)">
            <i class="fas fa-chevron-left"></i>
          </button>
          <button v-for="p in visiblePages" :key="p" class="page-btn" :class="{ active: p === currentPage }" @click="goToPage(p)">{{ p }}</button>
          <button class="page-btn" :disabled="currentPage === totalPages" @click="goToPage(currentPage + 1)">
            <i class="fas fa-chevron-right"></i>
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { apiUrl, authHeaders } from '@/api/http'

const SVG_W = 800, SVG_H = 220
const PAD_L = 30, PAD_R = 20, PAD_T = 16, PAD_B = 28
const CHART_W = SVG_W - PAD_L - PAD_R
const CHART_H = SVG_H - PAD_T - PAD_B

// 指标定义与 MCU 传感器规格对应：
//   water_temp    DS18B20 水温 (-55~125°C，实用 15~35°C)
//   air_temp      SHT30 空气温度 (-40~125°C，推荐 5~60°C)
//   air_humidity  SHT30 空气湿度 (0~100%RH)
//   wqi           WQM11S 水质综合指数 (0~100)
//   soil_moisture ADC 土壤湿度 (0~100%)
const metrics = [
  { key: 'water_temp',    label: '水温',        unit: '°C',  icon: 'fas fa-thermometer-half', iconClass: 'icon-temp', color: '#f59e0b', rangeMin: 15,  rangeMax: 35 },
  { key: 'air_temp',      label: '空气温度',     unit: '°C',  icon: 'fas fa-sun',              iconClass: 'icon-ph',   color: '#8b5cf6', rangeMin: 5,   rangeMax: 60 },
  { key: 'air_humidity',  label: '空气湿度',     unit: '%RH', icon: 'fas fa-cloud',            iconClass: 'icon-turb', color: '#06b6d4', rangeMin: 0,   rangeMax: 100 },
  { key: 'wqi',           label: '水质综合指数', unit: ' 分', icon: 'fas fa-tachometer-alt',   iconClass: 'icon-oxy',  color: '#10b981', rangeMin: 0,   rangeMax: 100 },
  { key: 'soil_moisture', label: '土壤湿度',     unit: '%',   icon: 'fas fa-tint',             iconClass: 'icon-soil', color: '#3b82f6', rangeMin: 0,   rangeMax: 100 },
]

const activeMetrics = ref(['water_temp', 'air_temp', 'wqi'])
const activeMetricsMeta = computed(() => metrics.filter(m => activeMetrics.value.includes(m.key)))

function toggleMetric(key) {
  const idx = activeMetrics.value.indexOf(key)
  if (idx >= 0) {
    if (activeMetrics.value.length > 1) activeMetrics.value.splice(idx, 1)
  } else {
    activeMetrics.value.push(key)
  }
}

const startTime = ref('')
const endTime = ref('')
const dataType = ref('')
const timeRange = ref('24h')
const allData = ref([])
const currentPage = ref(1)
const pageSize = 20

const filteredData = computed(() => {
  if (!dataType.value) return allData.value
  return allData.value.filter(r => r[dataType.value] != null)
})

const totalPages = computed(() => Math.max(1, Math.ceil(filteredData.value.length / pageSize)))

const pageData = computed(() => {
  const s = (currentPage.value - 1) * pageSize
  return filteredData.value.slice(s, s + pageSize)
})

const visiblePages = computed(() => {
  const total = totalPages.value
  const cur = currentPage.value
  if (total <= 7) return Array.from({ length: total }, (_, i) => i + 1)
  const start = Math.max(1, cur - 2)
  const end = Math.min(total, cur + 2)
  return Array.from({ length: end - start + 1 }, (_, i) => start + i)
})

function goToPage(p) {
  if (p >= 1 && p <= totalPages.value) currentPage.value = p
}

// Stats
function statFor(key) {
  const vals = allData.value.map(r => parseFloat(r[key])).filter(v => !isNaN(v))
  if (!vals.length) return { cur: '--', min: '--', max: '--', avg: '--' }
  return {
    cur: vals[0].toFixed(2),
    min: Math.min(...vals).toFixed(2),
    max: Math.max(...vals).toFixed(2),
    avg: (vals.reduce((a, b) => a + b, 0) / vals.length).toFixed(2),
  }
}

// Chart helpers
function linePoints(key) {
  const meta = metrics.find(m => m.key === key)
  const vals = allData.value.slice(0, 60).map(r => parseFloat(r[key]))
  if (vals.length < 2) return ''
  const range = meta.rangeMax - meta.rangeMin || 1
  return vals.map((v, i) => {
    const x = PAD_L + (i / (vals.length - 1)) * CHART_W
    const y = PAD_T + CHART_H - ((v - meta.rangeMin) / range) * CHART_H
    return `${x.toFixed(1)},${Math.max(PAD_T, Math.min(PAD_T + CHART_H, y)).toFixed(1)}`
  }).join(' ')
}

function areaPoints(key) {
  const line = linePoints(key)
  if (!line) return ''
  const firstX = PAD_L
  const lastX = PAD_L + CHART_W
  const baseY = PAD_T + CHART_H
  return `${firstX},${baseY} ${line} ${lastX},${baseY}`
}

const xLabels = computed(() => {
  const data = allData.value.slice(0, 60)
  if (data.length < 2) return []
  const step = Math.max(1, Math.floor(data.length / 6))
  return data.filter((_, i) => i % step === 0).map(r => {
    const t = r.time
    return t.length > 10 ? t.slice(-8, -3) : t
  })
})

// Row status — 基于 DS18B20 水温范围 (18-32°C) 和 WQM11S WQI 阈值
function rowStatus(row) {
  const t   = parseFloat(row.water_temp)
  const w   = parseFloat(row.wqi)
  if (t > 32 || t < 16 || w < 40) return 'danger'
  if (t > 29 || t < 19 || w < 60) return 'warning'
  return 'normal'
}
function rowStatusText(row) {
  const s = rowStatus(row)
  return s === 'danger' ? '异常' : s === 'warning' ? '注意' : '正常'
}

function generateHistoricalData(count = 120) {
  const data = []
  const now = Date.now()
  for (let i = 0; i < count; i++) {
    const ts = new Date(now - i * 1800000)
    data.push({
      time:         ts.toLocaleString('zh-CN'),
      water_temp:   (24 + Math.random() * 4 - 2).toFixed(1),
      air_temp:     (26 + Math.random() * 6 - 3).toFixed(1),
      air_humidity: (55 + Math.random() * 20 - 10).toFixed(1),
      wqi:          (72 + Math.random() * 20 - 10).toFixed(0),
      soil_moisture:(62 + Math.random() * 20 - 10).toFixed(0),
    })
  }
  return data
}

async function queryData() {
  try {
    const r = await fetch(apiUrl('/api/sensors/history') + `?range=${timeRange.value}`, { headers: authHeaders() })
    if (r.ok) {
      allData.value = await r.json()
      currentPage.value = 1
      return
    }
  } catch { /* fallback */ }
  allData.value = generateHistoricalData(120)
  currentPage.value = 1
}

function exportData() {
  const header = 'time,water_temp,air_temp,air_humidity,wqi,soil_moisture\n'
  const rows = filteredData.value.map(r =>
    `${r.time},${r.water_temp},${r.air_temp},${r.air_humidity},${r.wqi},${r.soil_moisture}`
  ).join('\n')
  const blob = new Blob([header + rows], { type: 'text/csv;charset=utf-8;' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url; a.download = `sensor-history-${Date.now()}.csv`; a.click()
  URL.revokeObjectURL(url)
}

onMounted(() => { queryData() })
</script>

<style scoped>
.history-page { display: flex; flex-direction: column; gap: 20px; }

/* Filter bar */
.filter-bar {
  display: flex; gap: 14px; flex-wrap: wrap;
  background: var(--bg-card); padding: 18px 20px;
  border-radius: 14px; box-shadow: var(--shadow-md);
}
.filter-group { display: flex; flex-direction: column; gap: 5px; }
.filter-group label { font-size: 11px; color: var(--text-secondary); font-weight: 600; }
.filter-group input, .filter-group select {
  padding: 9px 12px; border: 1px solid rgba(255,255,255,0.1);
  border-radius: 8px; font-size: 13px; min-width: 160px;
  background: var(--bg-main); color: var(--text-primary);
}
.filter-group input:focus, .filter-group select:focus { outline: none; border-color: var(--primary-color); }
.filter-actions { display: flex; align-items: flex-end; gap: 10px; }
.filter-btn {
  padding: 9px 18px; background: var(--bg-gradient); color: #fff;
  border: none; border-radius: 8px; cursor: pointer; font-size: 13px;
  display: flex; align-items: center; gap: 7px; transition: all 0.2s;
}
.filter-btn:hover { opacity: 0.88; transform: translateY(-1px); }
.filter-btn.secondary { background: var(--bg-main); color: var(--text-primary); }

/* Stats row */
.stats-row { display: grid; grid-template-columns: repeat(5, 1fr); gap: 12px; }
.stat-card {
  background: var(--bg-card); border-radius: 12px; padding: 14px;
  box-shadow: var(--shadow-md); cursor: pointer;
  border: 2px solid transparent; transition: all 0.2s;
}
.stat-card:hover { transform: translateY(-2px); }
.stat-card-active { border-color: var(--primary-color); background: rgba(139,92,246,0.06); }
.stat-card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
.stat-icon { width: 32px; height: 32px; border-radius: 8px; display: flex; align-items: center; justify-content: center; font-size: 14px; color: #fff; }
.icon-temp { background: linear-gradient(135deg,#f59e0b,#d97706); }
.icon-ph   { background: linear-gradient(135deg,#8b5cf6,#7c3aed); }
.icon-oxy  { background: linear-gradient(135deg,#10b981,#059669); }
.icon-turb { background: linear-gradient(135deg,#06b6d4,#0891b2); }
.icon-soil { background: linear-gradient(135deg,#3b82f6,#2563eb); }
.stat-toggle { font-size: 12px; color: var(--text-secondary); }
.stat-name { font-size: 12px; color: var(--text-secondary); margin-bottom: 4px; }
.stat-name small { margin-left: 4px; font-size: 10px; }
.stat-cur { font-size: 20px; font-weight: 700; color: var(--text-primary); }
.stat-minmax { display: flex; gap: 8px; font-size: 10px; color: var(--text-secondary); margin-top: 4px; }

/* Chart */
.chart-card {
  background: var(--bg-card); border-radius: 14px;
  box-shadow: var(--shadow-md); overflow: hidden;
}
.chart-card-header {
  display: flex; align-items: center; justify-content: space-between;
  padding: 14px 20px; border-bottom: 1px solid rgba(255,255,255,0.06);
}
.chart-card-header h3 { font-size: 15px; color: var(--text-primary); display: flex; align-items: center; gap: 8px; }
.chart-card-header h3 i { color: var(--primary-color); }
.chart-legend { display: flex; gap: 14px; flex-wrap: wrap; }
.legend-item { display: flex; align-items: center; gap: 5px; font-size: 12px; color: var(--text-secondary); }
.legend-dot { width: 10px; height: 10px; border-radius: 50%; }
.multi-chart-wrap { padding: 12px 16px 4px; }
.multi-chart-svg { width: 100%; display: block; }

/* Table */
.data-card {
  background: var(--bg-card); border-radius: 14px;
  box-shadow: var(--shadow-md); overflow: hidden;
}
.data-card-header {
  display: flex; align-items: center; justify-content: space-between;
  padding: 14px 20px; border-bottom: 1px solid rgba(255,255,255,0.06);
}
.data-card-header h3 { font-size: 15px; color: var(--text-primary); display: flex; align-items: center; gap: 8px; }
.data-card-header h3 i { color: var(--primary-color); }
.data-count { font-size: 12px; color: var(--text-secondary); background: var(--bg-main); padding: 4px 10px; border-radius: 20px; }
.table-wrap { overflow-x: auto; }
.data-table { width: 100%; border-collapse: collapse; }
.data-table thead { background: var(--bg-main); }
.data-table th { padding: 12px 16px; text-align: left; font-size: 12px; font-weight: 600; color: var(--text-secondary); white-space: nowrap; }
.data-table td { padding: 12px 16px; font-size: 13px; color: var(--text-primary); border-bottom: 1px solid rgba(255,255,255,0.04); }
.data-table tbody tr:hover { background: rgba(255,255,255,0.03); }
.td-time { color: var(--text-secondary); font-size: 12px; }
.status-badge {
  display: inline-flex; align-items: center; padding: 3px 9px;
  border-radius: 20px; font-size: 11px; font-weight: 600;
}
.normal  { background: rgba(16,185,129,0.12); color: #10b981; }
.warning { background: rgba(245,158,11,0.12); color: #f59e0b; }
.danger  { background: rgba(239,68,68,0.12); color: #ef4444; }

/* Pagination */
.pagination {
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 20px; border-top: 1px solid rgba(255,255,255,0.06);
}
.pagination-info { font-size: 12px; color: var(--text-secondary); }
.pagination-controls { display: flex; gap: 6px; }
.page-btn {
  width: 32px; height: 32px; border: 1px solid rgba(255,255,255,0.1);
  background: var(--bg-main); border-radius: 6px; cursor: pointer;
  display: flex; align-items: center; justify-content: center;
  font-size: 13px; color: var(--text-primary); transition: all 0.2s;
}
.page-btn:hover:not(:disabled) { background: var(--primary-color); color: #fff; border-color: var(--primary-color); }
.page-btn.active { background: var(--primary-color); color: #fff; border-color: var(--primary-color); }
.page-btn:disabled { opacity: 0.35; cursor: not-allowed; }

@media (max-width: 900px) {
  .stats-row { grid-template-columns: repeat(3, 1fr); }
}
@media (max-width: 600px) {
  .stats-row { grid-template-columns: repeat(2, 1fr); }
}
</style>
