/*
 * test_spi_protocol.c — PC 端纯 C 单元测试
 *
 * 用途：在不接硬件的情况下验证 spi_codec.c / spi_protocol.h 的正确性。
 * 编译： gcc -O2 -Wall -Iinclude -Ilinux/libaqua_spi tests/test_spi_protocol.c \
 *            linux/libaqua_spi/spi_codec.c -o tests/test_spi_protocol
 * 运行： tests/test_spi_protocol
 *
 * 失败时返回非 0，所有断言走 assert()，便于 CI 集成。
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "spi_protocol.h"
#include "spi_codec.h"

/* -------- 1. CRC-16/Modbus 已知向量 -------- */
static void test_crc_known_vectors(void)
{
    /* Modbus 标准测试向量："123456789" → 0x4B37 */
    const uint8_t v1[] = {'1','2','3','4','5','6','7','8','9'};
    uint16_t c1 = spi_crc16_modbus(v1, sizeof v1);
    assert(c1 == 0x4B37u);

    /* 空输入：初值未变 */
    uint16_t c2 = spi_crc16_modbus(NULL, 0);
    assert(c2 == 0xFFFFu);

    /* 单字节 0x00 */
    const uint8_t v3[] = {0x00};
    uint16_t c3 = spi_crc16_modbus(v3, 1);
    assert(c3 == 0x40BFu);

    printf("[OK] CRC-16/Modbus 已知向量\n");
}

/* -------- 2. CMD 帧打包：长度边界 -------- */
static void test_pack_cmd_boundaries(void)
{
    uint8_t frame[SPI_FRAME_LEN];
    uint8_t big[SPI_CMD_PAYLOAD_MAX + 1];
    memset(big, 0xCC, sizeof big);

    /* 正常长度 */
    assert(spi_pack_cmd(frame, 1, SPI_DEV_PUMP, SPI_CMD_PUMP_SET_PWM,
                        (uint8_t[]){80}, 1, 0) == 0);
    assert(frame[SPI_CMD_OFF_SOF0] == SPI_SOF0_CMD);
    assert(frame[SPI_CMD_OFF_SOF1] == SPI_SOF1_CMD);
    assert(frame[SPI_CMD_OFF_VER]  == SPI_PROTO_VERSION);
    assert(frame[SPI_CMD_OFF_DEV]  == SPI_DEV_PUMP);
    assert(frame[SPI_CMD_OFF_CMD]  == SPI_CMD_PUMP_SET_PWM);
    assert(frame[SPI_CMD_OFF_LEN]  == 1);
    assert(frame[SPI_CMD_OFF_PAYLOAD] == 80);

    /* payload 超长 → 拒绝 */
    assert(spi_pack_cmd(frame, 1, SPI_DEV_PUMP, 0, big, SPI_CMD_PAYLOAD_MAX + 1, 0) != 0);

    /* len = 0 + payload = NULL 合法（如 SYS_PING） */
    assert(spi_pack_cmd(frame, 2, SPI_DEV_SYSTEM, SPI_CMD_SYS_PING, NULL, 0, 0) == 0);
    assert(frame[SPI_CMD_OFF_LEN] == 0);

    /* CRC 字段非零 */
    uint16_t crc = spi_u16_get(&frame[SPI_OFF_CRC]);
    assert(crc != 0);

    printf("[OK] CMD 打包：长度边界\n");
}

/* -------- 3. RSP 帧打包 + uptime 字段 -------- */
static void test_pack_rsp_basic(void)
{
    uint8_t frame[SPI_FRAME_LEN];
    uint8_t payload[SPI_SENSOR_PAYLOAD_LEN];
    memset(payload, 0xAA, sizeof payload);

    assert(spi_pack_rsp(frame,
                        /*ack_seq*/ 7,
                        SPI_STATUS_OK,
                        SPI_RSP_TYPE_SENSOR_DATA,
                        payload, sizeof payload,
                        SPI_RSP_FLAG_DATA_READY,
                        /*uptime*/ 0x12345678u) == 0);

    assert(frame[SPI_RSP_OFF_SOF0]    == SPI_SOF0_RSP);
    assert(frame[SPI_RSP_OFF_SOF1]    == SPI_SOF1_RSP);
    assert(frame[SPI_RSP_OFF_ACK_SEQ] == 7);
    assert(frame[SPI_RSP_OFF_STATUS]  == SPI_STATUS_OK);
    assert(frame[SPI_RSP_OFF_TYPE]    == SPI_RSP_TYPE_SENSOR_DATA);
    assert(frame[SPI_RSP_OFF_LEN]     == SPI_SENSOR_PAYLOAD_LEN);
    assert(frame[SPI_RSP_OFF_FLAGS]   == SPI_RSP_FLAG_DATA_READY);

    uint32_t up = spi_u32_get(&frame[SPI_RSP_OFF_UPTIME]);
    assert(up == 0x12345678u);

    /* 保留区必须 0 */
    assert(frame[SPI_RSP_OFF_RESERVED]     == 0);
    assert(frame[SPI_RSP_OFF_RESERVED + 1] == 0);

    printf("[OK] RSP 打包：基础字段 + uptime\n");
}

