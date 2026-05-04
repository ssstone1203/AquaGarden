/*
 * aqua_spi_cli.c — AquaGarden SPI 命令行工具
 *
 * 通过 Unix Domain Socket 连接 aqua_spid，执行单条命令并打印结果。
 *
 * 用法示例：
 *   aqua_spi_cli ping                           # 检查 daemon
 *   aqua_spi_cli sys ping                       # 心跳 RA6E2
 *   aqua_spi_cli snapshot                       # 取缓存的传感器快照
 *   aqua_spi_cli stats                          # 看通信统计
 *   aqua_spi_cli pump start 80                  # 启泵 80%
 *   aqua_spi_cli pump stop
 *   aqua_spi_cli pump pwm 60
 *   aqua_spi_cli sensor poll                    # 走 SPI 实时拉一次
 *   aqua_spi_cli link soil 35 5                 # 阈值 35%, 滞回 5%
 *   aqua_spi_cli raw <dev_hex> <cmd_hex> [B0 B1 ...]   # 任意原始命令
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include "spi_protocol.h"
#include "spi_codec.h"
#include "aqua_ipc.h"

/* ------------------------------------------------------------------ */
/* IPC 调用                                                           */
/* ------------------------------------------------------------------ */

static int ipc_call(const aqua_ipc_req_t *req, aqua_ipc_rsp_t *rsp)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_un sa = { .sun_family = AF_UNIX };
    strncpy(sa.sun_path, AQUA_IPC_SOCK_PATH, sizeof sa.sun_path - 1);
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0)
    {
        fprintf(stderr, "无法连接 %s: %s（daemon 是否在跑？）\n",
                AQUA_IPC_SOCK_PATH, strerror(errno));
        close(fd);
        return -1;
    }

    if (write(fd, req, sizeof *req) != (ssize_t)sizeof *req)
    {
        perror("write");
        close(fd);
        return -1;
    }

    /* daemon 在 RPMsg write 阻塞时曾导致本 read 永久挂起；与 SPI 超时解耦的保护 */
    {
        unsigned tmo_ms = 8000;
        if (req->op == AQUA_OP_SEND_CMD)
        {
            tmo_ms = (unsigned)req->timeout_ms;
            if (tmo_ms < 100u) tmo_ms = 100u;
            tmo_ms = tmo_ms * 80u + 4000u;
            if (tmo_ms > 60000u) tmo_ms = 60000u;
        }
        struct timeval tv = {
            .tv_sec  = (long)(tmo_ms / 1000u),
            .tv_usec = (long)((tmo_ms % 1000u) * 1000u),
        };
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    }

    ssize_t n = read(fd, rsp, sizeof *rsp);
    close(fd);
    if (n != (ssize_t)sizeof *rsp)
    {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            fprintf(stderr, "IPC 接收超时（daemon 未在时限内应答，可能卡在 SPI/RPMsg）\n");
        else
            fprintf(stderr, "短回包 %zd / %zu\n", n, sizeof *rsp);
        return -1;
    }
    if (rsp->magic != AQUA_IPC_MAGIC)
    {
        fprintf(stderr, "回包魔数错 0x%08X\n", rsp->magic);
        return -1;
    }
    return 0;
}

static void make_req(aqua_ipc_req_t *r, uint32_t op)
{
    memset(r, 0, sizeof *r);
    r->magic = AQUA_IPC_MAGIC;
    r->op    = op;
    r->timeout_ms = 100;
}

/* ------------------------------------------------------------------ */
/* 打印 helper                                                        */
/* ------------------------------------------------------------------ */

static const char *status_name(uint8_t s)
{
    switch (s)
    {
    case SPI_STATUS_OK:              return "OK";
    case SPI_STATUS_CRC_ERR:         return "CRC_ERR";
    case SPI_STATUS_UNKNOWN_DEV:     return "UNKNOWN_DEV";
    case SPI_STATUS_UNKNOWN_CMD:     return "UNKNOWN_CMD";
    case SPI_STATUS_BAD_PAYLOAD_LEN: return "BAD_PAYLOAD_LEN";
    case SPI_STATUS_BUSY:            return "BUSY";
    case SPI_STATUS_VER_MISMATCH:    return "VER_MISMATCH";
    case SPI_STATUS_BAD_SOF:         return "BAD_SOF";
    default:                         return "UNKNOWN";
    }
}

