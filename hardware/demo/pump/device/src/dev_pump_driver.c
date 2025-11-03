#include "dev_pump_driver.h"
#include "dev_pump_xdefine.h"
#include <stdlib.h>

/**
 * @brief Initialize pump to proper work mode.
 * @param pump_set structure ptr
 */
void Pump_Init(pump_set_t* pump_set)
{
    if (pump_set->pump_in2_cfg_set != NULL)
    {
        free(pump_set->pump_in2_cfg_set);
    }
    pump_set->pump_in2_cfg_set = (pump_in2_cfg_t*)malloc(sizeof(pump_in2_cfg_t));

    pump_set->pump_in2_cfg_set = &g_pump_timer_ctrl;
	pump_set->flowrate_set = PUMP_FLOWRATE_MIN;
	pump_set->pump_in1_level_set = PUMP_IN1_RESET;
	
	R_GPT_Open(pump_set->pump_in2_cfg_set, &g_pump_timer_cfg);
	R_GPT_DutyCycleSet(pump_set->pump_in2_cfg_set, pump_set->flowrate_set, PUMP_IN2);
	R_IOPORT_PinWrite(&g_ioport_ctrl, PUMP_IN1, PUMP_IN1_SET);
}

/**
 * @brief Set flowrate of pump.
 * @param pump_set structure ptr
 * @param flowrate value, limited up to @refer PUMP_FLOWRATE_MAX
 */
void Pump_FlowrateSet(pump_set_t* pump_set, uint16_t flwrt_set)
{
	if(flwrt_set <= PUMP_FLOWRATE_MAX)
	{
		pump_set->flowrate_set = flwrt_set;

		R_GPT_DutyCycleSet(pump_set->pump_in2_cfg_set,
		                   g_pump_timer_cfg.period_counts - pump_set->flowrate_set,
		                   PUMP_IN2);
	}
	else
	{
	    pump_set->flowrate_set = PUMP_FLOWRATE_MAX;

	    R_GPT_DutyCycleSet(pump_set->pump_in2_cfg_set,
	                       g_pump_timer_cfg.period_counts - pump_set->flowrate_set,
	                       PUMP_IN2);
	}
	pump_set->flowrate_active = flwrt_set;
}

/**
 * @brief Set mode of pump.
 * @param pump_set structure ptr
 * @param pump mode
*		@arg PUMP_SLEEP: Pump into sleep-mode
*		@arg PUMP_BRAKE: Pump into brake
*		@arg PUMP_ACTIVE: Pump active
 */
void Pump_ModeSet(pump_set_t* pump_set, pump_modeset_e pump_mdset)
{
	switch(pump_mdset)
	{
		case PUMP_SLEEP:
		{
			pump_set->flowrate_set = PUMP_FLOWRATE_MIN;
			pump_set->pump_in1_level_set = PUMP_IN1_RESET;
			R_GPT_DutyCycleSet(&g_pump_timer_ctrl, pump_set->flowrate_set, PUMP_IN2);
			R_IOPORT_PinWrite(&g_ioport_ctrl, PUMP_IN1, pump_set->pump_in1_level_set);
			break;
		}
		case PUMP_BRAKE:
		{
			pump_set->flowrate_set = PUMP_FLOWRATE_MAX;
			pump_set->pump_in1_level_set = PUMP_IN1_SET;
			R_GPT_DutyCycleSet(&g_pump_timer_ctrl, pump_set->flowrate_set, PUMP_IN2);
			R_IOPORT_PinWrite(&g_ioport_ctrl, PUMP_IN1, pump_set->pump_in1_level_set);
			break;
		}
		case PUMP_ACTIVE:
		{
		    pump_set->flowrate_set = pump_set->flowrate_active;
			pump_set->pump_in1_level_set = PUMP_IN1_SET;
			R_GPT_DutyCycleSet(&g_pump_timer_ctrl, pump_set->flowrate_set, PUMP_IN2);
			R_IOPORT_PinWrite(&g_ioport_ctrl, PUMP_IN1, pump_set->pump_in1_level_set);
			break;
		}
		default:
		{
			Pump_EMO(pump_set);
			break;
		}
	}
}

/**
 * @brief Start pump.
 * @param pump_set structure ptr
 */
void Pump_Start(pump_set_t* pump_set)
{
    R_GPT_Start(pump_set->pump_in2_cfg_set);
}

/**
 * @brief Stop pump.
 * @param pump_set structure ptr
 */
void Pump_Stop(pump_set_t* pump_set)
{
    pump_set->pump_in1_level_set = PUMP_IN1_SET;
    R_IOPORT_PinWrite(pump_set->pump_in2_cfg_set, PUMP_IN1, pump_set->pump_in1_level_set);
    R_GPT_Stop(pump_set->pump_in2_cfg_set);
}

/**	
 * @brief Emergency off pump when encounter man-made error or other errors.
 * @param pump_set structure ptr
 * @note This function should not be used in other files.
 */
static void Pump_EMO(pump_set_t* pump_set)
{
	pump_set->flowrate_set = PUMP_FLOWRATE_MAX;
	pump_set->pump_in1_level_set = PUMP_IN1_SET;
	R_GPT_DutyCycleSet(&g_pump_timer_ctrl, pump_set->flowrate_set, PUMP_IN2);
	R_IOPORT_PinWrite(&g_ioport_ctrl, PUMP_IN1, pump_set->pump_in1_level_set);
}
