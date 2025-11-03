#ifndef DEV_PUMP_DRIVER_H_
#define DEV_PUMP_DRIVER_H_

#include "hal_data.h"
#include "common_data.h"

typedef bsp_io_level_t pump_io_level_e;
typedef gpt_instance_ctrl_t pump_in2_cfg_t;

/* Private enum---------*/
typedef enum
{
	PUMP_SLEEP = 0,
	PUMP_ACTIVE = 1,
	PUMP_BRAKE = 2,
	
}pump_modeset_e;

/* Private struct----------*/
typedef struct
{
	uint32_t flowrate_set;
	uint32_t flowrate_active;
	pump_io_level_e pump_in1_level_set;
	pump_in2_cfg_t* pump_in2_cfg_set;
	
}pump_set_t;

/* Pump_Imported function------------------------------------------------------*/
static void Pump_EMO(pump_set_t* pump_set);

/* Pump_Exported function------------------------------------------------------*/
void Pump_Init(pump_set_t* pump_set);
void Pump_FlowrateSet(pump_set_t* pump_set, uint16_t flwrt_set);
void Pump_ModeSet(pump_set_t* pump_set, pump_modeset_e pump_mdset);
void Pump_Start(pump_set_t* pump_set);

#endif
