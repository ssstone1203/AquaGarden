package com.aquagarden.dto;

/**
 * 串口桥推送的一帧完整结果：5 个主指标 + 可选 MCU 扩展（与 data_merge 一致）。
 */
public record HardwareSensorState(SensorSnapshot core, McuUplinkMeta mcu) {
    public static HardwareSensorState coreOnly(SensorSnapshot s) {
        return new HardwareSensorState(s, null);
    }
}
