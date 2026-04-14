const AGENT_DEBUG_KEY = 'ruisa_agent_debug_keyboard';

/** 浏览器 Web Speech API 实例（Chrome / Edge 等）；再点一次可取消聆听 */
let voiceRecognition = null;

function setMicListening(on) {
    const b = document.getElementById('agentMicBtn');
    if (b) b.classList.toggle('listening', on);
}

function openCameraPreview(mjpegPath) {
    const modal = document.getElementById('cameraModal');
    const img = document.getElementById('cameraPreviewImg');
    const path = mjpegPath || '/api/v1/camera/mjpeg';
    img.src = `${path}?t=${Date.now()}`;
    modal.classList.add('open');
    modal.setAttribute('aria-hidden', 'false');
}

function closeCameraPreview() {
    const modal = document.getElementById('cameraModal');
    const img = document.getElementById('cameraPreviewImg');
    img.src = '';
    modal.classList.remove('open');
    modal.setAttribute('aria-hidden', 'true');
}

function toggleVoiceInput() {
    if (!AgentWS.isConnected()) {
        showToast('未连接服务器');
        return;
    }
    if (AgentWS.debugKeyboard && AgentWS.needsAgentWake) {
        showToast('DEBUG：请先模拟唤醒');
        return;
    }
    const Rec = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!Rec) {
        showToast('当前浏览器不支持语音识别，请使用 Chrome 或 Edge（需安全来源：localhost 或 HTTPS）');
        return;
    }
    if (voiceRecognition) {
        try {
            voiceRecognition.abort();
        } catch (e) { /* ignore */ }
        voiceRecognition = null;
        setMicListening(false);
        return;
    }
    const rec = new Rec();
    voiceRecognition = rec;
    rec.lang = 'zh-CN';
    rec.interimResults = false;
    rec.maxAlternatives = 1;
    rec.continuous = false;

    rec.onstart = () => setMicListening(true);

    rec.onend = () => {
        voiceRecognition = null;
        setMicListening(false);
    };

    rec.onerror = (ev) => {
        voiceRecognition = null;
        setMicListening(false);
        if (ev.error === 'not-allowed') {
            showToast('麦克风权限被拒绝，请在浏览器地址栏旁允许麦克风');
        } else if (ev.error === 'no-speech') {
            showToast('未检测到语音，请再试一次');
        } else if (ev.error !== 'aborted') {
            showToast('语音识别：' + (ev.error || '失败'));
        }
    };

    rec.onresult = (ev) => {
        let text = '';
        for (let i = ev.resultIndex; i < ev.results.length; i++) {
            text += ev.results[i][0].transcript;
        }
        text = (text || '').trim();
        if (text) {
            const input = document.getElementById('agentInput');
            input.value = text;
            addMsg('user', text);
            showTyping();
            AgentWS.sendMessage(text);
        } else {
            showToast('未识别到有效内容');
        }
    };

    try {
        rec.start();
    } catch (e) {
        voiceRecognition = null;
        setMicListening(false);
        showToast('无法启动语音识别：' + (e.message || String(e)));
    }
}

function showToast(msg, type = 'error') {
    const c = document.getElementById('toastContainer');
    const el = document.createElement('div');
    el.className = 'toast ' + type;
    el.textContent = msg;
    c.appendChild(el);
    setTimeout(() => el.remove(), 4000);
}

function formatTime(d) {
    return `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}`;
}

function appendTerm(level, msg) {
    const body = document.getElementById('termBody');
    const t = new Date();
    const line = document.createElement('div');
    line.className = 'log-line';
    const cls = level === 'ERROR' ? 'e' : level === 'SUCCESS' ? 's' : 'i';
    line.innerHTML = `<span class="t">${formatTime(t)}</span><span class="${cls}">[${level}]</span> ${escapeHtml(msg)}`;
    body.appendChild(line);
    body.scrollTop = body.scrollHeight;
}

