#include "dev_sht30_driver.h"
#include "../ra_gen/main_service.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

#define SHT30_CMD_MEASURE_H_MSB (0x24U)
#define SHT30_CMD_MEASURE_H_LSB (0x00U)
#define SHT30_PERIOD_MS         (1000U)
#define SHT30_CONVERT_MS        (20U)
#define SHT30_STEP_TIMEOUT_MS   (200U)

typedef enum e_sht30_state
{
    SHT30_STATE_IDLE = 0,
    SHT30_STATE_TX_WAIT,
    SHT30_STATE_CONVERTING,
    SHT30_STATE_RX_WAIT,
} sht30_state_t;

static bool s_opened;
static volatile i2c_master_event_t s_i2c_event;
static i2c_master_callback_args_t s_i2c_callback_memory;

static sht30_state_t s_state;
static TickType_t    s_step_tick;
static TickType_t    s_period_tick;
static uint8_t       s_rx[6];
static float         s_temp_c;
static float         s_humi_pct;
static bool          s_valid;
static uint16_t      s_fail_count;

static void sht30_i2c_callback(i2c_master_callback_args_t * p_args)
{
    if (NULL != p_args)
    {
        s_i2c_event = p_args->event;
    }
}

static uint8_t sht30_crc8(const uint8_t * p_data, uint8_t len)
{
    uint8_t crc = 0xFFU;

    for (uint8_t i = 0U; i < len; i++)
    {
        crc ^= p_data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            crc = (0U != (crc & 0x80U)) ? (uint8_t) ((crc << 1) ^ 0x31U) : (uint8_t) (crc << 1);
        }
    }

    return crc;
}

static void sht30_fail(void)
{
    if (s_fail_count < UINT16_MAX)
    {
        s_fail_count++;
    }
    s_state = SHT30_STATE_IDLE;
    s_period_tick = xTaskGetTickCount();
}

static bool sht30_elapsed(TickType_t start, uint32_t ms)
{
    return (uint32_t) (xTaskGetTickCount() - start) >= pdMS_TO_TICKS(ms);
}

void dev_sht30_init(void)
{
    if (!s_opened)
    {
        if (FSP_SUCCESS == g_i2c_master0.p_api->open(g_i2c_master0.p_ctrl, g_i2c_master0.p_cfg))
        {
            (void) g_i2c_master0.p_api->callbackSet(g_i2c_master0.p_ctrl,
                                                    sht30_i2c_callback,
                                                    NULL,
                                                    &s_i2c_callback_memory);
            s_opened = true;
        }
    }
    s_state = SHT30_STATE_IDLE;
    s_valid = false;
    s_period_tick = xTaskGetTickCount();
}

void dev_sht30_process(void)
{
    uint8_t cmd[2] = {SHT30_CMD_MEASURE_H_MSB, SHT30_CMD_MEASURE_H_LSB};
    fsp_err_t err;

    if (!s_opened)
    {
        dev_sht30_init();
        if (!s_opened)
        {
            return;
        }
    }

    /* Guard against a stuck transaction (callback never arrives). */
    if ((SHT30_STATE_IDLE != s_state) && sht30_elapsed(s_step_tick, SHT30_STEP_TIMEOUT_MS))
    {
        sht30_fail();
        return;
    }

    switch (s_state)
    {
        case SHT30_STATE_IDLE:
            if (sht30_elapsed(s_period_tick, SHT30_PERIOD_MS))
            {
                s_i2c_event = I2C_MASTER_EVENT_START;
                err = g_i2c_master0.p_api->write(g_i2c_master0.p_ctrl, cmd, sizeof(cmd), false);
                if (FSP_SUCCESS == err)
                {
                    s_step_tick = xTaskGetTickCount();
                    s_state = SHT30_STATE_TX_WAIT;
                }
                else
                {
                    sht30_fail();
                }
            }
            break;

        case SHT30_STATE_TX_WAIT:
            if (I2C_MASTER_EVENT_TX_COMPLETE == s_i2c_event)
            {
                s_step_tick = xTaskGetTickCount();
                s_state = SHT30_STATE_CONVERTING;
            }
            else if (I2C_MASTER_EVENT_ABORTED == s_i2c_event)
            {
                sht30_fail();
            }
            break;

        case SHT30_STATE_CONVERTING:
            if (sht30_elapsed(s_step_tick, SHT30_CONVERT_MS))
            {
                s_i2c_event = I2C_MASTER_EVENT_START;
                err = g_i2c_master0.p_api->read(g_i2c_master0.p_ctrl, s_rx, sizeof(s_rx), false);
                if (FSP_SUCCESS == err)
                {
                    s_step_tick = xTaskGetTickCount();
                    s_state = SHT30_STATE_RX_WAIT;
                }
                else
                {
                    sht30_fail();
                }
            }
            break;

        case SHT30_STATE_RX_WAIT:
            if (I2C_MASTER_EVENT_RX_COMPLETE == s_i2c_event)
            {
                if ((sht30_crc8(&s_rx[0], 2U) == s_rx[2]) && (sht30_crc8(&s_rx[3], 2U) == s_rx[5]))
                {
                    uint16_t raw_t = (uint16_t) (((uint16_t) s_rx[0] << 8) | s_rx[1]);
                    uint16_t raw_h = (uint16_t) (((uint16_t) s_rx[3] << 8) | s_rx[4]);
                    float humi;

                    s_temp_c = (-45.0F) + (175.0F * (float) raw_t / 65535.0F);
                    humi = 100.0F * (float) raw_h / 65535.0F;
                    if (humi > 100.0F)
                    {
                        humi = 100.0F;
                    }
                    if (humi < 0.0F)
                    {
                        humi = 0.0F;
                    }
                    s_humi_pct = humi;
                    s_valid = true;
                    s_state = SHT30_STATE_IDLE;
                    s_period_tick = xTaskGetTickCount();
                }
                else
                {
                    sht30_fail();
                }
            }
            else if (I2C_MASTER_EVENT_ABORTED == s_i2c_event)
            {
                sht30_fail();
            }
            break;

        default:
            s_state = SHT30_STATE_IDLE;
            break;
    }
}

bool dev_sht30_get(float * p_temp_c, float * p_humi_pct)
{
    if ((NULL == p_temp_c) || (NULL == p_humi_pct) || !s_valid)
    {
        return false;
    }

    *p_temp_c = s_temp_c;
    *p_humi_pct = s_humi_pct;
    return true;
}

uint16_t dev_sht30_get_fail_count(void)
{
    return s_fail_count;
}
