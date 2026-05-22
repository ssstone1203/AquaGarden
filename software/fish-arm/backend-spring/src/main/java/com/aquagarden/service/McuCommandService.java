package com.aquagarden.service;

import org.springframework.stereotype.Service;

import java.util.concurrent.atomic.AtomicReference;

/**
 * MCU 水泵命令队列（内存，单条覆盖式）。
 * <p>
 * 前端通过 POST /api/mcu/pump 入队一条命令，
 * serial_bridge.py 通过 GET /api/mcu/pump/pending 轮询并消费。
 * <p>
 * "覆盖式"语义：如果 bridge 还没来得及拉取，新的命令会覆盖旧命令，
 * 因为水泵控制只需要执行最新的意图。
 */
@Service
public class McuCommandService {

    /** 命令类型常量，与 MCU Communicate_Task_entry.c 中的定义一致 */
    public static final int CMD_STOP    = 0x01;
    public static final int CMD_START   = 0x02;
    public static final int CMD_SET_PWM = 0x03;

    private final AtomicReference<PendingCommand> pending = new AtomicReference<>(null);

    /**
     * 入队一条水泵命令。
     *
     * @param cmd   命令类型 (CMD_STOP / CMD_START / CMD_SET_PWM)
     * @param power 力度 0-100（STOP 时忽略）
     */
    public void enqueue(int cmd, int power) {
        pending.set(new PendingCommand(cmd, clamp(power), System.currentTimeMillis()));
    }

    /**
     * 消费（取出并清空）当前待发送命令。返回 null 表示无待发命令。
     */
    public PendingCommand drain() {
        return pending.getAndSet(null);
    }

    /**
     * 查看当前待发送命令（不消费）。
     */
    public PendingCommand peek() {
        return pending.get();
    }

    private static int clamp(int v) {
        return Math.max(0, Math.min(100, v));
    }

    /**
     * 待发送命令。
     */
    public record PendingCommand(int cmd, int power, long enqueueTimeMs) {

        /** 命令名称（用于日志和 JSON 响应） */
        public String cmdName() {
            return switch (cmd) {
                case CMD_STOP    -> "stop";
                case CMD_START   -> "start";
                case CMD_SET_PWM -> "set_pwm";
                default          -> "unknown";
            };
        }
    }
}
