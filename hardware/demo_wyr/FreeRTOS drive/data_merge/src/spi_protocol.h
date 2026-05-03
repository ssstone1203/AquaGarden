/*
 * spi_protocol.h — AquaGarden 飞腾派(Master) ↔ RA6E2(Slave) SPI 通信协议
 *
 * 该文件被三处共同 include：
 *   1) Linux 用户态 daemon / CLI / 共享库（通过 spidev 驱动 SPI Master）
 *   2) PC 端单元测试
 *   3) RA6E2 FreeRTOS 工程（SPI Slave，FSP HAL）
 *
 * 所有偏移、字段、命令编号都集中在本头文件，**禁止在其他地方重新定义**。
 *
 * 协议版本 v1。后续不兼容修改请提升 SPI_PROTO_VERSION 并保留 v1 解析路径。
 */

#ifndef AQUAGARDEN_SPI_PROTOCOL_H
#define AQUAGARDEN_SPI_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 协议版本 / 帧总长 / 端序 / 位序                                     */
/* ------------------------------------------------------------------ */

#define SPI_PROTO_VERSION       0x01u

/* 定长 64 字节，CMD/RSP 共用同一帧长（SPI 全双工对齐） */
#define SPI_FRAME_LEN           64u

/* 多字节字段一律小端（Little Endian），位序 MSB First */

/* ------------------------------------------------------------------ */
/* 帧头 / 帧尾魔数                                                    */
/* ------------------------------------------------------------------ */

#define SPI_SOF0_CMD            0xA5u   /* Master → Slave 帧起始字节 0 */
#define SPI_SOF1_CMD            0x5Au   /* Master → Slave 帧起始字节 1 */
#define SPI_SOF0_RSP            0x5Au   /* Slave  → Master 帧起始字节 0（与 CMD 镜像） */
#define SPI_SOF1_RSP            0xA5u

/* ------------------------------------------------------------------ */
/* CMD 帧布局（MOSI 方向，主 → 从）                                    */
/* ------------------------------------------------------------------ */

#define SPI_CMD_OFF_SOF0        0u
#define SPI_CMD_OFF_SOF1        1u
#define SPI_CMD_OFF_VER         2u
#define SPI_CMD_OFF_SEQ         3u
#define SPI_CMD_OFF_DEV         4u
#define SPI_CMD_OFF_CMD         5u
#define SPI_CMD_OFF_LEN         6u
#define SPI_CMD_OFF_FLAGS       7u
#define SPI_CMD_OFF_PAYLOAD     8u
#define SPI_CMD_PAYLOAD_MAX     16u    /* PAYLOAD 最大长度 */
#define SPI_CMD_OFF_RESERVED   24u    /* 24..61 共 38 字节保留 */
#define SPI_CMD_RESERVED_LEN   38u

/* ------------------------------------------------------------------ */
/* RSP 帧布局（MISO 方向，从 → 主）                                    */
/* ------------------------------------------------------------------ */

#define SPI_RSP_OFF_SOF0        0u
#define SPI_RSP_OFF_SOF1        1u
#define SPI_RSP_OFF_VER         2u
#define SPI_RSP_OFF_ACK_SEQ     3u    /* 被确认的命令 SEQ；0xFF 表示无对应命令(Idle) */
#define SPI_RSP_OFF_STATUS      4u    /* 见 spi_status_t */
#define SPI_RSP_OFF_TYPE        5u    /* 见 spi_rsp_type_t */
#define SPI_RSP_OFF_LEN         6u    /* PAYLOAD 实际有效字节数 */
#define SPI_RSP_OFF_FLAGS       7u
#define SPI_RSP_OFF_PAYLOAD     8u
#define SPI_RSP_PAYLOAD_MAX    48u    /* PAYLOAD 最大长度 */
#define SPI_RSP_OFF_UPTIME    56u    /* uint32_t LE，单位 ms（49.7 天溢出，仅供诊断） */
#define SPI_RSP_OFF_RESERVED  60u    /* 60..61 共 2 字节保留 */
#define SPI_RSP_RESERVED_LEN   2u

/* CRC16 始终位于帧尾两字节（CMD/RSP 共享） */
#define SPI_OFF_CRC            62u
#define SPI_CRC_LEN             2u

/* ------------------------------------------------------------------ */
/* FLAGS 位（CMD 与 RSP 各自独立解释）                                  */
/* ------------------------------------------------------------------ */

/* CMD FLAGS */
#define SPI_CMD_FLAG_NEED_RSP   (1u << 0)   /* 主机要求"下一次事务"必须能取回本帧响应 */
/* bit1..7 保留 */

