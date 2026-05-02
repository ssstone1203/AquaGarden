/*
 * Communicate_Task_entry.c — RA6E2 SPI Slave 通信任务（替换 UART 版）
 *
 * 与原 UART 版本相比：
 *   - 删除 SCI0 直寄存器收发、host_uart0_*、host_parse_downlink_stream
 *   - 主循环改为：g_com_spi.p_api->writeRead() 启动一次 64B 全双工事务，
 *     等待 Com_SPI_Callback 在 EOT 中断里 give 信号量，然后处理 RX、装 TX，
 *     立即重新 writeRead。
 *   - 复用：CRC16/Modbus、字段编码 helper、host_apply_command(...)（语义重写）
 *           、host_update_alarm_flags、host_build_uplink_frame
 *   - 命令空间：DEV(1B) + CMD(1B) 二级（见 spi_protocol.h），不再用扁平 HOST_CMD_*
 *
 * 部署到 e2studio 工程：
 *   1) 把本文件覆盖到 src/Communicate_Task_entry.c
 *   2) 复制 ra6e2_patch/spi_protocol.h、spi_codec.h、spi_codec.c 到 src/
 *      （由 sync_from_canonical.sh 维护与 Linux 端一致）
 *   3) 在 FSP Configurator 中把 SPI1 (g_com_spi) 的 TX/RX DMAC 字宽从
 *      TRANSFER_SIZE_2_BYTE 改为 TRANSFER_SIZE_1_BYTE，重新生成代码
 *   4) （可选）SCI0 UART 模块仍可保留作日志，但不再承担与上位机通信
 */

#include "Communicate_Task.h"
#include "sensor_fusion.h"
#include "wqs_sensor.h"
#include "ds18b20.h"
#include "sht30.h"

#include "spi_protocol.h"
#include "spi_codec.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <string.h>

/* FSP 在 Communicate_Task.c 中定义 */
extern const spi_instance_t g_com_spi;

/* ------------------------------------------------------------------ */
/* 应用层版本号                                                       */
/* ------------------------------------------------------------------ */

#define APP_VER_MAJOR  1u
#define APP_VER_MINOR  0u
#define APP_VER_PATCH  0u

#define HOST_ALARM_PRESSURE_HIGH_KG   (4.8F)

/* ------------------------------------------------------------------ */
/* 双缓冲帧 + 同步原语                                                */
/* ------------------------------------------------------------------ */

/* 必须用对齐缓冲，DMAC 才能稳定搬 1 字节宽度 */
static uint8_t s_tx_frame[SPI_FRAME_LEN] __attribute__((aligned(4)));
static uint8_t s_rx_frame[SPI_FRAME_LEN] __attribute__((aligned(4)));

static SemaphoreHandle_t s_spi_done_sem;
static volatile spi_event_t s_last_event;

/* ------------------------------------------------------------------ */
/* 字段助手（复用原 UART 版的浮点 → 定点压缩函数）                    */
/* ------------------------------------------------------------------ */

static uint8_t app_clamp_percent_u8(uint8_t v)
{
    return (v > 100u) ? 100u : v;
}

static int16_t app_float_to_i16_x10(float v)
{
    float s = v * 10.0F;
    if (s > 32767.0F)  return 32767;
    if (s < -32768.0F) return -32768;
    return (int16_t)s;
}

static uint16_t app_float_to_u16_x100(float v)
{
    float s = v * 100.0F;
    if (s < 0.0F)       return 0;
    if (s > 65535.0F)   return 65535;
    return (uint16_t)s;
}

/* ------------------------------------------------------------------ */
/* 装配 SENSOR_DATA 48B PAYLOAD（字段顺序 = §3.3）                    */
/* ------------------------------------------------------------------ */

