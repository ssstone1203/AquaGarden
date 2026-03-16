// API配置
const API_BASE_URL = 'http://localhost:8000';

// 密码强度检查
function checkPasswordStrength(password) {
    const strengthFill = document.getElementById('strengthFill');
    const strengthText = document.getElementById('strengthText');
    
    if (!password) {
        strengthFill.className = 'strength-fill';
        strengthText.textContent = '';
        return 0;
    }
    
    let strength = 0;
    
    if (password.length >= 6) strength += 1;
    if (password.length >= 10) strength += 1;
    if (/[a-z]/.test(password) && /[A-Z]/.test(password)) strength += 1;
    if (/\d/.test(password)) strength += 1;
    if (/[^a-zA-Z0-9]/.test(password)) strength += 1;
    
    if (strength <= 2) {
        strengthFill.className = 'strength-fill weak';
        strengthText.textContent = '密码强度：弱';
        return 1;
    } else if (strength <= 3) {
        strengthFill.className = 'strength-fill medium';
        strengthText.textContent = '密码强度：中';
        return 2;
    } else {
        strengthFill.className = 'strength-fill strong';
        strengthText.textContent = '密码强度：强';
        return 3;
    }
}

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

const toggleConfirmPassword = document.getElementById('toggleConfirmPassword');
const confirmPasswordInput = document.getElementById('confirmPassword');

if (toggleConfirmPassword && confirmPasswordInput) {
    toggleConfirmPassword.addEventListener('click', () => {
        const type = confirmPasswordInput.getAttribute('type') === 'password' ? 'text' : 'password';
        confirmPasswordInput.setAttribute('type', type);
        
        const icon = toggleConfirmPassword.querySelector('i');
        if (type === 'password') {
            icon.className = 'fas fa-eye';
        } else {
            icon.className = 'fas fa-eye-slash';
        }
    });
}

// 注册表单提交
const registerForm = document.getElementById('registerForm');
const errorMessage = document.getElementById('errorMessage');
const registerBtn = document.getElementById('registerBtn');

if (registerForm) {
    registerForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        const username = document.getElementById('username').value.trim();
        const email = document.getElementById('email').value.trim();
        const password = document.getElementById('password').value;
        const confirmPassword = document.getElementById('confirmPassword').value;
        const agreeTerms = document.getElementById('agreeTerms').checked;
        
        // 清除之前的错误消息
        errorMessage.classList.remove('show');
        errorMessage.textContent = '';
        
        // 验证
        if (!username || username.length < 3) {
            showError('用户名至少需要3个字符');
            return;
        }
        
        if (password.length < 6) {
            showError('密码至少需要6个字符');
            return;
        }
        
        if (password !== confirmPassword) {
            showError('两次输入的密码不一致');
            return;
        }
        
        if (!agreeTerms) {
            showError('请同意服务条款和隐私政策');
            return;
        }
        
        // 禁用按钮，防止重复提交
        registerBtn.disabled = true;
        registerBtn.innerHTML = '<span>注册中...</span><i class="fas fa-spinner fa-spin"></i>';
        
        try {
            // 发送注册请求
            const response = await fetch(`${API_BASE_URL}/api/register`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    username: username,
                    email: email || null,
                    password: password
                })
            });

            const data = await response.json();

            if (response.ok) {
                // 注册成功
                showSuccess('注册成功！正在跳转到登录页面...');
                
                // 延迟跳转
                setTimeout(() => {
                    window.location.href = 'login.html';
                }, 2000);
            } else {
                // 注册失败
                showError(data.detail || '注册失败，请重试');
                registerBtn.disabled = false;
                registerBtn.innerHTML = '<span>注册账号</span><i class="fas fa-user-plus"></i>';
            }
        } catch (error) {
            console.error('注册请求失败:', error);
            showError('网络错误，请检查服务器是否运行');
            registerBtn.disabled = false;
            registerBtn.innerHTML = '<span>注册账号</span><i class="fas fa-user-plus"></i>';
        }
    });
}

// 显示错误消息
function showError(message) {
    errorMessage.textContent = message;
    errorMessage.classList.add('show');
}

// 显示成功消息
function showSuccess(message) {
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
        <span>${message}</span>
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

// 回车键快速提交
document.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        const submitButton = document.querySelector('.register-button');
        if (submitButton && !submitButton.disabled) {
            submitButton.click();
        }
    }
});

// 页面加载动画
document.addEventListener('DOMContentLoaded', () => {
    const registerCard = document.querySelector('.register-card');
    if (registerCard) {
        registerCard.style.opacity = '0';
        registerCard.style.transform = 'translateY(20px)';
        
        setTimeout(() => {
            registerCard.style.transition = 'all 0.6s ease';
            registerCard.style.opacity = '1';
            registerCard.style.transform = 'translateY(0)';
        }, 100);
    }
    
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
