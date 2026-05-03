/*
 * aqua_spi_slave.h — RPMsg endpoint + dispatch loop 公开接口
 */

#ifndef AQUAGARDEN_AQUA_SPI_SLAVE_H
#define AQUAGARDEN_AQUA_SPI_SLAVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* RPMsg name service。必须与 Linux 端 aqua_backend_rpmsg.c 的 service_name 一致。 */
#define AQUA_RPMSG_SERVICE      "aqua-spi"

/* 收到 CMD 后等多少微秒再发 NOP_READ，给 RA6E2 装好 RSP。
 * 与 v1 daemon 的 -g 参数等价（默认 5 ms）。 */
#define AQUA_RA6E2_PREP_US      5000U

/* 阻塞执行：初始化 FSPIM + OpenAMP，跑主循环直到 Linux remoteproc stop。
 * 返回 0 = 正常退出（已 PSCI CPU off 不会真正返回），负值 = 初始化失败。 */
int aqua_spi_slave_run(void);

#ifdef __cplusplus
}
#endif

#endif /* AQUAGARDEN_AQUA_SPI_SLAVE_H */
