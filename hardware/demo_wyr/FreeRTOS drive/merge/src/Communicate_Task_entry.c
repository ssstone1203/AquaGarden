#include "Communicate_Task.h"
#include "sensor_fusion.h"
#include "wqs_sensor.h"
#include "ds18b20.h"
#include "sht30.h"

#define HOST_UART_BAUD_BRR                 (53U)   /* ~115200 for PCLKB=50MHz with BGDM=1 */
#define HOST_UPLINK_SYNC0                  (0x55U)
#define HOST_UPLINK_SYNC1                  (0xAAU)
#define HOST_UPLINK_VERSION                (0x01U)
#define HOST_DOWNLINK_SYNC0                (0x5AU)
#define HOST_DOWNLINK_SYNC1                (0xA5U)
#define HOST_DOWNLINK_MAX_PAYLOAD          (16U)
#define HOST_ALARM_PRESSURE_HIGH_KG        (4.8F)
#define HOST_UART_WAIT_TIMEOUT_MS          (5U)

enum
{
    HOST_CMD_SET_MANUAL_PUMP = 0x01U,
    HOST_CMD_SET_SOIL_CFG    = 0x02U,
    HOST_CMD_SET_LINKAGE_CFG = 0x03U,
};

static uint16_t host_crc16_modbus(const uint8_t * p_data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0U; i < len; i++)
    {
        crc ^= p_data[i];
        for (uint8_t b = 0U; b < 8U; b++)
        {
            if (0U != (crc & 0x0001U))
            {
                crc = (uint16_t) ((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc = (uint16_t) (crc >> 1U);
            }
        }
    }
    return crc;
}

static void host_uart0_init(void)
{
    R_BSP_MODULE_START(FSP_IP_SCI, 0U);

    R_SCI0->SCR = 0x00U;
    R_SCI0->SMR = 0x00U;          /* 8N1, async, PCLK/1 */
    R_SCI0->SEMR_b.BGDM = 1U;     /* Double-speed mode */
    R_SCI0->SEMR_b.ABCS = 0U;
    R_SCI0->SEMR_b.ABCSE = 0U;
    R_SCI0->SEMR_b.BRME = 0U;
    R_SCI0->BRR = HOST_UART_BAUD_BRR;
    R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    R_SCI0->SCR_b.RE = 1U;
    R_SCI0->SCR_b.TE = 1U;
}

static bool host_uart_wait_tdre(TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    while (0U == R_SCI0->SSR_b.TDRE)
    {
        if ((xTaskGetTickCount() - start) >= timeout_ticks)
        {
            return false;
        }
        taskYIELD();
    }
    return true;
}

static bool host_uart_wait_tend(TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    while (0U == R_SCI0->SSR_b.TEND)
    {
        if ((xTaskGetTickCount() - start) >= timeout_ticks)
        {
            return false;
        }
        taskYIELD();
    }
    return true;
}

static bool host_uart0_write_bytes(const uint8_t * p_data, uint16_t len)
{
    const TickType_t timeout_ticks = pdMS_TO_TICKS(HOST_UART_WAIT_TIMEOUT_MS);

    for (uint16_t i = 0U; i < len; i++)
    {
        if (!host_uart_wait_tdre(timeout_ticks))
        {
            return false;
        }
        R_SCI0->TDR = p_data[i];
        R_SCI0->SSR_b.TDRE = 0U;
    }

    if (!host_uart_wait_tend(timeout_ticks))
    {
        return false;
    }

    return true;
}

static bool host_uart0_try_read_byte(uint8_t * p_byte)
{
    if (NULL == p_byte)
    {
        return false;
    }

    if ((0U != R_SCI0->SSR_b.ORER) || (0U != R_SCI0->SSR_b.FER) || (0U != R_SCI0->SSR_b.PER))
    {
        R_SCI0->SSR_b.ORER = 0U;
        R_SCI0->SSR_b.FER = 0U;
        R_SCI0->SSR_b.PER = 0U;
        g_comm_rx_crc_error_count++;
        g_alarm_flags |= SENSOR_ALARM_COMM_RX_ERROR;
        g_alarm_latched_flags |= SENSOR_ALARM_COMM_RX_ERROR;
    }

    if (0U == R_SCI0->SSR_b.RDRF)
    {
        return false;
    }

    *p_byte = R_SCI0->RDR;
    R_SCI0->SSR_b.RDRF = 0U;
    return true;
}

static int16_t host_float_to_i16_x10(float value)
{
    float scaled = value * 10.0F;
    if (scaled > 32767.0F)
    {
        return 32767;
    }
    if (scaled < -32768.0F)
    {
        return -32768;
    }
    return (int16_t) scaled;
}

static uint16_t host_float_to_u16_x100(float value)
{
    float scaled = value * 100.0F;
    if (scaled < 0.0F)
    {
        return 0U;
    }
    if (scaled > 65535.0F)
    {
        return 65535U;
    }
    return (uint16_t) scaled;
}

static void host_u16_put(uint8_t * p_buf, uint16_t value)
{
    p_buf[0] = (uint8_t) (value & 0xFFU);
    p_buf[1] = (uint8_t) ((value >> 8U) & 0xFFU);
}

static void host_u32_put(uint8_t * p_buf, uint32_t value)
{
    p_buf[0] = (uint8_t) (value & 0xFFU);
    p_buf[1] = (uint8_t) ((value >> 8U) & 0xFFU);
    p_buf[2] = (uint8_t) ((value >> 16U) & 0xFFU);
    p_buf[3] = (uint8_t) ((value >> 24U) & 0xFFU);
}

static uint16_t host_u16_get(const uint8_t * p_buf)
{
    return (uint16_t) ((uint16_t) p_buf[0] | ((uint16_t) p_buf[1] << 8U));
}

static int16_t host_i16_get(const uint8_t * p_buf)
{
    return (int16_t) host_u16_get(p_buf);
}

static void host_update_alarm_flags(void)
{
    uint32_t alarm = 0U;

    if (0U != g_soil_sensor_state)
    {
        alarm |= SENSOR_ALARM_SOIL_SENSOR_FAULT;
    }
    if (FSP_SUCCESS != g_pressure_last_err)
    {
        alarm |= SENSOR_ALARM_PRESSURE_READ_FAIL;
    }
    if (FSP_SUCCESS != g_air_last_err)
    {
        alarm |= SENSOR_ALARM_AIR_READ_FAIL;
    }
    if (FSP_SUCCESS != g_wqs_last_err)
    {
        alarm |= SENSOR_ALARM_WQS_READ_FAIL;
    }
    if (FSP_SUCCESS != g_uwt_last_err)
    {
        alarm |= SENSOR_ALARM_UWT_READ_FAIL;
    }
    if ((FSP_SUCCESS == g_uwt_last_err) && (g_uwt_temperature_c >= g_ctrl_water_temp_high_c))
    {
        alarm |= SENSOR_ALARM_WATER_TEMP_HIGH;
    }
    if ((0U != g_wqs_last_read_ok) && (g_wqs_info.wqs_info_wqi <= g_ctrl_wqi_low_threshold))
    {
        alarm |= SENSOR_ALARM_WQI_LOW;
    }
    if ((g_pressure_latest.pressure_kg[0] >= HOST_ALARM_PRESSURE_HIGH_KG) ||
        (g_pressure_latest.pressure_kg[1] >= HOST_ALARM_PRESSURE_HIGH_KG) ||
        (g_pressure_latest.pressure_kg[2] >= HOST_ALARM_PRESSURE_HIGH_KG))
    {
        alarm |= SENSOR_ALARM_PRESSURE_HIGH;
    }
    if (g_comm_rx_crc_error_count > 0U)
    {
        alarm |= SENSOR_ALARM_COMM_RX_ERROR;
    }

    g_alarm_flags = alarm;
    g_alarm_latched_flags |= alarm;
    g_jscope_alarm_flags = alarm;
}

static uint16_t host_build_uplink_frame(uint8_t * p_out)
{
    uint8_t * p = p_out;
    uint8_t payload[40];
    uint8_t * q = payload;

    host_u32_put(q, g_jscope_time_ms); q += 4;
    host_u16_put(q, (uint16_t) host_float_to_i16_x10(g_sht30_temperature_c)); q += 2;
    host_u16_put(q, (uint16_t) host_float_to_i16_x10(g_sht30_humidity_rh)); q += 2;
    host_u16_put(q, (uint16_t) host_float_to_i16_x10(g_uwt_temperature_c)); q += 2;
    *q++ = g_soil_moisture_percent;
    *q++ = g_wqs_info.wqs_info_wqi;
    *q++ = g_pump_actual_power_percent;
    *q++ = g_control_need_watering;
    host_u16_put(q, host_float_to_u16_x100(g_pressure_latest.pressure_kg[0])); q += 2;
    host_u16_put(q, host_float_to_u16_x100(g_pressure_latest.pressure_kg[1])); q += 2;
    host_u16_put(q, host_float_to_u16_x100(g_pressure_latest.pressure_kg[2])); q += 2;
    host_u32_put(q, g_alarm_flags); q += 4;
    host_u16_put(q, (uint16_t) g_air_retry_count); q += 2;
    host_u16_put(q, (uint16_t) g_wqs_retry_count); q += 2;
    host_u16_put(q, (uint16_t) g_uwt_retry_count); q += 2;

    uint16_t payload_len = (uint16_t) (q - payload);
    *p++ = HOST_UPLINK_SYNC0;
    *p++ = HOST_UPLINK_SYNC1;
    *p++ = HOST_UPLINK_VERSION;
    *p++ = (uint8_t) (g_comm_tx_seq & 0xFFU);
    host_u16_put(p, payload_len); p += 2;
    for (uint16_t i = 0U; i < payload_len; i++)
    {
        *p++ = payload[i];
    }

    uint16_t crc = host_crc16_modbus(p_out, (uint16_t) (p - p_out));
    host_u16_put(p, crc);
    p += 2;
    return (uint16_t) (p - p_out);
}

static void host_apply_command(uint8_t cmd, const uint8_t * p_payload, uint8_t len)
{
    switch (cmd)
    {
        case HOST_CMD_SET_MANUAL_PUMP:
            if (len >= 2U)
            {
                g_pump_manual_mode = (0U != p_payload[0]) ? 1U : 0U;
                g_pump_manual_power_percent = p_payload[1];
                if (g_pump_manual_power_percent > 100U)
                {
                    g_pump_manual_power_percent = 100U;
                }
            }
            break;

        case HOST_CMD_SET_SOIL_CFG:
            if (len >= 2U)
            {
                g_soil_watering_threshold = p_payload[0];
                g_soil_watering_hysteresis = p_payload[1];
            }
            break;

        case HOST_CMD_SET_LINKAGE_CFG:
            if (len >= 4U)
            {
                g_ctrl_enable_soil = (0U != (p_payload[0] & 0x01U)) ? 1U : 0U;
                g_ctrl_enable_water_temp = (0U != (p_payload[0] & 0x02U)) ? 1U : 0U;
                g_ctrl_enable_wqi = (0U != (p_payload[0] & 0x04U)) ? 1U : 0U;
                g_ctrl_water_temp_high_c = (float) host_i16_get(&p_payload[1]) / 10.0F;
                g_ctrl_wqi_low_threshold = p_payload[3];
                if (g_ctrl_wqi_low_threshold > 100U)
                {
                    g_ctrl_wqi_low_threshold = 100U;
                }
                if (g_ctrl_water_temp_high_c < 0.0F)
                {
                    g_ctrl_water_temp_high_c = 0.0F;
                }
                if (g_ctrl_water_temp_high_c > 100.0F)
                {
                    g_ctrl_water_temp_high_c = 100.0F;
                }
            }
            break;

        default:
            break;
    }
}

static void host_parse_downlink_stream(void)
{
    static uint8_t  state = 0U;
    static uint8_t  len = 0U;
    static uint8_t  cmd = 0U;
    static uint8_t  payload[HOST_DOWNLINK_MAX_PAYLOAD];
    static uint8_t  idx = 0U;
    static uint8_t  crc_lo = 0U;
    static uint8_t  frame[2U + 1U + 1U + HOST_DOWNLINK_MAX_PAYLOAD];
    uint8_t byte = 0U;

    while (host_uart0_try_read_byte(&byte))
    {
        switch (state)
        {
            case 0U:
                state = (byte == HOST_DOWNLINK_SYNC0) ? 1U : 0U;
                break;
            case 1U:
                state = (byte == HOST_DOWNLINK_SYNC1) ? 2U : 0U;
                break;
            case 2U:
                len = byte;
                if ((len < 1U) || (len > (1U + HOST_DOWNLINK_MAX_PAYLOAD)))
                {
                    state = 0U;
                }
                else
                {
                    state = 3U;
                }
                break;
            case 3U:
                cmd = byte;
                idx = 0U;
                if (len == 1U)
                {
                    state = 5U;
                }
                else
                {
                    state = 4U;
                }
                break;
            case 4U:
                payload[idx++] = byte;
                if (idx >= (uint8_t) (len - 1U))
                {
                    state = 5U;
                }
                break;
            case 5U:
                crc_lo = byte;
                state = 6U;
                break;
            case 6U:
            {
                uint16_t frame_len = (uint16_t) (2U + 1U + 1U + (len - 1U));
                uint16_t expect_crc;
                uint16_t rx_crc = (uint16_t) ((uint16_t) crc_lo | ((uint16_t) byte << 8U));

                frame[0] = HOST_DOWNLINK_SYNC0;
                frame[1] = HOST_DOWNLINK_SYNC1;
                frame[2] = len;
                frame[3] = cmd;
                for (uint8_t i = 0U; i < (uint8_t) (len - 1U); i++)
                {
                    frame[4U + i] = payload[i];
                }

                expect_crc = host_crc16_modbus(frame, frame_len);
                if (expect_crc == rx_crc)
                {
                    host_apply_command(cmd, payload, (uint8_t) (len - 1U));
                    g_comm_rx_cmd_count++;
                }
                else
                {
                    g_comm_rx_crc_error_count++;
                    g_alarm_flags |= SENSOR_ALARM_COMM_RX_ERROR;
                    g_alarm_latched_flags |= SENSOR_ALARM_COMM_RX_ERROR;
                }
                state = 0U;
                break;
            }
            default:
                state = 0U;
                break;
        }
    }
}

void Com_SPI_Callback(spi_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}
                /* Com_Thread entry function */
                /* pvParameters contains TaskHandle_t */
                void Communicate_Task_entry(void * pvParameters)
                {
                    FSP_PARAMETER_NOT_USED(pvParameters);
                    /* Keep telemetry thread responsive even if generated task priority gets reset by RASC. */
                    vTaskPrioritySet(NULL, 2U);
                    host_uart0_init();

                    const TickType_t period_ticks = pdMS_TO_TICKS(250U);
                    while(1)
                    {
                        sensor_fusion_update_jscope_time();
                        host_update_alarm_flags();

                        uint8_t frame[64];
                        uint16_t frame_len = host_build_uplink_frame(frame);
                        bool tx_ok = host_uart0_write_bytes(frame, frame_len);
                        g_comm_last_tx_ok = tx_ok ? 1U : 0U;
                        if (tx_ok)
                        {
                            g_comm_tx_seq++;
                        }
                        else
                        {
                            g_alarm_flags |= SENSOR_ALARM_COMM_RX_ERROR;
                            g_alarm_latched_flags |= SENSOR_ALARM_COMM_RX_ERROR;
                        }

                        host_parse_downlink_stream();
                        vTaskDelay(period_ticks);
                    }
                }
