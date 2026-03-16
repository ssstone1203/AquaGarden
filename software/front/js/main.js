// API配置
const API_BASE_URL = 'http://localhost:8000';
let authToken = localStorage.getItem('token');

// 时间显示更新
function updateTime() {
    const now = new Date();
    const timeString = now.toLocaleTimeString('zh-CN', { 
        hour: '2-digit', 
        minute: '2-digit', 
        second: '2-digit' 
    });
    const timeElement = document.getElementById('timeText');
    if (timeElement) {
        timeElement.textContent = timeString;
    }
}

// 每秒更新时间
setInterval(updateTime, 1000);
updateTime();

// 传感器数据更新
async function updateSensorData() {
    try {
        const response = await fetch(`${API_BASE_URL}/api/sensors`, {
            headers: {
                'Authorization': `Bearer ${authToken}`
            }
        });
        
        if (response.ok) {
            const data = await response.json();
            
            // 更新温度
            document.getElementById('temperature').textContent = data.temperature;
            document.getElementById('tempValue').textContent = data.temperature;
            
            // 更新pH值
            document.getElementById('ph').textContent = data.ph;
            document.getElementById('phValue').textContent = data.ph;
            
            // 更新溶解氧
            document.getElementById('oxygen').textContent = data.oxygen;
            document.getElementById('oxygenValue').textContent = data.oxygen;
            
            // 更新浊度
            document.getElementById('turbidity').textContent = data.turbidity;
            document.getElementById('turbidityValue').textContent = data.turbidity;
            
            addLog('数据更新', `温度: ${data.temperature}°C, pH: ${data.ph}, 溶解氧: ${data.oxygen}mg/L, 浊度: ${data.turbidity}NTU`);
        } else if (response.status === 401) {
            addLog('错误', '登录已过期，请重新登录');
            setTimeout(() => window.location.href = 'login.html', 2000);
        }
    } catch (error) {
        console.error('获取传感器数据失败:', error);
        // 演示模式下的本地模拟
        generateLocalSensorData();
    }
}

// 演示模式下的本地模拟数据
function generateRandomData(min, max, decimals = 1) {
    const value = Math.random() * (max - min) + min;
    return decimals === 0 ? Math.round(value) : parseFloat(value.toFixed(decimals));
}

function generateLocalSensorData() {
    // 更新温度
    const temp = generateRandomData(24, 27);
    document.getElementById('temperature').textContent = temp;
    document.getElementById('tempValue').textContent = temp;
    
    // 更新pH值
    const ph = generateRandomData(6.8, 7.5);
    document.getElementById('ph').textContent = ph;
    document.getElementById('phValue').textContent = ph;
    
    // 更新溶解氧
    const oxygen = generateRandomData(7, 9);
    document.getElementById('oxygen').textContent = oxygen;
    document.getElementById('oxygenValue').textContent = oxygen;
    
    // 更新浊度
    const turbidity = generateRandomData(8, 15, 0);
    document.getElementById('turbidity').textContent = turbidity;
    document.getElementById('turbidityValue').textContent = turbidity;
    
    // 添加日志
    addLog('数据更新', `温度: ${temp}°C, pH: ${ph}, 溶解氧: ${oxygen}mg/L, 浊度: ${turbidity}NTU`);
}

// 初始更新传感器数据
updateSensorData();

// 每5秒更新一次传感器数据
setInterval(updateSensorData, 5000);

// 日志系统
const logContainer = document.getElementById('logContainer');

function addLog(type, message) {
    if (!logContainer) return;
    
    const logEntry = document.createElement('div');
    logEntry.className = 'log-entry log-info';
    
    const now = new Date();
    const timeString = now.toLocaleTimeString('zh-CN');
    
    logEntry.innerHTML = `
        <span class="log-time">[${timeString}]</span>
        <span class="log-message">${type}: ${message}</span>
    `;
    
    logContainer.appendChild(logEntry);
    
    // 保持日志在合理数量
    while (logContainer.children.length > 50) {
        logContainer.removeChild(logContainer.firstChild);
    }
    
    // 自动滚动到底部
    logContainer.scrollTop = logContainer.scrollHeight;
}

// 清空日志
const clearLogBtn = document.getElementById('clearLog');
if (clearLogBtn) {
    clearLogBtn.addEventListener('click', () => {
        if (logContainer) {
            logContainer.innerHTML = `
                <div class="log-entry log-system">
                    <span class="log-time">[系统]</span>
                    <span class="log-message">日志已清空</span>
                </div>
            `;
        }
    });
}

