/*
 * aqua_backend.h — daemon SPI 后端抽象接口
 *
 * 一个"后端"对外只承担一件事：
 *   把 64 字节 CMD 帧发给 RA6E2，把 64 字节 RSP 帧取回来。
 *
 * v1 后端 (aqua_backend_spidev.c)：
 *   - 直接 ioctl /dev/spidev0.0
 *   - 内部跑 CMD + 5 ms gap + NOP_READ 两次事务
 *
 * v2 后端 (aqua_backend_rpmsg.c)：
 *   - write/read /dev/rpmsg0 各一次
 *   - 飞腾派裸机核固件在内部跑两次 SPI 事务，对 daemon 透明
 *
 * 两版 backend 接口完全一致，daemon 主循环 (aqua_daemon_core.c) 不感知差异。
 */

#ifndef AQUAGARDEN_AQUA_BACKEND_H
#define AQUAGARDEN_AQUA_BACKEND_H

#include <stdint.h>

#include "spi_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    AQUA_BACKEND_NONE   = 0,
    AQUA_BACKEND_SPIDEV = 1,    /* v1: /dev/spidev0.0 */
    AQUA_BACKEND_RPMSG  = 2,    /* v2: /dev/rpmsg0 (OpenAMP) */
} aqua_backend_kind_t;

/* 通用日志回调（由 daemon 提供，避免 backend 依赖具体日志实现） */
typedef void (*aqua_log_fn)(int prio, const char *fmt, ...);

typedef struct aqua_backend_ops
{
    /* 阻塞收发：发 64B CMD，收 64B RSP。
     * 返回 0 = OK；
     *      -EIO = 物理层错（spidev ioctl / rpmsg read/write）；
     *      -ETIMEDOUT = 超时；
     *      其他负值 = backend 自定义。
     * 注意：CRC 校验由 daemon_core 做，backend 不解析帧内容。
     */
    int  (*xfer)(void *ctx,
                 const uint8_t cmd_frame[SPI_FRAME_LEN],
                 uint8_t       rsp_frame[SPI_FRAME_LEN]);

    /* 连续错误自愈：close 后重新 open（spidev 重置 / remoteproc restart 等）。
     * 返回 0 = OK；负值 = 失败（daemon 会继续运行但后续 xfer 都会失败）。
     */
    int  (*reset)(void *ctx);

    /* 释放所有资源。daemon 退出时调用。 */
    void (*close)(void *ctx);

    aqua_backend_kind_t kind;
    const char         *name;     /* 给日志用，如 "spidev" / "rpmsg" */
} aqua_backend_ops_t;

/* ------------------------------------------------------------------ */
/* spidev 后端 (v1)                                                   */
/* ------------------------------------------------------------------ */

typedef struct
{
    /* 调用方设置 */
    const char *device_path;      /* "/dev/spidev0.0" */
    uint32_t    speed_hz;         /* 1_000_000 */
    uint32_t    frame_gap_us;     /* 5000 = CMD/NOP 间隔，给 RA6E2 装 RSP */
    aqua_log_fn log;              /* 可为 NULL */

    /* 内部 */
    int      fd;
    uint64_t io_err_count;        /* 累计 ioctl 失败次数（daemon GET_STATS 用） */
} aqua_backend_spidev_ctx_t;

extern const aqua_backend_ops_t aqua_backend_spidev_ops;

/* 初始化 + 打开。成功返回 0。 */
int aqua_backend_spidev_open(aqua_backend_spidev_ctx_t *ctx);

/* ------------------------------------------------------------------ */
/* rpmsg 后端 (v2 OpenAMP)                                            */
/* ------------------------------------------------------------------ */

typedef struct
{
    /* 调用方设置 */
    const char *ctrl_path;        /* "/dev/rpmsg_ctrl0" */
    const char *device_path;      /* "/dev/rpmsg0" */
    const char *service_name;     /* "aqua-spi"，与裸机核 endpoint name 一致 */
    uint32_t    timeout_ms;       /* 单次 read 超时；0 = 阻塞 */
    aqua_log_fn log;

    /* 内部 */
    int      ctrl_fd;
    int      rpmsg_fd;
    uint64_t io_err_count;
} aqua_backend_rpmsg_ctx_t;

extern const aqua_backend_ops_t aqua_backend_rpmsg_ops;

int aqua_backend_rpmsg_open(aqua_backend_rpmsg_ctx_t *ctx);

/* ------------------------------------------------------------------ */
/* 自动选择                                                            */
/* ------------------------------------------------------------------ */

/* 优先 rpmsg，失败回退 spidev。
 * out_ops / out_ctx 由本函数填好，调用方不需要自己 open。
 * 返回 0 = OK；-1 = 两个后端都不可用。
 *
 * 调用前必须把 spidev_ctx / rpmsg_ctx 的"输入字段"准备好（路径/速率/日志）。
 * 没填的字段会用 default 值（见实现）。
 */
int aqua_backend_autoselect(aqua_backend_spidev_ctx_t *spidev_ctx,
                            aqua_backend_rpmsg_ctx_t  *rpmsg_ctx,
                            const aqua_backend_ops_t **out_ops,
                            void                     **out_ctx);

/* 把 backend->io_err_count 拷出来给 daemon 上报到 IPC stats。
 * （把 backend 内部计数和 daemon 的协议层计数解耦） */
uint64_t aqua_backend_io_err_count(const aqua_backend_ops_t *ops, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AQUAGARDEN_AQUA_BACKEND_H */