function escapeHtml(t) {
    const d = document.createElement('div');
    d.textContent = t;
    return d.innerHTML;
}

function addMsg(role, text) {
    const box = document.getElementById('agentMessages');
    const div = document.createElement('div');
    div.className = 'msg ' + role;
    div.innerHTML = escapeHtml(text) + `<div class="msg-time">${formatTime(new Date())}</div>`;
    box.appendChild(div);
    box.scrollTop = box.scrollHeight;
}

let typingEl = null;
function showTyping() {
    hideTyping();
    const box = document.getElementById('agentMessages');
    typingEl = document.createElement('div');
    typingEl.className = 'msg assistant typing';
    typingEl.id = 'typingInd';
    typingEl.textContent = '正在思考…';
    box.appendChild(typingEl);
    box.scrollTop = box.scrollHeight;
}
function hideTyping() {
    const t = document.getElementById('typingInd');
    if (t) t.remove();
    typingEl = null;
}

function setConnected(on) {
    const dot = document.getElementById('statusDot');
    const tx = document.getElementById('statusText');
    dot.classList.toggle('on', on);
    tx.textContent = on ? '已连接' : '未连接';
}

function updateDebugUI() {
    const bar = document.getElementById('agentDebugBar');
    const hint = document.getElementById('agentDebugHint');
    const wake = document.getElementById('agentDebugWakeBtn');
    const dk = AgentWS.debugKeyboard;
    bar.classList.toggle('show', dk);
    const need = dk && AgentWS.needsAgentWake;
    hint.textContent = need
        ? '请先「模拟唤醒」或输入框留空按 Enter（等同 agent.py DEBUG 按 Enter）'
        : '已唤醒，可发指令。';
    wake.disabled = !AgentWS.isConnected() || !need;
}

function toggleDebugMode() {
    if (!AgentWS.isConnected()) {
        showToast('请先等待 WebSocket 连接');
        return;
    }
    const btn = document.getElementById('dbgToggle');
    const next = !AgentWS.debugKeyboard;
    AgentWS.sendSetDebugKeyboard(next);
    localStorage.setItem(AGENT_DEBUG_KEY, next ? '1' : '0');
    btn.textContent = next ? '键盘DEBUG·开' : '键盘DEBUG·关';
    btn.classList.toggle('active', next);
}

function sendMessage() {
    if (!AgentWS.isConnected()) {
        showToast('未连接服务器');
        return;
    }
    if (AgentWS.debugKeyboard && AgentWS.needsAgentWake) {
        showToast('DEBUG：请先模拟唤醒');
        return;
    }
    const input = document.getElementById('agentInput');
    const text = (input.value || '').trim();
    if (!text) return;
    input.value = '';
    addMsg('user', text);
    showTyping();
    AgentWS.sendMessage(text);
}

function sendQuick(text) {
    if (!AgentWS.isConnected()) return showToast('未连接');
    if (AgentWS.debugKeyboard && AgentWS.needsAgentWake) return showToast('DEBUG：请先模拟唤醒');
    document.getElementById('agentInput').value = text;
    addMsg('user', text);
    document.getElementById('agentInput').value = '';
    showTyping();
    AgentWS.sendMessage(text);
}

function sendDebugWake() {
    if (!AgentWS.isConnected()) return;
    showTyping();
    AgentWS.sendDebugWake();
}

function onAgentInput(ev) {
    if (ev.key !== 'Enter' || ev.shiftKey) return;
    ev.preventDefault();
    if (AgentWS.debugKeyboard && AgentWS.needsAgentWake) {
        const input = document.getElementById('agentInput');
        if (!(input.value || '').trim()) sendDebugWake();
        else showToast('DEBUG：请先清空输入框并模拟唤醒');
        return;
    }
    sendMessage();
}

