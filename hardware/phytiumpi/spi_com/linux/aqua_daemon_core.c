/*
 * aqua_daemon_core.c — daemon 主循环 + IPC 处理（backend 无关）
 *
 * 这里跟 v1 原始 aqua_spid.c 的 main_loop / ipc_handle / hello_handshake
 * 在行为上完全一致，区别仅在于：
 *   - 不再直接 ioctl spidev；
 *   - 把 64B CMD 帧交给 backend->xfer()；
 *   - 校验回来的 64B RSP 帧；
 *   - 连续错 N 次触发 backend->reset()。
 */

#define _GNU_SOURCE
#include "aqua_daemon_core.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "spi_protocol.h"
#include "spi_codec.h"
#include "aqua_ipc.h"

/* ------------------------------------------------------------------ */
/* 单例 daemon 状态（aqua_daemon_run 单线程持有）                      */
/* ------------------------------------------------------------------ */

typedef struct
{
    const aqua_daemon_config_t *cfg;

    int sock_fd;
    int signal_fd[2];          /* self-pipe 把信号变成 poll 事件 */

    uint8_t next_seq;          /* 0..255 循环 */

    uint8_t snapshot[SPI_SENSOR_PAYLOAD_LEN];
    bool    snapshot_valid;

    aqua_ipc_stats_t stats;

    volatile sig_atomic_t quit;
} daemon_state_t;

static daemon_state_t *g_state = NULL;

/* ------------------------------------------------------------------ */
/* 日志：前台走 stderr，后台走 syslog                                  */
/* ------------------------------------------------------------------ */

static void log_msg(int prio, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (g_state && g_state->cfg->foreground)
    {
        const char *lvl = (prio == LOG_ERR) ? "ERR" :
                          (prio == LOG_WARNING) ? "WRN" :
                          (prio == LOG_INFO) ? "INF" : "DBG";
        fprintf(stderr, "[%s] ", lvl);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
    }
    else
    {
        vsyslog(prio, fmt, ap);
    }
    va_end(ap);
}

#define LOGE(fmt, ...) log_msg(LOG_ERR,     fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) log_msg(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) log_msg(LOG_INFO,    fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) do { if (g_state && g_state->cfg->verbose) log_msg(LOG_DEBUG, fmt, ##__VA_ARGS__); } while (0)

/* ------------------------------------------------------------------ */
/* 对 backend 的薄封装：发 1 个命令并校验响应                          */
/* ------------------------------------------------------------------ */

static int do_send_cmd(uint8_t dev, uint8_t cmd,
                       const uint8_t *payload, uint8_t len, uint8_t flags,
                       uint8_t *rsp_frame)
{
    daemon_state_t *s   = g_state;
    const aqua_backend_ops_t *be = s->cfg->backend;
    void *ctx = s->cfg->backend_ctx;

    uint8_t tx[SPI_FRAME_LEN], rx[SPI_FRAME_LEN];
    uint8_t seq = s->next_seq++;

    if (spi_pack_cmd(tx, seq, dev, cmd, payload, len, flags | SPI_CMD_FLAG_NEED_RSP) != 0)
        return -EINVAL;

    int rc = be->xfer(ctx, tx, rx);
    s->stats.tx_cmd_count++;
    if (rc != 0)
    {
        s->stats.spidev_io_err_count++;     /* 字段名沿用，语义=后端 IO 错误 */

        /* rpmsg virtio 写路径在资源紧张时返回 -ENOMEM；若仍累计 consecutive_err 并
         * reset()，会反复 RPMSG_CREATE_EPT，/dev/rpmsgN 暴增，进一步耗尽缓冲（见现场
         * ret=-12 与 rpmsg384+）。此类错误不得触发 auto-reset。 */
        if (be->kind == AQUA_BACKEND_RPMSG && rc == -ENOMEM)
        {
            s->stats.consecutive_err_count = 0;
            LOGD("backend(%s) xfer 失败 rc=%d (ENOMEM)", be->name, rc);
            static int rpmsg_enomem_warned;
            if (!rpmsg_enomem_warned)
            {
                rpmsg_enomem_warned = 1;
                LOGW("RPMsg xfer ENOMEM(-12)：不再 auto-reset（避免新建 endpoint 耗尽资源）。"
                     " 请先冷启动或检查裸机固件是否含「Linux 断连后仍驻留 / 可重连」补丁。");
            }
            return rc;
        }

        s->stats.consecutive_err_count++;
        LOGD("backend(%s) xfer 失败 rc=%d", be->name, rc);
        if (s->stats.consecutive_err_count >= s->cfg->max_consec_err)
        {
            LOGW("连续 %u 次 IO 错，重置 backend(%s)...",
                 s->cfg->max_consec_err, be->name);
            be->reset(ctx);
            s->stats.consecutive_err_count = 0;
        }
        return rc;
    }

    int v = spi_validate_frame(rx, /*is_cmd*/0);
    if (v != 0)
    {
        if (v == -((int)SPI_STATUS_BAD_SOF)) s->stats.rx_sof_err_count++;
        else if (v == -((int)SPI_STATUS_CRC_ERR)) s->stats.rx_crc_err_count++;
        s->stats.consecutive_err_count++;
        LOGD("RSP 校验失败 v=%d (seq=%u dev=0x%02X cmd=0x%02X)", v, seq, dev, cmd);
        if (s->stats.consecutive_err_count >= s->cfg->max_consec_err)
        {
            LOGW("连续 %u 次帧错，重置 backend(%s)...",
                 s->cfg->max_consec_err, be->name);
            be->reset(ctx);
            s->stats.consecutive_err_count = 0;
        }
        return v;
    }

    s->stats.rx_rsp_ok_count++;
    s->stats.consecutive_err_count = 0;
    s->stats.last_uptime_ms = spi_u32_get(&rx[SPI_RSP_OFF_UPTIME]);
    if (rsp_frame) memcpy(rsp_frame, rx, SPI_FRAME_LEN);
    return 0;
}

