#include "Actuator_Comm_Task.h"
#include "pump_drv8870.h"
#include "sensor_fusion.h"
#include "sht30.h"
#include "ds18b20.h"
#include "app_startup.h"

#include "FreeRTOS.h"
#include "task.h"
#include "r_uart_api.h"
#include <string.h>

extern const uart_instance_t g_com_uart0;

#define UART_SYNC0            0x55U
#define UART_SYNC1            0xAAU
#define UART_VERSION          0x01U
#define UART_HEADER_LEN       6U
#define UART_PAYLOAD_LEN      30U
#define UART_FRAME_LEN        (UART_HEADER_LEN + UART_PAYLOAD_LEN + 2U)
#define UART_CRC_LEN          2U
#define UART_CMD_PUMP_STOP    0x01U
#define UART_CMD_PUMP_START   0x02U
#define UART_CMD_PUMP_SET_PWM 0x03U
#define UART_CMD_LIGHT_SET    0x10U
#define UART_CMD_LIGHT_OFF    0x11U
#define UART_CTRL_LIGHT_OFF   0xFFU
#define PUMP_PERIOD_MS        200U
#define UART_TX_PERIOD_MS     250U

static volatile uint8_t  s_uart_opened;
static volatile uint8_t  s_uart_rx_buf[UART_FRAME_LEN];
static volatile uint8_t  s_uart_rx_idx;
static volatile uint8_t  s_uart_frame_ready;
static volatile uint8_t  s_uart_tx_seq;

static uint16_t uart_crc16_modbus(const uint8_t * data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            crc = (crc & 1U) ? (uint16_t) ((crc >> 1) ^ 0xA001U) : (uint16_t) (crc >> 1);
        }
    }

    return crc;
}

void UART_Rx_Callback(uart_callback_args_t * p_args)
{
    if (p_args == NULL)
    {
        return;
    }

    if (p_args->event == UART_EVENT_RX_CHAR)
    {
        uint8_t byte = (uint8_t) (p_args->data & 0xFFU);

        if (s_uart_rx_idx == 0)
        {
            if (byte == UART_SYNC0)
            {
                s_uart_rx_buf[s_uart_rx_idx++] = byte;
            }
        }
        else if (s_uart_rx_idx == 1)
        {
            if (byte == UART_SYNC1)
            {
                s_uart_rx_buf[s_uart_rx_idx++] = byte;
            }
            else
            {
                s_uart_rx_idx = 0;
                if (byte == UART_SYNC0)
                {
                    s_uart_rx_buf[s_uart_rx_idx++] = byte;
                }
            }
        }
        else
        {
            s_uart_rx_buf[s_uart_rx_idx++] = byte;
            if (s_uart_rx_idx >= UART_FRAME_LEN)
            {
                s_uart_frame_ready = 1U;
                s_uart_rx_idx      = 0U;
            }
        }
    }
}

static bool uart_comm_init(void)
{
    if (s_uart_opened)
    {
        return true;
    }

    fsp_err_t err = g_com_uart0.p_api->open(g_com_uart0.p_ctrl, g_com_uart0.p_cfg);
    if (err != FSP_SUCCESS)
    {
        return false;
    }

    err = g_com_uart0.p_api->callbackSet(g_com_uart0.p_ctrl, UART_Rx_Callback, NULL, NULL);
    if (err != FSP_SUCCESS)
    {
        g_com_uart0.p_api->close(g_com_uart0.p_ctrl);
        return false;
    }

    s_uart_opened = 1U;
    s_uart_rx_idx = 0U;
    s_uart_frame_ready = 0U;
    s_uart_tx_seq = 0U;
    return true;
}

