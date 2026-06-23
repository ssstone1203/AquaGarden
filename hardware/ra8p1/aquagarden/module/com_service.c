#include "com_service.h"
#include "com_protocol.h"
#include "aquagarden_app.h"
#include "../ra_gen/main_service.h"
#include <string.h>

#define COM_RX_BUF_LEN      (2U + 1U + 1U + COM_DOWN_MAX_PAYLOAD + 2U)
#define COM_TX_PERIOD_TICKS (25U)      /* 25 x 10 ms = 250 ms */
#define COM_TX_POLL_TIMEOUT (100000U)

/* RX collected in ISR context */
static volatile uint8_t s_rx_buf[COM_RX_BUF_LEN];
static volatile uint8_t s_rx_index;
static volatile uint8_t s_rx_expected_len;
static volatile uint8_t s_pending_buf[COM_RX_BUF_LEN];
static volatile uint8_t s_pending_len;
static volatile uint8_t s_frame_pending;
static volatile uint8_t s_rx_error_flag;

/* TX */
static uint8_t s_tx_frame[COM_UP_FRAME_LEN];
static uint8_t s_tx_seq;
static uint8_t s_tx_tick_count;

static uint8_t s_frame_buf[COM_RX_BUF_LEN];
static uart_callback_args_t s_uart_callback_memory;
static sci_b_uart_extended_cfg_t s_uart_extend_cfg;
static uart_cfg_t s_uart_cfg;
static uint8_t s_initialized;

static void com_reset_rx(void)
{
    s_rx_index = 0U;
    s_rx_expected_len = 0U;
}

static void com_rx_byte(uint8_t byte)
{
    if (0U == s_rx_index)
    {
        if (COM_DOWN_SYNC0 == byte)
        {
            s_rx_buf[s_rx_index++] = byte;
        }
        return;
    }

    if (1U == s_rx_index)
    {
        if (COM_DOWN_SYNC1 == byte)
        {
            s_rx_buf[s_rx_index++] = byte;
        }
        else
        {
            com_reset_rx();
            if (COM_DOWN_SYNC0 == byte)
            {
                s_rx_buf[s_rx_index++] = byte;
            }
        }
        return;
    }

    s_rx_buf[s_rx_index++] = byte;

    if (3U == s_rx_index)
    {
        uint8_t len = s_rx_buf[2];
        if ((0U == len) || (len > (uint8_t) (1U + COM_DOWN_MAX_PAYLOAD)))
        {
            com_reset_rx();
            s_rx_error_flag = 1U;
            return;
        }
        s_rx_expected_len = (uint8_t) (2U + 1U + len + 2U);
    }

    if ((s_rx_expected_len > 0U) && (s_rx_index >= s_rx_expected_len))
    {
        if (0U == s_frame_pending)
        {
            for (uint8_t i = 0U; i < s_rx_expected_len; i++)
            {
                s_pending_buf[i] = s_rx_buf[i];
            }
            s_pending_len = s_rx_expected_len;
            s_frame_pending = 1U;
        }
        com_reset_rx();
    }
    else if (s_rx_index >= COM_RX_BUF_LEN)
    {
        com_reset_rx();
        s_rx_error_flag = 1U;
    }
}

/* SCI UART callback (ISR context): RX only, no RTOS calls.
 * TX is done by task-context register polling, so TX interrupts stay disabled
 * (see DEBUG_RECORD.md: the FSP interrupt-driven write path faults on this MCU). */
static void com_uart_callback(uart_callback_args_t * p_args)
{
    if ((NULL != p_args) && (UART_EVENT_RX_CHAR == p_args->event))
    {
        com_rx_byte((uint8_t) (p_args->data & 0xFFU));
    }
}

static void com_disable_tx_irqs(void)
{
    if (s_uart_cfg.txi_irq >= 0)
    {
        R_BSP_IrqDisable(s_uart_cfg.txi_irq);
        R_BSP_IrqStatusClear(s_uart_cfg.txi_irq);
    }
    if (s_uart_cfg.tei_irq >= 0)
    {
        R_BSP_IrqDisable(s_uart_cfg.tei_irq);
        R_BSP_IrqStatusClear(s_uart_cfg.tei_irq);
    }
}

