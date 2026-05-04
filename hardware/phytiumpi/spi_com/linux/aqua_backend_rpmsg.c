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
#include <time.h>
#include <syslog.h>
#include <unistd.h>
#include <dirent.h>

#include <sys/ioctl.h>
#include <linux/rpmsg.h>

#include "spi_protocol.h"

static void deadline_monotonic(struct timespec *d, uint32_t add_ms)
{
    clock_gettime(CLOCK_MONOTONIC, d);
    d->tv_nsec += (long)(add_ms % 1000u) * 1000000L;
    d->tv_sec  += (time_t)(add_ms / 1000u);
    while (d->tv_nsec >= 1000000000L)
    {
        d->tv_sec++;
        d->tv_nsec -= 1000000000L;
    }
}

/* 距离 deadline 剩余毫秒，已过期返回 0 */
static int ms_until_deadline(const struct timespec *deadline)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long sec  = deadline->tv_sec - now.tv_sec;
    long nsec = deadline->tv_nsec - now.tv_nsec;
    if (nsec < 0)
    {
        sec--;
        nsec += 1000000000L;
    }
    if (sec < 0) return 0;
    if (sec > 86400) return 86400000; /* 防止 poll 溢出 */
    return (int)(sec * 1000L + nsec / 1000000L);
}

#define DEFAULT_RPMSG_CTRL     "/dev/rpmsg_ctrl0"
#define DEFAULT_RPMSG_DEV      "/dev/rpmsg0"
#define DEFAULT_RPMSG_SERVICE  "aqua-spi"
#define DEFAULT_RSP_TIMEOUT_MS 100u

/* autoselect 里复用，与 aqua_backend_spidev.c 内常量一致 */
#define AS_DEFAULT_SPIDEV_PATH "/dev/spidev0.0"

/* ------------------------------------------------------------------ */
/* Debug NDJSON（DEBUG MODE / session a3c220）                            */
/* ------------------------------------------------------------------ */

// #region agent log
static FILE *agent_dbg_open_log(void)
{
    FILE *fp = fopen("/home/user/.cursor/debug-a3c220.log", "a");
    if (!fp)
        fp = fopen("/tmp/debug-a3c220.ndjson", "a");
    return fp;
}

static void agent_dbg_rpmsg_xfer(ssize_t wr, ssize_t rr, uint32_t tmo_ms, const char *phase)
{
    struct timespec ts;
    long long ms;
    long long errno_hint = (wr < 0) ? (long long)(-wr) : 0LL;

    if (!phase)
        phase = "xfer";

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return;
    ms = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;

    FILE *fp = agent_dbg_open_log();
    if (!fp)
        return;
    fprintf(fp,
            "{\"sessionId\":\"a3c220\",\"timestamp\":%lld,"
            "\"location\":\"aqua_backend_rpmsg.c\",\"message\":\"%s\","
            "\"hypothesisId\":\"H_disconnect_kills_firmware_enomem\","
            "\"data\":{\"write_ret\":%zd,\"read_ret\":%zd,\"timeout_ms\":%u,"
            "\"neg_errno_hint\":%lld},\"runId\":\"diag\"}\n",
            ms, phase, wr, rr, (unsigned)tmo_ms, errno_hint);
    fclose(fp);
}
// #endregion

/* ------------------------------------------------------------------ */
/* 内部 helper                                                         */
/* ------------------------------------------------------------------ */

/*
 * rpmsg 字符设备在 O_NONBLOCK 下依赖 rpmsg_trysendto/read；但部分内核版本上
 * poll(POLLOUT) 不会因「可发送」置位（或长期不唤醒），若在 write 前先 poll，
 * 会误超时 -ETIMEDOUT(-110)。正确顺序：先试 read/write，仅 EAGAIN 再 poll。
 */
static ssize_t read_exact(int fd, void *buf, size_t want, uint32_t timeout_ms)
{
    if (timeout_ms == 0) timeout_ms = DEFAULT_RSP_TIMEOUT_MS;
    struct timespec deadline;
    deadline_monotonic(&deadline, timeout_ms);

    uint8_t *q = buf;
    size_t    got = 0;
    while (got < want)
    {
        ssize_t n = read(fd, q + got, want - got);
        if (n > 0)
        {
            got += (size_t)n;
            continue;
        }
        if (n == 0)
            return -EIO;
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                return -errno;
        }

        int wait = ms_until_deadline(&deadline);
        if (wait <= 0)
            return -ETIMEDOUT;
        struct pollfd pf = { .fd = fd, .events = POLLIN };
        int           pr = poll(&pf, 1, wait);
        if (pr == 0)
            return -ETIMEDOUT;
        if (pr < 0)
        {
            if (errno == EINTR)
                continue;
            return -errno;
        }
    }
    return (ssize_t)got;
}

