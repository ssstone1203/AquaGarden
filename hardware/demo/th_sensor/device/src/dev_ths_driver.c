/*
 * dev_ths_driver.c
 *
 *  Created on: 2025年11月14日
 *      Author: davidwang
 */
#include "dev_ths_driver.h"

void THS_PeriodicModeStart(ths_cmd_t* ths_cmd, ths_repeat_type_e ths_repeat_type)
{
    switch(ths_repeat_type)
    {
        case THS_REPEAT_TYPE_HIGH:
        {
            ths_cmd->ths_cmd_raw = THS_CMD_PERIODIC_H_MPS_1;
            ths_cmd->ths_cmd_byte[0] = ths_cmd->ths_cmd_raw >> 8;
            ths_cmd->ths_cmd_byte[1] = (uint8_t)ths_cmd->ths_cmd_raw;

            // 启动Periodic模式
            R_SCI_I2C_Write(&g_i2c0_ctrl, ths_cmd->ths_cmd_byte, 2, false);
            break;
        }
        case THS_REPEAT_TYPE_LOW:
        {
            break;
        }
        case THS_REPEAT_TYPE_MEDIUM:
        {
            break;
        }
        default:
        {
            break;
        }
    }
}

void THS_PeriodicDataRead(ths_cmd_t *ths_cmd, ths_data_t *ths_data)
{
    ths_cmd->ths_cmd_raw = THS_CMD_PERIODIC_DATA_FETCH;
    ths_cmd->ths_cmd_byte[0] = ths_cmd->ths_cmd_raw >> 8;
    ths_cmd->ths_cmd_byte[1] = (uint8_t) ths_cmd->ths_cmd_raw;

    // 发送读取命令
    R_SCI_I2C_Write(&g_i2c0_ctrl, ths_cmd->ths_cmd_byte, 2, false);

    //读取6字节数据 (温度+CRC + 湿度+CRC)
    R_SCI_I2C_Read(&g_i2c0_ctrl, ths_data->ths_data_raw, 6, false);

    // 转换温度数据
    ths_data->ths_data_temp_raw = (uint16_t) (ths_data->ths_data_raw[0] << 8) | ths_data->ths_data_raw[1];
    ths_data->ths_data_temp = (float) (-45 + 175 * ((float) ths_data->ths_data_temp_raw / 65535.0));

    // 转换湿度数据
    ths_data->ths_data_rh_raw = (uint16_t) (ths_data->ths_data_raw[3] << 8) | ths_data->ths_data_raw[4];
    ths_data->ths_data_rh = (float)(100 * ((float)ths_data->ths_data_rh_raw / 65535.0));
}

// 停止Periodic模式
void THS_PeriodicModeStop(ths_cmd_t* ths_cmd)
{
    ths_cmd->ths_cmd_raw = THS_CMD_PERIODIC_STOP;
    ths_cmd->ths_cmd_byte[0] = ths_cmd->ths_cmd_raw >> 8;
    ths_cmd->ths_cmd_byte[1] = (uint8_t) ths_cmd->ths_cmd_raw;

    // 停止Periodic模式
    R_SCI_I2C_Write(&g_i2c0_ctrl, ths_cmd->ths_cmd_byte, 2, false);
}

