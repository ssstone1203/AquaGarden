// 密码显示/隐藏切换
const togglePassword = document.getElementById('togglePassword');
const passwordInput = document.getElementById('password');

if (togglePassword && passwordInput) {
    togglePassword.addEventListener('click', () => {
        const type = passwordInput.getAttribute('type') === 'password' ? 'text' : 'password';
        passwordInput.setAttribute('type', type);
        
        const icon = togglePassword.querySelector('i');
        if (type === 'password') {
            icon.className = 'fas fa-eye';
        } else {
            icon.className = 'fas fa-eye-slash';
        }
    });
}

// 登录表单提交
const loginForm = document.getElementById('loginForm');
const errorMessage = document.getElementById('errorMessage');

// API基础URL
const API_BASE_URL = 'http://localhost:8090';

if (loginForm) {
    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;
        const remember = document.getElementById('remember').checked;
        
        // 清除之前的错误消息
        errorMessage.classList.remove('show');
        errorMessage.textContent = '';
        
        try {
            // 尝试向后端API发送登录请求
            const response = await fetch(`${API_BASE_URL}/api/login`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    username: username,
                    password: password
                })
            });

            if (response.ok) {
                const data = await response.json();
                
                // 保存token到localStorage
                localStorage.setItem('token', data.access_token);
                
                // 如果选择了"记住我"，保存到localStorage
                if (remember) {
                    localStorage.setItem('username', username);
                }
                
                // 登录成功
                showSuccess();
                
                // 延迟跳转，让用户看到成功提示
                setTimeout(() => {
                    window.location.href = '/index.html';
                }, 1000);
            } else {
                // 登录失败
                const errorData = await response.json();
                showError(errorData.detail || '用户名或密码错误，请重试！');
            }
        } catch (error) {
            console.error('登录请求失败:', error);
            // 如果请求失败，使用本地验证（演示模式）
            if (username === 'admin' && password === 'admin123') {
                // 登录成功
                showSuccess();
                
                // 如果选择了"记住我"，保存到localStorage
                if (remember) {
                    localStorage.setItem('username', username);
                }
                
                // 延迟跳转，让用户看到成功提示
                setTimeout(() => {
                    window.location.href = '/index.html';
                }, 1000);
            } else {
                // 登录失败
                showError('用户名或密码错误，请重试！');
            }
        }
    });
}

// 显示错误消息
function showError(message) {
    errorMessage.textContent = message;
    errorMessage.classList.add('show');
}

// 显示成功消息
function showSuccess() {
    // 创建成功提示
    const successDiv = document.createElement('div');
    successDiv.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        background: linear-gradient(135deg, #10b981, #059669);
        color: white;
        padding: 16px 24px;
        border-radius: 10px;
        box-shadow: 0 10px 30px rgba(16, 185, 129, 0.3);
        display: flex;
        align-items: center;
        gap: 12px;
        font-size: 14px;
        font-weight: 500;
        z-index: 9999;
        animation: slideIn 0.3s ease;
    `;
    
    successDiv.innerHTML = `
        <i class="fas fa-check-circle" style="font-size: 20px;"></i>
        <span>登录成功！正在跳转...</span>
    `;
    
    document.body.appendChild(successDiv);
    
    // 3秒后移除提示
    setTimeout(() => {
        successDiv.style.animation = 'slideOut 0.3s ease';
        setTimeout(() => {
            document.body.removeChild(successDiv);
        }, 300);
    }, 2500);
}

// 添加CSS动画
const style = document.createElement('style');
style.textContent = `
    @keyframes slideIn {
        from {
            transform: translateX(100%);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }
    
    @keyframes slideOut {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(100%);
            opacity: 0;
        }
    }
`;
document.head.appendChild(style);

// 检查是否有保存的用户名
window.addEventListener('load', () => {
    const savedUsername = localStorage.getItem('username');
    if (savedUsername) {
        document.getElementById('username').value = savedUsername;
        document.getElementById('remember').checked = true;
    }
});

// 输入框焦点效果
const inputs = document.querySelectorAll('input');
inputs.forEach(input => {
    input.addEventListener('focus', () => {
        input.parentElement.style.transform = 'scale(1.01)';
    });
    
    input.addEventListener('blur', () => {
        input.parentElement.style.transform = 'scale(1)';
    });
});

// 回车键快速登录
document.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        const submitButton = document.querySelector('.login-button');
        if (submitButton) {
            submitButton.click();
        }
    }
});

// 添加一些动画效果
document.addEventListener('DOMContentLoaded', () => {
    // 淡入效果
    const loginCard = document.querySelector('.login-card');
    if (loginCard) {
        loginCard.style.opacity = '0';
        loginCard.style.transform = 'translateY(20px)';
        
        setTimeout(() => {
            loginCard.style.transition = 'all 0.6s ease';
            loginCard.style.opacity = '1';
            loginCard.style.transform = 'translateY(0)';
        }, 100);
    }
    
    // 左侧内容动画
    const featureItems = document.querySelectorAll('.feature-item');
    featureItems.forEach((item, index) => {
        item.style.opacity = '0';
        item.style.transform = 'translateX(-20px)';
        
        setTimeout(() => {
            item.style.transition = 'all 0.6s ease';
            item.style.opacity = '1';
            item.style.transform = 'translateX(0)';
        }, 200 + (index * 100));
    });
});

// 防止表单自动填充时的样式问题
window.addEventListener('load', () => {
    const inputs = document.querySelectorAll('input');
    inputs.forEach(input => {
        if (input.value) {
            input.style.background = 'white';
        }
    });
});
