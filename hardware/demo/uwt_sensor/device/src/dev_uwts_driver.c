/*
 * dev_uwts_driver.c
 *
 *  Created on: 2025年11月13日
 *      Author: davidwang
 */
#include "dev_uwts_driver.h"

// 初始化DS18B20
void UWTS_Init(void)
{
    // 确保引脚初始化为输出高电平
    R_IOPORT_PinCfg(&g_ioport_ctrl, UWTS_PIN_DQ,
                    IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);
}

void UWTS_Reset(uwts_data_t* uwts_data)
{
    uwts_data->uwts_flag_presence = 0;

    // 配置引脚为输出
    R_IOPORT_PinCfg(&g_ioport_ctrl, UWTS_PIN_DQ,
                    IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);

    // 拉低总线480us（复位脉冲）
    R_BSP_SoftwareDelay(480, BSP_DELAY_UNITS_MICROSECONDS);

    // 释放总线，配置为输入（上拉电阻拉高）
    R_IOPORT_PinCfg(&g_ioport_ctrl, UWTS_PIN_DQ,
                    IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE);

    // 等待15-60us后DS18B20会拉低总线
    R_BSP_SoftwareDelay(60, BSP_DELAY_UNITS_MICROSECONDS);

    // 读取存在脉冲
    R_IOPORT_PinRead(&g_ioport_ctrl, UWTS_PIN_DQ, &uwts_data->uwts_data_dq_level);
    if(uwts_data->uwts_data_dq_level == BSP_IO_LEVEL_LOW)
    {
        uwts_data->uwts_flag_presence = 1;  // 设备存在
    }

    // 等待存在脉冲结束（60-240us）
    R_BSP_SoftwareDelay(240, BSP_DELAY_UNITS_MICROSECONDS);

    // 读取总线状态，确保已释放
    R_IOPORT_PinRead(&g_ioport_ctrl, UWTS_PIN_DQ, &uwts_data->uwts_data_dq_level);
}

// 传感器写一位数据
void UWTS_WriteBit(uint8_t bit)
{
    // 配置为输出模式
    R_IOPORT_PinCfg(&g_ioport_ctrl, UWTS_PIN_DQ,
                    IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);

    // 拉低总线开始写时序
    if(bit == 1)
    {
        // 写1时序：拉低1-15us后释放
        R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MICROSECONDS);     // 保持5us低电平
        R_IOPORT_PinCfg(&g_ioport_ctrl, UWTS_PIN_DQ,
                        IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE);
        R_BSP_SoftwareDelay(60, BSP_DELAY_UNITS_MICROSECONDS); // 等待写周期结束
    }
    else
    {
        // 写0时序：保持低电平60us
        R_BSP_SoftwareDelay(60, BSP_DELAY_UNITS_MICROSECONDS);
        R_IOPORT_PinCfg(&g_ioport_ctrl, UWTS_PIN_DQ,
                        IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE);
    }

    // 恢复时间
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
}

// 传感器读一位数据
uint8_t UWTS_ReadBit(void)
{
    uint8_t bit_value = 0;

    // 配置为输出，拉低总线开始读时序
    R_IOPORT_PinCfg(&g_ioport_ctrl, UWTS_PIN_DQ,
                    IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);

    // 保持低电平至少1us
    R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MICROSECONDS);

    // 释放总线，配置为输入
    R_IOPORT_PinCfg(&g_ioport_ctrl, UWTS_PIN_DQ,
                    IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE);

    // 等待15us后采样
    R_BSP_SoftwareDelay(15, BSP_DELAY_UNITS_MICROSECONDS);

    // 读取数据线状态
    bsp_io_level_t pin_level;
    R_IOPORT_PinRead(&g_ioport_ctrl, UWTS_PIN_DQ, &pin_level);

    if(pin_level == BSP_IO_LEVEL_HIGH)
    {
        bit_value = 1;
    }
    else
    {
        bit_value = 0;
    }

    // 等待读周期结束
    R_BSP_SoftwareDelay(45, BSP_DELAY_UNITS_MICROSECONDS);

    return bit_value;
}

// 写一个字节
void UWTS_WriteByte(uint8_t data)
{
    uint8_t i;

    for(i = 0; i < 8; i++)
    {
        UWTS_WriteBit(data & 0x01);
        data >>= 1;
    }
}

// 读一个字节
uint8_t UWTS_ReadByte(void)
{
    uint8_t i;
    uint8_t data = 0;

    for(i = 0; i < 8; i++)
    {
        data >>= 1;
        if(UWTS_ReadBit())
        {
            data |= 0x80;
        }
    }

    return data;
}

// 开始温度转换
void UWTS_ConversionStart(uwts_data_t* uwts_data)
{
    UWTS_Reset(uwts_data);
    UWTS_WriteByte(0xCC);  // 跳过ROM命令
    UWTS_WriteByte(UWTS_CMD_TEMP_CONVERT);
//    UWTS_WriteByte(UWTS_CMD_TEMP_CONVERT);  // 开始转换命令
}

// 读取温度值
void UWTS_TemperatureRead(uwts_data_t* uwts_data)
{
    UWTS_Reset(uwts_data);
    UWTS_WriteByte(UWTS_CMD_SKIP_ROM);  // 跳过ROM命令
    UWTS_WriteByte(UWTS_CMD_READ_SCRATCHPAD);  // 读暂存器命令

    // 读取温度数据（前两个字节）
    uwts_data->uwts_data_temp_l = UWTS_ReadByte();
    uwts_data->uwts_data_temp_h = UWTS_ReadByte();

    // 组合16位温度值
    uwts_data->uwts_data_temp = (uint16_t)(uwts_data->uwts_data_temp_h << 8) |
                                (uint16_t)uwts_data->uwts_data_temp_l;

    // 转换为实际温度值
    uwts_data->uwts_data_temp_real = uwts_data->uwts_data_temp * 0.0625f;
}
