#include "stepper_slideway.h"
#include "string.h"
#include "bsp_api.h"

void Stepper_Init(stepper_ctrl_t* stepper_ctrl)
{
	if (stepper_ctrl == NULL)
	{
		return;
	}

	R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);

	memset(stepper_ctrl, 0, sizeof(stepper_ctrl_t));
	stepper_ctrl->dev_i2c_addr = DEVICE_ADDR;

	if (FSP_SUCCESS != R_SCI_I2C_Open(&g_stepper_i2c_ctrl, &g_stepper_i2c_cfg))
	{
		return;
	}

	if (Stepper_RepositionRead(stepper_ctrl) == 0)
	{
		Stepper_RepositionSet(stepper_ctrl, 1);
		for (;;)
		{
			R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);
			if (Stepper_RepositionRead(stepper_ctrl) == 1)
			{
				Stepper_SubdivisionSet(stepper_ctrl, SUBDIVISION_8);
				break;
			}
		}
	}
}

int8_t Stepper_RepositionRead(stepper_ctrl_t* stepper_ctrl)
{
	if (stepper_ctrl == NULL)
	{
		return -1;
	}

	(void)R_SCI_I2C_SlaveAddressSet(&g_stepper_i2c_ctrl,
	                                (uint32_t)stepper_ctrl->dev_i2c_addr,
	                                I2C_MASTER_ADDR_MODE_7BIT);

	uint8_t reg = MOTOR_AUTO_REPOSITION_REG;
	if (FSP_SUCCESS != R_SCI_I2C_Write(&g_stepper_i2c_ctrl, &reg, 1, true))
	{
		return -1;
	}

	if (FSP_SUCCESS != R_SCI_I2C_Read(&g_stepper_i2c_ctrl, &stepper_ctrl->is_reset, 1, false))
	{
		return -1;
	}

	return (int8_t)stepper_ctrl->is_reset;
}

void Stepper_RepositionSet(stepper_ctrl_t* stepper_ctrl, uint8_t reposition_set)
{
	if (stepper_ctrl == NULL)
	{
		return;
	}

	stepper_ctrl->reposition_set = reposition_set;

	(void)R_SCI_I2C_SlaveAddressSet(&g_stepper_i2c_ctrl,
	                                (uint32_t)stepper_ctrl->dev_i2c_addr,
	                                I2C_MASTER_ADDR_MODE_7BIT);

	uint8_t buf[2] = { MOTOR_AUTO_REPOSITION_REG, stepper_ctrl->reposition_set };
	(void)R_SCI_I2C_Write(&g_stepper_i2c_ctrl, buf, sizeof(buf), false);
}

void Stepper_SubdivisionSet(stepper_ctrl_t* stepper_ctrl, uint8_t subdivision_set)
{
	if (stepper_ctrl == NULL)
	{
		return;
	}

	stepper_ctrl->subdivision_set = subdivision_set;

	(void)R_SCI_I2C_SlaveAddressSet(&g_stepper_i2c_ctrl,
	                                (uint32_t)stepper_ctrl->dev_i2c_addr,
	                                I2C_MASTER_ADDR_MODE_7BIT);

	uint8_t buf[2] = { MOTOR_STEPS_DRIVER_MODE_REG, stepper_ctrl->subdivision_set };
	(void)R_SCI_I2C_Write(&g_stepper_i2c_ctrl, buf, sizeof(buf), false);
}

void Stepper_StepMoveSet(stepper_ctrl_t* stepper_ctrl, int32_t step_move_set)
{
	if (stepper_ctrl == NULL)
	{
		return;
	}

	stepper_ctrl->step_move_set = step_move_set;

	(void)R_SCI_I2C_SlaveAddressSet(&g_stepper_i2c_ctrl,
	                                (uint32_t)stepper_ctrl->dev_i2c_addr,
	                                I2C_MASTER_ADDR_MODE_7BIT);

	uint8_t buf[5];
	buf[0] = MOTOR_STEPS_REG;
	memcpy(&buf[1], &stepper_ctrl->step_move_set, sizeof(int32_t));
	(void)R_SCI_I2C_Write(&g_stepper_i2c_ctrl, buf, sizeof(buf), false);
}