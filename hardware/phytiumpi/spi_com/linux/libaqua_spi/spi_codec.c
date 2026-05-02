/*
 * spi_codec.c — 帧打包 / 解析 / CRC 实现
 *
 * 仅依赖 stdint.h / string.h，可同时编译进 Linux 用户态、单元测试、RA6E2。
 */

#include <string.h>

#include "spi_codec.h"

uint16_t spi_crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u)
                             : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

int spi_pack_cmd(uint8_t *frame,
                 uint8_t seq,
                 uint8_t dev,
                 uint8_t cmd,
                 const uint8_t *payload,
                 uint8_t len,
                 uint8_t flags)
{
    if (frame == NULL)              return -1;
    if (len > SPI_CMD_PAYLOAD_MAX)  return -1;

    memset(frame, 0, SPI_FRAME_LEN);

    frame[SPI_CMD_OFF_SOF0]  = SPI_SOF0_CMD;
    frame[SPI_CMD_OFF_SOF1]  = SPI_SOF1_CMD;
    frame[SPI_CMD_OFF_VER]   = SPI_PROTO_VERSION;
    frame[SPI_CMD_OFF_SEQ]   = seq;
    frame[SPI_CMD_OFF_DEV]   = dev;
    frame[SPI_CMD_OFF_CMD]   = cmd;
    frame[SPI_CMD_OFF_LEN]   = len;
    frame[SPI_CMD_OFF_FLAGS] = flags;

    if (payload != NULL && len > 0)
    {
        memcpy(&frame[SPI_CMD_OFF_PAYLOAD], payload, len);
    }

    uint16_t crc = spi_crc16_modbus(frame, SPI_OFF_CRC);
    spi_u16_put(&frame[SPI_OFF_CRC], crc);
    return 0;
}

int spi_pack_rsp(uint8_t *frame,
                 uint8_t ack_seq,
                 uint8_t status,
                 uint8_t type,
                 const uint8_t *payload,
                 uint8_t len,
                 uint8_t flags,
                 uint32_t uptime_ms)
{
    if (frame == NULL)              return -1;
    if (len > SPI_RSP_PAYLOAD_MAX)  return -1;

    memset(frame, 0, SPI_FRAME_LEN);

    frame[SPI_RSP_OFF_SOF0]    = SPI_SOF0_RSP;
    frame[SPI_RSP_OFF_SOF1]    = SPI_SOF1_RSP;
    frame[SPI_RSP_OFF_VER]     = SPI_PROTO_VERSION;
    frame[SPI_RSP_OFF_ACK_SEQ] = ack_seq;
    frame[SPI_RSP_OFF_STATUS]  = status;
    frame[SPI_RSP_OFF_TYPE]    = type;
    frame[SPI_RSP_OFF_LEN]     = len;
    frame[SPI_RSP_OFF_FLAGS]   = flags;

    if (payload != NULL && len > 0)
    {
        memcpy(&frame[SPI_RSP_OFF_PAYLOAD], payload, len);
    }

    spi_u32_put(&frame[SPI_RSP_OFF_UPTIME], uptime_ms);

    uint16_t crc = spi_crc16_modbus(frame, SPI_OFF_CRC);
    spi_u16_put(&frame[SPI_OFF_CRC], crc);
    return 0;
}

int spi_validate_frame(const uint8_t *frame, int is_cmd)
{
    if (frame == NULL) return -((int)SPI_STATUS_BAD_SOF);

    uint8_t sof0 = is_cmd ? SPI_SOF0_CMD : SPI_SOF0_RSP;
    uint8_t sof1 = is_cmd ? SPI_SOF1_CMD : SPI_SOF1_RSP;

    if (frame[0] != sof0 || frame[1] != sof1)
        return -((int)SPI_STATUS_BAD_SOF);

    if (frame[SPI_CMD_OFF_VER] != SPI_PROTO_VERSION)
        return -((int)SPI_STATUS_VER_MISMATCH);

    uint8_t  len    = is_cmd ? frame[SPI_CMD_OFF_LEN] : frame[SPI_RSP_OFF_LEN];
    uint8_t  maxlen = is_cmd ? SPI_CMD_PAYLOAD_MAX    : SPI_RSP_PAYLOAD_MAX;
    if (len > maxlen)
        return -((int)SPI_STATUS_BAD_PAYLOAD_LEN);

    uint16_t calc = spi_crc16_modbus(frame, SPI_OFF_CRC);
    uint16_t got  = spi_u16_get(&frame[SPI_OFF_CRC]);
    if (calc != got)
        return -((int)SPI_STATUS_CRC_ERR);

    return 0;
}