static const char *rsp_type_name(uint8_t t)
{
    switch (t)
    {
    case SPI_RSP_TYPE_ACK:         return "ACK";
    case SPI_RSP_TYPE_SENSOR_DATA: return "SENSOR_DATA";
    case SPI_RSP_TYPE_STATUS:      return "STATUS";
    case SPI_RSP_TYPE_VERSION:     return "VERSION";
    default:                       return "?";
    }
}

static void print_sensor_payload(const uint8_t *p, uint32_t uptime_ms)
{
    uint32_t ts   = spi_u32_get(&p[SPI_SENS_OFF_TIMESTAMP_MS]);
    int16_t  airT = spi_i16_get(&p[SPI_SENS_OFF_AIR_TEMP_X10]);
    int16_t  airH = spi_i16_get(&p[SPI_SENS_OFF_AIR_HUMIDITY_X10]);
    int16_t  watT = spi_i16_get(&p[SPI_SENS_OFF_WATER_TEMP_X10]);
    uint8_t  soil = p[SPI_SENS_OFF_SOIL_MOISTURE_PCT];
    uint8_t  wqi  = p[SPI_SENS_OFF_WQI];
    uint8_t  pump = p[SPI_SENS_OFF_PUMP_POWER_PCT];
    uint8_t  need = p[SPI_SENS_OFF_NEED_WATERING];
    uint16_t pr0  = spi_u16_get(&p[SPI_SENS_OFF_PRESSURE_KG_0_X100]);
    uint16_t pr1  = spi_u16_get(&p[SPI_SENS_OFF_PRESSURE_KG_1_X100]);
    uint16_t pr2  = spi_u16_get(&p[SPI_SENS_OFF_PRESSURE_KG_2_X100]);
    uint32_t alm  = spi_u32_get(&p[SPI_SENS_OFF_ALARM_FLAGS]);

    printf("传感器快照  (RA6E2 ts=%u ms, daemon uptime=%u ms)\n", ts, uptime_ms);
    printf("  空气   : %.1f °C  %.1f %%RH\n", airT / 10.0, airH / 10.0);
    printf("  水温   : %.1f °C\n", watT / 10.0);
    printf("  土壤   : %u %% (need_watering=%u)\n", soil, need);
    printf("  WQI    : %u\n", wqi);
    printf("  水泵   : %u %%\n", pump);
    printf("  压力   : %.2f / %.2f / %.2f kg\n", pr0 / 100.0, pr1 / 100.0, pr2 / 100.0);
    printf("  告警位 : 0x%08X\n", alm);
}

/* ------------------------------------------------------------------ */
/* 子命令                                                             */
/* ------------------------------------------------------------------ */

static int cmd_ping_daemon(void)
{
    aqua_ipc_req_t req; aqua_ipc_rsp_t rsp;
    make_req(&req, AQUA_OP_PING_DAEMON);
    if (ipc_call(&req, &rsp) != 0) return 1;
    if (rsp.rc != 0) { fprintf(stderr, "daemon rc=%d\n", rsp.rc); return 1; }
    printf("daemon: alive\n");
    return 0;
}

static int cmd_stats(void)
{
    aqua_ipc_req_t req; aqua_ipc_rsp_t rsp;
    make_req(&req, AQUA_OP_GET_STATS);
    if (ipc_call(&req, &rsp) != 0) return 1;
    if (rsp.rc != 0) { fprintf(stderr, "rc=%d\n", rsp.rc); return 1; }
    const aqua_ipc_stats_t *s = &rsp.stats;
    printf("SPI 通信统计:\n");
    printf("  tx_cmd          = %llu\n", (unsigned long long)s->tx_cmd_count);
    printf("  rx_rsp_ok       = %llu\n", (unsigned long long)s->rx_rsp_ok_count);
    printf("  rx_crc_err      = %llu\n", (unsigned long long)s->rx_crc_err_count);
    printf("  rx_sof_err      = %llu\n", (unsigned long long)s->rx_sof_err_count);
    printf("  rx_timeout      = %llu\n", (unsigned long long)s->rx_timeout_count);
    printf("  spidev_io_err   = %llu\n", (unsigned long long)s->spidev_io_err_count);
    printf("  consecutive_err = %u\n",   s->consecutive_err_count);
    printf("  last_uptime_ms  = %u\n",   s->last_uptime_ms);
    return 0;
}

static int cmd_snapshot(void)
{
    aqua_ipc_req_t req; aqua_ipc_rsp_t rsp;
    make_req(&req, AQUA_OP_GET_SNAPSHOT);
    if (ipc_call(&req, &rsp) != 0) return 1;
    if (rsp.rc != 0)
    {
        fprintf(stderr, "rc=%d (尚无缓存数据？)\n", rsp.rc);
        return 1;
    }
    print_sensor_payload(rsp.payload, rsp.uptime_ms);
    return 0;
}

