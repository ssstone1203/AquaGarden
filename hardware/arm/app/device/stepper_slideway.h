#ifndef _STEPPER_SLIDEWAY_H_
#define _STEPPER_SLIDEWAY_H_

#include "hal_data.h"

#define DEVICE_ADDR						0x35
#define MOTOR_STEPS_DRIVER_MODE_REG		0x15
#define MOTOR_AUTO_REPOSITION_REG		0x16
#define MOTOR_STEPS_REG					0x18
#define MOTOR_STEPS_TIME_REG			0x1C

#define SUBDIVISION_NONE				0x00
#define SUBDIVISION_2					0x01
#define SUBDIVISION_4					0x02
#define SUBDIVISION_8					0x03
#define SUBDIVISION_16					0x07

/* 8细分度下的最大总步数 */
#define MAX_DIV_8_STEPS					10400

typedef struct  
{
	uint16_t dev_i2c_addr;
	uint8_t reposition_set;
	uint8_t subdivision_set;
	int32_t step_move_set;
	uint8_t is_reset;
}stepper_ctrl_t;

void Stepper_Init(stepper_ctrl_t* stepper_ctrl);
int8_t Stepper_RepositionRead(stepper_ctrl_t* stepper_ctrl);
void Stepper_RepositionSet(stepper_ctrl_t* stepper_ctrl, uint8_t reposition_set);
void Stepper_SubdivisionSet(stepper_ctrl_t* stepper_ctrl, uint8_t subdivision_set);
void Stepper_StepMoveSet(stepper_ctrl_t* stepper_ctrl, int32_t step_move_set);
#endif