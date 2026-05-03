/*
 * aqua_backend_rpmsg.c — v2 后端：通过 OpenAMP RPMsg 把 64B 帧
 * 交给飞腾派裸机核固件 openamp_spi_core0.elf
 *
 * 这一层不感知 SPI 物理细节：裸机核固件已经在内部把
 * "CMD + 5ms gap + NOP_READ" 两次 SPI 事务做完，
 * 只把最终 64B RSP 帧通过同一个 RPMsg endpoint 回传给我们。
 *
 * 协议：
 *   Linux→裸机核 RPMsg 包 = 64B CMD frame，纯字节，无额外包头
 *   裸机核→Linux RPMsg 包 = 64B RSP frame，纯字节，无额外包头
 *
 * 与裸机核固件的契合点：endpoint name == AQUA_RPMSG_SERVICE
 */

#define _GNU_SOURCE
#include "aqua_backend.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/rpmsg.h>

#include "spi_protocol.h"

#define DEFAULT_RPMSG_CTRL     "/dev/rpmsg_ctrl0"
#define DEFAULT_RPMSG_DEV      "/dev/rpmsg0"
#define DEFAULT_RPMSG_SERVICE  "aqua-spi"
#define DEFAULT_RSP_TIMEOUT_MS 100u

/* autoselect 里复用，与 aqua_backend_spidev.c 内常量一致 */
#define AS_DEFAULT_SPIDEV_PATH "/dev/spidev0.0"

/* ------------------------------------------------------------------ */
/* 内部 helper                                                         */
/* ------------------------------------------------------------------ */

/* 读完整 N 字节，处理 EINTR；超时阈值 timeout_ms，0 = 阻塞等待。
 * 返回实际读字节数；负值 = -errno。
 */
static ssize_t read_exact(int fd, void *buf, size_t want, uint32_t timeout_ms)
{
    if (timeout_ms)
    {
        struct pollfd p = { .fd = fd, .events = POLLIN };
        int pr = poll(&p, 1, (int)timeout_ms);
        if (pr == 0) return -ETIMEDOUT;
        if (pr < 0)  return -errno;
    }
    size_t got = 0;
    uint8_t *q = buf;
    while (got < want)
    {
        ssize_t n = read(fd, q + got, want - got);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -errno;
        }
        if (n == 0) return -EIO;            /* EOF：远端断了 */
        got += (size_t)n;
        /* RPMsg 一次 read 通常拿一整包；万一包大于剩余缓冲就是协议错 */
        if ((size_t)n != want && got != want) break;
    }
    return (ssize_t)got;
}

static ssize_t write_all(int fd, const void *buf, size_t want)
{
    size_t done = 0;
    const uint8_t *p = buf;
    while (done < want)
    {
        ssize_t n = write(fd, p + done, want - done);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -errno;
        }
        done += (size_t)n;
    }
    return (ssize_t)done;
}

static int rpmsg_open_inner(aqua_backend_rpmsg_ctx_t *c)
{
    int ctrl = -1, dev = -1;
    struct rpmsg_endpoint_info ept;

    ctrl = open(c->ctrl_path, O_RDWR);
    if (ctrl < 0)
    {
        if (c->log) c->log(LOG_ERR, "打开 %s 失败: %s（remoteproc/rpmsg 是否已加载？）",
                           c->ctrl_path, strerror(errno));
        return -1;
    }

    memset(&ept, 0, sizeof ept);
    snprintf(ept.name, sizeof ept.name, "%s", c->service_name);
    ept.src = 0;
    ept.dst = 0xFFFFFFFFu;          /* "any"，让内核去匹配裸机核 ann ouncing 出来的 endpoint */

    /* RPMSG_CREATE_EPT_IOCTL 在裸机核 announce 服务名之后才会成功；
     * 如果远端还没就绪，这里会返回 ENODEV。我们只重试一次内核默认的等待。 */
    if (ioctl(ctrl, RPMSG_CREATE_EPT_IOCTL, &ept) < 0)
    {
        if (c->log) c->log(LOG_ERR, "RPMSG_CREATE_EPT_IOCTL '%s' 失败: %s "
                                    "（裸机核固件未启动或 endpoint 名不匹配）",
                                    c->service_name, strerror(errno));
        close(ctrl);
        return -1;
    }

    dev = open(c->device_path, O_RDWR);
    if (dev < 0)
    {
        if (c->log) c->log(LOG_ERR, "打开 %s 失败: %s",
                           c->device_path, strerror(errno));
        close(ctrl);
        return -1;
    }

    c->ctrl_fd  = ctrl;
    c->rpmsg_fd = dev;
    if (c->log) c->log(LOG_INFO, "已打开 RPMsg endpoint '%s' via %s",
                       c->service_name, c->device_path);
    return 0;
}

static void rpmsg_close_inner(aqua_backend_rpmsg_ctx_t *c)
{
    if (c->rpmsg_fd >= 0) { close(c->rpmsg_fd); c->rpmsg_fd = -1; }
    if (c->ctrl_fd  >= 0) { close(c->ctrl_fd);  c->ctrl_fd  = -1; }
}

