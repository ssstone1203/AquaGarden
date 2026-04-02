/**
 * Agent 对话 WebSocket 管理模块
 * 管理与大模型 Agent 的实时对话连接
 */

const AgentWS = {
    _ws: null,
    _sessionId: null,
    _reconnectAttempts: 0,
    _maxReconnectAttempts: 5,
    _reconnectDelay: 3000,
    _pingInterval: null,
    _callbacks: {
        onConnected: null,
        onDisconnected: null,
        onError: null,
        onResponse: null,
        onTaskStart: null,
        onTaskLog: null,
        onTaskProgress: null,
        onTaskComplete: null,
        onTaskError: null,
        onSessionEnded: null,
    },

    /**
     * 连接到 Agent 对话服务
     */
    connect(token) {
        return new Promise((resolve, reject) => {
            if (this._ws && this._ws.readyState === WebSocket.OPEN) {
                resolve();
                return;
            }

            const wsUrl = `${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/api/v1/ws/agent/chat?token=${encodeURIComponent(token)}`;

            console.log('[AgentWS] 连接中...', wsUrl);
            this._ws = new WebSocket(wsUrl);

            this._ws.onopen = () => {
                console.log('[AgentWS] 已连接');
                this._reconnectAttempts = 0;
                this._startPing();
                if (this._callbacks.onConnected) {
                    this._callbacks.onConnected();
                }
                resolve();
            };

            this._ws.onclose = (event) => {
                console.log('[AgentWS] 连接关闭', event.code, event.reason);
                this._stopPing();
                if (this._callbacks.onDisconnected) {
                    this._callbacks.onDisconnected(event);
                }
                // 尝试重连
                if (this._reconnectAttempts < this._maxReconnectAttempts && event.code !== 1000) {
                    this._scheduleReconnect(token);
                }
            };

            this._ws.onerror = (error) => {
                console.error('[AgentWS] 错误', error);
                if (this._callbacks.onError) {
                    this._callbacks.onError(error);
                }
                reject(error);
            };

            this._ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this._handleMessage(data);
                } catch (e) {
                    console.error('[AgentWS] 消息解析失败', e);
                }
            };
        });
    },

    /**
     * 断开连接
     */
    disconnect() {
        this._stopPing();
        if (this._ws) {
            this._ws.close(1000, '用户主动断开');
            this._ws = null;
        }
        this._sessionId = null;
    },

    /**
     * 发送消息
     * @param {string} text - 用户输入的文本
     * @param {string} [imageData] - 可选的 base64 图片数据
     */
    sendMessage(text, imageData = null) {
        if (!this._ws || this._ws.readyState !== WebSocket.OPEN) {
            console.error('[AgentWS] 未连接');
            return false;
        }

        const message = {
            type: 'message',
            text: text,
        };

        if (imageData) {
            message.image_data = imageData;
        }

        this._ws.send(JSON.stringify(message));
        return true;
    },

    /**
     * 结束会话
     */
    endSession() {
        if (!this._ws || this._ws.readyState !== WebSocket.OPEN) {
            return false;
        }
        this._ws.send(JSON.stringify({ type: 'end_session' }));
        return true;
    },

    /**
     * 注册回调
     */
    on(event, callback) {
        if (this._callbacks.hasOwnProperty(event)) {
            this._callbacks[event] = callback;
        }
    },

    /**
     * 移除回调
     */
    off(event) {
        if (this._callbacks.hasOwnProperty(event)) {
            this._callbacks[event] = null;
        }
    },

    /**
     * 是否已连接
     */
    isConnected() {
        return this._ws && this._ws.readyState === WebSocket.OPEN;
    },

    // ── 私有方法 ────────────────────────────────────────────────────────────

    _handleMessage(data) {
        const type = data.type || '';

        switch (type) {
            case 'connected':
                this._sessionId = data.session_id;
                console.log('[AgentWS] 会话已建立:', this._sessionId);
                break;

            case 'response':
                if (this._callbacks.onResponse) {
                    this._callbacks.onResponse({
                        response: data.response,
                        taskTriggered: data.task_triggered,
                        taskName: data.task_name,
                        taskParams: data.task_params,
                        sessionState: data.session_state,
                        shouldEnd: data.should_end,
                        timestamp: data.timestamp,
                    });
                }
                break;

            case 'task_start':
                if (this._callbacks.onTaskStart) {
                    this._callbacks.onTaskStart({
                        task: data.task,
                        params: data.params,
                    });
                }
                break;

            case 'task_log':
                if (this._callbacks.onTaskLog) {
                    this._callbacks.onTaskLog({
                        task: data.task,
                        message: data.message,
                        level: data.level || 'INFO',
                    });
                }
                break;

            case 'task_progress':
                if (this._callbacks.onTaskProgress) {
                    this._callbacks.onTaskProgress({
                        task: data.task,
                        step: data.step,
                        progress: data.progress,
                        message: data.message,
                    });
                }
                break;

            case 'task_complete':
                if (this._callbacks.onTaskComplete) {
                    this._callbacks.onTaskComplete({
                        task: data.task,
                        result: data.result,
                    });
                }
                break;

            case 'task_error':
                if (this._callbacks.onTaskError) {
                    this._callbacks.onTaskError({
                        task: data.task,
                        error: data.error,
                    });
                }
                break;

            case 'session_ended':
                this._sessionId = null;
                if (this._callbacks.onSessionEnded) {
                    this._callbacks.onSessionEnded();
                }
                break;

            case 'pong':
                // 心跳响应
                break;

            default:
                console.log('[AgentWS] 未知消息类型:', type, data);
        }
    },

    _startPing() {
        this._stopPing();
        this._pingInterval = setInterval(() => {
            if (this._ws && this._ws.readyState === WebSocket.OPEN) {
                this._ws.send(JSON.stringify({ type: 'ping' }));
            }
        }, 30000); // 每 30 秒发送一次心跳
    },

    _stopPing() {
        if (this._pingInterval) {
            clearInterval(this._pingInterval);
            this._pingInterval = null;
        }
    },

    _scheduleReconnect(token) {
        this._reconnectAttempts++;
        const delay = this._reconnectDelay * this._reconnectAttempts;
        console.log(`[AgentWS] ${delay/1000}s 后尝试重连 (${this._reconnectAttempts}/${this._maxReconnectAttempts})`);
        setTimeout(() => {
            this.connect(token).catch(() => {});
        }, delay);
    },
};

// 导出到全局
window.AgentWS = AgentWS;
