/**
 * AquaGarden API 服务层
 * 统一管理所有后端 API 调用，包括认证 Token 自动注入和刷新逻辑
 *
 * 后端基础地址: http://localhost:8000
 * API 前缀: /api/v1
 */

// ── 配置 ─────────────────────────────────────────────────────────────────────
const API_BASE = 'http://localhost:8000/api/v1';

// ── Token 管理 ───────────────────────────────────────────────────────────────
const tokenManager = {
    getAccessToken: () => localStorage.getItem('ag_access_token'),
    getRefreshToken: () => localStorage.getItem('ag_refresh_token'),

    setTokens: (accessToken, refreshToken) => {
        localStorage.setItem('ag_access_token', accessToken);
        localStorage.setItem('ag_refresh_token', refreshToken);
    },

    clearTokens: () => {
        localStorage.removeItem('ag_access_token');
        localStorage.removeItem('ag_refresh_token');
        localStorage.removeItem('ag_user_info');
    },

    getUserInfo: () => {
        const info = localStorage.getItem('ag_user_info');
        return info ? JSON.parse(info) : null;
    },

    setUserInfo: (user) => {
        localStorage.setItem('ag_user_info', JSON.stringify(user));
    },

    isLoggedIn: () => !!localStorage.getItem('ag_access_token'),
};

// ── Token 刷新 ───────────────────────────────────────────────────────────────
let isRefreshing = false;
let refreshSubscribers = [];

function subscribeTokenRefresh(callback) {
    refreshSubscribers.push(callback);
}

function onTokenRefreshed(newAccessToken) {
    refreshSubscribers.forEach(cb => cb(newAccessToken));
    refreshSubscribers = [];
}

async function tryRefreshToken() {
    if (isRefreshing) return false;

    const refreshToken = tokenManager.getRefreshToken();
    if (!refreshToken) return false;

    isRefreshing = true;

    try {
        const response = await fetch(`${API_BASE}/auth/refresh`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ refresh_token: refreshToken }),
        });

        if (!response.ok) {
            tokenManager.clearTokens();
            return false;
        }

        const data = await response.json();
        tokenManager.setTokens(data.access_token, data.refresh_token);
        onTokenRefreshed(data.access_token);
        return true;
    } catch (err) {
        console.error('Token 刷新失败:', err);
        tokenManager.clearTokens();
        return false;
    } finally {
        isRefreshing = false;
    }
}

// ── 统一请求方法 ─────────────────────────────────────────────────────────────
async function apiRequest(url, options = {}) {
    const { method = 'GET', body = null, headers = {}, isFormData = false } = options;

    // 构造 headers
    const requestHeaders = { ...headers };
    if (!isFormData) {
        requestHeaders['Content-Type'] = 'application/json';
    }
    const accessToken = tokenManager.getAccessToken();
    if (accessToken) {
        requestHeaders['Authorization'] = `Bearer ${accessToken}`;
    }

    const fetchOptions = {
        method,
        headers: requestHeaders,
    };

    if (body) {
        fetchOptions.body = isFormData ? body : JSON.stringify(body);
    }

    const response = await fetch(`${API_BASE}${url}`, fetchOptions);

    // 401: Token 过期，尝试刷新
    if (response.status === 401) {
        const refreshed = await tryRefreshToken();
        if (refreshed) {
            // 重试原请求
            requestHeaders['Authorization'] = `Bearer ${tokenManager.getAccessToken()}`;
            const retryResponse = await fetch(`${API_BASE}${url}`, {
                method,
                headers: requestHeaders,
                body: body ? (isFormData ? body : JSON.stringify(body)) : undefined,
            });

            if (!retryResponse.ok) {
                const errData = await retryResponse.json().catch(() => ({}));
                throw new ApiError(retryResponse.status, errData.detail || '请求失败', retryResponse.status);
            }

            return retryResponse.json();
        } else {
            // 刷新失败，跳转登录
            window.location.href = 'login.html';
            throw new ApiError(401, '登录已过期，请重新登录', 401);
        }
    }

    if (!response.ok) {
        const errData = await response.json().catch(() => ({}));
        throw new ApiError(response.status, errData.detail || '请求失败', response.status);
    }

    return response.json();
}

