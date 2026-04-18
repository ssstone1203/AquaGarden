<template>
  <div class="alerts-page">
    <div class="alert-cards-grid">
      <div class="alert-card">
        <div class="alert-card-header temperature"><i class="fas fa-thermometer-half"></i><h3>温度报警设置</h3></div>
        <div class="range-inputs">
          <div class="input-group"><label>最低温度 (°C)</label><input v-model.number="tempMin" type="number" step="0.1" /></div>
          <div class="input-group"><label>最高温度 (°C)</label><input v-model.number="tempMax" type="number" step="0.1" /></div>
        </div>
        <div class="toggle-row">
          <div class="toggle-label"><i class="fas fa-bell"></i><span>启用温度报警</span></div>
          <div class="toggle-switch" :class="{ active: enTemp }" @click="enTemp = !enTemp"></div>
        </div>
        <button type="button" class="save-btn" @click="saveAlert('temperature')"><i class="fas fa-save"></i>保存设置</button>
      </div>
      <div class="alert-card">
        <div class="alert-card-header ph"><i class="fas fa-flask"></i><h3>pH值报警设置</h3></div>
        <div class="range-inputs">
          <div class="input-group"><label>最低pH值</label><input v-model.number="phMin" type="number" step="0.1" /></div>
          <div class="input-group"><label>最高pH值</label><input v-model.number="phMax" type="number" step="0.1" /></div>
        </div>
        <div class="toggle-row">
          <div class="toggle-label"><i class="fas fa-bell"></i><span>启用pH值报警</span></div>
          <div class="toggle-switch" :class="{ active: enPh }" @click="enPh = !enPh"></div>
        </div>
        <button type="button" class="save-btn" @click="saveAlert('ph')"><i class="fas fa-save"></i>保存设置</button>
      </div>
      <div class="alert-card">
        <div class="alert-card-header oxygen"><i class="fas fa-wind"></i><h3>溶解氧报警设置</h3></div>
        <div class="range-inputs">
          <div class="input-group"><label>最低溶解氧 (mg/L)</label><input v-model.number="oxMin" type="number" step="0.1" /></div>
          <div class="input-group"><label>最高溶解氧 (mg/L)</label><input v-model.number="oxMax" type="number" step="0.1" /></div>
        </div>
        <div class="toggle-row">
          <div class="toggle-label"><i class="fas fa-bell"></i><span>启用溶解氧报警</span></div>
          <div class="toggle-switch" :class="{ active: enOx }" @click="enOx = !enOx"></div>
        </div>
        <button type="button" class="save-btn" @click="saveAlert('oxygen')"><i class="fas fa-save"></i>保存设置</button>
      </div>
      <div class="alert-card">
        <div class="alert-card-header turbidity"><i class="fas fa-eye"></i><h3>浊度报警设置</h3></div>
        <div class="range-inputs">
          <div class="input-group"><label>最低浊度 (NTU)</label><input v-model.number="tuMin" type="number" step="1" /></div>
          <div class="input-group"><label>最高浊度 (NTU)</label><input v-model.number="tuMax" type="number" step="1" /></div>
        </div>
        <div class="toggle-row">
          <div class="toggle-label"><i class="fas fa-bell"></i><span>启用浊度报警</span></div>
          <div class="toggle-switch" :class="{ active: enTu }" @click="enTu = !enTu"></div>
        </div>
        <button type="button" class="save-btn" @click="saveAlert('turbidity')"><i class="fas fa-save"></i>保存设置</button>
      </div>
    </div>
    <div class="notification-section">
      <h3><i class="fas fa-paper-plane"></i> 通知设置</h3>
      <div class="notification-options">
        <div class="notification-item">
          <div class="notification-info"><i class="fas fa-envelope"></i><span>邮件通知</span></div>
          <div class="toggle-switch" :class="{ active: nMail }" @click="nMail = !nMail"></div>
        </div>
        <div class="notification-item">
          <div class="notification-info"><i class="fas fa-sms"></i><span>短信通知</span></div>
          <div class="toggle-switch" :class="{ active: nSms }" @click="nSms = !nSms"></div>
        </div>
        <div class="notification-item">
          <div class="notification-info"><i class="fas fa-bell"></i><span>推送通知</span></div>
          <div class="toggle-switch" :class="{ active: nPush }" @click="nPush = !nPush"></div>
        </div>
      </div>
      <div class="contact-inputs">
        <div class="input-group"><label>通知邮箱</label><input v-model="mailAddr" type="email" placeholder="请输入邮箱地址" /></div>
        <div class="input-group"><label>通知手机</label><input v-model="phone" type="tel" placeholder="请输入手机号码" /></div>
      </div>
      <button type="button" class="save-btn" @click="saveNotify"><i class="fas fa-save"></i>保存通知设置</button>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'

import '@/assets/styles/alerts-page.css'

const tempMin = ref(22)
const tempMax = ref(28)
const phMin = ref(6.5)
const phMax = ref(7.5)
const oxMin = ref(6.0)
const oxMax = ref(10.0)
const tuMin = ref(5)
const tuMax = ref(20)
const enTemp = ref(true)
const enPh = ref(true)
const enOx = ref(true)
const enTu = ref(true)
const nMail = ref(true)
const nSms = ref(false)
const nPush = ref(true)
const mailAddr = ref('admin@aquagarden.com')
const phone = ref('+86 138 0000 0000')

const names = { temperature: '温度', ph: 'pH值', oxygen: '溶解氧', turbidity: '浊度' }

function saveAlert(type) {
  window.alert(`${names[type]}报警设置已保存！`)
}

function saveNotify() {
  window.alert('通知设置已保存！')
}
</script>
