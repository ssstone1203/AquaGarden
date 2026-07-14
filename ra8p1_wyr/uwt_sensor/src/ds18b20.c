/* DS18B20：单总线位带 + Skip ROM + CRC + 全局调试变量（UWS_DQ） */

#include "ds18b20.h"

#include "bsp_api.h"
#include "bsp_pin_cfg.h"
#include "common_data.h"
#include "r_ioport.h"

#define OW_SKIP_ROM      (0xCCU)
#define OW_CONVERT_T     (0x44U)
#define OW_READ_SCRATCH  (0xBEU)
#define CFG_RES_MASK     (0x60U)
#define OW_POLL_MAX      (20000U)

volatile float     g_uwt_temperature_c;
volatile int16_t   g_uwt_temperature_raw;
volatile fsp_err_t g_uwt_last_err;

/*--------------------------------------------------------------------------------------------------
 * 1-Wire（NMOS 开漏 + 输出 1 = 空闲高）
 *------------------------------------------------------------------------------------------------*/

static void delay_us(uint32_t us)
{
    R_BSP_SoftwareDelay(us, BSP_DELAY_UNITS_MICROSECONDS);
}

static void bus_idle(void)
{
    uint32_t c = (uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_NMOS_ENABLE |
                 (uint32_t) IOPORT_CFG_PORT_OUTPUT_HIGH;
    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, UWS_DQ, c);
}

static void bus_low(void)
{
    uint32_t c = (uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_NMOS_ENABLE |
                 (uint32_t) IOPORT_CFG_PORT_OUTPUT_LOW;
    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, UWS_DQ, c);
}

static uint8_t bus_read_pin(void)
{
    bsp_io_level_t lev = BSP_IO_LEVEL_HIGH;
    (void) R_IOPORT_PinRead(&g_ioport_ctrl, UWS_DQ, &lev);
    return (BSP_IO_LEVEL_LOW == lev) ? 0U : 1U;
}

static bool ow_reset(void)
{
    bus_low();
    delay_us(500);
    bus_idle();
    delay_us(70);
    if (0U != bus_read_pin())
    {
        delay_us(410);
        return false;
    }
    delay_us(410);
    return true;
}

static void ow_write_bit(uint8_t bit)
{
    if (bit & 1U)
    {
        bus_low();
        delay_us(6);
        bus_idle();
        delay_us(54);
    }
    else
    {
        bus_low();
        delay_us(60);
        bus_idle();
        delay_us(1);
    }
}

static uint8_t ow_read_bit(void)
{
    bus_low();
    delay_us(6);
    bus_idle();
    delay_us(9);
    uint8_t b = bus_read_pin();
    delay_us(55);
    return b;
}

static void ow_write_byte(uint8_t b)
{
    for (uint_fast8_t i = 0; i < 8U; i++)
    {
        ow_write_bit((uint8_t) (b & 1U));
        b = (uint8_t) (b >> 1);
    }
}

static uint8_t ow_read_byte(void)
{
    uint8_t v = 0;
    for (uint_fast8_t i = 0; i < 8U; i++)
    {
        if (ow_read_bit())
        {
            v |= (uint8_t) (1U << i);
        }
    }
    return v;
}

static bool ow_wait_convert(void)
{
    for (uint32_t n = 0; n < OW_POLL_MAX; n++)
    {
        if (ow_read_bit())
        {
            return true;
        }
    }
    return false;
}

/*--------------------------------------------------------------------------------------------------
 * DS18B20 数据
 *------------------------------------------------------------------------------------------------*/

static uint8_t crc8(const uint8_t * d, unsigned n)
{
    uint8_t c = 0;
    while (n-- > 0U)
    {
        uint8_t in = *d++;
        for (uint_fast8_t i = 8U; i > 0U; i--)
        {
            uint8_t m = (uint8_t) ((c ^ in) & 1U);
            c         = (uint8_t) (c >> 1);
            if (m)
            {
                c ^= 0x8CU;
            }
            in = (uint8_t) (in >> 1);
        }
    }
    return c;
}

static float to_celsius(int16_t raw, uint8_t cfg_byte)
{
    uint8_t r = (uint8_t) ((cfg_byte & CFG_RES_MASK) >> 5);
    switch (r)
    {
        case 0: raw = (int16_t) (raw & (int16_t) 0xFFF8); break;
        case 1: raw = (int16_t) (raw & (int16_t) 0xFFFC); break;
        case 2: raw = (int16_t) (raw & (int16_t) 0xFFFE); break;
        default: break;
    }
    float t = ((float) raw) / 16.0f;
    if (t < DS18B20_TEMP_MIN_C)
    {
        t = DS18B20_TEMP_MIN_C;
    }
    if (t > DS18B20_TEMP_MAX_C)
    {
        t = DS18B20_TEMP_MAX_C;
    }
    return t;
}

/*--------------------------------------------------------------------------------------------------
 * API
 *------------------------------------------------------------------------------------------------*/

void DS18B20_Init(void)
{
    g_uwt_last_err = FSP_ERR_NOT_INITIALIZED;
    delay_us(1000);
    bus_idle();
}

fsp_err_t DS18B20_ConvertT_Start(void)
{
    if (!ow_reset())
    {
        g_uwt_last_err = FSP_ERR_NOT_FOUND;
        return g_uwt_last_err;
    }
    ow_write_byte(OW_SKIP_ROM);
    ow_write_byte(OW_CONVERT_T);
    g_uwt_last_err = FSP_SUCCESS;
    return FSP_SUCCESS;
}

fsp_err_t DS18B20_ReadResult(void)
{
    uint8_t s[9];

    if (!ow_reset())
    {
        g_uwt_last_err = FSP_ERR_NOT_FOUND;
        return g_uwt_last_err;
    }
    ow_write_byte(OW_SKIP_ROM);
    ow_write_byte(OW_READ_SCRATCH);
    for (unsigned i = 0; i < 9U; i++)
    {
        s[i] = ow_read_byte();
    }
    if (s[8] != crc8(s, 8U))
    {
        g_uwt_last_err = FSP_ERR_INVALID_DATA;
        return g_uwt_last_err;
    }

    int16_t raw = (int16_t) ((uint16_t) s[0] | ((uint16_t) s[1] << 8));
    g_uwt_temperature_raw   = raw;
    g_uwt_temperature_c     = to_celsius(raw, s[4]);
    g_uwt_last_err          = FSP_SUCCESS;
    return FSP_SUCCESS;
}

fsp_err_t DS18B20_MeasureBlocking(void)
{
    fsp_err_t e = DS18B20_ConvertT_Start();
    if (FSP_SUCCESS != e)
    {
        return e;
    }
    if (!ow_wait_convert())
    {
        g_uwt_last_err = FSP_ERR_TIMEOUT;
        return g_uwt_last_err;
    }
    return DS18B20_ReadResult();
}
