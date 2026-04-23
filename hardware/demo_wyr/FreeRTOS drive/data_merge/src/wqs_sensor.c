#include "wqs_sensor.h"
#include "WQS_Task.h"

// ---- UART protocol constants (from WQM11S spec) ----
// Command frames (10 bytes)
static const uint8_t g_wqs_cmd_detect[10] = {0x55, 0xAA, 0x03, 0x0A, 0x00, 0xFE, 0x00, 0x00, 0x00, 0xFF};
static const uint8_t g_wqs_cmd_calibrate[10] = {0x55, 0xAA, 0x02, 0x0A, 0x00, 0x0A, 0x00, 0x00, 0x00, 0xFF};

// Detection response frame length (27 bytes)
enum
{
    WQS_INFO_LEN = 27,

    // Bytes inside the response (Figure 3)
    IDX_TOC_H = 8,
    IDX_TOC_L = 9,
    IDX_COD_H = 10,
    IDX_COD_L = 11,
    IDX_SD_H = 12,
    IDX_SD_L = 13,
    IDX_ZD_H = 14,
    IDX_ZD_L = 15,
    IDX_TH_H = 16,
    IDX_TH_L = 17,
    IDX_TDS_H = 18,
    IDX_TDS_L = 19,
    IDX_UV254_H = 20,
    IDX_UV254_L = 21,
    IDX_EC_H = 22,
    IDX_EC_L = 23,
    IDX_WQI = 24,
    IDX_CRC8 = 25,
    // Last byte is 0xFF at IDX 26
};

// ---- CRC8 (same algorithm as spec snippet) ----
static uint8_t wqs_crc8_calc(const uint8_t* p_buf, uint32_t length)
{
    // Spec snippet:
    // unsigned char get_crc8(unsigned char * p_buf, unsigned int length)
    // {
    //   unsigned char i, tmp, crc_val, crc = 0;
    //   do {
    //     tmp = crc ^ *p_buf;
    //     crc_val = 0;
    //     for(i = 0; i < 8; i++) {
    //       if(((crc_val ^ tmp) & 0x01)) {
    //         crc_val ^= 0x18;
    //         crc_val >>= 1;
    //         crc_val |= 0x80;
    //       } else {
    //         crc_val >>= 1;
    //       }
    //       tmp >>= 1;
    //     }
    //     crc = crc_val;
    //     p_buf++;
    //   } while(--length);
    //   return crc;
    // }
    uint8_t i;
    uint8_t tmp;
    uint8_t crc_val;
    uint8_t crc = 0;

    while (length-- > 0)
    {
        tmp = (uint8_t)(crc ^ *p_buf);
        crc_val = 0;
        for (i = 0; i < 8; i++)
        {
            if (((crc_val ^ tmp) & 0x01U) != 0U)
            {
                crc_val ^= 0x18U;
                crc_val >>= 1U;
                crc_val |= 0x80U;
            }
            else
            {
                crc_val >>= 1U;
            }
            tmp >>= 1U;
        }
        crc = crc_val;
        p_buf++;
    }

    return crc;
}

// ---- Global variables for Debug Watch ----
volatile wqs_info_t g_wqs_info = {0};
volatile uint32_t g_wqs_measure_count = 0;
volatile uint8_t g_wqs_last_read_ok = 0;

// Set in UART callback when RX received enough bytes for this read request.
volatile uint8_t g_wqs_uart_rx_done = 0;

// ---- UART callback (fix linker error) ----
void WQS_UartCpltCallback(uart_callback_args_t *p_args)
{
    if (p_args == NULL)
    {
        return;
    }

    if (p_args->event == UART_EVENT_RX_COMPLETE)
    {
        g_wqs_uart_rx_done = 1U;
    }
}

void WQS_Init(wqs_cmd_t* wqs_cmd)
{
    if (wqs_cmd == NULL)
    {
        return;
    }

    // Open UART instance once. Without this, read/write will fail.
    fsp_err_t err = g_wqs_uart.p_api->open(g_wqs_uart.p_ctrl, g_wqs_uart.p_cfg);
    if ((FSP_SUCCESS != err) && (FSP_ERR_ALREADY_OPEN != err))
    {
        return;
    }

    wqs_cmd->wqs_cmd_detect = g_wqs_cmd_detect;
    wqs_cmd->wqs_cmd_calibrate = g_wqs_cmd_calibrate;
}

void WQS_CmdSend(const wqs_cmd_t* wqs_cmd, wqs_cmd_type_e wqs_cmd_type)
{
    if ((wqs_cmd == NULL) || (wqs_cmd->wqs_cmd_detect == NULL) || (wqs_cmd->wqs_cmd_calibrate == NULL))
    {
        return;
    }

    switch (wqs_cmd_type)
    {
        case WQS_CMD_TYPE_DETECT:
            (void) g_wqs_uart.p_api->write(g_wqs_uart.p_ctrl, wqs_cmd->wqs_cmd_detect, 10U);
            break;

        case WQS_CMD_TYPE_CALIBRATE:
            (void) g_wqs_uart.p_api->write(g_wqs_uart.p_ctrl, wqs_cmd->wqs_cmd_calibrate, 10U);
            break;

        default:
            break;
    }
}

