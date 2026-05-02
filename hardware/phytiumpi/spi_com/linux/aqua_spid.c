/*
 * aqua_spid.c — AquaGarden Linux 端 SPI 守护进程
 *
 * 职责：
 *   1) 独占持有 /dev/spidev0.0，按 1 MHz / Mode 0 / 8-bit 配置；
 *   2) 周期性 (默认 250 ms) 自动 POLL_ALL，缓存最近一份 SensorData；
 *   3) 提供 Unix Domain Socket (/tmp/aqua_spi.sock) 给本机其他进程发命令；
 *   4) 维护通信统计、连续错误自愈（关闭/重开 spidev）。
 *
 * 典型部署：systemd 单服务，--foreground 调试，无参数则后台 daemonize。
 *
 * 编译：见 spi_com/linux/Makefile
 *
 * 设计要点：单线程串行，所有 SPI 事务和 IPC 处理都在主循环里，避免锁。
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <linux/spi/spidev.h>

#include "spi_protocol.h"
#include "spi_codec.h"
#include "aqua_ipc.h"

/* ------------------------------------------------------------------ */
/* 配置默认值                                                         */
/* ------------------------------------------------------------------ */

#define DEFAULT_SPIDEV          "/dev/spidev0.0"
#define DEFAULT_SPEED_HZ        1000000u    /* 1 MHz */
#define DEFAULT_PERIOD_MS       250u        /* 主轮询周期 */
#define DEFAULT_FRAME_GAP_US    5000u       /* CMD 与 NOP_READ 之间的间隔（µs），给 RA6E2 装 RSP */
#define DEFAULT_RSP_TIMEOUT_MS  100u        /* 客户端 SEND_CMD 默认等响应时间 */
#define MAX_CONSEC_ERRORS       5u          /* 连续 N 次错误触发 spidev 重置 */

/* ------------------------------------------------------------------ */
/* 全局状态（单线程，无需锁）                                         */
/* ------------------------------------------------------------------ */

static struct
{
    /* 启动配置 */
    const char *spidev_path;
    uint32_t    speed_hz;
    uint32_t    period_ms;
    uint32_t    frame_gap_us;
    bool        foreground;
    bool        verbose;

    /* 运行时句柄 */
    int         spi_fd;
    int         sock_fd;          /* listening socket */
    int         signal_fd[2];     /* self-pipe trick：信号唤醒 poll() */

    /* SPI 序列号（0..255 循环） */
    uint8_t     next_seq;

    /* 最近一次成功的 SensorData 快照 + 时间戳 */
    uint8_t     snapshot[SPI_SENSOR_PAYLOAD_LEN];
    bool        snapshot_valid;
    struct timespec snapshot_ts;

    /* 累计统计 */
    aqua_ipc_stats_t stats;

    /* 是否退出 */
    volatile sig_atomic_t quit;
} g;

/* ------------------------------------------------------------------ */
/* 日志：daemon 模式走 syslog，前台模式走 stderr                       */
/* ------------------------------------------------------------------ */

static void log_msg(int prio, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (g.foreground)
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
#define LOGD(fmt, ...) do { if (g.verbose) log_msg(LOG_DEBUG, fmt, ##__VA_ARGS__); } while (0)

/* ------------------------------------------------------------------ */
/* spidev 打开 / 关闭 / 重置                                          */
/* ------------------------------------------------------------------ */

static int spi_open(void)
{
    int fd = open(g.spidev_path, O_RDWR);
    if (fd < 0)
    {
        LOGE("打开 %s 失败: %s", g.spidev_path, strerror(errno));
        return -1;
    }

    /*
     * 重要：飞腾 spi-phytium 的 mode_bits 不含 SPI_CS_HIGH，
     * 当 GPIO 作 CS 时 spidev 会自动合并 SPI_CS_HIGH，导致 SPI_IOC_WR_MODE32
     * 失败 (EINVAL)。Mode 0 是探测默认值，**直接跳过 mode ioctl** 即可。
     * 详见同目录 spi0_scope_demo.c 的注释。
     */

    uint8_t bits = 8;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
        LOGW("SPI_IOC_WR_BITS_PER_WORD 失败 (忽略): %s", strerror(errno));

    uint32_t hz = g.speed_hz;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &hz) < 0)
    {
        LOGE("SPI_IOC_WR_MAX_SPEED_HZ %u 失败: %s", hz, strerror(errno));
        close(fd);
        return -1;
    }

    LOGI("已打开 %s @ %u Hz, 8-bit, Mode 0", g.spidev_path, g.speed_hz);
    return fd;
}

