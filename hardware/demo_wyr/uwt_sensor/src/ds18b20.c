/* DS18B20 bit-bang on one GPIO; timings follow Dallas / Maxim 1-Wire. */

#include "ds18b20.h"

#include "bsp_api.h"
#include <stddef.h>
#include "common_data.h"
#include "r_ioport.h"

#define OW_CMD_SKIP_ROM       (0xCCU)
#define OW_CMD_CONVERT_T      (0x44U)
#define OW_CMD_READ_SCRATCH   (0xBEU)

#define CFG_RESOLUTION_MASK   (0x60U)                  /* R1:R0 in configuration register byte */
#define OW_CONVERT_TIMEOUT_BITS    (20000U)            /* ~1.2 s worth of read slots @ ~60 µs */

/*--------------------------------------------------------------------------------------------------
 * Local helpers
 *------------------------------------------------------------------------------------------------*/

static void delay_us(uint32_t us)
{
    R_BSP_SoftwareDelay(us, BSP_DELAY_UNITS_MICROSECONDS);
}

static void bus_release(void)
{
    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, DS18B20_DQ_PIN, (uint32_t) IOPORT_CFG_PORT_DIRECTION_INPUT);
}

static void bus_drive_low(void)
{
    uint32_t cfg = (uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_PORT_OUTPUT_LOW;

    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, DS18B20_DQ_PIN, cfg);
}

static uint8_t bus_read_level(void)
{
    bsp_io_level_t lev = BSP_IO_LEVEL_HIGH;

    (void) R_IOPORT_PinRead(&g_ioport_ctrl, DS18B20_DQ_PIN, &lev);

    return (BSP_IO_LEVEL_LOW == lev) ? 0U : 1U;
}

/* Dallas/Maxim 1-Wire CRC-8 (poly 0x31), used on scratchpad bytes 0..7 vs byte 8 */
static uint8_t crc8_dallas(const uint8_t * data, unsigned int len)
{
    uint8_t crc = 0;

    for (unsigned int n = 0; n < len; n++)
    {
        uint8_t inbyte = data[n];
        for (int i = 8; i > 0; i--)
        {
            uint8_t mix = (uint8_t) ((crc ^ inbyte) & 1U);
            crc      = (uint8_t) (crc >> 1);
            if (0U != mix)
            {
                crc ^= 0x8CU;
            }
            inbyte = (uint8_t) (inbyte >> 1);
        }
    }

    return crc;
}

/* 复位脉冲 + 存在脉冲检测；无应答返回 false */
static bool ow_reset(void)
{
    bus_drive_low();
    delay_us(500);
    bus_release();
    delay_us(70);
    if (0U != bus_read_level())           /* 存在时应被器件拉低 */
    {
        delay_us(410);
        return false;
    }
    delay_us(410);
    return true;
}

static void ow_write_bit(uint8_t bit)
{
    if (0U != (bit & 1U))
    {
        bus_drive_low();
        delay_us(6);
        bus_release();
        delay_us(54);
    }
    else
    {
        bus_drive_low();
        delay_us(60);
        bus_release();
        delay_us(1);
    }
}

static uint8_t ow_read_bit(void)
{
    bus_drive_low();
    delay_us(6);
    bus_release();
    delay_us(9);
    {
        uint8_t const b = bus_read_level();
        delay_us(55);
        return b;
    }
}

static void ow_write_byte(uint8_t byte)
{
    for (uint_fast8_t m = 0; m < 8U; m++)
    {
        ow_write_bit((uint8_t) (byte & 1U));
        byte = (uint8_t) (byte >> 1);
    }
}

static uint8_t ow_read_byte(void)
{
    uint8_t v = 0;
    for (uint_fast8_t m = 0; m < 8U; m++)
    {
        if (0U != ow_read_bit())
        {
            v |= (uint8_t) (1U << m);
        }
    }
    return v;
}

/* 转换进行中读时隙返回 0，完成返回 1（见手册 “Convert T”） */
static fsp_err_t ow_wait_convert_done(void)
{
    for (uint32_t n = 0; n < OW_CONVERT_TIMEOUT_BITS; n++)
    {
        if (0U != ow_read_bit())
        {
            return FSP_SUCCESS;
        }
    }
    return FSP_ERR_TIMEOUT;
}

static uint8_t resolution_bits_from_cfg(uint8_t cfg_reg)
{
    uint8_t const r = (uint8_t) ((cfg_reg & CFG_RESOLUTION_MASK) >> 5);
    switch (r)
    {
        case 0: return 9U;
        case 1: return 10U;
        case 2: return 11U;
        default: return 12U;
    }
}

static float raw_to_celsius(int16_t raw, uint8_t resolution_bits)
{
    int16_t adjusted = raw;

    switch (resolution_bits)
    {
        case 9:
            adjusted = (int16_t) (raw & (int16_t) 0xFFF8);
            break;
        case 10:
            adjusted = (int16_t) (raw & (int16_t) 0xFFFC);
            break;
        case 11:
            adjusted = (int16_t) (raw & (int16_t) 0xFFFE);
            break;
        default:
            break;
    }
    return ((float) adjusted) / 16.0f;
}

static float clamp_temp_c(float t)
{
    if (t < DS18B20_TEMP_MIN_C)
    {
        return DS18B20_TEMP_MIN_C;
    }
    if (t > DS18B20_TEMP_MAX_C)
    {
        return DS18B20_TEMP_MAX_C;
    }
    return t;
}

/*--------------------------------------------------------------------------------------------------
 * API
 *------------------------------------------------------------------------------------------------*/

void DS18B20_Init(void)
{
    /* 外设电源建立时间 + 总线空闲为高（释放） */
    delay_us(1000);
    bus_release();
}

fsp_err_t DS18B20_ReadTemperatureC(float * p_temp_c, int16_t * p_raw_opt)
{
    uint8_t   scratch[9];
    fsp_err_t err;

    if (false == ow_reset())
    {
        return FSP_ERR_NOT_FOUND;
    }

    ow_write_byte(OW_CMD_SKIP_ROM);
    ow_write_byte(OW_CMD_CONVERT_T);

    err = ow_wait_convert_done();
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    if (false == ow_reset())
    {
        return FSP_ERR_NOT_FOUND;
    }

    ow_write_byte(OW_CMD_SKIP_ROM);
    ow_write_byte(OW_CMD_READ_SCRATCH);

    for (unsigned int i = 0; i < 9U; i++)
    {
        scratch[i] = ow_read_byte();
    }

    if (scratch[8] != crc8_dallas(scratch, 8U))
    {
        return FSP_ERR_INVALID_DATA;
    }

    {
        int16_t const raw_temp = (int16_t) ((uint16_t) scratch[0] | ((uint16_t) scratch[1] << 8));
        float         t        = raw_to_celsius(raw_temp, resolution_bits_from_cfg(scratch[4]));

        t = clamp_temp_c(t);

        if (NULL != p_raw_opt)
        {
            *p_raw_opt = raw_temp;
        }
        if (NULL != p_temp_c)
        {
            *p_temp_c = t;
        }
    }

    return FSP_SUCCESS;
}