/* ------------------------------------------------------------------ */
/* ops 实现                                                            */
/* ------------------------------------------------------------------ */

static int op_xfer(void *_c,
                   const uint8_t cmd_frame[SPI_FRAME_LEN],
                   uint8_t       rsp_frame[SPI_FRAME_LEN])
{
    aqua_backend_rpmsg_ctx_t *c = _c;
    if (c->rpmsg_fd < 0) return -EIO;

    ssize_t w = write_all(c->rpmsg_fd, cmd_frame, SPI_FRAME_LEN);
    if (w != (ssize_t)SPI_FRAME_LEN)
    {
        c->io_err_count++;
        if (c->log) c->log(LOG_ERR, "rpmsg write 失败: %zd", w);
        return (int)((w < 0) ? w : -EIO);
    }

    ssize_t r = read_exact(c->rpmsg_fd, rsp_frame, SPI_FRAME_LEN, c->timeout_ms);
    if (r != (ssize_t)SPI_FRAME_LEN)
    {
        c->io_err_count++;
        if (c->log) c->log(LOG_ERR, "rpmsg read 失败: %zd", r);
        return (int)((r < 0) ? r : -EIO);
    }
    return 0;
}

static int op_reset(void *_c)
{
    aqua_backend_rpmsg_ctx_t *c = _c;
    if (c->log) c->log(LOG_WARNING,
                       "rpmsg 后端 reset：close 端点 + 重连"
                       "（如裸机核已挂死，需先 echo stop/start > /sys/class/remoteproc/.../state）");
    rpmsg_close_inner(c);
    return rpmsg_open_inner(c);
}

static void op_close(void *_c)
{
    rpmsg_close_inner(_c);
}

const aqua_backend_ops_t aqua_backend_rpmsg_ops = {
    .xfer  = op_xfer,
    .reset = op_reset,
    .close = op_close,
    .kind  = AQUA_BACKEND_RPMSG,
    .name  = "rpmsg",
};

/* ------------------------------------------------------------------ */
/* 公开 helper                                                         */
/* ------------------------------------------------------------------ */

int aqua_backend_rpmsg_open(aqua_backend_rpmsg_ctx_t *c)
{
    if (!c) return -EINVAL;
    if (!c->ctrl_path)    c->ctrl_path    = DEFAULT_RPMSG_CTRL;
    if (!c->device_path)  c->device_path  = DEFAULT_RPMSG_DEV;
    if (!c->service_name) c->service_name = DEFAULT_RPMSG_SERVICE;
    if (c->timeout_ms == 0) c->timeout_ms = DEFAULT_RSP_TIMEOUT_MS;
    c->ctrl_fd  = -1;
    c->rpmsg_fd = -1;
    return rpmsg_open_inner(c);
}

/* ------------------------------------------------------------------ */
/* autoselect                                                          */
/* ------------------------------------------------------------------ */

int aqua_backend_autoselect(aqua_backend_spidev_ctx_t *spidev_ctx,
                            aqua_backend_rpmsg_ctx_t  *rpmsg_ctx,
                            const aqua_backend_ops_t **out_ops,
                            void                     **out_ctx)
{
    /* 优先 v2：rpmsg 控制节点存在就尝试 */
    if (rpmsg_ctx)
    {
        if (!rpmsg_ctx->ctrl_path)   rpmsg_ctx->ctrl_path   = DEFAULT_RPMSG_CTRL;
        if (!rpmsg_ctx->device_path) rpmsg_ctx->device_path = DEFAULT_RPMSG_DEV;
        if (access(rpmsg_ctx->ctrl_path, R_OK | W_OK) == 0)
        {
            if (aqua_backend_rpmsg_open(rpmsg_ctx) == 0)
            {
                *out_ops = &aqua_backend_rpmsg_ops;
                *out_ctx = rpmsg_ctx;
                return 0;
            }
        }
    }

    /* fallback v1：spidev */
    if (spidev_ctx)
    {
        if (!spidev_ctx->device_path) spidev_ctx->device_path = AS_DEFAULT_SPIDEV_PATH;
        if (access(spidev_ctx->device_path, R_OK | W_OK) == 0)
        {
            if (aqua_backend_spidev_open(spidev_ctx) == 0)
            {
                *out_ops = &aqua_backend_spidev_ops;
                *out_ctx = spidev_ctx;
                return 0;
            }
        }
    }
    return -1;
}

uint64_t aqua_backend_io_err_count(const aqua_backend_ops_t *ops, void *ctx)
{
    if (!ops || !ctx) return 0;
    if (ops->kind == AQUA_BACKEND_SPIDEV)
        return ((aqua_backend_spidev_ctx_t *)ctx)->io_err_count;
    if (ops->kind == AQUA_BACKEND_RPMSG)
        return ((aqua_backend_rpmsg_ctx_t  *)ctx)->io_err_count;
    return 0;
}
