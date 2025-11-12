/*
 * dev_water_quality_sen.c
 *
 *  Created on: 2025年11月12日
 *      Author: davidwang
 */
#include "dev_water_quality_sen.h"

const uint8_t g_wqs_cmd_detect[WQS_CMD_LENGTH] =
{0x55, 0xAA, 0x03, 0x0A, 0x00, 0xFE, 0x00, 0x00, 0x00, 0xFF};

const uint8_t g_wqs_cmd_calibrate[WQS_CMD_LENGTH] =
{0x55, 0xAA, 0x02, 0x0A, 0x00, 0x0A, 0x00, 0x00, 0x00, 0xFF};

void WQS_Init(wqs_cmd_t* wqs_cmd)
{
    R_SCI_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);

    wqs_cmd->wqs_cmd_detect = g_wqs_cmd_detect;
    wqs_cmd->wqs_cmd_calibrate = g_wqs_cmd_calibrate;
}

void WQS_CmdSend(wqs_cmd_t* wqs_cmd, wqs_cmd_type_e wqs_cmd_type)
{
    switch(wqs_cmd_type)
    {
        case WQS_CMD_TYPE_DETECT:
        {
            R_SCI_UART_Write(&g_uart0_ctrl, wqs_cmd->wqs_cmd_detect, WQS_CMD_LENGTH);
            break;
        }
        case WQS_CMD_TYPE_CALIBRATE:
        {
            R_SCI_UART_Write(&g_uart0_ctrl, wqs_cmd->wqs_cmd_calibrate, WQS_CMD_LENGTH);
            break;
        }
    }
}

void WQS_InfoGet(wqs_info_t *wqs_info)
{
    if (R_SCI_UART_Read (&g_uart0_ctrl, wqs_info->wqs_info_raw,
                         sizeof(wqs_info->wqs_info_raw)) == FSP_SUCCESS)
    {
        wqs_info->wqs_info_toc = ((float)(wqs_info->wqs_info_raw[8] * 256) + wqs_info->wqs_info_raw[9]) / 100;
        wqs_info->wqs_info_cod = ((float)(wqs_info->wqs_info_raw[10] * 256) + wqs_info->wqs_info_raw[11]) / 100;
        wqs_info->wqs_info_sd = ((float)(wqs_info->wqs_info_raw[12] * 256) + wqs_info->wqs_info_raw[13]) / 100;
        wqs_info->wqs_info_zd = ((float)(wqs_info->wqs_info_raw[14] * 256) + wqs_info->wqs_info_raw[15]) / 100;
        wqs_info->wqs_info_temp = ((float)(wqs_info->wqs_info_raw[16] * 256) + wqs_info->wqs_info_raw[17]) / 10;
        wqs_info->wqs_info_tds = (uint16_t)(wqs_info->wqs_info_raw[18] * 256) + wqs_info->wqs_info_raw[19];
        wqs_info->wqs_info_uv254 = ((float)(wqs_info->wqs_info_raw[20] * 256) + wqs_info->wqs_info_raw[21]) / 10000;
        wqs_info->wqs_info_ec = (uint16_t)(wqs_info->wqs_info_raw[22] * 256) + wqs_info->wqs_info_raw[23];
        wqs_info->wqs_info_wqi = wqs_info->wqs_info_raw[24];

        //debug
        R_SCI_UART_Write(&g_uart_debug_ctrl, wqs_info->wqs_info_raw, sizeof(wqs_info->wqs_info_raw));
//        R_SCI_UART_Write(&g_uart_debug_ctrl, (uint8_t*)&wqs_info->wqs_info_temp, 4);
    }
}

void WQS_UartCpltCallback(uart_callback_args_t *p_args)
{
    if(p_args->event == UART_EVENT_RX_COMPLETE)
    {

    }
}