// ── 自定义错误类 ─────────────────────────────────────────────────────────────
class ApiError extends Error {
    constructor(status, message, httpStatus) {
        super(message);
        this.name = 'ApiError';
        this.status = status;
        this.httpStatus = httpStatus;
    }
}

// ── 认证 API ─────────────────────────────────────────────────────────────────
const authAPI = {
    /**
     * 用户登录
     * POST /api/v1/auth/login
     * @param {string} username
     * @param {string} password
     * @returns {Promise<{access_token, refresh_token, token_type, expires_in}>}
     */
    login: async (username, password) => {
        const data = await apiRequest('/auth/login', {
            method: 'POST',
            body: { username, password },
        });
        tokenManager.setTokens(data.access_token, data.refresh_token);
        // 获取用户信息
        try {
            const user = await authAPI.me();
            tokenManager.setUserInfo(user);
        } catch (e) { /* ignore */ }
        return data;
    },

    /**
     * 用户注册
     * POST /api/v1/auth/register
     * @param {string} username
     * @param {string} password
     * @param {string} [email]
     * @returns {Promise<User>}
     */
    register: async (username, password, email = null) => {
        return apiRequest('/auth/register', {
            method: 'POST',
            body: { username, password, email },
        });
    },

    /**
     * 退出登录
     * POST /api/v1/auth/logout
     */
    logout: async () => {
        try {
            await apiRequest('/auth/logout', { method: 'POST' });
        } catch (e) { /* ignore */ }
        tokenManager.clearTokens();
    },

    /**
     * 刷新 Token
     * POST /api/v1/auth/refresh
     * @param {string} refreshToken
     * @returns {Promise<{access_token, refresh_token, token_type, expires_in}>}
     */
    refresh: async (refreshToken) => {
        return apiRequest('/auth/refresh', {
            method: 'POST',
            body: { refresh_token: refreshToken },
        });
    },

    /**
     * 获取当前用户信息
     * GET /api/v1/auth/me
     * @returns {Promise<User>}
     */
    me: async () => {
        return apiRequest('/auth/me');
    },

    /**
     * 修改密码
     * POST /api/v1/auth/password/change
     * @param {string} oldPassword
     * @param {string} newPassword
     */
    changePassword: async (oldPassword, newPassword) => {
        return apiRequest('/auth/password/change', {
            method: 'POST',
            body: { old_password: oldPassword, new_password: newPassword },
        });
    },
};

