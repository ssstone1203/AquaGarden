/*
 * aqua_spid.c — AquaGarden Linux 端 SPI 守护进程（v1 spidev 直驱版）
 *
 * v2 OpenAMP 化改造后，本文件被压缩成"strong-typed entry"：
 *   - 永远使用 spidev 后端，不会自动 fallback 到 rpmsg
 *   - 所有 SPI 收发逻辑、IPC、主循环都在 aqua_daemon_core 与
 *     aqua_backend_spidev 中共享
 *
 * 与 v2 路径并存关系：
 *   - aqua_spid.service     ↔ 本二进制（强制 spidev）
 *   - aqua_rpmsgd.service   ↔ aqua_rpmsgd.c（autoselect，优先 rpmsg）
 *   - 两个 service 互斥（systemd Conflicts），同一时刻只有一个跑
 *
 * 命令行参数与历史版本完全保持兼容。
 */

#define _GNU_SOURCE
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "aqua_backend.h"
#include "aqua_daemon_core.h"

#define DEFAULT_SPIDEV          "/dev/spidev0.0"
#define DEFAULT_SPEED_HZ        1000000u
#define DEFAULT_PERIOD_MS       250u
#define DEFAULT_FRAME_GAP_US    5000u

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
        "  -h          帮助\n"
        "\n"
        "本程序强制使用 spidev 后端（v1 路径）。要走 OpenAMP/RPMsg，请用 aqua_rpmsgd。\n",
        argv0, DEFAULT_SPIDEV, DEFAULT_SPEED_HZ,
        DEFAULT_PERIOD_MS, DEFAULT_FRAME_GAP_US);
}

/* 把 syslog 风格调用桥接到 daemon_core 的 log 接口
 * （daemon_core 自己也有日志，这里只给 backend 内部用） */
static void backend_log(int prio, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsyslog(prio, fmt, ap);
    va_end(ap);
}

int main(int argc, char **argv)
{
    aqua_backend_spidev_ctx_t spidev_ctx;
    aqua_daemon_config_t cfg;

    memset(&spidev_ctx, 0, sizeof spidev_ctx);
    spidev_ctx.device_path  = DEFAULT_SPIDEV;
    spidev_ctx.speed_hz     = DEFAULT_SPEED_HZ;
    spidev_ctx.frame_gap_us = DEFAULT_FRAME_GAP_US;

    aqua_daemon_config_init(&cfg);
    cfg.progname  = "aqua_spid";

    int opt;
    while ((opt = getopt(argc, argv, "d:s:p:g:fvh")) != -1)
    {
        switch (opt)
        {
        case 'd': spidev_ctx.device_path  = optarg; break;
        case 's': spidev_ctx.speed_hz     = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'p': cfg.period_ms           = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'g': spidev_ctx.frame_gap_us = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'f': cfg.foreground          = true; break;
        case 'v': cfg.verbose             = true; break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    /* backend 自身的日志通道：syslog（前台模式 daemon_core 会另开 LOG_PERROR） */
    if (!cfg.foreground)
        openlog(cfg.progname, LOG_PID, LOG_DAEMON);
    spidev_ctx.log = backend_log;

    if (aqua_backend_spidev_open(&spidev_ctx) != 0)
    {
        fprintf(stderr, "[ERR] 打开 spidev 后端失败\n");
        return 1;
    }

    cfg.backend     = &aqua_backend_spidev_ops;
    cfg.backend_ctx = &spidev_ctx;

    return aqua_daemon_run(&cfg);
}
