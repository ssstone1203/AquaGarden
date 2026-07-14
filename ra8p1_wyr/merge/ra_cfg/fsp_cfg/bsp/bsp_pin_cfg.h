/* generated configuration header file - do not edit */
#ifndef BSP_PIN_CFG_H_
#define BSP_PIN_CFG_H_
#include "r_ioport.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

#define SOIL_ADC_A0 (BSP_IO_PORT_00_PIN_00)
#define TDS_ADC_A1 (BSP_IO_PORT_00_PIN_01)
#define PUMP_IN2 (BSP_IO_PORT_01_PIN_04)
#define PUMP_IN1 (BSP_IO_PORT_01_PIN_05)
#define PUMP_VREF (BSP_IO_PORT_02_PIN_06)
#define THS_SDA2 (BSP_IO_PORT_05_PIN_14)
#define THS_SCL2 (BSP_IO_PORT_05_PIN_15)
#define UWS_DQ (BSP_IO_PORT_06_PIN_01)

extern const ioport_cfg_t g_bsp_pin_cfg; /* R7KA8P1KFLCAC.pincfg */

void BSP_PinConfigSecurityInit();

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER
#endif /* BSP_PIN_CFG_H_ */
