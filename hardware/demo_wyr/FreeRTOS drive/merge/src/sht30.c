#include "sht30.h"
#include "THS_Task.h"

#include "bsp_api.h"

#define SHT30_CMD_SINGLE_SHOT_HIGH_CS_OFF_MS     (0x24U)
#define SHT30_CMD_SINGLE_SHOT_HIGH_CS_OFF_LS     (0x00U)
#define SHT30_MEASURE_DELAY_HIGH_REPEATABILITY_MS (55U)
#define SHT30_I2C_WAIT_TIMEOUT_MS                 (200U)

volatile float g_sht30_temperature_c   = 0.0F;
volatile float g_sht30_temperature_f   = 0.0F;
volatile float g_sht30_humidity_rh     = 0.0F;
volatile uint16_t g_sht30_temperature_raw;
volatile uint16_t g_sht30_humidity_raw;
volatile uint32_t g_sht30_last_i2c_stage = 0U;
volatile uint32_t g_sht30_last_status = (uint32_t) FSP_ERR_NOT_INITIALIZED;

static i2c_master_instance_t const * gp_i2c;
static uint8_t                       g_slave_7bit;

static volatile uint8_t g_i2c_wait_state;

static uint8_t sht30_crc8(const uint8_t * data, uint8_t len);
static float     sht30_clampf(float v, float lo, float hi);
static fsp_err_t i2c_wait(void);
static fsp_err_t i2c_write_cmd(const uint8_t * cmd, uint32_t len);
static fsp_err_t i2c_read_bytes(uint8_t * buf, uint32_t len);

void THS_I2CMaster_CpltCallback(i2c_master_callback_args_t * p_args)
{
    switch (p_args->event)
    {
        case I2C_MASTER_EVENT_TX_COMPLETE:
        case I2C_MASTER_EVENT_RX_COMPLETE:
            g_i2c_wait_state = 0;
            break;
        case I2C_MASTER_EVENT_ABORTED:
            g_i2c_wait_state = 2;
            break;
        default:
            break;
    }
}

static uint8_t sht30_crc8(const uint8_t * data, uint8_t len)
{
    uint8_t crc = 0xFF;

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            if (crc & 0x80U)
            {
                crc = (uint8_t) (((unsigned int) crc << 1U) ^ 0x31U);
            }
            else
            {
                crc = (uint8_t) ((unsigned int) crc << 1U);
            }
        }
    }

    return crc;
}

static float sht30_clampf(float v, float lo, float hi)
{
    if (v < lo)
    {
        return lo;
    }

    if (v > hi)
    {
        return hi;
    }

    return v;
}

