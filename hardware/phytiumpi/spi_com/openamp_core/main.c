/*
 * AquaGarden 飞腾派裸机从核固件入口
 *
 * 按设计文档 v2 §0 / §5：
 *   - 通过 OpenAMP RPMsg 接收 Linux 业务进程发来的 64 字节 SPI CMD 帧
 *   - 用 FSPIM 驱动直接驱动 FSPI0（即 v1 spidev 用的同一个控制器）
 *   - 内部完成 "CMD + 5ms gap + NOP_READ" 两次 SPI 事务
 *   - 把 64 字节 SPI RSP 帧通过同一个 endpoint 回传给 Linux
 *
 * 远程核生命周期由 Linux remoteproc 管理，主程序只负责：
 *   1) 初始化（FSPIM、libmetal、OpenAMP）
 *   2) 进入 RPMsg 主循环
 *   3) 收到 stop 信号后清理并 PSCI CPU off
 *
 * 本文件仅做 banner 打印 + 调 aqua_spi_slave_run()，
 * 不在 main 里写任何业务逻辑（便于后续把同一个固件搬给其它项目复用）。
 */

#include <stdio.h>

#include "ftypes.h"
#include "fdebug.h"
#include "sdkconfig.h"
#include "aqua_spi_slave.h"

int main(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf(" AquaGarden OpenAMP SPI core\r\n");
    printf(" build: %s %s\r\n", __DATE__, __TIME__);
    printf(" target: %s\r\n", CONFIG_TARGET_NAME);
    printf("========================================\r\n");

    return aqua_spi_slave_run();
}
