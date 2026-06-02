/*
 * atomizer.c
 *
 *  Created on: 2026年6月2日
 *      Author: david
 */

#include "atomizer.h"

void atom_init()
{
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_06_PIN_01, BSP_IO_LEVEL_LOW);
}

void atom_on()
{
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_06_PIN_01, BSP_IO_LEVEL_HIGH);
}

void atom_off()
{
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_06_PIN_01, BSP_IO_LEVEL_LOW);
}