// 模式切换
const modeBtns = document.querySelectorAll('.mode-btn');
modeBtns.forEach(btn => {
    btn.addEventListener('click', async () => {
        modeBtns.forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        const mode = btn.dataset.mode;
        
        // 尝试调用后端API
        try {
            const response = await fetch(`${API_BASE_URL}/api/mode`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${authToken}`
                },
                body: JSON.stringify({ mode: mode })
            });
            
            if (response.ok) {
                addLog('系统', `已切换到${mode === 'demo' ? '演示' : '服务'}模式`);
            } else {
                addLog('系统', `切换到${mode === 'demo' ? '演示' : '服务'}模式`);
            }
        } catch (error) {
            console.error('模式切换请求失败:', error);
            addLog('系统', `切换到${mode === 'demo' ? '演示' : '服务'}模式`);
        }
    });
});

// 机械臂控制
const ctrlBtns = document.querySelectorAll('.ctrl-btn[data-dir]');
let posX = 0, posY = 0, posZ = 0;

ctrlBtns.forEach(btn => {
    btn.addEventListener('click', () => {
        const direction = btn.dataset.dir;
        const step = 1;
        
        switch(direction) {
            case 'up':
                posZ += step;
                break;
            case 'down':
                posZ -= step;
                break;
            case 'left':
                posX -= step;
                break;
            case 'right':
                posX += step;
                break;
            case 'forward':
                posY += step;
                break;
            case 'backward':
                posY -= step;
                break;
        }
        
        // 限制范围
        posX = Math.max(-10, Math.min(10, posX));
        posY = Math.max(-10, Math.min(10, posY));
        posZ = Math.max(0, Math.min(20, posZ));
        
        // 更新显示
        document.getElementById('posX').textContent = posX;
        document.getElementById('posY').textContent = posY;
        document.getElementById('posZ').textContent = posZ;
        
        addLog('机械臂', `移动到位置 X:${posX}, Y:${posY}, Z:${posZ}`);
    });
});

// 居中按钮
const centerBtn = document.querySelector('.ctrl-center');
if (centerBtn) {
    centerBtn.addEventListener('click', () => {
        posX = 0;
        posY = 0;
        posZ = 0;
        document.getElementById('posX').textContent = posX;
        document.getElementById('posY').textContent = posY;
        document.getElementById('posZ').textContent = posZ;
        addLog('机械臂', '返回中心位置');
    });
}

// 登出功能
const logoutBtn = document.getElementById('logoutBtn');
if (logoutBtn) {
    logoutBtn.addEventListener('click', () => {
        if (confirm('确定要退出系统吗？')) {
            addLog('系统', '用户已登出');
            setTimeout(() => {
                window.location.href = 'login.html';
            }, 500);
        }
    });
}

// 模拟摄像头画面更新
function updateCameraFeeds() {
    // 这里可以添加实际的摄像头画面更新逻辑
    // 例如：从服务器获取最新的图片
}

// 图表初始化（需要Chart.js库）
function initChart() {
    const canvas = document.getElementById('sensorChart');
    if (!canvas) return;
    
    // 这里可以使用Chart.js来创建实际的图表
    // 示例代码（需要先引入Chart.js库）
    /*
    const ctx = canvas.getContext('2d');
    const chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: '温度',
                data: [],
                borderColor: 'rgb(239, 68, 68)',
                tension: 0.4
            }, {
                label: 'pH值',
                data: [],
                borderColor: 'rgb(59, 130, 246)',
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: false
                }
            }
        }
    });
    */
}

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', () => {
    addLog('系统', '系统已启动');
    addLog('传感器', '所有传感器已连接');
    addLog('摄像头', '摄像头在线');
    
    // 初始化图表
    initChart();
});

// 键盘控制（可选）
document.addEventListener('keydown', (e) => {
    const ctrlBtn = document.querySelector(`.ctrl-btn[data-dir]`);
    if (!ctrlBtn) return;
    
    switch(e.key) {
        case 'ArrowUp':
            e.preventDefault();
            document.querySelector('.ctrl-btn[data-dir="forward"]')?.click();
            break;
        case 'ArrowDown':
            e.preventDefault();
            document.querySelector('.ctrl-btn[data-dir="backward"]')?.click();
            break;
        case 'ArrowLeft':
            e.preventDefault();
            document.querySelector('.ctrl-btn[data-dir="left"]')?.click();
            break;
        case 'ArrowRight':
            e.preventDefault();
            document.querySelector('.ctrl-btn[data-dir="right"]')?.click();
            break;
        case 'w':
        case 'W':
            document.querySelector('.ctrl-btn[data-dir="up"]')?.click();
            break;
        case 's':
        case 'S':
            document.querySelector('.ctrl-btn[data-dir="down"]')?.click();
            break;
    }
});

// 提示用户键盘控制
setTimeout(() => {
    addLog('提示', '可使用方向键和W/S键控制机械臂');
}, 2000);

// 定期添加一些系统日志
setInterval(() => {
    const messages = [
        '系统运行正常',
        '传感器数据稳定',
        '水质参数正常',
        '自动监控进行中'
    ];
    const randomMessage = messages[Math.floor(Math.random() * messages.length)];
    addLog('状态', randomMessage);
}, 30000); // 每30秒