/* RSP FLAGS */
#define SPI_RSP_FLAG_DATA_READY (1u << 0)   /* 从机仍有数据待主机继续轮询 */
#define SPI_RSP_FLAG_ALARM      (1u << 1)   /* 从机有未读告警（详见 SensorData.alarm_flags） */
#define SPI_RSP_FLAG_READY_GPIO (1u << 2)   /* v1 预留：将来通过 GPIO 通知就绪 */
/* bit3..7 保留 */

/* ------------------------------------------------------------------ */
/* 设备类别 (DEV)                                                     */
/* ------------------------------------------------------------------ */

typedef enum
{
    SPI_DEV_SYSTEM   = 0x00u,
    SPI_DEV_PUMP     = 0x01u,
    SPI_DEV_SENSOR   = 0x02u,
    SPI_DEV_LINKAGE  = 0x03u,
    /* 0x04..0xEF 预留给后续设备 */
    /* 0xF0..0xFF 厂商自定义 */
} spi_dev_t;

/* ------------------------------------------------------------------ */
/* 命令 ID（按设备类别分桶）                                           */
/* ------------------------------------------------------------------ */

/* DEV_SYSTEM */
typedef enum
{
    SPI_CMD_SYS_NOP          = 0x00u,   /* 空操作；用于把上一帧 RSP 取回 */
    SPI_CMD_SYS_PING         = 0x01u,   /* 心跳，从机回 STATUS=OK */
    SPI_CMD_SYS_GET_VERSION  = 0x02u,   /* 请求从机协议版本号；RSP_TYPE=STATUS */
    SPI_CMD_SYS_GET_UPTIME   = 0x03u,   /* 仅返回 uptime(已在 RSP 帧固定字段)；RSP_TYPE=ACK */
    SPI_CMD_SYS_RESET_ALARM  = 0x04u,   /* 清除从机锁存的告警位 */
    SPI_CMD_SYS_HELLO        = 0x05u,   /* 主机重启后用于通知从机 SEQ 已复位为 0 */
} spi_cmd_system_t;

/* DEV_PUMP */
typedef enum
{
    SPI_CMD_PUMP_START          = 0x01u, /* [0]=power% (0..100)；可省略 */
    SPI_CMD_PUMP_STOP           = 0x02u, /* 无参数 */
    SPI_CMD_PUMP_SET_PWM        = 0x03u, /* [0]=power% */
    SPI_CMD_PUMP_SET_AUTO       = 0x04u, /* 切回自动模式 */
    SPI_CMD_PUMP_SET_MANUAL     = 0x05u, /* [0]=enable, [1]=power% */
    SPI_CMD_PUMP_SET_CYCLE_CFG  = 0x10u, /* 11 字节，见 §3.2.2 文档 */
    SPI_CMD_PUMP_SET_CYCLE_CTRL = 0x11u, /* [0]=enable, [1]=start, [2..5]=total_count(LE) */
    SPI_CMD_PUMP_GET_STATUS     = 0x20u, /* RSP_TYPE=STATUS */
} spi_cmd_pump_t;

/* DEV_SENSOR */
typedef enum
{
    SPI_CMD_SENSOR_POLL_ALL      = 0x10u, /* 核心命令；RSP_TYPE=SENSOR_DATA */
    SPI_CMD_SENSOR_POLL_AIR      = 0x11u,
    SPI_CMD_SENSOR_POLL_WATER    = 0x12u,
    SPI_CMD_SENSOR_POLL_SOIL     = 0x13u,
    SPI_CMD_SENSOR_POLL_PRESSURE = 0x14u,
} spi_cmd_sensor_t;

/* DEV_LINKAGE */
typedef enum
{
    SPI_CMD_LINK_SET_SOIL_CFG = 0x01u,  /* [0]=threshold%, [1]=hysteresis% */
    SPI_CMD_LINK_SET_RULE     = 0x02u,  /* 4 字节：[0]=mask, [1..2]=water_temp_high*10(i16 LE), [3]=wqi_low_threshold */
} spi_cmd_linkage_t;

/* ------------------------------------------------------------------ */
/* 响应类型 (RSP_TYPE)                                                */
/* ------------------------------------------------------------------ */

typedef enum
{
    SPI_RSP_TYPE_ACK         = 0x00u, /* 仅 STATUS 有意义，PAYLOAD 全 0 */
    SPI_RSP_TYPE_SENSOR_DATA = 0x01u, /* PAYLOAD 见 spi_sensor_payload_t */
    SPI_RSP_TYPE_STATUS      = 0x02u, /* 设备具体状态结构（如 PUMP_STATUS） */
    SPI_RSP_TYPE_VERSION     = 0x03u, /* PAYLOAD 见 spi_version_payload_t */
} spi_rsp_type_t;