/* Polled transmit straight to the SCI registers. Keeps the ISR path minimal and
 * avoids the FSP TXI/TEI interrupt sequence that destabilises this project. */
static fsp_err_t com_uart_write_polling(uint8_t const * p_data, uint32_t len)
{
    if ((NULL == g_uart0_ctrl.p_reg) || (NULL == p_data))
    {
        return FSP_ERR_ASSERTION;
    }

    com_disable_tx_irqs();
    g_uart0_ctrl.p_reg->CCR0 &= (uint32_t) ~(R_SCI_B0_CCR0_TIE_Msk | R_SCI_B0_CCR0_TEIE_Msk);
    g_uart0_ctrl.p_reg->CCR0 |= R_SCI_B0_CCR0_TE_Msk;

    for (uint32_t i = 0U; i < len; i++)
    {
        uint32_t timeout = COM_TX_POLL_TIMEOUT;
        while ((0U == g_uart0_ctrl.p_reg->CSR_b.TDRE) && (timeout > 0U))
        {
            timeout--;
        }
        if (0U == timeout)
        {
            return FSP_ERR_TIMEOUT;
        }

        g_uart0_ctrl.p_reg->TDR_BY = p_data[i];
        g_uart0_ctrl.p_reg->CFCLR = R_SCI_B0_CFCLR_TDREC_Msk;
    }

    uint32_t timeout = COM_TX_POLL_TIMEOUT;
    while ((0U == g_uart0_ctrl.p_reg->CSR_b.TEND) && (timeout > 0U))
    {
        timeout--;
    }
    if (0U == timeout)
    {
        return FSP_ERR_TIMEOUT;
    }

    g_uart0_ctrl.p_reg->CCR0 &= (uint32_t) ~(R_SCI_B0_CCR0_TIE_Msk | R_SCI_B0_CCR0_TEIE_Msk | R_SCI_B0_CCR0_TE_Msk);
    com_disable_tx_irqs();
    return FSP_SUCCESS;
}

static void com_handle_rx_frame(void)
{
    com_downlink_frame_t frame;
    uint8_t len = 0U;

    if (0U == s_frame_pending)
    {
        return;
    }

    if (s_pending_len <= COM_RX_BUF_LEN)
    {
        len = s_pending_len;
        (void) memcpy(s_frame_buf, (const void *) s_pending_buf, len);
    }
    s_frame_pending = 0U;

    if (0U == len)
    {
        return;
    }

    if (com_downlink_decode(s_frame_buf, len, &frame))
    {
        com_apply_downlink(&frame);
    }
    else
    {
        aqua_app_note_comm_error();
    }
}

static void com_send_uplink(void)
{
    aqua_snapshot_t snapshot;

    aqua_app_get_snapshot(&snapshot);
    com_build_uplink(s_tx_seq++, &snapshot, s_tx_frame);

    if (FSP_SUCCESS != com_uart_write_polling(s_tx_frame, sizeof(s_tx_frame)))
    {
        aqua_app_note_comm_error();
    }
}

void com_service_init(void)
{
    if (0U != s_initialized)
    {
        return;
    }

    s_uart_extend_cfg = g_uart0_cfg_extend;
    s_uart_extend_cfg.rx_fifo_trigger = SCI_B_UART_RX_FIFO_TRIGGER_1;
    s_uart_cfg = g_uart0_cfg;
    s_uart_cfg.p_extend = &s_uart_extend_cfg;

    if (FSP_SUCCESS == g_uart0.p_api->open(g_uart0.p_ctrl, &s_uart_cfg))
    {
        com_disable_tx_irqs();
        (void) g_uart0.p_api->callbackSet(g_uart0.p_ctrl, com_uart_callback, NULL, &s_uart_callback_memory);
        s_initialized = 1U;
    }
}

void com_service_process_10ms(void)
{
    if (0U == s_initialized)
    {
        return;
    }

    if (0U != s_rx_error_flag)
    {
        s_rx_error_flag = 0U;
        aqua_app_note_comm_error();
    }

    if (0U != s_frame_pending)
    {
        com_handle_rx_frame();
    }

    s_tx_tick_count++;
    if (s_tx_tick_count >= COM_TX_PERIOD_TICKS)
    {
        s_tx_tick_count = 0U;
        com_send_uplink();
    }
}
