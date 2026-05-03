/*
 * aqua_daemon_core.h — daemon 主循环 + IPC 处理（与 backend 解耦）
 *
 * v1 (aqua_spid) 与 v2 (aqua_rpmsgd) 的 main() 都做同一件事：
 *   1) 解析命令行
 *   2) 选 backend 并 open
 *   3) 调 aqua_daemon_run() 进入主循环
 *
 * 主循环本身只关心：
 *   - 周期性自动 SENSOR_POLL_ALL
 *   - 监听 /tmp/aqua_spi.sock 处理客户端 IPC
 *   - 维护协议层统计（CRC 错、SOF 错、连续错）
 * 所有 SPI 物理收发都委托给 backend->xfer()。
 */

#ifndef AQUAGARDEN_AQUA_DAEMON_CORE_H
#define AQUAGARDEN_AQUA_DAEMON_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "aqua_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* daemon 启动配置 */
typedef struct
{
    const aqua_backend_ops_t *backend;        /* 必填 */
    void                     *backend_ctx;    /* 必填 */
    const char               *progname;       /* 用于 syslog ident */
    uint32_t                  period_ms;      /* 周期 POLL_ALL，默认 250 */
    uint32_t                  hello_timeout_ms; /* 上电握手 PING 总超时，默认 5000 */
    uint32_t                  max_consec_err; /* 触发 backend reset 的连续错阈值，默认 5 */
    bool                      foreground;     /* true=前台日志走 stderr；false=daemonize+syslog */
    bool                      verbose;
} aqua_daemon_config_t;

/* 默认值填充 */
void aqua_daemon_config_init(aqua_daemon_config_t *cfg);

/* 阻塞主循环；收到 SIGINT/SIGTERM 后清理并返回 0。
 * 出错返回非 0（IPC socket 创建失败等）。
 *
 * 函数会自己装信号处理器，调用方不需要预先装。
 */
int aqua_daemon_run(const aqua_daemon_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* AQUAGARDEN_AQUA_DAEMON_CORE_H */
