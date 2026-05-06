/*
 * aqua_spi_master.c — 飞腾派 FSPI0 (FSPIM) Master 简单封装
 *
 * 设计要点：
 *   - Polling 模式（FSPIM_TRANS_POLL）：64 B / 1 MHz = 0.5 ms，阻塞代价可忽略
 *   - 显式 FSpimSetChipSelection() 控 CS，每次 xfer 一次 CS 边沿，
 *     对应 RA6E2 端 DMAC 的 "帧结束 + 复位" 信号（抗错位）
 *   - SPI Mode 1：CPOL=0/CPHA=1（RA6E2 FSP Slave 不支持 CPHA=0）
 *   - 8-bit 字宽（FSPIM_1_BYTE）
 *
 * 引脚：飞腾派 board/phytiumpi_firefly/fio_mux.c::FIOPadSetSpimMux(0)
 *      已配置好 SPI0_SCK/MOSI/MISO/CSN0 复用为 SPI 功能（FUNC2）
 */

#include "aqua_spi_master.h"

#include <string.h>

#include "fparameters.h"
#include "fspim.h"
#include "fio_mux.h"
#include "fdebug.h"
#include "spi_protocol.h"           /* SPI_FRAME_LEN */

/* 从机无响应时 FSpimTransferPollFifo 会死等；用带停滞上界的版本以便 RPMsg 仍能回 BUSY 帧 */
#define AQUA_SPI_POLL_STALL_MAX  900000u

#define AQUA_TAG "AQUA_SPIM"
#define AQUA_E(fmt, ...)  FT_DEBUG_PRINT_E(AQUA_TAG, fmt, ##__VA_ARGS__)
#define AQUA_W(fmt, ...)  FT_DEBUG_PRINT_W(AQUA_TAG, fmt, ##__VA_ARGS__)
#define AQUA_I(fmt, ...)  FT_DEBUG_PRINT_I(AQUA_TAG, fmt, ##__VA_ARGS__)

static FSpim s_spim;
static int   s_ready = 0;

int aqua_spi_master_init(void)
{
    if (s_ready)
        return 0;

    FIOMuxInit();
    FIOPadSetSpimMux(AQUA_SPI_ID);  /* SCK/MOSI/MISO/CSN0 全 IOPad mux */

    FSpimConfig cfg = *FSpimLookupConfig(AQUA_SPI_ID);
    cfg.work_mode    = FSPIM_DEV_MASTER_MODE;
    cfg.slave_dev_id = FSPIM_SLAVE_DEV_0;
    cfg.cpol         = FSPIM_CPOL_LOW;       /* CPOL=0 */
    cfg.cpha         = FSPIM_CPHA_2_EDGE;    /* CPHA=1 → Mode 1 */
    cfg.n_bytes      = FSPIM_1_BYTE;         /* 8 bit/字 */
    cfg.sclk_hz      = AQUA_SPI_SCLK_HZ;     /* 1 MHz 默认 */
    cfg.trans_way    = TRANS_WAY_POLL;
    cfg.en_test      = FALSE;                /* 真发线，不走内部 loopback */
    cfg.en_dma       = FALSE;

    FError err = FSpimCfgInitialize(&s_spim, &cfg);
    if (err != FSPIM_SUCCESS)
    {
        AQUA_E("FSpimCfgInitialize failed: 0x%x", (unsigned)err);
        return -1;
    }

    s_ready = 1;
    AQUA_I("FSPI%u inited @ %u Hz, Mode 1, 8-bit, POLL", AQUA_SPI_ID, AQUA_SPI_SCLK_HZ);
    return 0;
}

int aqua_spi_master_xfer_64(const u8 *tx, u8 *rx)
{
    if (!s_ready)
        return -1;

    FSpimSetChipSelection(&s_spim, TRUE);    /* CS↓ */
    FError err = FSpimTransferPollFifoStallBound(&s_spim, tx, rx, SPI_FRAME_LEN,
                                                 AQUA_SPI_POLL_STALL_MAX);
    FSpimSetChipSelection(&s_spim, FALSE);   /* CS↑ */

    if (err != FSPIM_SUCCESS)
    {
        AQUA_E("FSpim SPI xfer failed: 0x%x", (unsigned)err);
        return -1;
    }
    return 0;
}

void aqua_spi_master_deinit(void)
{
    if (!s_ready) return;
    FSpimDeInitialize(&s_spim);
    /* 不调 FIOMuxDeInit() —— 主核 Linux 后续可能要走 fallback v1 spidev，
     * 让 IOPad 复用保持在 SPI 状态下，避免短暂错配。 */
    s_ready = 0;
}