document.getElementById('agentSendBtn').addEventListener('click', sendMessage);
document.getElementById('agentMicBtn').addEventListener('click', toggleVoiceInput);
document.getElementById('agentInput').addEventListener('keydown', onAgentInput);
document.getElementById('agentDebugWakeBtn').addEventListener('click', sendDebugWake);
document.getElementById('dbgToggle').addEventListener('click', toggleDebugMode);
document.getElementById('cameraModalClose').addEventListener('click', closeCameraPreview);
document.getElementById('cameraModalBackdrop').addEventListener('click', closeCameraPreview);
document.querySelectorAll('.quick-btn').forEach((b) => {
    b.addEventListener('click', () => sendQuick(b.getAttribute('data-q')));
});

AgentWS.on('onConnected', () => {
    setConnected(true);
    const want = localStorage.getItem(AGENT_DEBUG_KEY) === '1';
    if (want) AgentWS.sendSetDebugKeyboard(true);
    const btn = document.getElementById('dbgToggle');
    btn.classList.toggle('active', want);
    btn.textContent = want ? '键盘DEBUG·开' : '键盘DEBUG·关';
    updateDebugUI();
    appendTerm('INFO', 'WebSocket 已连接；意图解析与 agent/dialogue.py 一致，任务由 agent/tasks.py 执行。');
});

AgentWS.on('onDisconnected', () => {
    setConnected(false);
    hideTyping();
    closeCameraPreview();
    if (voiceRecognition) {
        try { voiceRecognition.abort(); } catch (e) { /* ignore */ }
        voiceRecognition = null;
        setMicListening(false);
    }
    updateDebugUI();
    appendTerm('ERROR', 'WebSocket 已断开');
});

AgentWS.on('onResponse', (data) => {
    hideTyping();
    if (data.response) addMsg('assistant', data.response);
    updateDebugUI();
    if (data.shouldEnd) appendTerm('SUCCESS', '会话结束（拜拜）→ 机械臂归位');
});

AgentWS.on('onDebugMode', () => {
    const btn = document.getElementById('dbgToggle');
    btn.classList.toggle('active', AgentWS.debugKeyboard);
    btn.textContent = AgentWS.debugKeyboard ? '键盘DEBUG·开' : '键盘DEBUG·关';
    updateDebugUI();
});

AgentWS.on('onTaskStart', (d) => {
    const ind = document.getElementById('agentTaskIndicator');
    ind.classList.add('running');
    document.getElementById('agentTaskText').textContent = `执行中: ${d.task}`;
    appendTerm('INFO', `task_start → ${d.task}`);
});

AgentWS.on('onTaskLog', (d) => {
    appendTerm('INFO', d.message || '');
});

AgentWS.on('onTaskComplete', (d) => {
    document.getElementById('agentTaskIndicator').classList.remove('running');
    appendTerm('SUCCESS', `task_complete → ${d.task}: ${JSON.stringify(d.result || {})}`);
    if (d.task === 'answer' && d.result && d.result.answer) {
        addMsg('assistant', '【题目解答】\n' + d.result.answer);
    }
});

AgentWS.on('onTaskError', (d) => {
    document.getElementById('agentTaskIndicator').classList.remove('running');
    appendTerm('ERROR', `${d.task}: ${d.error}`);
});

AgentWS.on('onCameraPreview', (d) => {
    if (d.show) openCameraPreview(d.mjpegPath);
    else closeCameraPreview();
});

(async function init() {
    const greet = '你好呀！我是小臂，你的学习小助手~ 有什么需要帮忙的吗？\n可打字或点输入框左侧 🎤 用中文语音说话（Chrome/Edge）；视觉任务请配置机械臂 USB 摄像头（CAMERA_INDEX 等）。';
    addMsg('assistant', greet);
    try {
        await AgentWS.connect();
    } catch (e) {
        setConnected(false);
        showToast('无法连接 WebSocket，请从后端启动 uvicorn 并打开本页');
        appendTerm('ERROR', String(e));
    }
})();
