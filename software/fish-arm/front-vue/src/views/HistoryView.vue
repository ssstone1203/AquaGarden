<template>
  <div class="history-page">
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
          <option value="temperature">温度</option>
          <option value="ph">pH值</option>
          <option value="oxygen">溶解氧</option>
          <option value="turbidity">浊度</option>
        </select>
      </div>
      <div class="filter-actions">
        <button type="button" class="filter-btn" @click="queryData"><i class="fas fa-search"></i>查询</button>
        <button type="button" class="filter-btn secondary" @click="exportData"><i class="fas fa-download"></i>导出</button>
      </div>
    </div>
    <div class="data-card">
      <table class="data-table">
        <thead>
          <tr>
            <th>时间</th>
            <th>水温 (°C)</th>
            <th>pH值</th>
            <th>溶解氧 (mg/L)</th>
            <th>浊度 (NTU)</th>
            <th>状态</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, i) in pageData" :key="i">
            <td>{{ row.time }}</td>
            <td>{{ row.temperature }}</td>
            <td>{{ row.ph }}</td>
            <td>{{ row.oxygen }}</td>
            <td>{{ row.turbidity }}</td>
            <td><span class="status-badge normal">正常</span></td>
          </tr>
        </tbody>
      </table>
      <div class="pagination">
        <div class="pagination-info">
          共 <span>{{ allData.length }}</span> 条记录，当前第 <span>{{ currentPage }}</span> 页
        </div>
        <div class="pagination-controls">
          <button
            v-for="p in pageButtons"
            :key="p"
            type="button"
            class="page-btn"
            :class="{ active: p === currentPage }"
            @click="goToPage(p)"
          >
            {{ p }}
          </button>
        </div>
      </div>
    </div>
    <div class="chart-section">
      <div class="chart-header">
        <h3><i class="fas fa-chart-area"></i> 数据趋势分析</h3>
        <div class="filter-group" style="flex-direction: row; gap: 12px">
          <select v-model="timeRange" style="padding: 8px 12px">
            <option value="24h">最近24小时</option>
            <option value="7d">最近7天</option>
            <option value="30d">最近30天</option>
          </select>
        </div>
      </div>
      <div class="chart-container">
        <div
          v-for="(row, idx) in chartData"
          :key="idx"
          class="chart-bar"
          :style="{ height: barHeight(row) + '%' }"
          :title="`${row.time}: ${row.temperature}°C`"
        ></div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'

import '@/assets/styles/history-page.css'

const startTime = ref('')
const endTime = ref('')
const dataType = ref('')
const timeRange = ref('24h')
const allData = ref([])
const currentPage = ref(1)
const pageSize = 20

const pageData = computed(() => {
  const start = (currentPage.value - 1) * pageSize
  return allData.value.slice(start, start + pageSize)
})

const totalPages = computed(() => Math.max(1, Math.ceil(allData.value.length / pageSize)))

const pageButtons = computed(() => {
  const n = Math.min(totalPages.value, 5)
  return Array.from({ length: n }, (_, i) => i + 1)
})

const chartData = computed(() => allData.value.slice(0, 24))

function generateHistoricalData(count = 100) {
  const data = []
  const now = Date.now()
  for (let i = 0; i < count; i++) {
    const timestamp = new Date(now - i * 3600000)
    data.push({
      time: timestamp.toLocaleString('zh-CN'),
      temperature: (25 + Math.random() * 3).toFixed(1),
      ph: (6.8 + Math.random() * 0.5).toFixed(1),
      oxygen: (7.5 + Math.random() * 1.5).toFixed(1),
      turbidity: Math.floor(10 + Math.random() * 10),
    })
  }
  return data
}

function goToPage(p) {
  currentPage.value = p
}

function queryData() {
  void startTime.value
  void endTime.value
  void dataType.value
  allData.value = generateHistoricalData(100)
  currentPage.value = 1
}

function exportData() {
  window.alert('数据导出功能')
}

function barHeight(row) {
  const chartSlice = chartData.value
  const maxVal = Math.max(...chartSlice.map((d) => parseFloat(d.temperature)), 1)
  return (parseFloat(row.temperature) / maxVal) * 100
}

onMounted(() => {
  allData.value = generateHistoricalData(100)
})
</script>