/* virtio-rpmsg 部分栈在上游未读走 RX 时长时间不给 POLLOUT；排空前读一下可解冻 TX。 */
static void rpmsg_try_discard_readable(int fd)
{
    uint8_t scratch[512];

    for (unsigned k = 0; k < 128u; k++)
    {
        ssize_t r = read(fd, scratch, sizeof scratch);
        if (r > 0)
            continue;
        if (r == 0)
            break;
        if (errno == EINTR)
            continue;
        break;
    }
}

static ssize_t write_all_timeout(int fd, const void *buf, size_t want, uint32_t timeout_ms)
{
    if (timeout_ms == 0) timeout_ms = DEFAULT_RSP_TIMEOUT_MS;
    struct timespec deadline;
    deadline_monotonic(&deadline, timeout_ms);

    const uint8_t *p   = (const uint8_t *)buf;
    size_t          done = 0;
    while (done < want)
    {
        ssize_t n = write(fd, p + done, want - done);
        if (n > 0)
        {
            done += (size_t)n;
            continue;
        }
        if (n == 0)
            return -EIO;
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                return -errno;
        }

        int wait = ms_until_deadline(&deadline);
        if (wait <= 0)
            return -ETIMEDOUT;
        /*
         * 单等 POLLOUT 在个别内核/virtio 组合上会误超时：
         * 同时监听 POLLIN；若可读则排空，避免因未消费的入站 SKB 卡住发送。
         */
        struct pollfd po = { .fd = fd, .events = (short)(POLLIN | POLLOUT) };
        int           pr = poll(&po, 1, wait);
        if (pr == 0)
            return -ETIMEDOUT;
        if (pr < 0)
        {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (po.revents & (POLLERR | POLLHUP | POLLNVAL))
            return -EIO;
        if (po.revents & POLLIN)
            rpmsg_try_discard_readable(fd);
    }
    return (ssize_t)done;
}

static int rpmsg_dev_on_primary_virtio(const char *rpmsg_d_name)
{
    char  lpath[384], target[512];
    snprintf(lpath, sizeof lpath, "/sys/class/rpmsg/%s/device", rpmsg_d_name);
    ssize_t n = readlink(lpath, target, sizeof target - 1);
    if (n < 0) return 0;
    target[n] = '\0';
    /* 飞腾 OpenAMP Aqua SPI 挂在第一个 virtio 控制通道上；勿误选其它 virtio 的同名残留节点 */
    return strstr(target, "virtio0") != NULL;
}

/*
 * 当前 sysfs 中与 service 同名、且优先 virtio0 的 rpmsg%d 最大编号（与原先 pick_latest 规则一致）。
 * 无匹配返回 -1。
 */
static int rpmsg_aqua_sysfs_max_id(const char *service_name)
{
    DIR           *dir = opendir("/sys/class/rpmsg");
    struct dirent *de;
    int            best_id    = -1;
    int            best_v0_id = -1;

    if (!dir) return -1;

    while ((de = readdir(dir)) != NULL)
    {
        int   id;
        char  extra;
        char  path[320], name[64] = {0};
        FILE *fp;

        if (sscanf(de->d_name, "rpmsg%d%c", &id, &extra) != 1)
            continue;

        snprintf(path, sizeof path, "/sys/class/rpmsg/%s/name", de->d_name);
        fp = fopen(path, "r");
        if (!fp) continue;
        if (fgets(name, sizeof name, fp) == NULL)
        {
            fclose(fp);
            continue;
        }
        fclose(fp);
        name[strcspn(name, "\r\n")] = '\0';

        if (strcmp(name, service_name) != 0)
            continue;
        if (id > best_id)
            best_id = id;
        if (rpmsg_dev_on_primary_virtio(de->d_name) && id > best_v0_id)
            best_v0_id = id;
    }
    closedir(dir);

    return (best_v0_id >= 0) ? best_v0_id : best_id;
}

