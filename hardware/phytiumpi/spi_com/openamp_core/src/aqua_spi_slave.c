/*
 * aqua_spi_slave.c — RPMsg endpoint + dispatch loop
 *
 * 骨架来自 phytium-standalone-sdk/example/system/amp/openamp_for_linux/
 *   src/slaver_00_example.c —— 资源表、kick driver、共享内存声明、
 *   platform_create_proc / platform_setup_share_mems / platform_create_rpmsg_vdev
 *   全部不动，**只替换 endpoint 回调**：
 *     原 echo: 收到啥 → memcpy 后用 rpmsg_send 原样发回
 *     本工程: 收到 64B CMD → 跑 SPI 双事务 → 把 64B RSP 回去
 *
 * 关键不变量：本文件不解析 CMD/RSP 的任何字段（DEV/CMD/STATUS 等）。
 * 业务命令的语义全在 RA6E2 那边，本固件只搬字节。
 * 所以：业务命令新增/修改 不需要重新烧本固件。
 */

#include <stdio.h>
#include <string.h>

#include <openamp/open_amp.h>
#include <metal/alloc.h>

#include "ftypes.h"
#include "fdebug.h"
#include "fcache.h"
#include "fpsci.h"
#include "fsleep.h"

#include "platform_info.h"
#include "rpmsg_service.h"
#include "rsc_table.h"
#include "helper.h"

#include "openamp_configs.h"
#include "libmetal_configs.h"
#include "memory_layout.h"

#include "spi_protocol.h"
#include "spi_codec.h"

#include "aqua_spi_slave.h"
#include "aqua_spi_master.h"

#define AQUA_TAG "AQUA_SPI"
#define AQUA_E(fmt, ...) FT_DEBUG_PRINT_E(AQUA_TAG, fmt, ##__VA_ARGS__)
#define AQUA_W(fmt, ...) FT_DEBUG_PRINT_W(AQUA_TAG, fmt, ##__VA_ARGS__)
#define AQUA_I(fmt, ...) FT_DEBUG_PRINT_I(AQUA_TAG, fmt, ##__VA_ARGS__)

/* ------------------------------------------------------------------ */
/* 资源表 / 共享内存 / kick driver / remoteproc_priv                   */
/* （与 SDK echo 例程保持同布局，地址来自 common/memory_layout.h        */
/*  与 common/libmetal_configs.h，与 Linux 主核约定一致。）             */
/* ------------------------------------------------------------------ */

struct remoteproc remoteproc_aqua;
static struct rpmsg_device *rpdev_aqua = NULL;

static struct remote_resource_table __resource resources __attribute__((used)) = {
    /* Version */
    1,
    /* Number of table entries */
    NUM_TABLE_ENTRIES,
    /* reserved */
    {0, 0,},
    /* Offsets of rsc entries */
    { offsetof(struct remote_resource_table, rpmsg_vdev), },
    /* Virtio device entry */
    {
        RSC_VDEV,
        VIRTIO_ID_RPMSG_,
        VDEV_NOTIFYID,
        RPMSG_IPU_C0_FEATURES,
        0, 0, 0, NUM_VRINGS,
        {0, 0},
    },
    /* Vring rsc entry - part of vdev rsc entry */
    {SLAVE00_TX_VRING_ADDR, VRING_ALIGN, SLAVE00_VRING_NUM, 1, 0},
    {SLAVE00_RX_VRING_ADDR, VRING_ALIGN, SLAVE00_VRING_NUM, 2, 0},
};

static metal_phys_addr_t poll_phys_addr = SLAVE00_KICK_IO_ADDR;

struct metal_device kick_driver_aqua = {
    .name = SLAVE_00_KICK_DEV_NAME,
    .bus = NULL,
    .num_regions = 1,
    .regions = {{
        .virt = (void *)SLAVE00_KICK_IO_ADDR,
        .physmap = &poll_phys_addr,
        .size = 0x1000,
        .page_shift = -1UL,
        .page_mask = -1UL,
        .mem_flags = SLAVE00_SOURCE_TABLE_ATTRIBUTE,
        .ops = {NULL},
    }},
    .irq_num = 1,
    .irq_info = (void *)SLAVE_00_SGI,
};

