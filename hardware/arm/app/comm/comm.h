#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_data.h"

/* 缓冲区大小 */
#define COMM_RX_BUF_SIZE    64
#define COMM_TX_BUF_SIZE    64

/**
 * @brief 初始化通信模块（打开 g_com_uart，注册回调）
 */
void Comm_Init(void);

/**
 * @brief 判断是否已接收到完整一行（以 '\n' 结尾）
 * @return true 表示有一行数据可读
 */
bool Comm_HasLine(void);

/**
 * @brief 读取一行文本（不含换行符，末尾追加 '\0'）
 * @param out_buf 输出缓冲区
 * @param max_len 缓冲区最大字节数（含 '\0'）
 * @return 实际读取的字符数（不含 '\0'），未就绪时返回 0
 */
uint8_t Comm_ReadLine(char *out_buf, uint8_t max_len);

/**
 * @brief 发送字节数组
 * @param data 数据指针
 * @param len  字节数（不超过 COMM_TX_BUF_SIZE）
 */
void Comm_SendBytes(const uint8_t *data, uint16_t len);

/**
 * @brief 发送以 '\0' 结尾的字符串
 */
void Comm_SendStr(const char *str);

/**
 * @brief g_com_uart UART 事件回调（由 FSP 驱动调用）
 */
void Comm_UartCallback(uart_callback_args_t *p_args);

#endif /* COMM_H */