static uint8_t app_build_sensor_payload(uint8_t *p)
{
    memset(p, 0, SPI_SENSOR_PAYLOAD_LEN);

    spi_u32_put(&p[SPI_SENS_OFF_TIMESTAMP_MS],        g_jscope_time_ms);
    spi_u16_put(&p[SPI_SENS_OFF_AIR_TEMP_X10],        (uint16_t)app_float_to_i16_x10(g_sht30_temperature_c));
    spi_u16_put(&p[SPI_SENS_OFF_AIR_HUMIDITY_X10],    (uint16_t)app_float_to_i16_x10(g_sht30_humidity_rh));
    spi_u16_put(&p[SPI_SENS_OFF_WATER_TEMP_X10],      (uint16_t)app_float_to_i16_x10(g_uwt_temperature_c));
    p[SPI_SENS_OFF_SOIL_MOISTURE_PCT]               = g_soil_moisture_percent;
    p[SPI_SENS_OFF_WQI]                             = g_wqs_info.wqs_info_wqi;
    p[SPI_SENS_OFF_PUMP_POWER_PCT]                  = g_pump_actual_power_percent;
    p[SPI_SENS_OFF_NEED_WATERING]                   = g_control_need_watering;
    spi_u16_put(&p[SPI_SENS_OFF_PRESSURE_KG_0_X100], app_float_to_u16_x100(g_pressure_latest.pressure_kg[0]));
    spi_u16_put(&p[SPI_SENS_OFF_PRESSURE_KG_1_X100], app_float_to_u16_x100(g_pressure_latest.pressure_kg[1]));
    spi_u16_put(&p[SPI_SENS_OFF_PRESSURE_KG_2_X100], app_float_to_u16_x100(g_pressure_latest.pressure_kg[2]));
    spi_u32_put(&p[SPI_SENS_OFF_ALARM_FLAGS],        g_alarm_flags);
    spi_u16_put(&p[SPI_SENS_OFF_AIR_RETRY],          (uint16_t)g_air_retry_count);
    spi_u16_put(&p[SPI_SENS_OFF_WQS_RETRY],          (uint16_t)g_wqs_retry_count);
    spi_u16_put(&p[SPI_SENS_OFF_UWT_RETRY],          (uint16_t)g_uwt_retry_count);
    p[SPI_SENS_OFF_PUMP_CYCLE_ENABLE]               = g_pump_cycle_enable;
    p[SPI_SENS_OFF_PUMP_CYCLE_START]                = g_pump_cycle_start;
    p[SPI_SENS_OFF_PUMP_CYCLE_ACTIVE]               = g_pump_cycle_active;
    p[SPI_SENS_OFF_PUMP_CYCLE_STATE]                = g_pump_cycle_state;
    p[SPI_SENS_OFF_PUMP_CYCLE_POWER]                = g_pump_cycle_power_percent;
    spi_u16_put(&p[SPI_SENS_OFF_PUMP_CYCLE_DONE],    (uint16_t)g_pump_cycle_done_count);

    return SPI_SENSOR_PAYLOAD_LEN;
}

/* ------------------------------------------------------------------ */
/* 告警位刷新（搬自原 host_update_alarm_flags）                       */
/* ------------------------------------------------------------------ */

static void app_update_alarm_flags(void)
{
    uint32_t alarm = 0u;

    if (0u != g_soil_sensor_state)                                        alarm |= SENSOR_ALARM_SOIL_SENSOR_FAULT;
    if (FSP_SUCCESS != g_pressure_last_err)                               alarm |= SENSOR_ALARM_PRESSURE_READ_FAIL;
    if (FSP_SUCCESS != g_air_last_err)                                    alarm |= SENSOR_ALARM_AIR_READ_FAIL;
    if (FSP_SUCCESS != g_wqs_last_err)                                    alarm |= SENSOR_ALARM_WQS_READ_FAIL;
    if (FSP_SUCCESS != g_uwt_last_err)                                    alarm |= SENSOR_ALARM_UWT_READ_FAIL;
    if ((FSP_SUCCESS == g_uwt_last_err) && (g_uwt_temperature_c >= g_ctrl_water_temp_high_c))
                                                                          alarm |= SENSOR_ALARM_WATER_TEMP_HIGH;
    if ((0u != g_wqs_last_read_ok) && (g_wqs_info.wqs_info_wqi <= g_ctrl_wqi_low_threshold))
                                                                          alarm |= SENSOR_ALARM_WQI_LOW;
    if ((g_pressure_latest.pressure_kg[0] >= HOST_ALARM_PRESSURE_HIGH_KG) ||
        (g_pressure_latest.pressure_kg[1] >= HOST_ALARM_PRESSURE_HIGH_KG) ||
        (g_pressure_latest.pressure_kg[2] >= HOST_ALARM_PRESSURE_HIGH_KG))
                                                                          alarm |= SENSOR_ALARM_PRESSURE_HIGH;
    if (g_comm_rx_crc_error_count > 0u)                                   alarm |= SENSOR_ALARM_COMM_RX_ERROR;

    g_alarm_flags = alarm;
    g_alarm_latched_flags |= alarm;
    g_jscope_alarm_flags = alarm;
}

