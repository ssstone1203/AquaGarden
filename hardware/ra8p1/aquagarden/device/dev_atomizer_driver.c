#include "dev_atomizer_driver.h"
#include "../ra_gen/common_data.h"

#define ATOMIZER_PIN BSP_IO_PORT_04_PIN_02

static uint8_t s_atomizer_state;

void dev_atomizer_init(void)
{
    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, ATOMIZER_PIN,
                           IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);
    s_atomizer_state = 0U;
}

fsp_err_t dev_atomizer_set(uint8_t state)
{
    fsp_err_t err;

    state = (0U == state) ? 0U : 1U;
    err = R_IOPORT_PinWrite(&g_ioport_ctrl, ATOMIZER_PIN,
                            (0U == state) ? BSP_IO_LEVEL_LOW : BSP_IO_LEVEL_HIGH);
    if (FSP_SUCCESS == err)
    {
        s_atomizer_state = state;
    }

    return err;
}

uint8_t dev_atomizer_get(void)
{
    return s_atomizer_state;
}