/* ------------------------------------------------------------------ */
/* 周期 POLL_ALL                                                      */
/* ------------------------------------------------------------------ */

static void periodic_poll_all(void)
{
    uint8_t rsp[SPI_FRAME_LEN];
    int rc = do_send_cmd(SPI_DEV_SENSOR, SPI_CMD_SENSOR_POLL_ALL, NULL, 0, 0, rsp);
    if (rc != 0)
    {
        LOGD("周期 POLL_ALL 失败 rc=%d", rc);
        return;
    }
    if (rsp[SPI_RSP_OFF_TYPE]   != SPI_RSP_TYPE_SENSOR_DATA ||
        rsp[SPI_RSP_OFF_STATUS] != SPI_STATUS_OK            ||
        rsp[SPI_RSP_OFF_LEN]    != SPI_SENSOR_PAYLOAD_LEN)
    {
        LOGD("POLL_ALL 响应异常 type=%u status=%u len=%u",
             rsp[SPI_RSP_OFF_TYPE], rsp[SPI_RSP_OFF_STATUS], rsp[SPI_RSP_OFF_LEN]);
        return;
    }
    memcpy(g_state->snapshot, &rsp[SPI_RSP_OFF_PAYLOAD], SPI_SENSOR_PAYLOAD_LEN);
    g_state->snapshot_valid = true;
}

/* ------------------------------------------------------------------ */
/* IPC 处理                                                           */
/* ------------------------------------------------------------------ */

static void write_all_ignore(int fd, const void *buf, size_t n)
{
    ssize_t r = write(fd, buf, n);
    (void)r;
}

static void fill_rsp_from_frame(aqua_ipc_rsp_t *out, const uint8_t *frame)
{
    out->status   = frame[SPI_RSP_OFF_STATUS];
    out->rsp_type = frame[SPI_RSP_OFF_TYPE];
    out->ack_seq  = frame[SPI_RSP_OFF_ACK_SEQ];
    out->flags    = frame[SPI_RSP_OFF_FLAGS];
    out->rsp_len  = frame[SPI_RSP_OFF_LEN];
    out->uptime_ms= spi_u32_get(&frame[SPI_RSP_OFF_UPTIME]);
    if (out->rsp_len > SPI_RSP_PAYLOAD_MAX) out->rsp_len = SPI_RSP_PAYLOAD_MAX;
    memcpy(out->payload, &frame[SPI_RSP_OFF_PAYLOAD], out->rsp_len);
}

