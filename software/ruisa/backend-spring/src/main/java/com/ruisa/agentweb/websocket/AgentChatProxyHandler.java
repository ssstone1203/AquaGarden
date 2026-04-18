package com.ruisa.agentweb.websocket;

import java.io.IOException;
import java.net.URI;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.LinkedBlockingQueue;

import org.springframework.stereotype.Component;
import org.springframework.web.socket.CloseStatus;
import org.springframework.web.socket.TextMessage;
import org.springframework.web.socket.WebSocketHandler;
import org.springframework.web.socket.WebSocketMessage;
import org.springframework.web.socket.WebSocketSession;
import org.springframework.web.socket.client.WebSocketClient;
import org.springframework.web.socket.client.standard.StandardWebSocketClient;
import org.springframework.web.socket.handler.ConcurrentWebSocketSessionDecorator;

import com.ruisa.agentweb.config.AgentProxyProperties;

/**
 * Bridges browser {@code /api/v1/ws/agent/chat} to the Python FastAPI agent WebSocket.
 */
@Component
public class AgentChatProxyHandler implements WebSocketHandler {

    private static final String ATTR_SAFE_BROWSER = "safeBrowser";
    private static final String ATTR_PENDING = "pendingQueue";
    private static final String ATTR_PYTHON = "pythonSession";

    private final WebSocketClient client = new StandardWebSocketClient();
    private final AgentProxyProperties props;

    public AgentChatProxyHandler(AgentProxyProperties props) {
        this.props = props;
    }

    private URI pythonWsUri() {//将http URL转换成websocket url
        String base = props.getBaseUrl().trim().replaceAll("/+$", "");
        String ws;
        if (base.startsWith("https://")) {
            ws = "wss://" + base.substring("https://".length());
        } else if (base.startsWith("http://")) {
            ws = "ws://" + base.substring("http://".length());
        } else {
            ws = base.startsWith("ws") ? base : "ws://" + base;
        }
        return URI.create(ws + "/api/v1/ws/agent/chat");
    }

    @Override
    public void afterConnectionEstablished(WebSocketSession browserRaw) {//当浏览器连接到此端点触发
        WebSocketSession safeBrowser =//创建一个安全的浏览器会话
                new ConcurrentWebSocketSessionDecorator(browserRaw, 60_000, 512 * 1024);
        browserRaw.getAttributes().put(ATTR_SAFE_BROWSER, safeBrowser);

        BlockingQueue<WebSocketMessage<?>> pending = new LinkedBlockingQueue<>();//创建一个阻塞队列，用于存储浏览器发送的消息
        browserRaw.getAttributes().put(ATTR_PENDING, pending);

        CompletableFuture<WebSocketSession> future =//创建一个异步任务，用于执行python websocket连接
                client.execute(
                        new WebSocketHandler() {
                            @Override
                            public void afterConnectionEstablished(WebSocketSession python) {
                                drain(browserRaw, python, pending);
                            }

                            @Override
                            public void handleMessage(WebSocketSession session, WebSocketMessage<?> message)
                                    throws Exception {
                                if (safeBrowser.isOpen()) {
                                    safeBrowser.sendMessage(copyMessage(message));
                                }
                            }

                            @Override
                            public void handleTransportError(WebSocketSession session, Throwable exception) {
                                closeQuietly(safeBrowser);
                            }

                            @Override
                            public void afterConnectionClosed(WebSocketSession session, CloseStatus closeStatus) {
                                closeQuietly(safeBrowser);
                            }

                            @Override
                            public boolean supportsPartialMessages() {
                                return false;
                            }
                        },
                        null,
                        pythonWsUri());

        future.whenComplete((python, ex) -> {
            if (ex != null) {
                closeQuietly(safeBrowser);
            }
        });
    }

    private void drain(
            WebSocketSession browserRaw,
            WebSocketSession python,
            BlockingQueue<WebSocketMessage<?>> pending) {
        synchronized (pending) {
            browserRaw.getAttributes().put(ATTR_PYTHON, python);
            WebSocketMessage<?> m;
            while ((m = pending.poll()) != null) {
                try {
                    if (python.isOpen()) {
                        python.sendMessage(copyMessage(m));
                    }
                } catch (IOException e) {
                    break;
                }
            }
        }
    }

    @Override
    public void handleMessage(WebSocketSession browserRaw, WebSocketMessage<?> message) throws Exception {
        @SuppressWarnings("unchecked")
        BlockingQueue<WebSocketMessage<?>> pending =
                (BlockingQueue<WebSocketMessage<?>>) browserRaw.getAttributes().get(ATTR_PENDING);
        if (pending == null) {
            return;
        }
        synchronized (pending) {
            WebSocketSession python =
                    (WebSocketSession) browserRaw.getAttributes().get(ATTR_PYTHON);
            if (python != null && python.isOpen()) {
                python.sendMessage(copyMessage(message));
            } else {
                pending.offer(copyMessage(message));
            }
        }
    }

    @Override
    public void handleTransportError(WebSocketSession session, Throwable exception) {
        WebSocketSession python = (WebSocketSession) session.getAttributes().get(ATTR_PYTHON);
        closeQuietly(python);
        Object sb = session.getAttributes().get(ATTR_SAFE_BROWSER);
        if (sb instanceof WebSocketSession s) {
            closeQuietly(s);
        }
    }

    @Override
    public void afterConnectionClosed(WebSocketSession session, CloseStatus closeStatus) {
        WebSocketSession python = (WebSocketSession) session.getAttributes().get(ATTR_PYTHON);
        closeQuietly(python);
    }

    @Override
    public boolean supportsPartialMessages() {
        return false;
    }

    private static WebSocketMessage<?> copyMessage(WebSocketMessage<?> message) {
        if (message instanceof TextMessage tm) {
            return new TextMessage(tm.getPayload());
        }
        return message;
    }

    private static void closeQuietly(WebSocketSession s) {
        if (s == null || !s.isOpen()) {
            return;
        }
        try {
            s.close();
        } catch (IOException ignored) {
        }
    }
}
