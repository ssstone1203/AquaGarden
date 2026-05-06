/*
 * aqua_ipc.h — daemon ↔ 客户端 Unix Domain Socket 协议
 *
 * 仅 Linux 用户态使用（CLI、未来的 Web 后端、Python ctypes 绑定等）。
 * 不参与 SPI 物理层；与 RA6E2 无关。
 *
 * 单 socket / 单连接 / 单请求-单响应模型。客户端发送 aqua_ipc_req_t，
 * daemon 处理后回复 aqua_ipc_rsp_t，然后客户端关闭连接。
 */

#ifndef AQUAGARDEN_AQUA_IPC_H
#define AQUAGARDEN_AQUA_IPC_H

#include <stdint.h>

#include "spi_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* socket 路径（守护进程必须 chmod 0666 让普通用户也能用） */
#define AQUA_IPC_SOCK_PATH      "/tmp/aqua_spi.sock"

/* 协议魔数：用于在野指针/老客户端误连时拒绝处理 */
#define AQUA_IPC_MAGIC          0x41513031u   /* "AQ01" 小端 */

/* 客户端可发的操作码 */
typedef enum
{
    AQUA_OP_SEND_CMD     = 1, /* 同步发一帧 SPI CMD 并取回 RSP（CMD + NOP_READ 两次事务） */
    AQUA_OP_GET_SNAPSHOT = 2, /* 读取 daemon 缓存的最近一份 SensorData，不走 SPI */
    AQUA_OP_GET_STATS    = 3, /* 读取 daemon 累计统计（CRC 错误、丢帧等） */
    AQUA_OP_PING_DAEMON  = 4, /* 仅检查 daemon 是否在跑，不走 SPI */
} aqua_ipc_op_t;

/* 客户端 → daemon */
typedef struct
{
    uint32_t magic;                  /* AQUA_IPC_MAGIC */
    uint32_t op;                     /* aqua_ipc_op_t */

    /* 仅 AQUA_OP_SEND_CMD 使用以下字段 */
    uint8_t  dev;                    /* SPI 设备类别 */
    uint8_t  cmd;                    /* SPI 命令 ID */
    uint8_t  len;                    /* payload 字节数 */
    uint8_t  flags;                  /* SPI_CMD_FLAG_* */
    uint8_t  payload[SPI_CMD_PAYLOAD_MAX];

    uint32_t timeout_ms;             /* 等待 SPI RSP 超时（默认 100） */
} aqua_ipc_req_t;

/* daemon 持有的 SPI 累计统计（GET_STATS 返回） */
typedef struct
{
    uint64_t tx_cmd_count;           /* 发出的 CMD 帧总数 */
    uint64_t rx_rsp_ok_count;        /* 校验通过的 RSP 帧总数 */
    uint64_t rx_crc_err_count;       /* CRC 校验失败次数 */
    uint64_t rx_sof_err_count;       /* 帧头错误次数（疑似 SPI 错位） */
    uint64_t rx_timeout_count;       /* 等待响应超时次数 */
    uint64_t spidev_io_err_count;    /* spidev ioctl 失败次数 */
    uint32_t last_uptime_ms;         /* 最近一次 RSP 中带回的从机 uptime */
    uint32_t consecutive_err_count;  /* 当前连续错误次数（清零=最近一次成功） */
} aqua_ipc_stats_t;

/* daemon → 客户端 */
typedef struct
{
    uint32_t magic;                  /* AQUA_IPC_MAGIC */
    int32_t  rc;                     /* 0=OK；<0 为 -errno 或 spi_validate 的 -spi_status_t（如 -7=BAD_SOF，勿用 strerror 误读） */

    /* 当 op == SEND_CMD 时：以下是从机回的 RSP 帧解码后字段 */
    uint8_t  status;                 /* spi_status_t */
    uint8_t  rsp_type;               /* spi_rsp_type_t */
    uint8_t  ack_seq;
    uint8_t  flags;                  /* SPI_RSP_FLAG_* */
    uint8_t  rsp_len;                /* 实际有效 payload 长度 */
    uint8_t  pad[3];
    uint8_t  payload[SPI_RSP_PAYLOAD_MAX];
    uint32_t uptime_ms;              /* 从机 uptime（ms） */

    /* 当 op == GET_STATS 时：以下字段有效 */
    aqua_ipc_stats_t stats;

    /* 当 op == GET_SNAPSHOT 时：复用 payload[]+rsp_len 字段，
     * payload 即缓存的 SENSOR_DATA(48B)，rsp_len = 48 */
} aqua_ipc_rsp_t;

#ifdef __cplusplus
}
#endif

#endif /* AQUAGARDEN_AQUA_IPC_H */