/* ------------------------------------------------------------------ */
/* 状态码 (STATUS)                                                    */
/* ------------------------------------------------------------------ */

typedef enum
{
    SPI_STATUS_OK              = 0x00u,
    SPI_STATUS_CRC_ERR         = 0x01u, /* CRC 校验失败 */
    SPI_STATUS_UNKNOWN_DEV     = 0x02u, /* DEV 字段未识别 */
    SPI_STATUS_UNKNOWN_CMD     = 0x03u, /* CMD 字段在该 DEV 下未识别 */
    SPI_STATUS_BAD_PAYLOAD_LEN = 0x04u, /* PAYLOAD 长度不符合命令要求 */
    SPI_STATUS_BUSY            = 0x05u, /* 资源忙（比如水泵正在启动过渡中） */
    SPI_STATUS_VER_MISMATCH    = 0x06u, /* CMD 帧 VER 字段不被识别 */
    SPI_STATUS_BAD_SOF         = 0x07u, /* 帧头不对，疑似 SPI 错位 */
    /* 0x80..0xFF 厂商扩展 */
} spi_status_t;

/* ------------------------------------------------------------------ */
/* SENSOR_DATA PAYLOAD（48 字节，全部 LE）                             */
/* 字段顺序与 RA6E2 现有 host_build_uplink_frame 对齐                 */
/* ------------------------------------------------------------------ */

#define SPI_SENS_OFF_TIMESTAMP_MS         0u  /* u32  ms */
#define SPI_SENS_OFF_AIR_TEMP_X10         4u  /* i16  0.1°C */
#define SPI_SENS_OFF_AIR_HUMIDITY_X10     6u  /* i16  0.1%RH */
#define SPI_SENS_OFF_WATER_TEMP_X10       8u  /* i16  0.1°C */
#define SPI_SENS_OFF_SOIL_MOISTURE_PCT   10u  /* u8   % */
#define SPI_SENS_OFF_WQI                 11u  /* u8   0..100 */
#define SPI_SENS_OFF_PUMP_POWER_PCT      12u  /* u8   % */
#define SPI_SENS_OFF_NEED_WATERING       13u  /* u8   bool */
#define SPI_SENS_OFF_PRESSURE_KG_0_X100  14u  /* u16  0.01 kg */
#define SPI_SENS_OFF_PRESSURE_KG_1_X100  16u  /* u16 */
#define SPI_SENS_OFF_PRESSURE_KG_2_X100  18u  /* u16 */
#define SPI_SENS_OFF_ALARM_FLAGS         20u  /* u32  位掩码 */
#define SPI_SENS_OFF_AIR_RETRY           24u  /* u16 */
#define SPI_SENS_OFF_WQS_RETRY           26u  /* u16 */
#define SPI_SENS_OFF_UWT_RETRY           28u  /* u16 */
#define SPI_SENS_OFF_PUMP_CYCLE_ENABLE   30u  /* u8 */
#define SPI_SENS_OFF_PUMP_CYCLE_START    31u  /* u8 */
#define SPI_SENS_OFF_PUMP_CYCLE_ACTIVE   32u  /* u8 */
#define SPI_SENS_OFF_PUMP_CYCLE_STATE    33u  /* u8 */
#define SPI_SENS_OFF_PUMP_CYCLE_POWER    34u  /* u8 */
#define SPI_SENS_OFF_PUMP_CYCLE_DONE     35u  /* u16 */
/* 37..47 reserved (11B), 必须填 0 */
#define SPI_SENSOR_PAYLOAD_LEN           48u

/* ------------------------------------------------------------------ */
/* VERSION PAYLOAD（4 字节）                                           */
/* ------------------------------------------------------------------ */

#define SPI_VER_OFF_PROTO   0u  /* u8 */
#define SPI_VER_OFF_MAJOR   1u  /* u8 */
#define SPI_VER_OFF_MINOR   2u  /* u8 */
#define SPI_VER_OFF_PATCH   3u  /* u8 */
#define SPI_VERSION_PAYLOAD_LEN  4u

/* ------------------------------------------------------------------ */
/* SEQ 特殊值                                                          */
/* ------------------------------------------------------------------ */

#define SPI_SEQ_IDLE        0xFFu  /* 从机首次上电后未收到任何 CMD 时使用 */

#ifdef __cplusplus
}
#endif

#endif /* AQUAGARDEN_SPI_PROTOCOL_H */