static void spi_reset(void)
{
    LOGW("连续 %u 次错误，重置 spidev...", MAX_CONSEC_ERRORS);
    if (g.spi_fd >= 0) close(g.spi_fd);
    g.spi_fd = spi_open();
    g.stats.consecutive_err_count = 0;
    /* 短暂等待 RA6E2 端 DMAC 复位（保守 10 ms） */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10 * 1000 * 1000 };
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------ */
/* SPI 单次 64B 全双工事务                                            */
/* ------------------------------------------------------------------ */

static int spi_xfer_one(const uint8_t *tx, uint8_t *rx)
{
    if (g.spi_fd < 0) return -EIO;

    struct spi_ioc_transfer tr = {
        .tx_buf        = (unsigned long)tx,
        .rx_buf        = (unsigned long)rx,
        .len           = SPI_FRAME_LEN,
        .speed_hz      = g.speed_hz,
        .bits_per_word = 8,
        .cs_change     = 0,   /* 由 ioctl 自动管理 CS：每次 ioctl 一次 CS 边沿 */
    };

    int n = ioctl(g.spi_fd, SPI_IOC_MESSAGE(1), &tr);
    if (n < 1)
    {
        g.stats.spidev_io_err_count++;
        g.stats.consecutive_err_count++;
        LOGE("SPI_IOC_MESSAGE 失败: %s", strerror(errno));
        return -EIO;
    }
    g.stats.tx_cmd_count++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 高层：发 1 个命令 + 1 个 NOP，取回本帧响应（CMD + NOP_READ 模型）   */
/*                                                                    */
/* 输出：rsp[64] 为校验通过的 RSP 帧；若校验失败，返回负值并已写统计。 */
/* ------------------------------------------------------------------ */

static int spi_send_cmd(uint8_t dev, uint8_t cmd,
                        const uint8_t *payload, uint8_t len, uint8_t flags,
                        uint8_t *rsp_frame)
{
    uint8_t tx[SPI_FRAME_LEN], rx[SPI_FRAME_LEN];
    uint8_t seq = g.next_seq++;

    /* 第 1 次事务：发 CMD，丢弃 MISO（是上一帧的滞后响应） */
    if (spi_pack_cmd(tx, seq, dev, cmd, payload, len, flags | SPI_CMD_FLAG_NEED_RSP) != 0)
        return -EINVAL;
    int rc = spi_xfer_one(tx, rx);
    if (rc < 0) return rc;

    /* 帧间隔：给 RA6E2 在 ISR 中装好 RSP */
    if (g.frame_gap_us)
    {
        struct timespec ts = {
            .tv_sec  = g.frame_gap_us / 1000000,
            .tv_nsec = (g.frame_gap_us % 1000000) * 1000,
        };
        nanosleep(&ts, NULL);
    }

    /* 第 2 次事务：发 NOP，MISO 即本次命令的响应 */
    if (spi_pack_cmd(tx, g.next_seq++, SPI_DEV_SYSTEM, SPI_CMD_SYS_NOP, NULL, 0, 0) != 0)
        return -EINVAL;
    rc = spi_xfer_one(tx, rx);
    if (rc < 0) return rc;

    /* 校验 RSP */
    int v = spi_validate_frame(rx, /*is_cmd*/0);
    if (v != 0)
    {
        if (v == -((int)SPI_STATUS_BAD_SOF))
            g.stats.rx_sof_err_count++;
        else if (v == -((int)SPI_STATUS_CRC_ERR))
            g.stats.rx_crc_err_count++;
        g.stats.consecutive_err_count++;
        LOGD("RSP 校验失败 v=%d (seq=%u dev=0x%02X cmd=0x%02X)", v, seq, dev, cmd);
        if (g.stats.consecutive_err_count >= MAX_CONSEC_ERRORS)
            spi_reset();
        return v; /* 负值，调用方据此判断 */
    }

    g.stats.rx_rsp_ok_count++;
    g.stats.consecutive_err_count = 0;
    g.stats.last_uptime_ms = spi_u32_get(&rx[SPI_RSP_OFF_UPTIME]);

    if (rsp_frame) memcpy(rsp_frame, rx, SPI_FRAME_LEN);
    return 0;
}

/* ------------------------------------------------------------------ */
/* 周期任务：自动 POLL_ALL 并缓存                                     */
/* ------------------------------------------------------------------ */

static void periodic_poll_all(void)
{
    uint8_t rsp[SPI_FRAME_LEN];
    int rc = spi_send_cmd(SPI_DEV_SENSOR, SPI_CMD_SENSOR_POLL_ALL, NULL, 0, 0, rsp);
    if (rc != 0)
    {
        LOGD("周期 POLL_ALL 失败 rc=%d", rc);
        return;
    }
    if (rsp[SPI_RSP_OFF_TYPE]   != SPI_RSP_TYPE_SENSOR_DATA ||
        rsp[SPI_RSP_OFF_STATUS] != SPI_STATUS_OK            ||
        rsp[SPI_RSP_OFF_LEN]    != SPI_SENSOR_PAYLOAD_LEN)
    {
        LOGD("周期 POLL_ALL 响应异常 type=%u status=%u len=%u",
             rsp[SPI_RSP_OFF_TYPE], rsp[SPI_RSP_OFF_STATUS], rsp[SPI_RSP_OFF_LEN]);
        return;
    }
    memcpy(g.snapshot, &rsp[SPI_RSP_OFF_PAYLOAD], SPI_SENSOR_PAYLOAD_LEN);
    g.snapshot_valid = true;
    clock_gettime(CLOCK_MONOTONIC, &g.snapshot_ts);
}

/* ------------------------------------------------------------------ */
/* IPC：处理一个客户端连接（同步 read/write 一次后关闭）              */
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
        if (!g.snapshot_valid)
        {
            rsp.rc = -ENODATA;
            break;
        }
        rsp.rc = 0;
        rsp.rsp_type = SPI_RSP_TYPE_SENSOR_DATA;
        rsp.rsp_len  = SPI_SENSOR_PAYLOAD_LEN;
        memcpy(rsp.payload, g.snapshot, SPI_SENSOR_PAYLOAD_LEN);
        rsp.uptime_ms = g.stats.last_uptime_ms;
        break;

    case AQUA_OP_GET_STATS:
        rsp.rc = 0;
        rsp.stats = g.stats;
        break;

    case AQUA_OP_SEND_CMD:
    {
        if (req.len > SPI_CMD_PAYLOAD_MAX)
        {
            rsp.rc = -EINVAL;
            break;
        }
        uint8_t frame[SPI_FRAME_LEN];
        int rc = spi_send_cmd(req.dev, req.cmd, req.payload, req.len, req.flags, frame);
        if (rc < 0)
        {
            rsp.rc = rc;        /* 负值 = -spi_status_t 或 -EIO 等 */
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
/* 信号处理 → self-pipe                                               */
/* ------------------------------------------------------------------ */

static void on_signal(int sig)
{
    g.quit = 1;
    uint8_t b = (uint8_t)sig;
    if (g.signal_fd[1] >= 0)
    {
        ssize_t r = write(g.signal_fd[1], &b, 1);
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
/* 上电握手：连续 PING 直到成功 / 超时                                 */
/* ------------------------------------------------------------------ */

static int hello_handshake(void)
{
    /* 先发一次 SYS_HELLO 通知从机我们刚启动 */
    uint8_t frame[SPI_FRAME_LEN];
    g.next_seq = 0;
    (void)spi_send_cmd(SPI_DEV_SYSTEM, SPI_CMD_SYS_HELLO, NULL, 0, 0, frame);

    /* 然后做 PING 探测，最多 5 秒 */
    for (int i = 0; i < 50; i++)
    {
        int rc = spi_send_cmd(SPI_DEV_SYSTEM, SPI_CMD_SYS_PING, NULL, 0, 0, frame);
        if (rc == 0 && frame[SPI_RSP_OFF_STATUS] == SPI_STATUS_OK)
        {
            LOGI("RA6E2 上线，uptime=%u ms",
                 spi_u32_get(&frame[SPI_RSP_OFF_UPTIME]));
            return 0;
        }
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    LOGE("RA6E2 在 5 秒内未上线，daemon 仍继续运行（依赖周期重试）");
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

    while (!g.quit)
    {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long wait_ms = ts_diff_ms(&next_poll, &now);
        if (wait_ms < 0) wait_ms = 0;

        struct pollfd pfds[2] = {
            { .fd = g.sock_fd,      .events = POLLIN },
            { .fd = g.signal_fd[0], .events = POLLIN },
        };
        int n = poll(pfds, 2, (int)wait_ms);
        if (n < 0 && errno != EINTR)
        {
            LOGE("poll: %s", strerror(errno));
            break;
        }

        /* 有客户端连接：处理一个 */
        if (n > 0 && (pfds[0].revents & POLLIN))
        {
            int cfd = accept4(g.sock_fd, NULL, NULL, SOCK_CLOEXEC);
            if (cfd >= 0)
            {
                ipc_handle(cfd);
                close(cfd);
            }
        }

        /* 信号唤醒 */
        if (n > 0 && (pfds[1].revents & POLLIN))
        {
            uint8_t b;
            ssize_t r = read(g.signal_fd[0], &b, 1);
            (void)r;
            continue;
        }

        /* 周期到 → 跑一次 POLL_ALL */
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (ts_diff_ms(&now, &next_poll) >= 0)
        {
            periodic_poll_all();
            /* 单调累加，避免漂移 */
            next_poll.tv_nsec += (long)g.period_ms * 1000000L;
            while (next_poll.tv_nsec >= 1000000000L)
            {
                next_poll.tv_sec++;
                next_poll.tv_nsec -= 1000000000L;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* daemonize（detach 终端）                                           */
/* ------------------------------------------------------------------ */

static void daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid > 0) exit(0);

    setsid();
    pid = fork();
    if (pid < 0) { perror("fork2"); exit(1); }
    if (pid > 0) exit(0);

    if (chdir("/") != 0) { /* 极少失败，忽略 */ }
    umask(0);
    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, 0);
    dup2(devnull, 1);
    dup2(devnull, 2);
    if (devnull > 2) close(devnull);

    openlog("aqua_spid", LOG_PID, LOG_DAEMON);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

static void usage(const char *argv0)
{
    fprintf(stderr,
        "用法: %s [选项]\n"
        "  -d <设备>   spidev 路径 (默认 %s)\n"
        "  -s <Hz>     SCK 速率   (默认 %u)\n"
        "  -p <ms>     轮询周期   (默认 %u)\n"
        "  -g <us>     CMD/NOP_READ 间隔 (默认 %u)\n"
        "  -f          前台运行，日志走 stderr\n"
        "  -v          详细日志\n"
        "  -h          帮助\n",
        argv0, DEFAULT_SPIDEV, DEFAULT_SPEED_HZ, DEFAULT_PERIOD_MS, DEFAULT_FRAME_GAP_US);
}

int main(int argc, char **argv)
{
    g.spidev_path  = DEFAULT_SPIDEV;
    g.speed_hz     = DEFAULT_SPEED_HZ;
    g.period_ms    = DEFAULT_PERIOD_MS;
    g.frame_gap_us = DEFAULT_FRAME_GAP_US;
    g.foreground   = false;
    g.verbose      = false;
    g.spi_fd       = -1;
    g.sock_fd      = -1;
    g.signal_fd[0] = g.signal_fd[1] = -1;
    g.next_seq     = 0;

    int opt;
    while ((opt = getopt(argc, argv, "d:s:p:g:fvh")) != -1)
    {
        switch (opt)
        {
        case 'd': g.spidev_path = optarg; break;
        case 's': g.speed_hz    = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'p': g.period_ms   = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'g': g.frame_gap_us= (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'f': g.foreground  = true; break;
        case 'v': g.verbose     = true; break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    if (!g.foreground)
    {
        daemonize();
    }
    else
    {
        openlog("aqua_spid", LOG_PID | LOG_PERROR, LOG_DAEMON);
    }

    /* self-pipe 用于把信号变成 poll() 事件 */
    if (pipe2(g.signal_fd, O_CLOEXEC | O_NONBLOCK) < 0)
    {
        LOGE("pipe2: %s", strerror(errno));
        return 1;
    }
    struct sigaction sa = { .sa_handler = on_signal, .sa_flags = SA_RESTART };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    g.spi_fd = spi_open();
    if (g.spi_fd < 0) return 1;

    g.sock_fd = listen_socket();
    if (g.sock_fd < 0) return 1;

    LOGI("aqua_spid 启动：周期=%u ms，间隔=%u µs", g.period_ms, g.frame_gap_us);

    (void)hello_handshake();

    main_loop();

    LOGI("退出中...");
    if (g.spi_fd >= 0)  close(g.spi_fd);
    if (g.sock_fd >= 0) close(g.sock_fd);
    unlink(AQUA_IPC_SOCK_PATH);
    closelog();
    return 0;
}