// ── 机械臂 API ───────────────────────────────────────────────────────────────
const armAPI = {
    /**
     * 连接机械臂
     * POST /api/v1/arm/connect
     * @returns {Promise<{connected, port, baudrate}>}
     */
    connect: async () => {
        return apiRequest('/arm/connect', { method: 'POST' });
    },

    /**
     * 断开机械臂连接
     * POST /api/v1/arm/disconnect
     * @returns {Promise<{message}>}
     */
    disconnect: async () => {
        return apiRequest('/arm/disconnect', { method: 'POST' });
    },

    /**
     * 移动机械臂到目标坐标（IK 求解后驱动）
     * POST /api/v1/arm/command
     * @param {number} x - X 坐标 (cm)
     * @param {number} y - Y 坐标 (cm)
     * @param {number} z - Z 坐标 (cm)
     * @param {number} pitch - 俯仰角 (°)
     * @param {number} [minPitch] - 最小俯仰角 (°)，默认 -90
     * @param {number} [maxPitch] - 最大俯仰角 (°)，默认 90
     * @param {number} [duration] - 运动时间 (ms)，默认 1000
     * @returns {Promise<SerialCommandResponse>}
     */
    move: async (x, y, z, pitch, minPitch = -90.0, maxPitch = 90.0, duration = 1000) => {
        return apiRequest('/arm/command', {
            method: 'POST',
            body: {
                command: 'MOVE',
                params: { x, y, z, pitch, min_pitch: minPitch, max_pitch: maxPitch, duration },
            },
        });
    },

    /**
     * 获取机械臂实时状态
     * GET /api/v1/arm/status
     * @returns {Promise<ArmStatusResponse>}
     */
    getStatus: async () => {
        return apiRequest('/arm/status');
    },

    /**
     * 发送串口命令
     * POST /api/v1/arm/command
     * @param {string} command - PING | RESET | UNLOAD | READ_POS | MOVE | GRIPPER_OPEN | GRIPPER_CLOSE | DIST
     * @param {object} [params] - 命令参数，如 {x, y, z, pitch}
     * @returns {Promise<SerialCommandResponse>}
     */
    sendCommand: async (command, params = null) => {
        return apiRequest('/arm/command', {
            method: 'POST',
            body: { command, params },
        });
    },

    /**
     * 获取所有标定数据
     * GET /api/v1/arm/calibrations
     * @returns {Promise<CalibrationResponse[]>}
     */
    getCalibrations: async () => {
        return apiRequest('/arm/calibrations');
    },

    /**
     * 保存标定数据
     * POST /api/v1/arm/calibrations
     * @param {CalibrationSave} data
     * @returns {Promise<CalibrationResponse>}
     */
    saveCalibration: async (data) => {
        return apiRequest('/arm/calibrations', {
            method: 'POST',
            body: data,
        });
    },

    /**
     * 加载指定标定
     * POST /api/v1/arm/calibrations/{name}/load
     * @param {string} name
     * @returns {Promise<{message, name}>}
     */
    loadCalibration: async (name) => {
        return apiRequest(`/arm/calibrations/${encodeURIComponent(name)}/load`, {
            method: 'POST',
        });
    },

    /**
     * 上传图像检测颜色
     * POST /api/v1/arm/detect
     * @param {File} file - JPEG/PNG 图像文件
     * @param {string[]} [colors] - 颜色列表，默认 ['red','green','blue']
     * @returns {Promise<ColorDetectionResponse>}
     */
    detectColors: async (file, colors = ['red', 'green', 'blue']) => {
        const form = new FormData();
        form.append('file', file);
        const colorsParam = colors.join(',');

        const response = await fetch(`${API_BASE}/arm/detect?colors=${colorsParam}`, {
            method: 'POST',
            headers: {
                'Authorization': `Bearer ${tokenManager.getAccessToken()}`,
            },
            body: form,
        });

        if (response.status === 401) {
            const refreshed = await tryRefreshToken();
            if (refreshed) {
                const retryResponse = await fetch(`${API_BASE}/arm/detect?colors=${colorsParam}`, {
                    method: 'POST',
                    headers: {
                        'Authorization': `Bearer ${tokenManager.getAccessToken()}`,
                    },
                    body: form,
                });
                if (!retryResponse.ok) {
                    const errData = await retryResponse.json().catch(() => ({}));
                    throw new ApiError(retryResponse.status, errData.detail || '颜色检测失败', retryResponse.status);
                }
                return retryResponse.json();
            }
        }

        if (!response.ok) {
            const errData = await response.json().catch(() => ({}));
            throw new ApiError(response.status, errData.detail || '颜色检测失败', response.status);
        }

        return response.json();
    },

    /**
     * 执行夹取分拣任务
     * POST /api/v1/arm/clamp
     * @param {string} color - red | green | blue
     * @param {boolean} [useUltrasonic] - 是否使用超声波校正，默认 true
     * @returns {Promise<ClampTaskResponse>}
     */
    runClampTask: async (color, useUltrasonic = true) => {
        return apiRequest('/arm/clamp', {
            method: 'POST',
            body: { color, use_ultrasonic: useUltrasonic },
        });
    },

    /**
     * 获取操作日志
     * GET /api/v1/arm/logs
     * @param {number} [page] - 页码，默认 1
     * @param {number} [pageSize] - 每页数量，默认 50
     * @returns {Promise<PaginatedData<OperationLogEntry>>}
     */
    getLogs: async (page = 1, pageSize = 50) => {
        return apiRequest(`/arm/logs?page=${page}&page_size=${pageSize}`);
    },
};

// ── 导出 ─────────────────────────────────────────────────────────────────────
window.API = {
    base: API_BASE,
    auth: authAPI,
    arm: armAPI,
    token: tokenManager,
    ApiError,
};
