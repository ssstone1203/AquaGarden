#include "dev_ds18b20_driver.h"
#include "../ra_gen/common_data.h"
#include "FreeRTOS.h"
#include "task.h"

#define DS18B20_PIN              BSP_IO_PORT_09_PIN_12
#define DS18B20_CMD_SKIP_ROM     (0xCCU)
#define DS18B20_CMD_CONVERT_T    (0x44U)
#define DS18B20_CMD_READ_SCRATCH (0xBEU)
#define DS18B20_CONVERT_MS       (750U)

static uint8_t s_conversion_active;
static TickType_t s_conversion_start_tick;

static void ds18b20_drive_low(void)
{
    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, DS18B20_PIN,
                           IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);
}

static void ds18b20_release(void)
{
    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, DS18B20_PIN,
                           IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE);
}

static uint8_t ds18b20_read_level(void)
{
    bsp_io_level_t level = BSP_IO_LEVEL_LOW;
    (void) R_IOPORT_PinRead(&g_ioport_ctrl, DS18B20_PIN, &level);
    return (level == BSP_IO_LEVEL_HIGH) ? 1U : 0U;
}

static bool ds18b20_reset(void)
{
    bool present;

    ds18b20_drive_low();
    R_BSP_SoftwareDelay(480U, BSP_DELAY_UNITS_MICROSECONDS);
    ds18b20_release();
    R_BSP_SoftwareDelay(70U, BSP_DELAY_UNITS_MICROSECONDS);
    present = (0U == ds18b20_read_level());
    R_BSP_SoftwareDelay(240U, BSP_DELAY_UNITS_MICROSECONDS);

    return present;
}

static void ds18b20_write_bit(uint8_t bit)
{
    ds18b20_drive_low();
    if (0U != bit)
    {
        R_BSP_SoftwareDelay(6U, BSP_DELAY_UNITS_MICROSECONDS);
        ds18b20_release();
        R_BSP_SoftwareDelay(64U, BSP_DELAY_UNITS_MICROSECONDS);
    }
    else
    {
        R_BSP_SoftwareDelay(60U, BSP_DELAY_UNITS_MICROSECONDS);
        ds18b20_release();
        R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MICROSECONDS);
    }
}

static uint8_t ds18b20_read_bit(void)
{
    uint8_t bit;

    ds18b20_drive_low();
    R_BSP_SoftwareDelay(3U, BSP_DELAY_UNITS_MICROSECONDS);
    ds18b20_release();
    R_BSP_SoftwareDelay(12U, BSP_DELAY_UNITS_MICROSECONDS);
    bit = ds18b20_read_level();
    R_BSP_SoftwareDelay(55U, BSP_DELAY_UNITS_MICROSECONDS);

    return bit;
}

static void ds18b20_write_byte(uint8_t value)
{
    for (uint8_t i = 0U; i < 8U; i++)
    {
        ds18b20_write_bit((uint8_t) (value & 0x01U));
        value >>= 1;
    }
}

static uint8_t ds18b20_read_byte(void)
{
    uint8_t value = 0U;

    for (uint8_t i = 0U; i < 8U; i++)
    {
        value >>= 1;
        if (0U != ds18b20_read_bit())
        {
            value |= 0x80U;
        }
    }

    return value;
}

void dev_ds18b20_init(void)
{
    ds18b20_release();
    s_conversion_active = 0U;
}

fsp_err_t dev_ds18b20_measure(float * p_temp_c)
{
    uint8_t temp_l;
    uint8_t temp_h;
    int16_t raw;

    if (NULL == p_temp_c)
    {
        return FSP_ERR_INVALID_POINTER;
    }

    if (0U == s_conversion_active)
    {
        if (!ds18b20_reset())
        {
            return FSP_ERR_NOT_FOUND;
        }

        ds18b20_write_byte(DS18B20_CMD_SKIP_ROM);
        ds18b20_write_byte(DS18B20_CMD_CONVERT_T);
        s_conversion_start_tick = xTaskGetTickCount();
        s_conversion_active = 1U;
        return FSP_ERR_IN_USE;
    }

    if ((xTaskGetTickCount() - s_conversion_start_tick) < pdMS_TO_TICKS(DS18B20_CONVERT_MS))
    {
        return FSP_ERR_IN_USE;
    }

    if (!ds18b20_reset())
    {
        s_conversion_active = 0U;
        return FSP_ERR_NOT_FOUND;
    }

    ds18b20_write_byte(DS18B20_CMD_SKIP_ROM);
    ds18b20_write_byte(DS18B20_CMD_READ_SCRATCH);
    temp_l = ds18b20_read_byte();
    temp_h = ds18b20_read_byte();
    raw = (int16_t) (((uint16_t) temp_h << 8) | temp_l);

    *p_temp_c = (float) raw * 0.0625F;
    s_conversion_active = 0U;
    return FSP_SUCCESS;
}
