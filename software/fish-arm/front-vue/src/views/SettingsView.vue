<template>
  <div class="settings-page">
    <div class="settings-section">
      <h3><i class="fas fa-desktop"></i> 系统信息</h3>
      <div class="info-cards-grid">
        <div class="info-card"><i class="fas fa-code"></i><h4>系统版本</h4><p>v2.1.0</p></div>
        <div class="info-card"><i class="fas fa-calendar-alt"></i><h4>最后更新</h4><p>2025-03-16</p></div>
        <div class="info-card"><i class="fas fa-clock"></i><h4>运行时间</h4><p>{{ uptime }}</p></div>
        <div class="info-card"><i class="fas fa-microchip"></i><h4>设备ID</h4><p>AG-2025-001</p></div>
      </div>
    </div>
    <div class="settings-section">
      <h3><i class="fas fa-wifi"></i> 网络设置</h3>
      <div class="settings-grid">
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-network-wired"></i>
            <div class="setting-text"><h4>Wi-Fi连接</h4><p>当前网络连接状态</p></div>
          </div>
          <span style="color: var(--status-success); font-weight: 600">已连接</span>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-globe"></i>
            <div class="setting-text"><h4>IP地址</h4><p>设备局域网IP地址</p></div>
          </div>
          <span style="font-family: monospace">192.168.1.100</span>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-cloud-upload-alt"></i>
            <div class="setting-text"><h4>远程访问</h4><p>启用远程控制功能</p></div>
          </div>
          <div class="toggle-switch" :class="{ active: remote }" @click="remote = !remote"></div>
        </div>
      </div>
    </div>
    <div class="settings-section">
      <h3><i class="fas fa-cogs"></i> 设备管理</h3>
      <div class="settings-grid">
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-thermometer"></i>
            <div class="setting-text"><h4>传感器校准</h4><p>对传感器进行校准</p></div>
          </div>
          <button type="button" class="setting-btn" @click="calibrateSensors">校准</button>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-robot"></i>
            <div class="setting-text"><h4>机械臂校准</h4><p>校准机械臂位置</p></div>
          </div>
          <button type="button" class="setting-btn" @click="calibrateRobot">校准</button>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-camera"></i>
            <div class="setting-text"><h4>摄像头设置</h4><p>调整摄像头参数</p></div>
          </div>
          <button type="button" class="setting-btn" @click="openCameraSettings">设置</button>
        </div>
      </div>
    </div>
    <div class="settings-section">
      <h3><i class="fas fa-database"></i> 数据管理</h3>
      <div class="settings-grid">
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-hdd"></i>
            <div class="setting-text"><h4>数据存储</h4><p>已使用 / 总容量</p></div>
          </div>
          <span>12.5 GB / 64 GB</span>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-save"></i>
            <div class="setting-text"><h4>自动备份</h4><p>每天自动备份数据</p></div>
          </div>
          <div class="toggle-switch" :class="{ active: autoBackup }" @click="autoBackup = !autoBackup"></div>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-file-export"></i>
            <div class="setting-text"><h4>数据导出</h4><p>导出历史数据</p></div>
          </div>
          <button type="button" class="setting-btn" @click="exportData">导出</button>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-trash-alt"></i>
            <div class="setting-text"><h4>清除缓存</h4><p>清除临时数据文件</p></div>
          </div>
          <button type="button" class="setting-btn" @click="clearCache">清除</button>
        </div>
      </div>
      <div class="storage-bar"><div class="storage-fill" style="width: 20%"></div></div>
    </div>
    <div class="settings-section">
      <h3><i class="fas fa-shield-alt"></i> 安全设置</h3>
      <div class="settings-grid">
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-key"></i>
            <div class="setting-text"><h4>修改密码</h4><p>更改登录密码</p></div>
          </div>
          <button type="button" class="setting-btn" @click="changePassword">修改</button>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-mobile-alt"></i>
            <div class="setting-text"><h4>双因素认证</h4><p>启用两步验证</p></div>
          </div>
          <div class="toggle-switch" :class="{ active: twoFa }" @click="twoFa = !twoFa"></div>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-clipboard-list"></i>
            <div class="setting-text"><h4>操作日志</h4><p>记录所有操作</p></div>
          </div>
          <div class="toggle-switch" :class="{ active: opLog }" @click="opLog = !opLog"></div>
        </div>
      </div>
    </div>
    <div class="settings-section">
      <h3><i class="fas fa-power-off"></i> 系统操作</h3>
      <div class="settings-grid">
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-sync"></i>
            <div class="setting-text"><h4>重启系统</h4><p>重新启动控制系统</p></div>
          </div>
          <button type="button" class="setting-btn" @click="restartSystem">重启</button>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-history"></i>
            <div class="setting-text"><h4>恢复出厂设置</h4><p>清除所有设置和数据</p></div>
          </div>
          <button type="button" class="setting-btn danger" @click="resetFactory">恢复</button>
        </div>
      </div>
    </div>
    <div class="settings-section">
      <h3><i class="fas fa-life-ring"></i> 技术支持</h3>
      <div class="settings-grid">
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-book"></i>
            <div class="setting-text"><h4>在线帮助</h4><p>查看用户手册和常见问题</p></div>
          </div>
          <button type="button" class="setting-btn" @click="openHelp">查看</button>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-envelope"></i>
            <div class="setting-text"><h4>联系支持</h4><p>support@aquagarden.com</p></div>
          </div>
          <button type="button" class="setting-btn" @click="contactSupport">联系</button>
        </div>
        <div class="setting-row">
          <div class="setting-info">
            <i class="fas fa-download"></i>
            <div class="setting-text"><h4>检查更新</h4><p>查找系统更新</p></div>
          </div>
          <button type="button" class="setting-btn" @click="checkUpdate">检查</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'

import '@/assets/styles/settings-page.css'

const uptime = ref('15天 7小时')
const remote = ref(true)
const autoBackup = ref(true)
const twoFa = ref(false)
const opLog = ref(true)

function calibrateSensors() {
  window.alert('传感器校准')
}
function calibrateRobot() {
  window.alert('机械臂校准')
}
function openCameraSettings() {
  window.alert('摄像头设置')
}
function exportData() {
  window.alert('数据导出')
}
function clearCache() {
  window.alert('缓存已清除')
}
function changePassword() {
  window.alert('修改密码')
}
function restartSystem() {
  window.alert('重启系统')
}
function resetFactory() {
  window.alert('恢复出厂设置')
}
function openHelp() {
  window.alert('在线帮助')
}
function contactSupport() {
  window.alert('联系支持')
}
function checkUpdate() {
  window.alert('检查更新')
}
</script>
