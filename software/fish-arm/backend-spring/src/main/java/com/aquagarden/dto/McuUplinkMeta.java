package com.aquagarden.dto;

/**
 * data_merge 固件 {@code Communicate_Task_entry.c#host_build_uplink_frame} 中 payload
 * 在 5 个主传感器字段之外的扩展字段，与 C 端顺序一致。
 */
public record McuUplinkMeta(
        long mcuTimeMs,
        long alarmFlags,
        double pressureKg0,
        double pressureKg1,
        double pressureKg2,
        int airRetryCount,
        int wqsRetryCount,
        int uwtRetryCount,
        int pumpPowerPercent,
        int needWatering,
        int pumpCycleEnable,
        int pumpCycleStart,
        int pumpCycleActive,
        int pumpCycleState,
        int pumpCyclePowerPercent,
        int pumpCycleDoneCount
) {}
