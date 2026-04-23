#ifndef WQS_SENSOR_H_
#define WQS_SENSOR_H_

#include <stdint.h>
#include <stdbool.h>

// Water Quality Sensor (WQM11S/WQ series) UART protocol wrapper.

typedef struct
{
    uint8_t wqs_info_raw[27];

    // Figure 3 / spec derived values
    float    wqs_info_toc;    // 有机物综合评估 (TOC)
    float    wqs_info_cod;    // 无机物综合评估 (COD)
    float    wqs_info_sd;     // 色度
    float    wqs_info_zd;     // 浊度
    float    wqs_info_temp;  // 温度 (unit: degC)
    uint16_t wqs_info_tds;   // TDS (ppm)
    float    wqs_info_uv254; // UV254 (1/m)
    uint16_t wqs_info_ec;    // 电导率
    uint8_t  wqs_info_wqi;   // 水质综合评分

    // Debug / status
    uint8_t  wqs_crc_ok;     // 1=CRC8 ok, 0=CRC8 mismatch (if UART read succeeded)
} wqs_info_t;

typedef enum
{
    WQS_CMD_TYPE_DETECT = 0,
    WQS_CMD_TYPE_CALIBRATE = 1
} wqs_cmd_type_e;

typedef struct
{
    const uint8_t* wqs_cmd_detect;   // 10 bytes
    const uint8_t* wqs_cmd_calibrate;// 10 bytes
} wqs_cmd_t;

// Global variables for Keil watch window.
extern volatile wqs_info_t g_wqs_info;
extern volatile uint32_t    g_wqs_measure_count;
extern volatile uint8_t     g_wqs_last_read_ok;
extern volatile uint8_t     g_wqs_uart_rx_done;

void WQS_Init(wqs_cmd_t* wqs_cmd);
void WQS_CmdSend(const wqs_cmd_t* wqs_cmd, wqs_cmd_type_e wqs_cmd_type);
bool WQS_InfoGet(volatile wqs_info_t* wqs_info);

#endif // WQS_SENSOR_H_

