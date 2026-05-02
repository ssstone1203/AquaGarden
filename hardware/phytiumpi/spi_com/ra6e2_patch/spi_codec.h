/*
 * spi_codec.h — 帧打包 / 解析 / CRC 助手
 *
 * 与 spi_protocol.h 配合使用。本文件只做"字节层"操作，不依赖任何 OS / spidev。
 * 因此可以同时被 Linux 用户态、PC 单测、RA6E2 (FreeRTOS) 引用。
 */

#ifndef AQUAGARDEN_SPI_CODEC_H
#define AQUAGARDEN_SPI_CODEC_H

#include <stdint.h>
#include <stddef.h>

#include "spi_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 端序（小端）读写 ---------- */

static inline void spi_u16_put(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline void spi_u32_put(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline uint16_t spi_u16_get(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t spi_u32_get(const uint8_t *p)
{
    return  ((uint32_t)p[0])
          | ((uint32_t)p[1] << 8)
          | ((uint32_t)p[2] << 16)
          | ((uint32_t)p[3] << 24);
}

static inline int16_t spi_i16_get(const uint8_t *p)
{
    return (int16_t)spi_u16_get(p);
}

/* ---------- CRC-16/Modbus（多项式 0xA001，初值 0xFFFF，无最终 XOR） ---------- */

uint16_t spi_crc16_modbus(const uint8_t *data, uint16_t len);

/* ---------- CMD 帧打包 ----------
 *
 * frame  : 调用者提供的 SPI_FRAME_LEN(64) 字节缓冲，会被整体清零后填入
 * seq    : 主机自递增序列号（0..255 循环），用于响应配对
 * dev    : 设备类别（spi_dev_t）
 * cmd    : 命令 ID（spi_cmd_*_t）
 * payload: 命令参数指针，可为 NULL
 * len    : payload 字节数，0..SPI_CMD_PAYLOAD_MAX；超出返回 -1
 * flags  : SPI_CMD_FLAG_* 位组合
 *
 * 返回 0 表示成功；自动写入 SOF/VER/SEQ/DEV/CMD/LEN/FLAGS/PAYLOAD/CRC16。
 */
int spi_pack_cmd(uint8_t *frame,
                 uint8_t seq,
                 uint8_t dev,
                 uint8_t cmd,
                 const uint8_t *payload,
                 uint8_t len,
                 uint8_t flags);

/* ---------- RSP 帧打包（从机端使用） ----------
 *
 * frame      : SPI_FRAME_LEN(64) 缓冲，整体清零后填入
 * ack_seq    : 被确认的 CMD SEQ；首次上电无对应命令时用 SPI_SEQ_IDLE
 * status     : spi_status_t
 * type       : spi_rsp_type_t
 * payload    : 响应数据指针，可为 NULL
 * len        : payload 字节数，0..SPI_RSP_PAYLOAD_MAX
 * flags      : SPI_RSP_FLAG_*
 * uptime_ms  : 从机运行时间，写入 RSP_OFF_UPTIME 固定字段
 *
 * 返回 0 成功，-1 长度越界。
 */
int spi_pack_rsp(uint8_t *frame,
                 uint8_t ack_seq,
                 uint8_t status,
                 uint8_t type,
                 const uint8_t *payload,
                 uint8_t len,
                 uint8_t flags,
                 uint32_t uptime_ms);

/* ---------- 帧校验 ----------
 *
 * 返回 0 = OK；< 0 = 错误（具体值 = -spi_status_t）
 *   -SPI_STATUS_BAD_SOF / -SPI_STATUS_VER_MISMATCH / -SPI_STATUS_CRC_ERR / -SPI_STATUS_BAD_PAYLOAD_LEN
 *
 * `is_cmd` 为 1 时按 CMD 帧验证（SOF=A5,5A，PAYLOAD ≤16）；
 *          为 0 时按 RSP 帧验证（SOF=5A,A5，PAYLOAD ≤48）。
 */
int spi_validate_frame(const uint8_t *frame, int is_cmd);

#ifdef __cplusplus
}
#endif

#endif /* AQUAGARDEN_SPI_CODEC_H */
