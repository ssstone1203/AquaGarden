#include "ultrasound.h"

/*
 * HC-SR04 超声波测距模块 —— RA6M5 实现
 *
 * 硬件资源：
 *   TRIG  → P4_15 (GPIO Output)
 *   ECHO  → P4_14 (ICU IRQ9, 双沿触发)
 *   定时器 → GPT0  (g_ultra_echo_timer, PCLKD=100MHz, 不分频, 10ns/count)
 *
 * 测距流程：
 *   1. 拉高 TRIG ≥ 10us，触发传感器发射超声波
 *   2. ECHO 上升沿 → 复位并启动 GPT0 计数
 *   3. ECHO 下降沿 → 停止 GPT0，读取计数值
 *   4. 距离 = count × 10ns / 2 × 34000 cm/s = count × 0.00017 cm
 */

static volatile uint32_t s_echo_count;
static volatile int      s_step;        /* 0: 空闲/完成  1: 等待回波 */

/* ── ICU 回调：ECHO 引脚双沿中断 ── */
static void ultrasound_echo_irq_callback(external_irq_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);

    bsp_io_level_t level;
    R_IOPORT_PinRead(&g_ioport_ctrl, ULTRASOUND_ECHO_PIN, &level);

    if (level == BSP_IO_LEVEL_HIGH)
    {
        /* 上升沿：复位计数器并启动定时器 */
        g_ultra_echo_timer.p_api->reset(&g_ultra_echo_timer_ctrl);
        g_ultra_echo_timer.p_api->start(&g_ultra_echo_timer_ctrl);
    }
    else
    {
        /* 下降沿：停止定时器，读取计数值 */
        g_ultra_echo_timer.p_api->stop(&g_ultra_echo_timer_ctrl);

        timer_status_t status;
        g_ultra_echo_timer.p_api->statusGet(&g_ultra_echo_timer_ctrl, &status);
        s_echo_count = status.counter;
        s_step = 0;
    }
}

void Ultrasound_Init(void)
{
    /* 打开 GPT0 并将周期设为 32 位最大值，避免测量期间溢出 */
    g_ultra_echo_timer.p_api->open(&g_ultra_echo_timer_ctrl, &g_ultra_echo_timer_cfg);
    g_ultra_echo_timer.p_api->periodSet(&g_ultra_echo_timer_ctrl, UINT32_MAX);
    g_ultra_echo_timer.p_api->stop(&g_ultra_echo_timer_ctrl);

    /* 打开 ICU IRQ9，注册回调并使能中断 */
    g_ultra_echo_irq0.p_api->open(&g_ultra_echo_irq0_ctrl, &g_ultra_echo_irq0_cfg);
    g_ultra_echo_irq0.p_api->callbackSet(&g_ultra_echo_irq0_ctrl,
                                          ultrasound_echo_irq_callback, NULL, NULL);
    g_ultra_echo_irq0.p_api->enable(&g_ultra_echo_irq0_ctrl);
}

float Ultrasound_GetDistance(void)
{
    s_echo_count = 0;
    s_step       = 1;

    /* 确保定时器处于停止+归零状态 */
    g_ultra_echo_timer.p_api->stop(&g_ultra_echo_timer_ctrl);
    g_ultra_echo_timer.p_api->reset(&g_ultra_echo_timer_ctrl);

    /* 发送 TRIG 脉冲 (≥10us) */
    R_IOPORT_PinWrite(&g_ioport_ctrl, ULTRASOUND_TRIG_PIN, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(30, BSP_DELAY_UNITS_MICROSECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ULTRASOUND_TRIG_PIN, BSP_IO_LEVEL_LOW);

    /* 等待回波测量完成（超时约 30ms，对应 ~510cm，大于量程） */
    uint32_t timeout = 30000;
    while (s_step != 0 && timeout > 0)
    {
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
        timeout--;
    }

    if (s_step != 0)
    {
        s_step = 0;
        return -1.0f;
    }

    /*
     * PCLKD = 100 MHz, source_div = 1 → 每个计数 = 10 ns
     * distance_cm = count × 10ns ÷ 2 × 34000 cm/s
     *             = count × 0.00017 cm
     */
    return (float)s_echo_count * 0.00017f;
}
