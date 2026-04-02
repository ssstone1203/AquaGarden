/**
 * Agent WebSocket — 对接 backend agent_bridge（与 agent/agent.py 行为一致）
 * WS /api/v1/ws/agent/chat（无需登录）
 */
const AgentWS = {
    _ws: null,
    _sessionId: null,
    debugKeyboard: false,
    needsAgentWake: false,
    _reconnectAttempts: 0,
    _maxReconnectAttempts: 5,
    _reconnectDelay: 3000,
    _pingInterval: null,
    _callbacks: {
        onConnected: null,
        onDisconnected: null,
        onError: null,
        onResponse: null,
        onDebugMode: null,
        onTaskStart: null,
        onTaskLog: null,
        onTaskProgress: null,
        onTaskComplete: null,
        onTaskError: null,
        onTaskInterrupted: null,
        onInterruptAck: null,
        onSessionEnded: null,
    },

    connect() {
        return new Promise((resolve, reject) => {
            if (this._ws && this._ws.readyState === WebSocket.OPEN) {
                resolve();
                return;
            }
            const wsProto = location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${wsProto}//${location.host}/api/v1/ws/agent/chat`;
            console.log('[AgentWS] 连接', wsUrl);
            this._ws = new WebSocket(wsUrl);

            this._ws.onopen = () => {
                this._reconnectAttempts = 0;
                this._startPing();
                resolve();
            };

            this._ws.onclose = (event) => {
                this._stopPing();
                if (this._callbacks.onDisconnected) {
                    this._callbacks.onDisconnected(event);
                }
                if (this._reconnectAttempts < this._maxReconnectAttempts && event.code !== 1000) {
                    this._scheduleReconnect();
                }
            };

            this._ws.onerror = (error) => {
                console.error('[AgentWS]', error);
                if (this._callbacks.onError) {
                    this._callbacks.onError(error);
                }
                reject(error);
            };

            this._ws.onmessage = (event) => {
                try {
                    this._handleMessage(JSON.parse(event.data));
                } catch (e) {
                    console.error('[AgentWS] parse', e);
                }
            };
        });
    },

    disconnect() {
        this._stopPing();
        if (this._ws) {
            this._ws.close(1000, 'client');
            this._ws = null;
        }
        this._sessionId = null;
    },

    sendMessage(text, imageData = null) {
        if (!this._ws || this._ws.readyState !== WebSocket.OPEN) {
            console.error('[AgentWS] 未连接');
            return false;
        }
        const message = { type: 'message', text };
        if (imageData) message.image_data = imageData;
        this._ws.send(JSON.stringify(message));
        return true;
    },

    sendDebugWake() {
        if (!this._ws || this._ws.readyState !== WebSocket.OPEN) return false;
        this._ws.send(JSON.stringify({ type: 'debug_wake' }));
        return true;
    },

    sendSetDebugKeyboard(enabled) {
        if (!this._ws || this._ws.readyState !== WebSocket.OPEN) return false;
        this._ws.send(JSON.stringify({ type: 'set_debug_keyboard', enabled: !!enabled }));
        return true;
    },

    endSession() {
        if (!this._ws || this._ws.readyState !== WebSocket.OPEN) return false;
        this._ws.send(JSON.stringify({ type: 'end_session' }));
        return true;
    },

    /** 请求中断当前机械臂任务（协作式） */
    sendInterrupt() {
        if (!this._ws || this._ws.readyState !== WebSocket.OPEN) return false;
        this._ws.send(JSON.stringify({ type: 'interrupt' }));
        return true;
    },

    on(event, callback) {
        if (Object.prototype.hasOwnProperty.call(this._callbacks, event)) {
            this._callbacks[event] = callback;
        }
    },

    off(event) {
        if (Object.prototype.hasOwnProperty.call(this._callbacks, event)) {
            this._callbacks[event] = null;
        }
    },

    isConnected() {
        return this._ws && this._ws.readyState === WebSocket.OPEN;
    },

    _handleMessage(data) {
        const type = data.type || '';
        switch (type) {
            case 'connected':
                this._sessionId = data.session_id;
                this.debugKeyboard = !!data.agent_debug_keyboard;
                this.needsAgentWake = !!data.needs_wake;
                if (this._callbacks.onConnected) this._callbacks.onConnected(data);
                break;
            case 'debug_mode':
                this.debugKeyboard = !!data.agent_debug_keyboard;
                this.needsAgentWake = !!data.needs_wake;
                if (this._callbacks.onDebugMode) this._callbacks.onDebugMode(data);
                break;
            case 'response':
                if (typeof data.needs_wake === 'boolean') {
                    this.needsAgentWake = data.needs_wake;
                }
                if (this._callbacks.onResponse) {
                    this._callbacks.onResponse({
                        response: data.response,
                        taskTriggered: data.task_triggered,
                        taskName: data.task_name,
                        taskParams: data.task_params,
                        sessionState: data.session_state,
                        shouldEnd: data.should_end,
                        needsWake: data.needs_wake,
                        timestamp: data.timestamp,
                    });
                }
                break;
            case 'task_start':
                if (this._callbacks.onTaskStart) {
                    this._callbacks.onTaskStart({ task: data.task, params: data.params });
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
                        message: data.message,
                    });
                }
                break;
            case 'task_complete':
                if (this._callbacks.onTaskComplete) {
                    this._callbacks.onTaskComplete({ task: data.task, result: data.result });
                }
                break;
            case 'task_error':
                if (this._callbacks.onTaskError) {
                    this._callbacks.onTaskError({ task: data.task, error: data.error });
                }
                break;
            case 'task_interrupted':
                if (this._callbacks.onTaskInterrupted) {
                    this._callbacks.onTaskInterrupted({
                        task: data.task,
                        message: data.message,
                    });
                }
                break;
            case 'interrupt_ack':
                if (this._callbacks.onInterruptAck) {
                    this._callbacks.onInterruptAck({
                        ok: data.ok,
                        message: data.message,
                    });
                }
                break;
            case 'session_ended':
                this._sessionId = null;
                if (this._callbacks.onSessionEnded) this._callbacks.onSessionEnded();
                break;
            case 'pong':
                break;
            default:
                console.log('[AgentWS] unknown', type, data);
        }
    },

    _startPing() {
        this._stopPing();
        this._pingInterval = setInterval(() => {
            if (this._ws && this._ws.readyState === WebSocket.OPEN) {
                this._ws.send(JSON.stringify({ type: 'ping' }));
            }
        }, 30000);
    },

    _stopPing() {
        if (this._pingInterval) {
            clearInterval(this._pingInterval);
            this._pingInterval = null;
        }
    },

    _scheduleReconnect() {
        this._reconnectAttempts++;
        const delay = this._reconnectDelay * this._reconnectAttempts;
        setTimeout(() => {
            this.connect().catch(() => {});
        }, delay);
    },
};

window.AgentWS = AgentWS;