static bool uart_send_sensor_frame(void)
{
    uint8_t frame[UART_FRAME_LEN];

    frame[0] = UART_SYNC0;
    frame[1] = UART_SYNC1;
    frame[2] = UART_VERSION;
    frame[3] = s_uart_tx_seq++;

    uint16_t payload_len_le = UART_PAYLOAD_LEN;
    frame[4] = (uint8_t) (payload_len_le & 0xFFU);
    frame[5] = (uint8_t) ((payload_len_le >> 8) & 0xFFU);

    int16_t air_temp_x10   = (int16_t) (g_sht30_temperature_c * 10.0F);
    int16_t air_humi_x10   = (int16_t) (g_sht30_humidity_rh * 10.0F);
    int16_t water_temp_x10 = (int16_t) (g_uwt_temperature_c * 10.0F);

    frame[6]  = (uint8_t) ((uint32_t) g_jscope_time_ms & 0xFFU);
    frame[7]  = (uint8_t) (((uint32_t) g_jscope_time_ms >> 8) & 0xFFU);
    frame[8]  = (uint8_t) (((uint32_t) g_jscope_time_ms >> 16) & 0xFFU);
    frame[9]  = (uint8_t) (((uint32_t) g_jscope_time_ms >> 24) & 0xFFU);
    frame[10] = (uint8_t) ((uint16_t) air_temp_x10 & 0xFFU);
    frame[11] = (uint8_t) (((uint16_t) air_temp_x10 >> 8) & 0xFFU);
    frame[12] = (uint8_t) ((uint16_t) air_humi_x10 & 0xFFU);
    frame[13] = (uint8_t) (((uint16_t) air_humi_x10 >> 8) & 0xFFU);
    frame[14] = (uint8_t) ((uint16_t) water_temp_x10 & 0xFFU);
    frame[15] = (uint8_t) (((uint16_t) water_temp_x10 >> 8) & 0xFFU);
    frame[16] = g_soil_moisture_percent;
    frame[17] = g_tds_wqi;
    frame[18] = g_pump_actual_power_percent;
    frame[19] = g_control_need_watering;
    frame[20] = 0U;
    frame[21] = 0U;
    frame[22] = 0U;
    frame[23] = 0U;
    frame[24] = 0U;
    frame[25] = 0U;
    frame[26] = (uint8_t) ((uint32_t) g_alarm_flags & 0xFFU);
    frame[27] = (uint8_t) (((uint32_t) g_alarm_flags >> 8) & 0xFFU);
    frame[28] = (uint8_t) (((uint32_t) g_alarm_flags >> 16) & 0xFFU);
    frame[29] = (uint8_t) (((uint32_t) g_alarm_flags >> 24) & 0xFFU);
    frame[30] = (uint8_t) ((uint16_t) g_air_retry_count & 0xFFU);
    frame[31] = (uint8_t) (((uint16_t) g_air_retry_count >> 8) & 0xFFU);
    frame[32] = (uint8_t) ((uint16_t) g_tds_retry_count & 0xFFU);
    frame[33] = (uint8_t) (((uint16_t) g_tds_retry_count >> 8) & 0xFFU);
    frame[34] = (uint8_t) ((uint16_t) g_uwt_retry_count & 0xFFU);
    frame[35] = (uint8_t) (((uint16_t) g_uwt_retry_count >> 8) & 0xFFU);

    uint16_t crc = uart_crc16_modbus(frame, UART_FRAME_LEN - UART_CRC_LEN);
    frame[36] = (uint8_t) (crc & 0xFFU);
    frame[37] = (uint8_t) ((crc >> 8) & 0xFFU);

    fsp_err_t err = g_com_uart0.p_api->write(g_com_uart0.p_ctrl, frame, UART_FRAME_LEN);
    return (err == FSP_SUCCESS);
}

static void uart_handle_downlink(const uint8_t * frame)
{
    uint8_t cmd   = frame[2];
    uint8_t param = frame[3];

    switch (cmd)
    {
    case UART_CMD_PUMP_STOP:
        g_pump_enable               = 1U;
        g_pump_manual_mode          = 1U;
        g_pump_manual_power_percent = 0U;
        break;

    case UART_CMD_PUMP_START:
        g_pump_enable      = 1U;
        g_pump_manual_mode = 1U;
        if (param == 0U)
        {
            param = 60U;
        }
        if (param > 100U)
        {
            param = 100U;
        }
        g_pump_manual_power_percent = param;
        break;

    case UART_CMD_PUMP_SET_PWM:
        g_pump_enable      = 1U;
        g_pump_manual_mode = 1U;
        if (param > 100U)
        {
            param = 100U;
        }
        g_pump_manual_power_percent = param;
        break;

    case UART_CMD_LIGHT_SET:
        g_usb_light_enable       = 1U;
        g_usb_light_mode_request = param;
        break;

    case UART_CMD_LIGHT_OFF:
        g_usb_light_mode_request = UART_CTRL_LIGHT_OFF;
        break;

    default:
        break;
    }
}