static int rpmsg_pick_latest_dev(const char *service_name, char *out, size_t out_len)
{
    int pick = rpmsg_aqua_sysfs_max_id(service_name);

    if (pick < 0) return -1;
    snprintf(out, out_len, "/dev/rpmsg%d", pick);
    return 0;
}

static int rpmsg_open_inner(aqua_backend_rpmsg_ctx_t *c)
{
    int                          ctrl = -1, dev = -1;
    struct rpmsg_endpoint_info   ept;
    int                          id_before;
    int                          chosen_id = -1;
    unsigned                     w;

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

    /* 须在 ioctl 之前采样：CREATE_EPT 返回时新端点往往已在 sysfs，之后再读 max 会把「新建 id」算进 before，导致永远无法 id_now>id_before */
    id_before = rpmsg_aqua_sysfs_max_id(c->service_name);

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

    /*
     * IOCTL 返回后新建的 /dev/rpmsgN 可能晚一拍才出现在 sysfs；
     * 若立刻按「当前 max」open，会误绑旧 aqua-spi 节点 → write EAGAIN + POLLOUT 超时 -110。
     * 已用上面的 id_before（ioctl 前快照）比较 ioctl 后的 max，直到更大再 open。
     */
    for (w = 0; w < 3000u; w += 20u)
    {
        int id_now = rpmsg_aqua_sysfs_max_id(c->service_name);
        if (id_now > id_before || (id_before < 0 && id_now >= 0))
        {
            chosen_id = id_now;
            break;
        }
        {
            struct timespec sl = { 0, 20L * 1000L * 1000L };
            (void)nanosleep(&sl, NULL);
        }
    }

    if (chosen_id >= 0)
    {
        snprintf(c->resolved_device_path, sizeof c->resolved_device_path,
                 "/dev/rpmsg%d", chosen_id);
        c->device_path = c->resolved_device_path;
        if (c->log)
            c->log(LOG_INFO,
                   "RPMSG 选用 ioctl 后新建的 /dev/rpmsg%d（ioctl 前 sysfs max id=%d）",
                   chosen_id, id_before);
    }
    else
    {
        if (c->log)
            c->log(LOG_WARNING,
                   "RPMSG 3s 内 sysfs max id 未递增 (before=%d)，退回 pick_latest（可能误绑旧节点）",
                   id_before);
        if (rpmsg_pick_latest_dev(c->service_name, c->resolved_device_path,
                                  sizeof c->resolved_device_path) != 0)
        {
            if (c->log) c->log(LOG_ERR, "无法解析 %s 对应的 /dev/rpmsgN", c->service_name);
            close(ctrl);
            return -1;
        }
        c->device_path = c->resolved_device_path;
    }

    dev = open(c->device_path, O_RDWR);
    if (dev < 0)
    {
        if (c->log) c->log(LOG_ERR, "打开 %s 失败: %s",
                           c->device_path, strerror(errno));
        close(ctrl);
        return -1;
    }

    {
        int fl = fcntl(dev, F_GETFL, 0);
        if (fl >= 0) fcntl(dev, F_SETFL, fl | O_NONBLOCK);
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

    rpmsg_try_discard_readable(c->rpmsg_fd);

    ssize_t w = write_all_timeout(c->rpmsg_fd, cmd_frame, SPI_FRAME_LEN, c->timeout_ms);
    if (w != (ssize_t)SPI_FRAME_LEN)
    {
        c->io_err_count++;
        if (c->log) c->log(LOG_ERR, "rpmsg xfer: write 不完整/失败 ret=%zd（期待 %u 字节）",
                           w, SPI_FRAME_LEN);
        agent_dbg_rpmsg_xfer(w, -1, c->timeout_ms, "xfer_write_fail");
        return (int)((w < 0) ? w : -EIO);
    }

    ssize_t r = read_exact(c->rpmsg_fd, rsp_frame, SPI_FRAME_LEN, c->timeout_ms);
    if (r != (ssize_t)SPI_FRAME_LEN)
    {
        c->io_err_count++;
        if (c->log) c->log(LOG_ERR, "rpmsg xfer: read 不完整/失败 ret=%zd（期待 %u 字节）",
                           r, SPI_FRAME_LEN);
        agent_dbg_rpmsg_xfer(w, r, c->timeout_ms, "xfer_read_fail");
        return (int)((r < 0) ? r : -EIO);
    }
    agent_dbg_rpmsg_xfer(w, r, c->timeout_ms, "xfer_ok");
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