/* ------------------------------------------------------------------ */
/* 命令分发：按 (DEV, CMD) 二级映射                                    */
/* ------------------------------------------------------------------ */

/* 业务级 cmd 处理（PUMP / LINKAGE）。返回 spi_status_t。 */
static uint8_t app_handle_pump(uint8_t cmd, const uint8_t *p, uint8_t len)
{
    switch (cmd)
    {
    case SPI_CMD_PUMP_START:
        g_pump_manual_mode = 1u;
        if (len >= 1u)
        {
            g_pump_manual_power_percent = app_clamp_percent_u8(p[0]);
            g_pump_cycle_power_percent  = g_pump_manual_power_percent;
        }
        if (0u == g_pump_manual_power_percent)
        {
            g_pump_manual_power_percent = 60u;
            g_pump_cycle_power_percent  = 60u;
        }
        return SPI_STATUS_OK;

    case SPI_CMD_PUMP_STOP:
        g_pump_manual_mode = 1u;
        g_pump_manual_power_percent = 0u;
        return SPI_STATUS_OK;

    case SPI_CMD_PUMP_SET_PWM:
        if (len < 1u) return SPI_STATUS_BAD_PAYLOAD_LEN;
        g_pump_manual_mode = 1u;
        g_pump_manual_power_percent = app_clamp_percent_u8(p[0]);
        g_pump_cycle_power_percent  = g_pump_manual_power_percent;
        return SPI_STATUS_OK;

    case SPI_CMD_PUMP_SET_AUTO:
        g_pump_manual_mode = 0u;
        return SPI_STATUS_OK;

    case SPI_CMD_PUMP_SET_MANUAL:
        if (len < 2u) return SPI_STATUS_BAD_PAYLOAD_LEN;
        g_pump_manual_mode          = (0u != p[0]) ? 1u : 0u;
        g_pump_manual_power_percent = app_clamp_percent_u8(p[1]);
        return SPI_STATUS_OK;

    case SPI_CMD_PUMP_SET_CYCLE_CFG:
        if (len < 11u) return SPI_STATUS_BAD_PAYLOAD_LEN;
        g_pump_cycle_enable        = (0u != p[0]) ? 1u : 0u;
        g_pump_cycle_start         = (0u != p[1]) ? 1u : 0u;
        g_pump_cycle_power_percent = app_clamp_percent_u8(p[2]);
        g_pump_cycle_run_time_ms   = spi_u16_get(&p[3]);
        g_pump_cycle_stop_time_ms  = spi_u16_get(&p[5]);
        g_pump_cycle_interval_time_ms = spi_u16_get(&p[7]);
        g_pump_cycle_total_count   = spi_u16_get(&p[9]);
        return SPI_STATUS_OK;

    case SPI_CMD_PUMP_SET_CYCLE_CTRL:
        if (len < 1u) return SPI_STATUS_BAD_PAYLOAD_LEN;
        g_pump_cycle_enable = (0u != p[0]) ? 1u : 0u;
        if (len >= 2u) g_pump_cycle_start = (0u != p[1]) ? 1u : 0u;
        if (len >= 6u) g_pump_cycle_total_count = spi_u32_get(&p[2]);
        return SPI_STATUS_OK;

    case SPI_CMD_PUMP_GET_STATUS:
        return SPI_STATUS_OK; /* PAYLOAD 由调用方按 RSP_TYPE_STATUS 装 */

    default:
        return SPI_STATUS_UNKNOWN_CMD;
    }
}