static void app_update_alarm_flags(void)
{
    uint32_t alarm = 0U;

    if (0U != g_soil_sensor_state)
    {
        alarm |= SENSOR_ALARM_SOIL_SENSOR_FAULT;
    }
    if (FSP_SUCCESS != g_air_last_err)
    {
        alarm |= SENSOR_ALARM_AIR_READ_FAIL;
    }
    if (FSP_SUCCESS != g_tds_last_err)
    {
        alarm |= SENSOR_ALARM_TDS_READ_FAIL;
    }
    if (FSP_SUCCESS != g_uwt_last_err)
    {
        alarm |= SENSOR_ALARM_UWT_READ_FAIL;
    }
    if ((FSP_SUCCESS == g_uwt_last_err) && (g_uwt_temperature_c >= g_ctrl_water_temp_high_c))
    {
        alarm |= SENSOR_ALARM_WATER_TEMP_HIGH;
    }
    if ((FSP_SUCCESS == g_tds_last_err) && (g_tds_wqi <= g_ctrl_wqi_low_threshold))
    {
        alarm |= SENSOR_ALARM_WQI_LOW;
    }
    if (g_comm_rx_crc_error_count > 0U)
    {
        alarm |= SENSOR_ALARM_COMM_RX_ERROR;
    }

    g_alarm_flags         = alarm;
    g_alarm_latched_flags |= alarm;
    g_jscope_alarm_flags  = alarm;
}

static void actuator_poll_pump(TickType_t now, TickType_t * p_last_tick, uint8_t * p_last_power)
{
    if ((now - *p_last_tick) < pdMS_TO_TICKS(PUMP_PERIOD_MS))
    {
        return;
    }

    *p_last_tick = now;

    uint8_t need_watering = 0U;

    if ((0U != g_ctrl_enable_soil) && (0U != g_soil_need_watering))
    {
        need_watering = 1U;
    }

    if ((0U != g_ctrl_enable_water_temp) &&
        (FSP_SUCCESS == g_uwt_last_err) &&
        (g_uwt_temperature_c >= g_ctrl_water_temp_high_c))
    {
        need_watering = 1U;
    }

    if ((0U != g_ctrl_enable_wqi) &&
        (FSP_SUCCESS == g_tds_last_err) &&
        (g_tds_wqi <= g_ctrl_wqi_low_threshold))
    {
        need_watering = 1U;
    }

    g_control_need_watering = need_watering;

    if (0U == g_pump_enable)
    {
        g_pump_target_power_percent = 0U;
    }
    else if (0U != g_pump_manual_mode)
    {
        g_pump_target_power_percent = g_pump_manual_power_percent;
    }
    else
    {
        g_pump_target_power_percent = (0U != need_watering) ? 70U : 0U;
    }

    if (g_pump_target_power_percent != *p_last_power)
    {
        pump_drv8870_set_power(g_pump_target_power_percent);
        *p_last_power = g_pump_target_power_percent;
    }

    g_pump_actual_power_percent = g_pump_target_power_percent;
    g_pump_running              = (g_pump_actual_power_percent > 0U) ? 1U : 0U;
    g_jscope_pump_power_pct     = (float) g_pump_actual_power_percent;
}

static void actuator_poll_uart(TickType_t now, TickType_t * p_last_tx_tick)
{
    if (0U != s_uart_frame_ready)
    {
        s_uart_frame_ready = 0U;

        uint16_t crc = uart_crc16_modbus((const uint8_t *) s_uart_rx_buf, UART_FRAME_LEN - UART_CRC_LEN);
        uint16_t recv_crc = (uint16_t) (((uint16_t) s_uart_rx_buf[UART_FRAME_LEN - 1] << 8) |
                                        s_uart_rx_buf[UART_FRAME_LEN - 2]);

        if (crc == recv_crc)
        {
            g_comm_rx_cmd_count++;
            uart_handle_downlink((const uint8_t *) s_uart_rx_buf);
        }
        else
        {
            g_comm_rx_crc_error_count++;
        }
    }

    if ((now - *p_last_tx_tick) >= pdMS_TO_TICKS(UART_TX_PERIOD_MS))
    {
        app_update_alarm_flags();
        sensor_fusion_update_jscope_time();
        g_comm_last_tx_ok = uart_send_sensor_frame() ? 1U : 0U;
        g_comm_tx_seq++;
        *p_last_tx_tick = now;
    }
}

void Actuator_Comm_Task_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    uint8_t    last_pump_power = 0xFFU;
    TickType_t last_pump       = xTaskGetTickCount();
    TickType_t last_uart_tx    = last_pump;

    pump_drv8870_init();

    if (!uart_comm_init())
    {
        for (;;)
        {
            vTaskDelay(pdMS_TO_TICKS(100U));
        }
    }

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();

        g_task_hb_actuator++;

        actuator_poll_pump(now, &last_pump, &last_pump_power);
        actuator_poll_uart(now, &last_uart_tx);

        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}
