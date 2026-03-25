#include "hal_data.h"
#include "pc_control.h"

void hal_entry(void)
{
    R_IOPORT_Open(&g_ioport_ctrl, &g_bsp_pin_cfg);

    PcControl_Init();
    PcControl_Run();
}