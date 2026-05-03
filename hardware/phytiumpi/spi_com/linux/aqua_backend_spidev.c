/*
 * aqua_backend_spidev.c — v1 后端：直接 ioctl /dev/spidev0.0
 *
 * 一个 xfer() 内部跑两次 SPI 事务（CMD + 5ms gap + NOP_READ），
 * 与原 v1 aqua_spid.c::spi_send_cmd 的物理行为完全一致。
 *
 * 与裸机核固件无关；只要 spidev 节点存在并指向 FSPI0 就能工作。
 */

#define _GNU_SOURCE
#include "aqua_backend.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "spi_codec.h"
#include "spi_protocol.h"

#define DEFAULT_SPIDEV_PATH    "/dev/spidev0.0"
#define DEFAULT_SPEED_HZ       1000000u
#define DEFAULT_FRAME_GAP_US   5000u

/* ------------------------------------------------------------------ */
/* 内部 helper                                                         */
/* ------------------------------------------------------------------ */

static int spidev_open_inner(aqua_backend_spidev_ctx_t *c)
{
    int fd = open(c->device_path, O_RDWR);
    if (fd < 0)
    {
        if (c->log) c->log(LOG_ERR, "打开 %s 失败: %s",
                           c->device_path, strerror(errno));
        return -1;
    }
    /*
     * 重要：飞腾 spi-phytium 的 mode_bits 不含 SPI_CS_HIGH，
     * 当 GPIO 作 CS 时 spidev 会自动合并 SPI_CS_HIGH，导致
     * SPI_IOC_WR_MODE32 失败 (EINVAL)。Mode 0 是探测默认，跳过 mode ioctl。
     */

    uint8_t bits = 8;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
    {
        if (c->log) c->log(LOG_WARNING,
                           "SPI_IOC_WR_BITS_PER_WORD 失败 (忽略): %s",
                           strerror(errno));
    }

    uint32_t hz = c->speed_hz;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &hz) < 0)
    {
        if (c->log) c->log(LOG_ERR, "SPI_IOC_WR_MAX_SPEED_HZ %u 失败: %s",
                           hz, strerror(errno));
        close(fd);
        return -1;
    }
    if (c->log) c->log(LOG_INFO, "已打开 %s @ %u Hz, 8-bit, Mode 0",
                       c->device_path, c->speed_hz);
    return fd;
}

static int xfer_one(aqua_backend_spidev_ctx_t *c,
                    const uint8_t *tx, uint8_t *rx)
{
    if (c->fd < 0) return -EIO;

    struct spi_ioc_transfer tr = {
        .tx_buf        = (unsigned long)tx,
        .rx_buf        = (unsigned long)rx,
        .len           = SPI_FRAME_LEN,
        .speed_hz      = c->speed_hz,
        .bits_per_word = 8,
        .cs_change     = 0,
    };
    int n = ioctl(c->fd, SPI_IOC_MESSAGE(1), &tr);
    if (n < 1)
    {
        c->io_err_count++;
        if (c->log) c->log(LOG_ERR, "SPI_IOC_MESSAGE 失败: %s", strerror(errno));
        return -EIO;
    }
    return 0;
}

static void sleep_us(uint32_t us)
{
    if (us == 0) return;
    struct timespec ts = {
        .tv_sec  = us / 1000000,
        .tv_nsec = (long)(us % 1000000) * 1000L,
    };
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------ */
/* ops 实现                                                            */
/* ------------------------------------------------------------------ */

static int op_xfer(void *_c,
                   const uint8_t cmd_frame[SPI_FRAME_LEN],
                   uint8_t       rsp_frame[SPI_FRAME_LEN])
{
    aqua_backend_spidev_ctx_t *c = _c;

    /* 第 1 次事务：发 CMD，丢 MISO（是上一帧的滞后响应） */
    uint8_t rx_dummy[SPI_FRAME_LEN];
    int rc = xfer_one(c, cmd_frame, rx_dummy);
    if (rc) return rc;

    /* 帧间隔，给 RA6E2 在 ISR 中装好 RSP */
    sleep_us(c->frame_gap_us);

    /* 第 2 次事务：发 NOP，MISO 即本次命令的响应 */
    uint8_t nop[SPI_FRAME_LEN];
    if (spi_pack_cmd(nop, /*seq*/SPI_SEQ_IDLE,
                     SPI_DEV_SYSTEM, SPI_CMD_SYS_NOP,
                     NULL, 0, 0) != 0)
        return -EINVAL;
    rc = xfer_one(c, nop, rsp_frame);
    return rc;
}

static int op_reset(void *_c)
{
    aqua_backend_spidev_ctx_t *c = _c;
    if (c->log) c->log(LOG_WARNING, "spidev 后端 reset：close + 重新 open");
    if (c->fd >= 0) { close(c->fd); c->fd = -1; }
    /* 给 RA6E2 端 DMAC 一点复位时间 */
    sleep_us(10000);
    int fd = spidev_open_inner(c);
    if (fd < 0) return -1;
    c->fd = fd;
    return 0;
}

static void op_close(void *_c)
{
    aqua_backend_spidev_ctx_t *c = _c;
    if (c->fd >= 0) { close(c->fd); c->fd = -1; }
}

const aqua_backend_ops_t aqua_backend_spidev_ops = {
    .xfer  = op_xfer,
    .reset = op_reset,
    .close = op_close,
    .kind  = AQUA_BACKEND_SPIDEV,
    .name  = "spidev",
};

/* ------------------------------------------------------------------ */
/* 公开 helper                                                         */
/* ------------------------------------------------------------------ */

int aqua_backend_spidev_open(aqua_backend_spidev_ctx_t *c)
{
    if (!c) return -EINVAL;
    if (!c->device_path)  c->device_path  = DEFAULT_SPIDEV_PATH;
    if (!c->speed_hz)     c->speed_hz     = DEFAULT_SPEED_HZ;
    if (!c->frame_gap_us) c->frame_gap_us = DEFAULT_FRAME_GAP_US;
    c->fd = spidev_open_inner(c);
    return (c->fd < 0) ? -1 : 0;
}
