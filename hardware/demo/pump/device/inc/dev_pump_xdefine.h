#ifndef PUMP_XDEFINE_H
#define	PUMP_XDEFINE_H

/* pump flowrate set limit--------------*/
#define		PUMP_FLOWRATE_MAX		(g_pump_timer_cfg.period_counts)
#define		PUMP_FLOWRATE_MIN		0

/* pump IN1 status set---------------------*/
#define		PUMP_IN1_SET		BSP_IO_LEVEL_HIGH
#define		PUMP_IN1_RESET		BSP_IO_LEVEL_LOW

#endif