static void wqs_parse_info_from_raw(volatile wqs_info_t* wqs_info)
{
    // Table / formula from Figure 3 and spec
    wqs_info->wqs_info_toc = ((float)(wqs_info->wqs_info_raw[IDX_TOC_H] * 256U) + (float)wqs_info->wqs_info_raw[IDX_TOC_L]) / 100.0f;
    wqs_info->wqs_info_cod = ((float)(wqs_info->wqs_info_raw[IDX_COD_H] * 256U) + (float)wqs_info->wqs_info_raw[IDX_COD_L]) / 100.0f;
    wqs_info->wqs_info_sd  = ((float)(wqs_info->wqs_info_raw[IDX_SD_H]  * 256U) + (float)wqs_info->wqs_info_raw[IDX_SD_L])  / 100.0f;
    wqs_info->wqs_info_zd  = ((float)(wqs_info->wqs_info_raw[IDX_ZD_H]  * 256U) + (float)wqs_info->wqs_info_raw[IDX_ZD_L])  / 100.0f;
    wqs_info->wqs_info_temp = ((float)(wqs_info->wqs_info_raw[IDX_TH_H] * 256U) + (float)wqs_info->wqs_info_raw[IDX_TH_L]) / 10.0f;

    wqs_info->wqs_info_tds = (uint16_t)(wqs_info->wqs_info_raw[IDX_TDS_H] * 256U + wqs_info->wqs_info_raw[IDX_TDS_L]);
    wqs_info->wqs_info_uv254 = ((float)(wqs_info->wqs_info_raw[IDX_UV254_H] * 256U) + (float)wqs_info->wqs_info_raw[IDX_UV254_L]) / 10000.0f;
    wqs_info->wqs_info_ec = (uint16_t)(wqs_info->wqs_info_raw[IDX_EC_H] * 256U + wqs_info->wqs_info_raw[IDX_EC_L]);
    wqs_info->wqs_info_wqi = wqs_info->wqs_info_raw[IDX_WQI];
}

bool WQS_InfoGet(volatile wqs_info_t* wqs_info)
{
    if (wqs_info == NULL)
    {
        return false;
    }

    // Clear RX-done flag before starting a new read request.
    g_wqs_uart_rx_done = 0U;

    // Clear buffer to avoid parsing stale data in Debug when UART RX doesn't complete.
    for (uint32_t i = 0U; i < WQS_INFO_LEN; i++)
    {
        wqs_info->wqs_info_raw[i] = 0U;
    }

    // Configure UART to receive exactly 27 bytes.
    // Note: R_SCI_UART_Read configures an interrupt-based receive; it is not "blocking" by itself.
    fsp_err_t err = g_wqs_uart.p_api->read(g_wqs_uart.p_ctrl, (uint8_t *)wqs_info->wqs_info_raw, WQS_INFO_LEN);
    if (err != FSP_SUCCESS)
    {
        wqs_info->wqs_crc_ok = 0U;
        return false;
    }

    // Wait until we see a plausible response frame layout in the raw buffer.
    // This avoids relying solely on UART RX_COMPLETE callback behavior (which can differ by HAL configuration).
    // WQM11S response time: 1~3s, so timeout ~5s.
    uint32_t timeout_ms = 5000U;
    while (timeout_ms-- > 0U)
    {
        const uint8_t b0 = wqs_info->wqs_info_raw[0];
        const uint8_t b1 = wqs_info->wqs_info_raw[1];
        const uint8_t b26 = wqs_info->wqs_info_raw[26];

        // Frame header should be 0x55 0xAA and last byte should be 0xFF.
        if ((b0 == 0x55U) && (b1 == 0xAAU) && (b26 == 0xFFU))
        {
            break;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if ((wqs_info->wqs_info_raw[0] != 0x55U) || (wqs_info->wqs_info_raw[1] != 0xAAU) ||
        (wqs_info->wqs_info_raw[26] != 0xFFU))
    {
        wqs_info->wqs_crc_ok = 0U;
        return false;
    }

    // CRC8 check: spec says "倒数第二字节为CRC8，校验前面所有字节"
    {
        const uint8_t crc_in_frame = wqs_info->wqs_info_raw[IDX_CRC8];
        const uint8_t crc_calc =
            wqs_crc8_calc((const uint8_t *)wqs_info->wqs_info_raw, 25U); // bytes 0..24
        wqs_info->wqs_crc_ok = (crc_calc == crc_in_frame) ? 1U : 0U;
    }

    // Parse values regardless of CRC (helps debugging). You can change to only parse on CRC OK.
    wqs_parse_info_from_raw(wqs_info);

    return (wqs_info->wqs_crc_ok != 0U);
}