/* -------- 4. 帧校验：正反例 -------- */
static void test_validate_frame(void)
{
    uint8_t frame[SPI_FRAME_LEN];

    /* 4.1 正常 CMD */
    assert(spi_pack_cmd(frame, 0, SPI_DEV_SENSOR, SPI_CMD_SENSOR_POLL_ALL,
                        NULL, 0, SPI_CMD_FLAG_NEED_RSP) == 0);
    assert(spi_validate_frame(frame, /*is_cmd*/1) == 0);

    /* 4.2 SOF 损坏 */
    uint8_t bad = frame[0];
    frame[0] ^= 0xFF;
    assert(spi_validate_frame(frame, 1) == -((int)SPI_STATUS_BAD_SOF));
    frame[0] = bad;

    /* 4.3 CRC 损坏 */
    frame[SPI_OFF_CRC] ^= 0x55;
    assert(spi_validate_frame(frame, 1) == -((int)SPI_STATUS_CRC_ERR));
    frame[SPI_OFF_CRC] ^= 0x55;
    assert(spi_validate_frame(frame, 1) == 0);

    /* 4.4 VER 错 */
    frame[SPI_CMD_OFF_VER] = 0xFE;
    /* 改了 VER 之后 CRC 也需要更新才能进入 VER 检查（先看实现走哪条）；
       但当前实现先比较 SOF/VER/LEN，再比较 CRC，所以 VER 错误会先返回 */
    assert(spi_validate_frame(frame, 1) == -((int)SPI_STATUS_VER_MISMATCH));
    frame[SPI_CMD_OFF_VER] = SPI_PROTO_VERSION;

    /* 4.5 LEN 越界 */
    frame[SPI_CMD_OFF_LEN] = SPI_CMD_PAYLOAD_MAX + 1;
    assert(spi_validate_frame(frame, 1) == -((int)SPI_STATUS_BAD_PAYLOAD_LEN));
    frame[SPI_CMD_OFF_LEN] = 0;

    /* 4.6 RSP 帧路径 */
    assert(spi_pack_rsp(frame, 0, SPI_STATUS_OK, SPI_RSP_TYPE_ACK,
                        NULL, 0, 0, 1234) == 0);
    assert(spi_validate_frame(frame, /*is_cmd*/0) == 0);
    /* 用 CMD SOF 去校验 RSP 帧应当失败 */
    assert(spi_validate_frame(frame, 1) == -((int)SPI_STATUS_BAD_SOF));

    printf("[OK] 帧校验：正反例\n");
}

/* -------- 5. 端到端：打包 → 改 1 字节 → 校验失败 → 还原 → 校验通过 -------- */
static void test_round_trip_with_payload(void)
{
    uint8_t frame[SPI_FRAME_LEN];
    uint8_t payload[11] = {1, 1, 80, 0, 100, 0, 50, 0, 30, 0, 0xFF}; /* 模拟 PUMP_CYCLE_CFG */

    assert(spi_pack_cmd(frame, 42, SPI_DEV_PUMP, SPI_CMD_PUMP_SET_CYCLE_CFG,
                        payload, sizeof payload, 0) == 0);
    assert(spi_validate_frame(frame, 1) == 0);

    /* PAYLOAD 任一字节翻转必触发 CRC 错 */
    frame[SPI_CMD_OFF_PAYLOAD + 5] ^= 0x10;
    assert(spi_validate_frame(frame, 1) == -((int)SPI_STATUS_CRC_ERR));
    frame[SPI_CMD_OFF_PAYLOAD + 5] ^= 0x10;
    assert(spi_validate_frame(frame, 1) == 0);

    /* PAYLOAD 字段值原样可读出 */
    for (size_t i = 0; i < sizeof payload; i++)
        assert(frame[SPI_CMD_OFF_PAYLOAD + i] == payload[i]);

    printf("[OK] 端到端：打包/校验/篡改检测\n");
}

/* -------- 6. 关键尺寸断言（防止有人误改 spi_protocol.h） -------- */
static void test_layout_invariants(void)
{
    assert(SPI_FRAME_LEN == 64u);
    assert(SPI_OFF_CRC + SPI_CRC_LEN == SPI_FRAME_LEN);
    assert(SPI_CMD_OFF_PAYLOAD + SPI_CMD_PAYLOAD_MAX <= SPI_CMD_OFF_RESERVED);
    assert(SPI_CMD_OFF_RESERVED + SPI_CMD_RESERVED_LEN == SPI_OFF_CRC);
    assert(SPI_RSP_OFF_PAYLOAD + SPI_RSP_PAYLOAD_MAX == SPI_RSP_OFF_UPTIME);
    assert(SPI_RSP_OFF_UPTIME + 4 == SPI_RSP_OFF_RESERVED);
    assert(SPI_RSP_OFF_RESERVED + SPI_RSP_RESERVED_LEN == SPI_OFF_CRC);
    assert(SPI_SENSOR_PAYLOAD_LEN <= SPI_RSP_PAYLOAD_MAX);

    printf("[OK] 帧布局尺寸不变量\n");
}

int main(void)
{
    test_layout_invariants();
    test_crc_known_vectors();
    test_pack_cmd_boundaries();
    test_pack_rsp_basic();
    test_validate_frame();
    test_round_trip_with_payload();

    printf("\n*** 所有 SPI 协议层单元测试通过 ***\n");
    return 0;
}
