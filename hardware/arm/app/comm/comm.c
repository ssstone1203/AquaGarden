#include "comm.h"
#include "string.h"

/* ── RX：逐字节积累，检测换行 ──────────────────────────── */
static volatile uint8_t s_rx_buf[COMM_RX_BUF_SIZE];
static volatile uint8_t s_rx_idx    = 0;
static volatile bool    s_line_ready = false;

/* ── TX：异步发送，静态缓冲区保证发送期间数据有效 ──────── */
static volatile bool s_tx_busy = false;
static uint8_t       s_tx_buf[COMM_TX_BUF_SIZE];

/* ── UART 事件回调 ──────────────────────────────────────── */
void Comm_UartCallback(uart_callback_args_t *p_args)
{
    switch (p_args->event)
    {
        case UART_EVENT_TX_COMPLETE:
            s_tx_busy = false;
            break;

        case UART_EVENT_RX_CHAR:
        {
            uint8_t byte = (uint8_t)p_args->data;

            /* 上一行尚未被应用层读取时，新字节直接丢弃 */
            if (s_line_ready)
                break;

            if (byte == '\r')   /* 兼容 \r\n，忽略 CR */
                break;

            if (byte == '\n')
            {
                s_rx_buf[s_rx_idx] = '\0';
                s_line_ready = true;
            }
            else if (s_rx_idx < COMM_RX_BUF_SIZE - 1)
            {
                s_rx_buf[s_rx_idx++] = byte;
            }
            /* 超长行：静默截断，继续等待 \n */
            break;
        }

        default:
            break;
    }
}

/* ── 初始化 ─────────────────────────────────────────────── */
void Comm_Init(void)
{
    s_rx_idx     = 0;
    s_line_ready = false;
    s_tx_busy    = false;

    R_SCI_UART_Open(&g_com_uart_ctrl, &g_com_uart_cfg);
    R_SCI_UART_CallbackSet(&g_com_uart_ctrl, Comm_UartCallback, NULL, NULL);
}

/* ── 接收接口 ────────────────────────────────────────────── */
bool Comm_HasLine(void)
{
    return s_line_ready;
}

uint8_t Comm_ReadLine(char *out_buf, uint8_t max_len)
{
    if (!s_line_ready || out_buf == NULL || max_len == 0)
        return 0;

    uint8_t len = s_rx_idx;
    if (len >= max_len)
        len = max_len - 1;

    memcpy(out_buf, (const void *)s_rx_buf, len);
    out_buf[len] = '\0';

    s_rx_idx     = 0;
    s_line_ready = false;

    return len;
}

/* ── 发送接口 ────────────────────────────────────────────── */
void Comm_SendBytes(const uint8_t *data, uint16_t len)
{
    if (s_tx_busy || data == NULL || len == 0 || len > COMM_TX_BUF_SIZE)
        return;

    memcpy(s_tx_buf, data, len);
    s_tx_busy = true;
    R_SCI_UART_Write(&g_com_uart_ctrl, s_tx_buf, len);
}

void Comm_SendStr(const char *str)
{
    if (str == NULL)
        return;
    Comm_SendBytes((const uint8_t *)str, (uint16_t)strlen(str));
}