static int send_cmd(uint8_t dev, uint8_t cmd,
                    const uint8_t *payload, uint8_t len, uint8_t flags)
{
    aqua_ipc_req_t req; aqua_ipc_rsp_t rsp;
    make_req(&req, AQUA_OP_SEND_CMD);
    req.dev = dev; req.cmd = cmd; req.len = len; req.flags = flags;
    if (payload && len) memcpy(req.payload, payload, len);

    if (ipc_call(&req, &rsp) != 0) return 1;

    if (rsp.rc < 0)
    {
        fprintf(stderr, "SPI 失败 rc=%d\n", rsp.rc);
        return 1;
    }
    printf("RSP: status=%s type=%s ack_seq=%u flags=0x%02X len=%u uptime=%u ms\n",
           status_name(rsp.status), rsp_type_name(rsp.rsp_type),
           rsp.ack_seq, rsp.flags, rsp.rsp_len, rsp.uptime_ms);
    if (rsp.rsp_type == SPI_RSP_TYPE_SENSOR_DATA && rsp.rsp_len >= SPI_SENSOR_PAYLOAD_LEN)
    {
        printf("\n");
        print_sensor_payload(rsp.payload, rsp.uptime_ms);
    }
    return rsp.status == SPI_STATUS_OK ? 0 : 2;
}

/* ---- sys ---- */
static int cmd_sys(int argc, char **argv)
{
    if (argc < 1) { fprintf(stderr, "用法: sys <ping|version|reset_alarm|hello>\n"); return 1; }
    if      (!strcmp(argv[0], "ping"))        return send_cmd(SPI_DEV_SYSTEM, SPI_CMD_SYS_PING,        NULL, 0, 0);
    else if (!strcmp(argv[0], "version"))     return send_cmd(SPI_DEV_SYSTEM, SPI_CMD_SYS_GET_VERSION, NULL, 0, 0);
    else if (!strcmp(argv[0], "reset_alarm")) return send_cmd(SPI_DEV_SYSTEM, SPI_CMD_SYS_RESET_ALARM, NULL, 0, 0);
    else if (!strcmp(argv[0], "hello"))       return send_cmd(SPI_DEV_SYSTEM, SPI_CMD_SYS_HELLO,       NULL, 0, 0);
    fprintf(stderr, "未知 sys 子命令\n");
    return 1;
}

/* ---- pump ---- */
static int cmd_pump(int argc, char **argv)
{
    if (argc < 1)
    {
        fprintf(stderr, "用法: pump <start [%%] | stop | pwm <%%> | auto | manual <0/1> <%%>>\n");
        return 1;
    }
    if (!strcmp(argv[0], "start"))
    {
        if (argc >= 2) {
            uint8_t pwr = (uint8_t)atoi(argv[1]);
            return send_cmd(SPI_DEV_PUMP, SPI_CMD_PUMP_START, &pwr, 1, 0);
        }
        return send_cmd(SPI_DEV_PUMP, SPI_CMD_PUMP_START, NULL, 0, 0);
    }
    if (!strcmp(argv[0], "stop"))
        return send_cmd(SPI_DEV_PUMP, SPI_CMD_PUMP_STOP, NULL, 0, 0);
    if (!strcmp(argv[0], "pwm") && argc >= 2) {
        uint8_t pwr = (uint8_t)atoi(argv[1]);
        return send_cmd(SPI_DEV_PUMP, SPI_CMD_PUMP_SET_PWM, &pwr, 1, 0);
    }
    if (!strcmp(argv[0], "auto"))
        return send_cmd(SPI_DEV_PUMP, SPI_CMD_PUMP_SET_AUTO, NULL, 0, 0);
    if (!strcmp(argv[0], "manual") && argc >= 3) {
        uint8_t p[2] = { (uint8_t)atoi(argv[1]), (uint8_t)atoi(argv[2]) };
        return send_cmd(SPI_DEV_PUMP, SPI_CMD_PUMP_SET_MANUAL, p, 2, 0);
    }
    fprintf(stderr, "未知 pump 子命令\n");
    return 1;
}

