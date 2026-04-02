/**
 * AquaGarden WebSocket 客户端
 * 机械臂实时状态推送和标定数据通道
 *
 * WebSocket 端点:
 *   WS /api/v1/ws/arm/status       - 机械臂状态实时推送（每 2 秒）
 *   WS /api/v1/ws/arm/calibration - 标定数据实时推送
 */

// ── 状态 WebSocket ───────────────────────────────────────────────────────────
class ArmStatusWS {
    constructor() {
        this.ws = null;
        this.url = '';
        this.reconnectDelay = 2000;
        this.maxReconnectDelay = 30000;
        this.reconnectTimer = null;
        this.isManualClose = false;
        this.listeners = {
            status: [],     // 机械臂状态变化回调
            connected: [],  // 连接成功回调
            disconnected: [], // 断开连接回调
            error: [],      // 错误回调
        };
    }

    /**
     * 连接到机械臂状态 WebSocket
     * @param {string} [token] - JWT access token
     */
    connect(token = null) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) return;

        const tokenParam = token ? `?token=${encodeURIComponent(token)}` : '';
        this.url = `ws://localhost:8000/api/v1/ws/arm/status${tokenParam}`;
        this.isManualClose = false;

        try {
            this.ws = new WebSocket(this.url);
            this._setupEvents();
        } catch (err) {
            console.error('WebSocket 连接失败:', err);
            this._scheduleReconnect();
        }
    }

    _setupEvents() {
        this.ws.onopen = () => {
            console.log('[WS] 机械臂状态通道已连接');
            this.reconnectDelay = 2000; // 重置重连延迟
            this._emit('connected');
        };

        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'arm_status') {
                    this._emit('status', data);
                }
            } catch (err) {
                console.error('[WS] 解析消息失败:', err);
            }
        };

        this.ws.onerror = (err) => {
            console.error('[WS] 机械臂状态通道错误:', err);
            this._emit('error', err);
        };

        this.ws.onclose = (event) => {
            console.log(`[WS] 机械臂状态通道已断开 (code: ${event.code})`);
            this._emit('disconnected', event);
            if (!this.isManualClose) {
                this._scheduleReconnect();
            }
        };
    }

    _scheduleReconnect() {
        if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
        this.reconnectTimer = setTimeout(() => {
            console.log(`[WS] ${this.reconnectDelay / 1000}s 后尝试重连...`);
            this.connect(this._extractToken());
            // 指数退避
            this.reconnectDelay = Math.min(this.reconnectDelay * 1.5, this.maxReconnectDelay);
        }, this.reconnectDelay);
    }

    _extractToken() {
        try {
            const url = new URL(this.url);
            return url.searchParams.get('token');
        } catch {
            return null;
        }
    }

    /**
     * 手动关闭 WebSocket 连接
     */
    disconnect() {
        this.isManualClose = true;
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    /**
     * 订阅机械臂状态变化
     * @param {Function} callback - (ArmStatusData) => void
     */
    onStatus(callback) {
        this.listeners.status.push(callback);
        return () => this.listeners.status = this.listeners.status.filter(cb => cb !== callback);
    }

    /**
     * 订阅连接成功事件
     * @param {Function} callback - () => void
     */
    onConnected(callback) {
        this.listeners.connected.push(callback);
        return () => this.listeners.connected = this.listeners.connected.filter(cb => cb !== callback);
    }

    /**
     * 订阅断开连接事件
     * @param {Function} callback - () => void
     */
    onDisconnected(callback) {
        this.listeners.disconnected.push(callback);
        return () => this.listeners.disconnected = this.listeners.disconnected.filter(cb => cb !== callback);
    }

    /**
     * 订阅错误事件
     * @param {Function} callback - (Error) => void
     */
    onError(callback) {
        this.listeners.error.push(callback);
        return () => this.listeners.error = this.listeners.error.filter(cb => cb !== callback);
    }

    _emit(event, data) {
        (this.listeners[event] || []).forEach(cb => cb(data));
    }

    /** 是否已连接 */
    get isConnected() {
        return this.ws && this.ws.readyState === WebSocket.OPEN;
    }
}

// ── 标定 WebSocket ───────────────────────────────────────────────────────────
class CalibrationWS {
    constructor() {
        this.ws = null;
        this.url = '';
        this.pingInterval = null;
        this.isManualClose = false;
        this.listeners = {
            pong: [],
            connected: [],
            disconnected: [],
            error: [],
        };
    }

    /**
     * 连接到标定 WebSocket
     * @param {string} [token] - JWT access token
     */
    connect(token = null) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) return;

        const tokenParam = token ? `?token=${encodeURIComponent(token)}` : '';
        this.url = `ws://localhost:8000/api/v1/ws/arm/calibration${tokenParam}`;
        this.isManualClose = false;

        try {
            this.ws = new WebSocket(this.url);
            this._setupEvents();
        } catch (err) {
            console.error('[WS] 标定通道连接失败:', err);
            this._emit('error', err);
        }
    }

    _setupEvents() {
        this.ws.onopen = () => {
            console.log('[WS] 标定数据通道已连接');
            this._emit('connected');
            // 启动心跳
            this._startPing();
        };

        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'pong') {
                    this._emit('pong', data);
                }
            } catch (err) {
                console.error('[WS] 解析标定消息失败:', err);
            }
        };

        this.ws.onerror = (err) => {
            console.error('[WS] 标定通道错误:', err);
            this._emit('error', err);
        };

        this.ws.onclose = () => {
            console.log('[WS] 标定数据通道已断开');
            this._stopPing();
            this._emit('disconnected');
        };
    }

    _startPing() {
        this.pingInterval = setInterval(() => {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                this.ws.send(JSON.stringify({ type: 'ping' }));
            }
        }, 5000);
    }

    _stopPing() {
        if (this.pingInterval) {
            clearInterval(this.pingInterval);
            this.pingInterval = null;
        }
    }

    /**
     * 发送标定数据到服务器
     * @param {object} data - 标定数据
     */
    sendCalibrationData(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(data));
        }
    }

    /** 手动关闭 */
    disconnect() {
        this.isManualClose = true;
        this._stopPing();
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    /** 订阅 pong 响应 */
    onPong(callback) {
        this.listeners.pong.push(callback);
        return () => this.listeners.pong = this.listeners.pong.filter(cb => cb !== callback);
    }

    onConnected(callback) {
        this.listeners.connected.push(callback);
        return () => this.listeners.connected = this.listeners.connected.filter(cb => cb !== callback);
    }

    onDisconnected(callback) {
        this.listeners.disconnected.push(callback);
        return () => this.listeners.disconnected = this.listeners.disconnected.filter(cb => cb !== callback);
    }

    onError(callback) {
        this.listeners.error.push(callback);
        return () => this.listeners.error = this.listeners.error.filter(cb => cb !== callback);
    }

    _emit(event, data) {
        (this.listeners[event] || []).forEach(cb => cb(data));
    }

    get isConnected() {
        return this.ws && this.ws.readyState === WebSocket.OPEN;
    }
}

// ── 导出单例 ────────────────────────────────────────────────────────────────
const armStatusWS = new ArmStatusWS();
const calibrationWS = new CalibrationWS();

window.WS = {
    armStatus: armStatusWS,
    calibration: calibrationWS,
};
