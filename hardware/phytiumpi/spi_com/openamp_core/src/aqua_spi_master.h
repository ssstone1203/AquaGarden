/*
 * aqua_spi_master.h — 飞腾派 FSPI0 (FSPIM) 简单封装
 */

#ifndef AQUAGARDEN_AQUA_SPI_MASTER_H
#define AQUAGARDEN_AQUA_SPI_MASTER_H

#include "ftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 飞腾派排针 SPI0 = FSPI0_ID = 0，base 0x2803_A000，IRQ 191
 * （来自 phytium-standalone-sdk/soc/pe220x/fparameters_comm.h） */
#define AQUA_SPI_ID             0u

/* 1 MHz；与 Linux 端 v1 spidev 一致。
 * 上限受布线长度 / 屏蔽影响，飞腾派排针最稳 1 MHz；可调到 4 MHz 看抓包。 */
#define AQUA_SPI_SCLK_HZ        1000000u

/* 初始化 FSPIM 控制器 + IOPad mux。返回 0 = OK，负值 = 失败。 */
int aqua_spi_master_init(void);

/* 64 字节全双工事务。CS 由本函数显式拉低 → 拉高，对应一次帧边沿。
 * 返回 0 = OK，负值 = FSPIM API 错（已写日志）。 */
int aqua_spi_master_xfer_64(const u8 *tx, u8 *rx);

/* 释放 FSPIM 资源（cleanup 路径用）。 */
void aqua_spi_master_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* AQUAGARDEN_AQUA_SPI_MASTER_H */