/* ---- sensor ---- */
static int cmd_sensor(int argc, char **argv)
{
    uint8_t cmd = SPI_CMD_SENSOR_POLL_ALL;
    if (argc >= 1)
    {
        if      (!strcmp(argv[0], "poll") || !strcmp(argv[0], "all")) cmd = SPI_CMD_SENSOR_POLL_ALL;
        else if (!strcmp(argv[0], "air"))        cmd = SPI_CMD_SENSOR_POLL_AIR;
        else if (!strcmp(argv[0], "water"))      cmd = SPI_CMD_SENSOR_POLL_WATER;
        else if (!strcmp(argv[0], "soil"))       cmd = SPI_CMD_SENSOR_POLL_SOIL;
        else if (!strcmp(argv[0], "pressure"))   cmd = SPI_CMD_SENSOR_POLL_PRESSURE;
        else { fprintf(stderr, "未知 sensor 子命令\n"); return 1; }
    }
    return send_cmd(SPI_DEV_SENSOR, cmd, NULL, 0, 0);
}

/* ---- link ---- */
static int cmd_link(int argc, char **argv)
{
    if (argc < 1) { fprintf(stderr, "用法: link <soil <th%%> <hys%%> | rule <mask> <waterT*10> <wqi_low>>\n"); return 1; }
    if (!strcmp(argv[0], "soil") && argc >= 3) {
        uint8_t p[2] = { (uint8_t)atoi(argv[1]), (uint8_t)atoi(argv[2]) };
        return send_cmd(SPI_DEV_LINKAGE, SPI_CMD_LINK_SET_SOIL_CFG, p, 2, 0);
    }
    if (!strcmp(argv[0], "rule") && argc >= 4) {
        uint8_t p[4];
        p[0] = (uint8_t)strtoul(argv[1], NULL, 0);
        int16_t wt = (int16_t)atoi(argv[2]);
        p[1] = (uint8_t)(wt & 0xFF);
        p[2] = (uint8_t)((wt >> 8) & 0xFF);
        p[3] = (uint8_t)atoi(argv[3]);
        return send_cmd(SPI_DEV_LINKAGE, SPI_CMD_LINK_SET_RULE, p, 4, 0);
    }
    fprintf(stderr, "未知 link 子命令\n");
    return 1;
}

/* ---- raw ---- */
static int cmd_raw(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "用法: raw <dev> <cmd> [B0 B1 ...]\n"); return 1; }
    uint8_t dev = (uint8_t)strtoul(argv[0], NULL, 0);
    uint8_t cmd = (uint8_t)strtoul(argv[1], NULL, 0);
    uint8_t payload[SPI_CMD_PAYLOAD_MAX] = {0};
    int len = argc - 2;
    if (len > (int)SPI_CMD_PAYLOAD_MAX) { fprintf(stderr, "payload 太长\n"); return 1; }
    for (int i = 0; i < len; i++)
        payload[i] = (uint8_t)strtoul(argv[2 + i], NULL, 0);
    return send_cmd(dev, cmd, payload, (uint8_t)len, 0);
}

/* ------------------------------------------------------------------ */

static void usage(const char *argv0)
{
    fprintf(stderr,
        "用法: %s <子命令> [参数...]\n"
        "  ping                       检查 daemon 是否在跑\n"
        "  stats                      显示通信统计\n"
        "  snapshot                   读取缓存的 SensorData（不走 SPI）\n"
        "  sys ping|version|reset_alarm|hello\n"
        "  pump start [%%] | stop | pwm <%%> | auto | manual <0/1> <%%>\n"
        "  sensor [poll|air|water|soil|pressure]\n"
        "  link soil <th%%> <hys%%>\n"
        "  link rule <mask> <waterT*10> <wqi_low>\n"
        "  raw <dev> <cmd> [byte ...]\n",
        argv0);
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *sub = argv[1];
    int rest_argc = argc - 2;
    char **rest    = argv + 2;

    if      (!strcmp(sub, "ping"))     return cmd_ping_daemon();
    else if (!strcmp(sub, "stats"))    return cmd_stats();
    else if (!strcmp(sub, "snapshot")) return cmd_snapshot();
    else if (!strcmp(sub, "sys"))      return cmd_sys(rest_argc, rest);
    else if (!strcmp(sub, "pump"))     return cmd_pump(rest_argc, rest);
    else if (!strcmp(sub, "sensor"))   return cmd_sensor(rest_argc, rest);
    else if (!strcmp(sub, "link"))     return cmd_link(rest_argc, rest);
    else if (!strcmp(sub, "raw"))      return cmd_raw(rest_argc, rest);
    else if (!strcmp(sub, "-h") || !strcmp(sub, "--help")) { usage(argv[0]); return 0; }

    fprintf(stderr, "未知子命令: %s\n", sub);
    usage(argv[0]);
    return 1;
}
