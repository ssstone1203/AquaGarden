/*
 * aqua_rpmsgd.c — AquaGarden Linux 端 SPI 守护进程（v2 OpenAMP/RPMsg 版）
 *
 * 与 aqua_spid 共享同一份 daemon 主循环（aqua_daemon_core），
 * 区别仅在于：
 *   - 默认走 RPMsg 后端（飞腾派裸机核固件 openamp_spi_core0.elf）
 *   - 若 /dev/rpmsg_ctrl0 不可用，自动 fallback 到 spidev（v1 路径）
 *   - 可用 -B {auto|rpmsg|spidev} 显式强制
 *
 * 客户端协议（/tmp/aqua_spi.sock + aqua_ipc.h）与 v1 完全兼容，
 * 现有 aqua_spi_cli 不需要任何改动。
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

#define DEFAULT_RPMSG_CTRL      "/dev/rpmsg_ctrl0"
#define DEFAULT_RPMSG_DEV       "/dev/rpmsg0"
#define DEFAULT_RPMSG_SERVICE   "aqua-spi"
#define DEFAULT_RSP_TIMEOUT_MS  100u

#define DEFAULT_SPIDEV          "/dev/spidev0.0"
#define DEFAULT_SPEED_HZ        1000000u
#define DEFAULT_FRAME_GAP_US    5000u
#define DEFAULT_PERIOD_MS       250u

typedef enum { BE_AUTO = 0, BE_RPMSG, BE_SPIDEV } backend_choice_t;

static void usage(const char *argv0)
{
    fprintf(stderr,
        "用法: %s [选项]\n"
        "  -B <auto|rpmsg|spidev>   选择后端 (默认 auto：优先 rpmsg，回退 spidev)\n"
        "\n"
        "RPMsg (v2) 后端选项：\n"
        "  --rpmsg-ctrl <路径>      默认 %s\n"
        "  --rpmsg-dev  <路径>      默认 %s\n"
        "  --service    <名称>      默认 \"%s\"，必须与裸机核 endpoint 同名\n"
        "  -t <ms>                  RSP 超时 (默认 %u)\n"
        "\n"
        "spidev (v1) 后端选项（仅当后端选成 spidev 时生效）：\n"
        "  -d <设备>                spidev 路径 (默认 %s)\n"
        "  -s <Hz>                  SCK 速率 (默认 %u)\n"
        "  -g <us>                  CMD/NOP_READ 间隔 (默认 %u)\n"
        "\n"
        "通用：\n"
        "  -p <ms>                  POLL_ALL 周期 (默认 %u)\n"
        "  -f                       前台运行，日志走 stderr\n"
        "  -v                       详细日志\n"
        "  -h                       帮助\n",
        argv0, DEFAULT_RPMSG_CTRL, DEFAULT_RPMSG_DEV, DEFAULT_RPMSG_SERVICE,
        DEFAULT_RSP_TIMEOUT_MS, DEFAULT_SPIDEV, DEFAULT_SPEED_HZ,
        DEFAULT_FRAME_GAP_US, DEFAULT_PERIOD_MS);
}

static void backend_log(int prio, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsyslog(prio, fmt, ap);
    va_end(ap);
}

enum
{
    OPT_RPMSG_CTRL = 256,
    OPT_RPMSG_DEV,
    OPT_SERVICE,
};

int main(int argc, char **argv)
{
    aqua_backend_rpmsg_ctx_t  rpmsg_ctx;
    aqua_backend_spidev_ctx_t spidev_ctx;
    aqua_daemon_config_t      cfg;
    backend_choice_t          choice = BE_AUTO;

    memset(&rpmsg_ctx, 0, sizeof rpmsg_ctx);
    rpmsg_ctx.ctrl_path    = DEFAULT_RPMSG_CTRL;
    rpmsg_ctx.device_path  = DEFAULT_RPMSG_DEV;
    rpmsg_ctx.service_name = DEFAULT_RPMSG_SERVICE;
    rpmsg_ctx.timeout_ms   = DEFAULT_RSP_TIMEOUT_MS;

    memset(&spidev_ctx, 0, sizeof spidev_ctx);
    spidev_ctx.device_path  = DEFAULT_SPIDEV;
    spidev_ctx.speed_hz     = DEFAULT_SPEED_HZ;
    spidev_ctx.frame_gap_us = DEFAULT_FRAME_GAP_US;

    aqua_daemon_config_init(&cfg);
    cfg.progname  = "aqua_rpmsgd";

    static const struct option long_opts[] = {
        { "rpmsg-ctrl", required_argument, 0, OPT_RPMSG_CTRL },
        { "rpmsg-dev",  required_argument, 0, OPT_RPMSG_DEV  },
        { "service",    required_argument, 0, OPT_SERVICE    },
        { "help",       no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv,
                              "B:t:d:s:g:p:fvh", long_opts, NULL)) != -1)
    {
        switch (opt)
        {
        case 'B':
            if      (!strcmp(optarg, "auto"))   choice = BE_AUTO;
            else if (!strcmp(optarg, "rpmsg"))  choice = BE_RPMSG;
            else if (!strcmp(optarg, "spidev")) choice = BE_SPIDEV;
            else { fprintf(stderr, "未知后端: %s\n", optarg); return 1; }
            break;
        case OPT_RPMSG_CTRL: rpmsg_ctx.ctrl_path    = optarg; break;
        case OPT_RPMSG_DEV:  rpmsg_ctx.device_path  = optarg; break;
        case OPT_SERVICE:    rpmsg_ctx.service_name = optarg; break;
        case 't': rpmsg_ctx.timeout_ms   = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'd': spidev_ctx.device_path = optarg; break;
        case 's': spidev_ctx.speed_hz    = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'g': spidev_ctx.frame_gap_us= (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'p': cfg.period_ms          = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'f': cfg.foreground         = true; break;
        case 'v': cfg.verbose            = true; break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    if (!cfg.foreground)
        openlog(cfg.progname, LOG_PID, LOG_DAEMON);

    rpmsg_ctx.log  = backend_log;
    spidev_ctx.log = backend_log;

    const aqua_backend_ops_t *ops = NULL;
    void                     *ctx = NULL;

    switch (choice)
    {
    case BE_RPMSG:
        if (aqua_backend_rpmsg_open(&rpmsg_ctx) != 0)
        {
            fprintf(stderr, "[ERR] -B rpmsg 强制失败：rpmsg 不可用\n");
            return 1;
        }
        ops = &aqua_backend_rpmsg_ops; ctx = &rpmsg_ctx;
        break;
    case BE_SPIDEV:
        if (aqua_backend_spidev_open(&spidev_ctx) != 0)
        {
            fprintf(stderr, "[ERR] -B spidev 强制失败：spidev 不可用\n");
            return 1;
        }
        ops = &aqua_backend_spidev_ops; ctx = &spidev_ctx;
        break;
    case BE_AUTO:
        if (aqua_backend_autoselect(&spidev_ctx, &rpmsg_ctx, &ops, &ctx) != 0)
        {
            fprintf(stderr, "[ERR] 两个后端都不可用：\n"
                            "      - %s/%s 不可访问 (rpmsg) \n"
                            "      - %s 不可访问 (spidev)\n"
                            "请确认 OpenAMP 远程核已启动 或 spidev overlay 已加载\n",
                            rpmsg_ctx.ctrl_path, rpmsg_ctx.device_path,
                            spidev_ctx.device_path);
            return 1;
        }
        break;
    }

    cfg.backend     = ops;
    cfg.backend_ctx = ctx;
    return aqua_daemon_run(&cfg);
}