static void ipc_handle(int cfd)
{
    aqua_ipc_req_t req;
    aqua_ipc_rsp_t rsp;
    memset(&rsp, 0, sizeof rsp);
    rsp.magic = AQUA_IPC_MAGIC;

    ssize_t n = read(cfd, &req, sizeof req);
    if (n != (ssize_t)sizeof req)
    {
        LOGW("IPC 读包不完整 (%zd/%zu)", n, sizeof req);
        rsp.rc = -EBADMSG;
        write_all_ignore(cfd, &rsp, sizeof rsp);
        return;
    }
    if (req.magic != AQUA_IPC_MAGIC)
    {
        LOGW("IPC 魔数错 0x%08X", req.magic);
        rsp.rc = -EBADMSG;
        write_all_ignore(cfd, &rsp, sizeof rsp);
        return;
    }

    switch (req.op)
    {
    case AQUA_OP_PING_DAEMON:
        rsp.rc = 0;
        break;

    case AQUA_OP_GET_SNAPSHOT:
        if (!g_state->snapshot_valid)
        {
            rsp.rc = -ENODATA;
            break;
        }
        rsp.rc = 0;
        rsp.rsp_type = SPI_RSP_TYPE_SENSOR_DATA;
        rsp.rsp_len  = SPI_SENSOR_PAYLOAD_LEN;
        memcpy(rsp.payload, g_state->snapshot, SPI_SENSOR_PAYLOAD_LEN);
        rsp.uptime_ms = g_state->stats.last_uptime_ms;
        break;

    case AQUA_OP_GET_STATS:
        rsp.rc = 0;
        rsp.stats = g_state->stats;
        break;

    case AQUA_OP_SEND_CMD:
    {
        if (req.len > SPI_CMD_PAYLOAD_MAX)
        {
            rsp.rc = -EINVAL;
            break;
        }
        uint8_t frame[SPI_FRAME_LEN];
        int rc = do_send_cmd(req.dev, req.cmd, req.payload, req.len, req.flags, frame);
        if (rc < 0)
        {
            rsp.rc = rc;
            break;
        }
        rsp.rc = 0;
        fill_rsp_from_frame(&rsp, frame);
        break;
    }

    default:
        rsp.rc = -ENOTSUP;
        break;
    }

    write_all_ignore(cfd, &rsp, sizeof rsp);
}

/* ------------------------------------------------------------------ */
/* 信号处理                                                           */
/* ------------------------------------------------------------------ */

static void on_signal(int sig)
{
    if (!g_state) return;
    g_state->quit = 1;
    uint8_t b = (uint8_t)sig;
    if (g_state->signal_fd[1] >= 0)
    {
        ssize_t r = write(g_state->signal_fd[1], &b, 1);
        (void)r;
    }
}

/* ------------------------------------------------------------------ */
/* 监听 socket                                                        */
/* ------------------------------------------------------------------ */

static int listen_socket(void)
{
    unlink(AQUA_IPC_SOCK_PATH);

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        LOGE("socket() 失败: %s", strerror(errno));
        return -1;
    }
    struct sockaddr_un sa = { .sun_family = AF_UNIX };
    strncpy(sa.sun_path, AQUA_IPC_SOCK_PATH, sizeof sa.sun_path - 1);
    if (bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0)
    {
        LOGE("bind(%s) 失败: %s", AQUA_IPC_SOCK_PATH, strerror(errno));
        close(fd);
        return -1;
    }
    chmod(AQUA_IPC_SOCK_PATH, 0666);
    if (listen(fd, 8) < 0)
    {
        LOGE("listen() 失败: %s", strerror(errno));
        close(fd);
        return -1;
    }
    LOGI("监听 %s", AQUA_IPC_SOCK_PATH);
    return fd;
}

/* ------------------------------------------------------------------ */
/* 上电握手                                                           */
/* ------------------------------------------------------------------ */

