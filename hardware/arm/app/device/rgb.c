#include "rgb.h"
#include <string.h>

/*
 * WS2812B RGB 灯环驱动 — RA6M5 (GPT2 PWM 方式)
 *
 * 硬件资源:
 *   PWM 输出 → P113 (GPT2 GTIOC2A)
 *   定时器  → GPT2 (g_rgb_timer, PCLKD = 100 MHz, 不分频, 10 ns/tick)
 *
 * 原理:
 *   WS2812 每一位对应一个 PWM 周期 (1250 ns = 125 ticks)，
 *   用高电平占空比区分 0/1:
 *     Bit 0: T0H ≈ 400 ns (40 ticks)
 *     Bit 1: T1H ≈ 800 ns (80 ticks)
 *   利用 GPT 缓冲寄存器 (GTCCRC → GTCCRA) 在溢出时自动传输，
 *   实现无间隙的连续 PWM 输出。
 *   数据格式: GRB, MSB first。
 */

/*
 * 占空比写入 GTCCRC 缓冲寄存器时需 -1 补偿
 * (缓冲传输导致引脚电平变化延迟 1 个计数周期)
 */
#define DUTY_BIT0   39U   /* T0H ≈ 40 ticks × 10 ns = 400 ns */
#define DUTY_BIT1   79U   /* T1H ≈ 80 ticks × 10 ns = 800 ns */

static uint8_t s_led_buf[WS2812_NUM_LEDS][3];   /* GRB */
static uint8_t s_brightness = 255;

static uint8_t scale8(uint8_t val)
{
    return (uint8_t)(((uint16_t)val * s_brightness) >> 8);
}

/* ── 公开接口 ── */

void RGB_Init(void)
{
    memset(s_led_buf, 0, sizeof(s_led_buf));
    s_brightness = 255;

    /*
     * 以 PWM 模式打开 GPT2。
     * 生成的 g_rgb_timer_cfg.mode 为 PERIODIC，这里覆盖为 PWM，
     * 使 GTIOR 配置为: 溢出→HIGH, 比较匹配→LOW，
     * 从而每个周期开头输出高电平，结尾输出低电平。
     */
    timer_cfg_t pwm_cfg = g_rgb_timer_cfg;
    pwm_cfg.mode = TIMER_MODE_PWM;
    R_GPT_Open(&g_rgb_timer_ctrl, &pwm_cfg);

    RGB_Show();
}

void RGB_SetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= WS2812_NUM_LEDS) return;
    s_led_buf[index][0] = g;
    s_led_buf[index][1] = r;
    s_led_buf[index][2] = b;
}

void RGB_SetAll(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++)
        RGB_SetPixel(i, r, g, b);
}

void RGB_Clear(void)
{
    memset(s_led_buf, 0, sizeof(s_led_buf));
}

void RGB_SetBrightness(uint8_t brightness)
{
    s_brightness = brightness;
}

void RGB_Show(void)
{
    R_GPT0_Type * p_reg = g_rgb_timer_ctrl.p_reg;
    uint32_t      cmask = g_rgb_timer_ctrl.channel_mask;

    __disable_irq();

    p_reg->GTSTP = cmask;
    p_reg->GTCNT = 0;
    p_reg->GTST  = 0;

    uint8_t first = 1;

    for (uint8_t led = 0; led < WS2812_NUM_LEDS; led++)
    {
        for (uint8_t ch = 0; ch < 3; ch++)
        {
            uint8_t data = scale8(s_led_buf[led][ch]);

            for (uint8_t bit = 0; bit < 8; bit++)
            {
                uint32_t duty = (data & 0x80) ? DUTY_BIT1 : DUTY_BIT0;
                data <<= 1;

                if (first)
                {
                    p_reg->GTCCR[0] = duty;   /* GTCCRA: 首位直接生效 */
                    p_reg->GTCCR[2] = duty;   /* GTCCRC: 同步缓冲 */
                    p_reg->GTSTR    = cmask;   /* 启动定时器 */
                    first = 0;
                }
                else
                {
                    p_reg->GTCCR[2] = duty;    /* 写入缓冲, 溢出时自动传输 */
                    while (!(p_reg->GTST & R_GPT0_GTST_TCFPO_Msk)) {}
                    p_reg->GTST = 0;
                }
            }
        }
    }

    /* 等待最后一位 PWM 周期完成 */
    while (!(p_reg->GTST & R_GPT0_GTST_TCFPO_Msk)) {}

    p_reg->GTSTP = cmask;   /* 停止, 输出归 LOW (stop_level = LOW) */
    p_reg->GTST  = 0;

    __enable_irq();

    /* WS2812 复位信号: >50 µs 低电平 */
    R_BSP_SoftwareDelay(80, BSP_DELAY_UNITS_MICROSECONDS);
}