static uint8_t app_handle_linkage(uint8_t cmd, const uint8_t *p, uint8_t len)
{
    switch (cmd)
    {
    case SPI_CMD_LINK_SET_SOIL_CFG:
        if (len < 2u) return SPI_STATUS_BAD_PAYLOAD_LEN;
        g_soil_watering_threshold  = app_clamp_percent_u8(p[0]);
        g_soil_watering_hysteresis = app_clamp_percent_u8(p[1]);
        return SPI_STATUS_OK;

    case SPI_CMD_LINK_SET_RULE:
    {
        if (len < 4u) return SPI_STATUS_BAD_PAYLOAD_LEN;
        g_ctrl_enable_soil       = (0u != (p[0] & 0x01u)) ? 1u : 0u;
        g_ctrl_enable_water_temp = (0u != (p[0] & 0x02u)) ? 1u : 0u;
        g_ctrl_enable_wqi        = (0u != (p[0] & 0x04u)) ? 1u : 0u;
        float wt = (float)spi_i16_get(&p[1]) / 10.0F;
        if (wt < 0.0F)   wt = 0.0F;
        if (wt > 100.0F) wt = 100.0F;
        g_ctrl_water_temp_high_c = wt;
        uint8_t low = p[3];
        if (low > 100u) low = 100u;
        g_ctrl_wqi_low_threshold = low;
        return SPI_STATUS_OK;
    }

    default:
        return SPI_STATUS_UNKNOWN_CMD;
    }
}

/*
 * 解析 RX 帧并装配 TX 帧。
 * rx, tx 都是 SPI_FRAME_LEN 字节缓冲。
 */
static void app_dispatch(const uint8_t *rx, uint8_t *tx)
{
    /* 1. 帧整体校验 */
    int v = spi_validate_frame(rx, /*is_cmd*/1);
    if (v != 0)
    {
        uint8_t status = (uint8_t)(-v);
        if (status == SPI_STATUS_CRC_ERR)
        {
            g_comm_rx_crc_error_count++;
            g_alarm_flags |= SENSOR_ALARM_COMM_RX_ERROR;
            g_alarm_latched_flags |= SENSOR_ALARM_COMM_RX_ERROR;
        }
        /* 校验失败时，ack_seq 用 0xFF（无可信 SEQ） */
        spi_pack_rsp(tx, 0xFFu, status, SPI_RSP_TYPE_ACK, NULL, 0,
                     (g_alarm_flags ? SPI_RSP_FLAG_ALARM : 0),
                     (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS);
        return;
    }

    uint8_t  seq     = rx[SPI_CMD_OFF_SEQ];
    uint8_t  dev     = rx[SPI_CMD_OFF_DEV];
    uint8_t  cmd     = rx[SPI_CMD_OFF_CMD];
    uint8_t  len     = rx[SPI_CMD_OFF_LEN];
    const uint8_t *payload = &rx[SPI_CMD_OFF_PAYLOAD];

    g_comm_rx_cmd_count++;

    uint8_t status   = SPI_STATUS_OK;
    uint8_t rsp_type = SPI_RSP_TYPE_ACK;
    uint8_t rsp_payload[SPI_RSP_PAYLOAD_MAX];
    uint8_t rsp_len  = 0;

    switch (dev)
    {
    case SPI_DEV_SYSTEM:
        switch (cmd)
        {
        case SPI_CMD_SYS_NOP:
        case SPI_CMD_SYS_PING:
        case SPI_CMD_SYS_GET_UPTIME:
            /* 单纯 ACK，uptime 在 RSP 帧固定字段里 */
            break;
        case SPI_CMD_SYS_GET_VERSION:
            rsp_payload[SPI_VER_OFF_PROTO] = SPI_PROTO_VERSION;
            rsp_payload[SPI_VER_OFF_MAJOR] = APP_VER_MAJOR;
            rsp_payload[SPI_VER_OFF_MINOR] = APP_VER_MINOR;
            rsp_payload[SPI_VER_OFF_PATCH] = APP_VER_PATCH;
            rsp_len  = SPI_VERSION_PAYLOAD_LEN;
            rsp_type = SPI_RSP_TYPE_VERSION;
            break;
        case SPI_CMD_SYS_RESET_ALARM:
            g_alarm_latched_flags = 0u;
            break;
        case SPI_CMD_SYS_HELLO:
            /* 主机重启通知；可在此清统计/重置 SEQ 跟踪等 */
            g_comm_rx_crc_error_count = 0u;
            g_comm_rx_cmd_count       = 0u;
            break;
        default:
            status = SPI_STATUS_UNKNOWN_CMD;
            break;
        }
        break;

    case SPI_DEV_PUMP:
        status = app_handle_pump(cmd, payload, len);
        break;

    case SPI_DEV_SENSOR:
        if (cmd == SPI_CMD_SENSOR_POLL_ALL ||
            cmd == SPI_CMD_SENSOR_POLL_AIR ||
            cmd == SPI_CMD_SENSOR_POLL_WATER ||
            cmd == SPI_CMD_SENSOR_POLL_SOIL ||
            cmd == SPI_CMD_SENSOR_POLL_PRESSURE)
        {
            /* v1 实现里：所有 POLL_* 都返回完整 SensorData，主机各取所需 */
            rsp_len  = app_build_sensor_payload(rsp_payload);
            rsp_type = SPI_RSP_TYPE_SENSOR_DATA;
        }
        else
        {
            status = SPI_STATUS_UNKNOWN_CMD;
        }
        break;

    case SPI_DEV_LINKAGE:
        status = app_handle_linkage(cmd, payload, len);
        break;

    default:
        status = SPI_STATUS_UNKNOWN_DEV;
        break;
    }

    uint8_t flags = 0u;
    if (g_alarm_flags) flags |= SPI_RSP_FLAG_ALARM;

    uint32_t uptime = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    spi_pack_rsp(tx, seq, status, rsp_type, rsp_payload, rsp_len, flags, uptime);
}

/* ------------------------------------------------------------------ */
/* SPI 完成回调（中断上下文）                                          */
/* ------------------------------------------------------------------ */

void Com_SPI_Callback(spi_callback_args_t *p_args)
{
    if (p_args == NULL || s_spi_done_sem == NULL) return;

    s_last_event = p_args->event;

    BaseType_t hp_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_spi_done_sem, &hp_woken);
    portYIELD_FROM_ISR(hp_woken);
}