static int hello_handshake(uint32_t timeout_ms)
{
    uint8_t frame[SPI_FRAME_LEN];
    g_state->next_seq = 0;
    (void)do_send_cmd(SPI_DEV_SYSTEM, SPI_CMD_SYS_HELLO, NULL, 0, 0, frame);

    /* 100 ms 一次 PING，最多重试 timeout_ms / 100 次 */
    uint32_t tries = (timeout_ms + 99) / 100;
    if (tries == 0) tries = 1;

    for (uint32_t i = 0; i < tries; i++)
    {
        int rc = do_send_cmd(SPI_DEV_SYSTEM, SPI_CMD_SYS_PING, NULL, 0, 0, frame);
        if (rc == 0 && frame[SPI_RSP_OFF_STATUS] == SPI_STATUS_OK)
        {
            LOGI("RA6E2 上线，uptime=%u ms",
                 spi_u32_get(&frame[SPI_RSP_OFF_UPTIME]));
            return 0;
        }
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    LOGE("RA6E2 在 %u ms 内未上线，daemon 仍继续运行（依赖周期重试）", timeout_ms);
    return -1;
}

/* ------------------------------------------------------------------ */
/* 主循环                                                             */
/* ------------------------------------------------------------------ */

static long ts_diff_ms(const struct timespec *a, const struct timespec *b)
{
    return (a->tv_sec - b->tv_sec) * 1000L + (a->tv_nsec - b->tv_nsec) / 1000000L;
}

static void main_loop(void)
{
    struct timespec next_poll;
    clock_gettime(CLOCK_MONOTONIC, &next_poll);

    while (!g_state->quit)
    {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long wait_ms = ts_diff_ms(&next_poll, &now);
        if (wait_ms < 0) wait_ms = 0;

        struct pollfd pfds[2] = {
            { .fd = g_state->sock_fd,      .events = POLLIN },
            { .fd = g_state->signal_fd[0], .events = POLLIN },
        };
        int n = poll(pfds, 2, (int)wait_ms);
        if (n < 0 && errno != EINTR)
        {
            LOGE("poll: %s", strerror(errno));
            break;
        }

        if (n > 0 && (pfds[0].revents & POLLIN))
        {
            int cfd = accept4(g_state->sock_fd, NULL, NULL, SOCK_CLOEXEC);
            if (cfd >= 0)
            {
                ipc_handle(cfd);
                close(cfd);
            }
        }

        if (n > 0 && (pfds[1].revents & POLLIN))
        {
            uint8_t b;
            ssize_t r = read(g_state->signal_fd[0], &b, 1);
            (void)r;
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        if (ts_diff_ms(&now, &next_poll) >= 0)
        {
            periodic_poll_all();
            next_poll.tv_nsec += (long)g_state->cfg->period_ms * 1000000L;
            while (next_poll.tv_nsec >= 1000000000L)
            {
                next_poll.tv_sec++;
                next_poll.tv_nsec -= 1000000000L;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* daemonize                                                          */
/* ------------------------------------------------------------------ */

static void do_daemonize(const char *progname)
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid > 0) exit(0);

    setsid();
    pid = fork();
    if (pid < 0) { perror("fork2"); exit(1); }
    if (pid > 0) exit(0);

    if (chdir("/") != 0) { /* ignore */ }
    umask(0);
    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2);
    if (devnull > 2) close(devnull);

    openlog(progname, LOG_PID, LOG_DAEMON);
}

/* ------------------------------------------------------------------ */
/* 公开 API                                                           */
/* ------------------------------------------------------------------ */

void aqua_daemon_config_init(aqua_daemon_config_t *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->progname         = "aqua_daemon";
    cfg->period_ms        = 250;
    cfg->hello_timeout_ms = 5000;
    cfg->max_consec_err   = 5;
    cfg->foreground       = false;
    cfg->verbose          = false;
}

int aqua_daemon_run(const aqua_daemon_config_t *cfg)
{
    if (!cfg || !cfg->backend || !cfg->backend_ctx)
    {
        fprintf(stderr, "[ERR] aqua_daemon_run: backend 未初始化\n");
        return 1;
    }

    daemon_state_t state;
    memset(&state, 0, sizeof state);
    state.cfg          = cfg;
    state.sock_fd      = -1;
    state.signal_fd[0] = state.signal_fd[1] = -1;
    state.next_seq     = 0;

    g_state = &state;

    if (cfg->foreground)
        openlog(cfg->progname, LOG_PID | LOG_PERROR, LOG_DAEMON);
    else
        do_daemonize(cfg->progname);

    if (pipe2(state.signal_fd, O_CLOEXEC | O_NONBLOCK) < 0)
    {
        LOGE("pipe2: %s", strerror(errno));
        return 1;
    }
    struct sigaction sa = { .sa_handler = on_signal, .sa_flags = SA_RESTART };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    state.sock_fd = listen_socket();
    if (state.sock_fd < 0) return 1;

    LOGI("%s 启动：backend=%s 周期=%u ms",
         cfg->progname, cfg->backend->name, cfg->period_ms);

    (void)hello_handshake(cfg->hello_timeout_ms);

    main_loop();

    LOGI("退出中...");
    /* 把 backend 累计 IO 错也搬到 stats（最后一次报告） */
    state.stats.spidev_io_err_count = aqua_backend_io_err_count(cfg->backend, cfg->backend_ctx);

    cfg->backend->close(cfg->backend_ctx);
    if (state.sock_fd >= 0) close(state.sock_fd);
    if (state.signal_fd[0] >= 0) close(state.signal_fd[0]);
    if (state.signal_fd[1] >= 0) close(state.signal_fd[1]);
    unlink(AQUA_IPC_SOCK_PATH);
    closelog();

    g_state = NULL;
    return 0;
}