struct remoteproc_priv slave_aqua_priv = {
    .kick_dev_name      = SLAVE_00_KICK_DEV_NAME,
    .kick_dev_bus_name  = KICK_BUS_NAME,
    .cpu_id             = MASTER_CORE_MASK,    /* 给所有 core 发 SGI */
    .src_table_attribute= SLAVE00_SOURCE_TABLE_ATTRIBUTE,
    .share_mem_va       = SLAVE00_SHARE_MEM_ADDR,
    .share_mem_pa       = SLAVE00_SHARE_MEM_ADDR,
    .share_buffer_offset= SLAVE00_VRING_SIZE,
    .share_mem_size     = SLAVE00_SHARE_MEM_SIZE,
    .share_mem_attribute= SLAVE00_SHARE_BUFFER_ATTRIBUTE,
};

/* ------------------------------------------------------------------ */
/* SPI 收发缓冲                                                        */
/* （静态分配，避免每次 ISR 都 alloc）                                  */
/* ------------------------------------------------------------------ */

static u8 s_nop_frame [SPI_FRAME_LEN];   /* 预先打包好的 NOP CMD 帧 */
static u8 s_dummy_rx  [SPI_FRAME_LEN];   /* 第 1 次事务 MISO 的丢弃缓冲 */
static u8 s_rsp_frame [SPI_FRAME_LEN];   /* 第 2 次事务收回的 RSP */
static u8 s_busy_frame[SPI_FRAME_LEN];   /* SPI 错时塞给 Linux 的伪 RSP */

/* 上电时调一次：把 NOP 帧 / BUSY 伪响应预生成 */
static void aqua_pack_static_frames(void)
{
    spi_pack_cmd(s_nop_frame,
                 /*seq*/SPI_SEQ_IDLE,
                 SPI_DEV_SYSTEM,
                 SPI_CMD_SYS_NOP,
                 NULL, 0, 0);

    spi_pack_rsp(s_busy_frame,
                 /*ack_seq*/SPI_SEQ_IDLE,
                 SPI_STATUS_BUSY,
                 SPI_RSP_TYPE_ACK,
                 NULL, 0,
                 /*flags*/0,
                 /*uptime*/0);
}

/* ------------------------------------------------------------------ */
/* RPMsg endpoint 回调：纯透传，不解析协议字段                         */
/* ------------------------------------------------------------------ */

static volatile int s_shutdown = 0;

static int aqua_rpmsg_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
                         uint32_t src, void *priv)
{
    int rc;
    (void)priv;

    /* 任何回包前都要先锁定对端地址；否则 len 不匹配时直接 return 会导致 Linux read 超时(-ETIMEDOUT) */
    ept->dest_addr = src;

    if (len != SPI_FRAME_LEN)
    {
        AQUA_W("RPMsg len=%u 非 %u -> 仍回 BUSY 避免主核超时", (unsigned)len,
               (unsigned)SPI_FRAME_LEN);
        rc = rpmsg_send(ept, s_busy_frame, SPI_FRAME_LEN);
        if (rc < 0)
            AQUA_W("rpmsg_send busy(len) rc=%d", rc);
        return RPMSG_SUCCESS;
    }

    /* 第 1 次 SPI 事务：发 CMD，丢 MISO（是上一帧滞后响应） */
    rc = aqua_spi_master_xfer_64((const u8 *)data, s_dummy_rx);
    if (rc != 0)
        goto err_busy;

    /* 让 RA6E2 在 ISR + 任务唤醒中装好 RSP */
    fsleep_microsec(AQUA_RA6E2_PREP_US);

    /* 第 2 次 SPI 事务：发 NOP，MISO 即本次命令的响应 */
    rc = aqua_spi_master_xfer_64(s_nop_frame, s_rsp_frame);
    if (rc != 0)
        goto err_busy;

    /* 透传回 Linux —— 注意不要写 data 指针，那块是 vring buf，
     * 重发要用本端 alloc 的缓冲（s_rsp_frame）。 */
    rc = rpmsg_send(ept, s_rsp_frame, SPI_FRAME_LEN);
    if (rc < 0)
        AQUA_W("rpmsg_send rsp failed rc=%d", rc);
    return RPMSG_SUCCESS;

err_busy:
    /* SPI 出错也必须给 Linux 回点东西，否则它的 read() 会一直阻塞超时。
     * 回一个 STATUS=BUSY 的伪 RSP 让 daemon 累计 stats、上层判定失败。 */
    rc = rpmsg_send(ept, s_busy_frame, SPI_FRAME_LEN);
    if (rc < 0)
        AQUA_W("rpmsg_send busy failed rc=%d", rc);
    return RPMSG_SUCCESS;
}