/* ------------------------------------------------------------------ */
/* 任务入口                                                            */
/* ------------------------------------------------------------------ */

void Communicate_Task_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    /* 该任务必须比传感器采样任务高，避免装载 RSP 时被打断 */
    vTaskPrioritySet(NULL, 3u);

    s_spi_done_sem = xSemaphoreCreateBinary();
    configASSERT(s_spi_done_sem != NULL);

    /* 1) 打开 SPI Slave */
    fsp_err_t err = g_com_spi.p_api->open(g_com_spi.p_ctrl, g_com_spi.p_cfg);
    configASSERT(err == FSP_SUCCESS);

    /* 2) 上电先装一帧"Idle 响应"，让首次主机事务能拿到合法帧 */
    spi_pack_rsp(s_tx_frame, SPI_SEQ_IDLE, SPI_STATUS_OK,
                 SPI_RSP_TYPE_ACK, NULL, 0, 0,
                 (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS);

    const TickType_t xfer_wait = pdMS_TO_TICKS(1000u);

    for (;;)
    {
        /* 3) 启动一次 64B 全双工事务（异步：装好 DMAC 即返回）
         *    TX = s_tx_frame（已是上一次装好的响应或 Idle）
         *    RX = s_rx_frame
         */
        err = g_com_spi.p_api->writeRead(g_com_spi.p_ctrl,
                                         s_tx_frame, s_rx_frame,
                                         (uint32_t)SPI_FRAME_LEN,
                                         SPI_BIT_WIDTH_8_BITS);
        if (err != FSP_SUCCESS)
        {
            /* 启动失败：稍等重试 */
            vTaskDelay(pdMS_TO_TICKS(2u));
            continue;
        }

        /* 4) 等主机驱动 SCK 完成 + DMAC EOT */
        if (xSemaphoreTake(s_spi_done_sem, xfer_wait) != pdTRUE)
        {
            /* 主机长时间无事务（>1 s）：取消并重新挂 DMAC */
            (void)g_com_spi.p_api->close(g_com_spi.p_ctrl);
            err = g_com_spi.p_api->open(g_com_spi.p_ctrl, g_com_spi.p_cfg);
            configASSERT(err == FSP_SUCCESS);
            continue;
        }

        if (s_last_event != SPI_EVENT_TRANSFER_COMPLETE)
        {
            /* SPI 总线错误（PER/MODF/OVR）：记一次然后重试 */
            g_alarm_flags |= SENSOR_ALARM_COMM_RX_ERROR;
            g_alarm_latched_flags |= SENSOR_ALARM_COMM_RX_ERROR;
            continue;
        }

        /* 5) 解析本次 CMD 并装好下一次要发的 RSP */
        app_update_alarm_flags();
        app_dispatch(s_rx_frame, s_tx_frame);
        sensor_fusion_update_jscope_time();

        g_comm_tx_seq++;
        g_comm_last_tx_ok = 1u;
    }
}
