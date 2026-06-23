#include "com_protocol.h"
#include <string.h>

static uint8_t clamp_percent(uint8_t value)
{
    return (value > 100U) ? 100U : value;
}

static void put_u16_le(uint8_t * p, uint16_t value)
{
    p[0] = (uint8_t) (value & 0xFFU);
    p[1] = (uint8_t) ((value >> 8) & 0xFFU);
}

static void put_i16_le(uint8_t * p, int16_t value)
{
    put_u16_le(p, (uint16_t) value);
}

static void put_u32_le(uint8_t * p, uint32_t value)
{
    p[0] = (uint8_t) (value & 0xFFU);
    p[1] = (uint8_t) ((value >> 8) & 0xFFU);
    p[2] = (uint8_t) ((value >> 16) & 0xFFU);
    p[3] = (uint8_t) ((value >> 24) & 0xFFU);
}

static uint16_t get_u16_le(const uint8_t * p)
{
    return (uint16_t) (((uint16_t) p[1] << 8) | p[0]);
}

static int16_t get_i16_le(const uint8_t * p)
{
    return (int16_t) get_u16_le(p);
}

static int16_t float_to_i16_x10(float value)
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

uint16_t com_crc16_modbus(const uint8_t * p_data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0U; i < len; i++)
    {
        crc ^= p_data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            crc = (0U != (crc & 1U)) ? (uint16_t) ((crc >> 1) ^ 0xA001U) : (uint16_t) (crc >> 1);
        }
    }

    return crc;
}

bool com_downlink_decode(const uint8_t * p_frame, uint8_t frame_len, com_downlink_frame_t * p_out)
{
    uint8_t len;
    uint8_t payload_len;
    uint16_t crc_calc;
    uint16_t crc_recv;

    if ((NULL == p_frame) || (NULL == p_out) || (frame_len < 5U))
    {
        return false;
    }
    if ((COM_DOWN_SYNC0 != p_frame[0]) || (COM_DOWN_SYNC1 != p_frame[1]))
    {
        return false;
    }

    len = p_frame[2];
    if ((0U == len) || (frame_len != (uint8_t) (2U + 1U + len + 2U)))
    {
        return false;
    }

    payload_len = (uint8_t) (len - 1U);
    if (payload_len > COM_DOWN_MAX_PAYLOAD)
    {
        return false;
    }

    crc_calc = com_crc16_modbus(p_frame, (uint16_t) (frame_len - 2U));
    crc_recv = (uint16_t) (((uint16_t) p_frame[frame_len - 1U] << 8) | p_frame[frame_len - 2U]);
    if (crc_calc != crc_recv)
    {
        return false;
    }

    p_out->cmd = p_frame[3];
    p_out->len = payload_len;
    if (payload_len > 0U)
    {
        memcpy(p_out->payload, &p_frame[4], payload_len);
    }

    return true;
}

void com_apply_downlink(const com_downlink_frame_t * p_cmd)
{
    if (NULL == p_cmd)
    {
        return;
    }

    switch (p_cmd->cmd)
    {
        case COM_CMD_SET_MANUAL_PUMP:
            if (p_cmd->len >= 2U)
            {
                aqua_app_set_pump_manual(p_cmd->payload[0], clamp_percent(p_cmd->payload[1]));
            }
            break;

        case COM_CMD_SOIL_CONFIG:
            if (p_cmd->len >= 2U)
            {
                aqua_app_set_soil_config(clamp_percent(p_cmd->payload[0]), clamp_percent(p_cmd->payload[1]));
            }
            break;

        case COM_CMD_LINKAGE_CONFIG:
            if (p_cmd->len >= 5U)
            {
                aqua_app_set_linkage(p_cmd->payload[0], get_i16_le(&p_cmd->payload[1]), get_u16_le(&p_cmd->payload[3]));
            }
            break;

        case COM_CMD_PUMP_START:
            aqua_app_pump_start((p_cmd->len >= 1U), (p_cmd->len >= 1U) ? clamp_percent(p_cmd->payload[0]) : 0U);
            break;

        case COM_CMD_PUMP_STOP:
            aqua_app_pump_stop();
            break;

        case COM_CMD_SET_PUMP_PWM:
            if (p_cmd->len >= 1U)
            {
                aqua_app_set_pump_pwm(clamp_percent(p_cmd->payload[0]));
            }
            break;

        case COM_CMD_SET_PUMP_AUTO:
            aqua_app_set_pump_auto();
            break;

        case COM_CMD_USB_LIGHT_MODE:
            if (p_cmd->len >= 1U)
            {
                (void) aqua_app_set_usb_light_mode(p_cmd->payload[0]);
            }
            break;

        case COM_CMD_ATOMIZER_SET:
            if (p_cmd->len >= 1U)
            {
                if (p_cmd->payload[0] <= 1U)
                {
                    (void) aqua_app_set_atomizer(p_cmd->payload[0]);
                }
            }
            break;

        default:
            break;
    }
}

void com_build_uplink(uint8_t seq, const aqua_snapshot_t * p_snapshot, uint8_t out_frame[COM_UP_FRAME_LEN])
{
    uint8_t * p;
    uint16_t crc;

    if ((NULL == p_snapshot) || (NULL == out_frame))
    {
        return;
    }

    memset(out_frame, 0, COM_UP_FRAME_LEN);
    out_frame[0] = COM_UP_SYNC0;
    out_frame[1] = COM_UP_SYNC1;
    out_frame[2] = COM_UP_VERSION;
    out_frame[3] = seq;
    out_frame[4] = COM_UP_PAYLOAD_LEN;
    out_frame[5] = 0U;

    p = &out_frame[6];
    put_u32_le(&p[0], p_snapshot->timestamp_ms);
    put_i16_le(&p[4], float_to_i16_x10(p_snapshot->air_temp_c));
    put_i16_le(&p[6], float_to_i16_x10(p_snapshot->air_humi_pct));
    put_i16_le(&p[8], float_to_i16_x10(p_snapshot->water_temp_c));
    p[10] = p_snapshot->soil_moisture_pct;
    put_u16_le(&p[11], p_snapshot->tds_ntu);
    p[13] = p_snapshot->pump_pwm_pct;
    p[14] = p_snapshot->need_watering;
    p[15] = p_snapshot->atomizer_state;
    p[16] = p_snapshot->usb_light_mode;
    p[17] = 0U;
    p[18] = 0U;
    p[19] = 0U;
    put_u32_le(&p[20], p_snapshot->alarm_flags);
    put_u16_le(&p[24], p_snapshot->air_retry_count);
    put_u16_le(&p[26], p_snapshot->tds_retry_count);
    put_u16_le(&p[28], p_snapshot->uwt_retry_count);

    crc = com_crc16_modbus(out_frame, (uint16_t) (COM_UP_FRAME_LEN - 2U));
    out_frame[36] = (uint8_t) (crc & 0xFFU);
    out_frame[37] = (uint8_t) ((crc >> 8) & 0xFFU);
}