static void aqua_rpmsg_unbind(struct rpmsg_endpoint *ept)
{
    (void)ept;
    AQUA_I("Linux 端关闭了 endpoint，准备退出");
    s_shutdown = 1;
}

/* ------------------------------------------------------------------ */
/* 主循环骨架（与 SDK 例程同形态）                                    */
/* ------------------------------------------------------------------ */

static int aqua_rpmsg_app(struct rpmsg_device *rdev, void *priv)
{
    int rc;
    struct rpmsg_endpoint lept = {0};
    s_shutdown = 0;

    AQUA_I("正在创建 endpoint '%s' ...", AQUA_RPMSG_SERVICE);
    rc = rpmsg_create_ept(&lept, rdev,
                          AQUA_RPMSG_SERVICE,
                          0, RPMSG_ADDR_ANY,
                          aqua_rpmsg_cb, aqua_rpmsg_unbind);
    if (rc)
    {
        AQUA_E("rpmsg_create_ept '%s' 失败 rc=%d", AQUA_RPMSG_SERVICE, rc);
        return -1;
    }
    AQUA_I("endpoint 创建成功，等待 Linux 端连接");

    while (1)
    {
        platform_poll(priv);
        if (s_shutdown || rproc_get_stop_flag())
        {
            rproc_clear_stop_flag();
            break;
        }
    }

    rpmsg_destroy_ept(&lept);
    AQUA_I("退出 RPMsg 应用");
    return rc;
}

/* ------------------------------------------------------------------ */
/* 初始化 + 顶层 entry                                                */
/* ------------------------------------------------------------------ */

static int aqua_init(void)
{
    init_system();

    if (aqua_spi_master_init() != 0)
    {
        AQUA_E("aqua_spi_master_init 失败");
        return -1;
    }
    aqua_pack_static_frames();

    if (!platform_create_proc(&remoteproc_aqua, &slave_aqua_priv, &kick_driver_aqua))
    {
        AQUA_E("platform_create_proc 失败");
        return -1;
    }

    remoteproc_aqua.rsc_table = &resources;
    if (platform_setup_src_table(&remoteproc_aqua, remoteproc_aqua.rsc_table))
    {
        AQUA_E("platform_setup_src_table 失败");
        return -1;
    }
    AQUA_I("资源表配置完成");

    if (platform_setup_share_mems(&remoteproc_aqua))
    {
        AQUA_E("platform_setup_share_mems 失败");
        return -1;
    }
    AQUA_I("共享内存配置完成");

    rpdev_aqua = platform_create_rpmsg_vdev(&remoteproc_aqua, 0,
                                            VIRTIO_DEV_DEVICE, NULL, NULL);
    if (!rpdev_aqua)
    {
        AQUA_E("platform_create_rpmsg_vdev 失败");
        return -1;
    }
    AQUA_I("RPMsg vdev 创建完成");

    return 0;
}

int aqua_spi_slave_run(void)
{
    int rc;

    AQUA_I("启动 AquaGarden SPI 透传应用");
    if (aqua_init() != 0)
    {
        platform_cleanup(&remoteproc_aqua);
        AQUA_E("初始化失败，远程核退出");
        return -1;
    }

    rc = aqua_rpmsg_app(rpdev_aqua, &remoteproc_aqua);
    if (rc)
    {
        AQUA_E("rpmsg app 异常退出 rc=%d", rc);
        platform_cleanup(&remoteproc_aqua);
        return -1;
    }

    platform_release_rpmsg_vdev(rpdev_aqua, &remoteproc_aqua);
    aqua_spi_master_deinit();
    platform_cleanup(&remoteproc_aqua);
    AQUA_I("远程核停机 (PSCI CPU off)");

    FPsciCpuOff();
    return 0;
}
