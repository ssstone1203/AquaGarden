/* generated HAL source file - do not edit */
#include "hal_data.h"
iic_b_master_instance_ctrl_t g_i2c_master0_ctrl;
const iic_b_master_extended_cfg_t g_i2c_master0_extend =
{
    .timeout_mode             = IIC_B_MASTER_TIMEOUT_MODE_SHORT,
    .timeout_scl_low          = IIC_B_MASTER_TIMEOUT_SCL_LOW_ENABLED,
    .smbus_operation         = 0,
    /* Actual calculated bitrate: 99800. Actual calculated duty cycle: 50%. */ .clock_settings.brl_value = 244, .clock_settings.brh_value = 243, .clock_settings.cks_value = 2, .clock_settings.sdod_value = 0, .clock_settings.sdodcs_value = 0, .iic_clock_freq = 50000000,
};
const i2c_master_cfg_t g_i2c_master0_cfg =
{
    .channel             = 0,
    .rate                = I2C_MASTER_RATE_STANDARD,
    .slave               = 0x45,
    .addr_mode           = I2C_MASTER_ADDR_MODE_7BIT,
#define RA_NOT_DEFINED (1)
#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
                .p_transfer_tx       = NULL,
#else
                .p_transfer_tx       = &RA_NOT_DEFINED,
#endif
#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
                .p_transfer_rx       = NULL,
#else
                .p_transfer_rx       = &RA_NOT_DEFINED,
#endif
#undef RA_NOT_DEFINED
    .p_callback          = g_iic_b_master0_callback,
    .p_context           = NULL,
#if defined(VECTOR_NUMBER_IICB0_RXI)
    .rxi_irq             = VECTOR_NUMBER_IICB0_RXI,
#else
    .rxi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_TXI)
    .txi_irq             = VECTOR_NUMBER_IICB0_TXI,
#else
    .txi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_TEND)
    .tei_irq             = VECTOR_NUMBER_IICB0_TEND,
#elif defined(VECTOR_NUMBER_IICB0_TEI)
    .tei_irq             = VECTOR_NUMBER_IICB0_TEI,
#else
    .tei_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_EEI)
    .eri_irq             = VECTOR_NUMBER_IICB0_EEI,
#elif defined(VECTOR_NUMBER_IICB0_ERI)
    .eri_irq             = VECTOR_NUMBER_IICB0_ERI,
#else
    .eri_irq             = FSP_INVALID_VECTOR,
#endif
    .ipl                 = (12),
    .p_extend            = &g_i2c_master0_extend,
};
/* Instance structure to use this module. */
const i2c_master_instance_t g_i2c_master0 =
{
    .p_ctrl        = &g_i2c_master0_ctrl,
    .p_cfg         = &g_i2c_master0_cfg,
    .p_api         = &g_i2c_master_on_iic_b
};
void g_hal_init(void) {
g_common_init();
}