static fsp_err_t i2c_wait(void)
{
    uint32_t ms_left = SHT30_I2C_WAIT_TIMEOUT_MS;

    while (ms_left > 0U)
    {
        if (0U == g_i2c_wait_state)
        {
            return FSP_SUCCESS;
        }

        if (2U == g_i2c_wait_state)
        {
            return FSP_ERR_ABORTED;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        ms_left--;
    }

    (void) gp_i2c->p_api->abort(gp_i2c->p_ctrl);

    return FSP_ERR_TIMEOUT;
}

static fsp_err_t i2c_write_cmd(const uint8_t * cmd, uint32_t len)
{
    g_sht30_last_i2c_stage = 1U; /* write command */
    g_i2c_wait_state = 1;

    fsp_err_t err = gp_i2c->p_api->slaveAddressSet(gp_i2c->p_ctrl, g_slave_7bit, I2C_MASTER_ADDR_MODE_7BIT);
    if (FSP_SUCCESS != err)
    {
        g_i2c_wait_state = 0;

        return err;
    }

    err = gp_i2c->p_api->write(gp_i2c->p_ctrl, (uint8_t *) cmd, len, false);
    if (FSP_SUCCESS != err)
    {
        g_i2c_wait_state = 0;

        g_sht30_last_status = (uint32_t) err;
        return err;
    }

    err = i2c_wait();
    g_sht30_last_status = (uint32_t) err;
    return err;
}

static fsp_err_t i2c_read_bytes(uint8_t * buf, uint32_t len)
{
    g_sht30_last_i2c_stage = 2U; /* read bytes */
    g_i2c_wait_state = 1;

    fsp_err_t err = gp_i2c->p_api->slaveAddressSet(gp_i2c->p_ctrl, g_slave_7bit, I2C_MASTER_ADDR_MODE_7BIT);
    if (FSP_SUCCESS != err)
    {
        g_i2c_wait_state = 0;

        return err;
    }

    err = gp_i2c->p_api->read(gp_i2c->p_ctrl, buf, len, false);
    if (FSP_SUCCESS != err)
    {
        g_i2c_wait_state = 0;

        g_sht30_last_status = (uint32_t) err;
        return err;
    }

    err = i2c_wait();
    g_sht30_last_status = (uint32_t) err;
    return err;
}

fsp_err_t sht30_init(i2c_master_instance_t const * p_i2c, uint8_t i2c_address_7bit)
{
    if ((NULL == p_i2c) || (NULL == p_i2c->p_api) || (NULL == p_i2c->p_ctrl) || (NULL == p_i2c->p_cfg))
    {
        g_sht30_last_status = (uint32_t) FSP_ERR_ASSERTION;

        return FSP_ERR_ASSERTION;
    }

    gp_i2c       = p_i2c;
    g_slave_7bit = i2c_address_7bit & 0x7FU;

    fsp_err_t err = gp_i2c->p_api->open(gp_i2c->p_ctrl, gp_i2c->p_cfg);
    g_sht30_last_status = (uint32_t) err;

    return err;
}

void sht30_set_address_7bit(uint8_t i2c_address_7bit)
{
    /* ADDR 须接固定电平；若硬件改线可调用本函数切换 0x44/0x45，无需重新 open I2C。 */
    g_slave_7bit = i2c_address_7bit & 0x7FU;
}

fsp_err_t sht30_measure_single_shot(float * temperature_c, float * humidity_rh_percent, float * temperature_f,
    bool verify_crc)
{
    if (NULL == gp_i2c)
    {
        g_sht30_last_status = (uint32_t) FSP_ERR_ASSERTION;

        return FSP_ERR_ASSERTION;
    }

    /* Ensure bus/driver is idle before starting a new transfer.
     * Without this, repeated back-to-back transfers may hit FSP_ERR_IN_USE.
     */
    (void) gp_i2c->p_api->abort(gp_i2c->p_ctrl);
    R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);

    g_sht30_last_i2c_stage = 0U;

    uint8_t cmd[2] = {SHT30_CMD_SINGLE_SHOT_HIGH_CS_OFF_MS, SHT30_CMD_SINGLE_SHOT_HIGH_CS_OFF_LS};

    fsp_err_t err = i2c_write_cmd(cmd, 2U);
    if (FSP_SUCCESS != err)
    {
        g_sht30_last_status = (uint32_t) err;

        return err;
    }

    R_BSP_SoftwareDelay(SHT30_MEASURE_DELAY_HIGH_REPEATABILITY_MS, BSP_DELAY_UNITS_MILLISECONDS);

    uint8_t raw[6];
    err = i2c_read_bytes(raw, sizeof(raw));
    if (FSP_SUCCESS != err)
    {
        g_sht30_last_status = (uint32_t) err;

        return err;
    }

    if (verify_crc)
    {
        if (sht30_crc8(&raw[0], 2U) != raw[2])
        {
            g_sht30_last_status = (uint32_t) FSP_ERR_INVALID_DATA;

            return FSP_ERR_INVALID_DATA;
        }

        if (sht30_crc8(&raw[3], 2U) != raw[5])
        {
            g_sht30_last_status = (uint32_t) FSP_ERR_INVALID_DATA;

            return FSP_ERR_INVALID_DATA;
        }
    }

    uint16_t t_ticks  = (uint16_t) (((uint16_t) raw[0] << 8) | raw[1]);
    uint16_t rh_ticks = (uint16_t) (((uint16_t) raw[3] << 8) | raw[4]);

    g_sht30_temperature_raw = t_ticks;
    g_sht30_humidity_raw    = rh_ticks;

    /* 先用手册公式，再饱和到规格显示范围（公式可达范围见 sht30.h 中 FORMULA / SPEC 说明）。 */
    float const t_unc  = -45.0F + (175.0F * (float) t_ticks / 65535.0F);
    float const rh_unc = 100.0F * (float) rh_ticks / 65535.0F;
    float const t_c    = sht30_clampf(t_unc, SHT30_TEMP_C_SPEC_MIN, SHT30_TEMP_C_SPEC_MAX);
    float const rh_pct = sht30_clampf(rh_unc, SHT30_RH_PCT_SPEC_MIN, SHT30_RH_PCT_SPEC_MAX);
    float const t_f    = (t_c * 9.0F / 5.0F) + 32.0F;

    if (NULL != temperature_c)
    {
        *temperature_c = t_c;
    }

    if (NULL != humidity_rh_percent)
    {
        *humidity_rh_percent = rh_pct;
    }

    g_sht30_temperature_c = t_c;
    g_sht30_temperature_f = t_f;
    g_sht30_humidity_rh   = rh_pct;

    if (NULL != temperature_f)
    {
        *temperature_f = t_f;
    }
    g_sht30_last_status   = (uint32_t) FSP_SUCCESS;

    return FSP_SUCCESS;
}
