#include "dev_sht30_driver.h"
#include "../ra_gen/main_service.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* Match hardware/demo/th_sensor: periodic 1 mps high repeatability + fetch. */
#define SHT30_CMD_PERIODIC_START_MSB (0x21U)
#define SHT30_CMD_PERIODIC_START_LSB (0x30U)
#define SHT30_CMD_FETCH_MSB          (0xE0U)
#define SHT30_CMD_FETCH_LSB          (0x00U)
#define SHT30_PERIOD_MS              (1000U)
#define SHT30_STEP_TIMEOUT_MS        (200U)

typedef enum e_sht30_state
{
    SHT30_STATE_NEED_START = 0, /* send 0x2130 once (demo THS_PeriodicModeStart) */
    SHT30_STATE_START_TX_WAIT,
    SHT30_STATE_IDLE,           /* wait 1 s then fetch (demo loop delay) */
    SHT30_STATE_FETCH_TX_WAIT,  /* after writing 0xE000 */
    SHT30_STATE_RX_WAIT,        /* reading 6 bytes */
} sht30_state_t;

static bool s_opened;
static volatile i2c_master_event_t s_i2c_event;
static i2c_master_callback_args_t s_i2c_callback_memory;

static sht30_state_t s_state;
static TickType_t    s_step_tick;
static TickType_t    s_period_tick;
static uint8_t       s_tx_cmd[2]; /* must outlive async I2C write (demo uses ths_cmd_byte) */
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

static bool sht30_elapsed(TickType_t start, uint32_t ms)
{
    return (uint32_t) (xTaskGetTickCount() - start) >= pdMS_TO_TICKS(ms);
}

static void sht30_abort_bus(void)
{
    if (s_opened)
    {
        (void) g_i2c_master0.p_api->abort(g_i2c_master0.p_ctrl);
    }
}

static void sht30_fail(void)
{
    if (s_fail_count < UINT16_MAX)
    {
        s_fail_count++;
    }

    sht30_abort_bus();

    /* Re-issue periodic start like a recovered demo bring-up. */
    s_state = SHT30_STATE_NEED_START;
    s_period_tick = xTaskGetTickCount();
}

static fsp_err_t sht30_write_cmd(uint8_t msb, uint8_t lsb)
{
    /* FSP I2C write is async; buffer must remain valid until TX_COMPLETE. */
    s_tx_cmd[0] = msb;
    s_tx_cmd[1] = lsb;
    s_i2c_event = I2C_MASTER_EVENT_START;
    return g_i2c_master0.p_api->write(g_i2c_master0.p_ctrl, s_tx_cmd, sizeof(s_tx_cmd), false);
}

static void sht30_apply_demo_conversion(void)
{
    /* Same formulas as hardware/demo/th_sensor THS_PeriodicDataRead (no CRC gate). */
    uint16_t raw_t = (uint16_t) (((uint16_t) s_rx[0] << 8) | s_rx[1]);
    uint16_t raw_h = (uint16_t) (((uint16_t) s_rx[3] << 8) | s_rx[4]);

    s_temp_c = (-45.0F) + (175.0F * (float) raw_t / 65535.0F);
    s_humi_pct = 100.0F * (float) raw_h / 65535.0F;
    s_valid = true;
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

    s_state = SHT30_STATE_NEED_START;
    s_valid = false;
    s_period_tick = xTaskGetTickCount();
}

void dev_sht30_process(void)
{
    fsp_err_t err;

    if (!s_opened)
    {
        dev_sht30_init();
        if (!s_opened)
        {
            return;
        }
    }

    if ((SHT30_STATE_NEED_START != s_state) &&
        (SHT30_STATE_IDLE != s_state) &&
        sht30_elapsed(s_step_tick, SHT30_STEP_TIMEOUT_MS))
    {
        sht30_fail();
        return;
    }

    switch (s_state)
    {
        case SHT30_STATE_NEED_START:
            /* After a failure, back off 1 s (same cadence as demo's loop delay). */
            if ((0U != s_fail_count) && !sht30_elapsed(s_period_tick, SHT30_PERIOD_MS))
            {
                break;
            }

            /* demo: THS_PeriodicModeStart(..., HIGH) -> 0x2130 */
            err = sht30_write_cmd(SHT30_CMD_PERIODIC_START_MSB, SHT30_CMD_PERIODIC_START_LSB);
            if (FSP_SUCCESS == err)
            {
                s_step_tick = xTaskGetTickCount();
                s_state = SHT30_STATE_START_TX_WAIT;
            }
            else
            {
                sht30_fail();
            }
            break;

        case SHT30_STATE_START_TX_WAIT:
            if (I2C_MASTER_EVENT_TX_COMPLETE == s_i2c_event)
            {
                s_period_tick = xTaskGetTickCount();
                s_state = SHT30_STATE_IDLE;
            }
            else if (I2C_MASTER_EVENT_ABORTED == s_i2c_event)
            {
                sht30_fail();
            }
            break;

        case SHT30_STATE_IDLE:
            if (sht30_elapsed(s_period_tick, SHT30_PERIOD_MS))
            {
                /* demo: THS_PeriodicDataRead write 0xE000 then read 6 bytes.
                 * On IIC master we must wait TX complete before read (unlike demo's fire-and-forget). */
                err = sht30_write_cmd(SHT30_CMD_FETCH_MSB, SHT30_CMD_FETCH_LSB);
                if (FSP_SUCCESS == err)
                {
                    s_step_tick = xTaskGetTickCount();
                    s_state = SHT30_STATE_FETCH_TX_WAIT;
                }
                else
                {
                    sht30_fail();
                }
            }
            break;

        case SHT30_STATE_FETCH_TX_WAIT:
            if (I2C_MASTER_EVENT_TX_COMPLETE == s_i2c_event)
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
            else if (I2C_MASTER_EVENT_ABORTED == s_i2c_event)
            {
                sht30_fail();
            }
            break;

        case SHT30_STATE_RX_WAIT:
            if (I2C_MASTER_EVENT_RX_COMPLETE == s_i2c_event)
            {
                sht30_apply_demo_conversion();
                s_state = SHT30_STATE_IDLE;
                s_period_tick = xTaskGetTickCount();
            }
            else if (I2C_MASTER_EVENT_ABORTED == s_i2c_event)
            {
                sht30_fail();
            }
            break;

        default:
            s_state = SHT30_STATE_NEED_START;
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
