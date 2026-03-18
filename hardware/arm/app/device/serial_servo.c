#include "serial_servo.h"
#include "global.h"

servo_ctrl_t g_servo_ctrl;

static volatile bool s_uart_rx_complete = false;
static volatile bool s_uart_tx_complete = false;
static uint8_t       s_uart_rx_buf[32];

void Servo_UartCallback(uart_callback_args_t* p_args)
{
	switch (p_args->event)
	{
		case UART_EVENT_RX_COMPLETE:
			s_uart_rx_complete = true;
			break;
		case UART_EVENT_TX_COMPLETE:
			s_uart_tx_complete = true;
			break;
		default:
			break;
	}
}

void Servo_Init(servo_ctrl_t* servo_ctrl)
{
	memset(&servo_ctrl->servo_ctrl_tx, 0, sizeof(servo_frame_t));
	memset(&servo_ctrl->servo_ctrl_rx, 0, sizeof(servo_frame_t));

	R_SCI_UART_CallbackSet(&g_serial_servo_uart_ctrl, Servo_UartCallback, NULL, NULL);
}

/* 静态缓冲区：UART 驱动异步发送时只保存指针，在 TXI 中断中按指针读取，
 * 必须保证数据在发送完成前有效，不能用栈上的局部变量 */
static uint8_t servo_frame_buf[25];

void Servo_CmdFrameSend(servo_frame_t* servo_frame)
{
	uint8_t idx = 0;
	uint8_t i;

	memset(servo_frame_buf, 0, sizeof(servo_frame_buf));

	servo_frame_buf[idx++] = servo_frame->header[0];
	servo_frame_buf[idx++] = servo_frame->header[1];
	servo_frame_buf[idx++] = servo_frame->length;
	servo_frame_buf[idx++] = servo_frame->cmd;
	servo_frame_buf[idx++] = servo_frame->count;

	servo_frame_buf[idx++] = (uint8_t)(servo_frame->duration);
	servo_frame_buf[idx++] = (uint8_t)(servo_frame->duration >> 8);

	for (i = 0; i < servo_frame->count; i++)
	{
		servo_frame_buf[idx++] = servo_frame->params[i].servo_id;
		servo_frame_buf[idx++] = (uint8_t)(servo_frame->params[i].position);
		servo_frame_buf[idx++] = (uint8_t)(servo_frame->params[i].position >> 8);
	}

	R_SCI_UART_Write(&g_serial_servo_uart_ctrl, servo_frame_buf, idx);
}

void Servo_PositionSet(servo_ctrl_t* servo_ctrl, uint8_t servo_id, uint16_t position, uint16_t duration)
{
	if(position > 1000)
	{
		position = 1000;
	}

	servo_ctrl->servo_ctrl_tx.header[0] = SERVO_FRAME_HEADER;
	servo_ctrl->servo_ctrl_tx.header[1] = SERVO_FRAME_HEADER;
	servo_ctrl->servo_ctrl_tx.length = 8;
	servo_ctrl->servo_ctrl_tx.cmd = CMD_SERVO_MOVE;
	servo_ctrl->servo_ctrl_tx.count = 1;
	servo_ctrl->servo_ctrl_tx.duration = duration;
	servo_ctrl->servo_ctrl_tx.params[0].servo_id = servo_id;
	servo_ctrl->servo_ctrl_tx.params[0].position = position;

	Servo_CmdFrameSend(&servo_ctrl->servo_ctrl_tx);
}

/**
 * @brief 控制多个舵机掉电卸力（CMD_MULT_SERVO_UNLOAD, 0x20）
 *
 * 帧格式: 0x55 0x55 | count+3 | 0x20 | count | ID1 | ID2 | ... | IDn
 */
void Servo_MultUnload(uint8_t count, const uint8_t* servo_ids)
{
	uint8_t idx = 0;

	if (count == 0 || count > SERVO_MAX_COUNT || servo_ids == NULL)
		return;

	memset(servo_frame_buf, 0, sizeof(servo_frame_buf));

	servo_frame_buf[idx++] = SERVO_FRAME_HEADER;
	servo_frame_buf[idx++] = SERVO_FRAME_HEADER;
	servo_frame_buf[idx++] = count + 3;
	servo_frame_buf[idx++] = CMD_MULT_SERVO_UNLOAD;
	servo_frame_buf[idx++] = count;

	for (uint8_t i = 0; i < count; i++)
	{
		servo_frame_buf[idx++] = servo_ids[i];
	}

	R_SCI_UART_Write(&g_serial_servo_uart_ctrl, servo_frame_buf, idx);
}

/**
 * @brief 读取多个舵机角度位置值（CMD_MULT_SERVO_POS_READ, 0x21）
 *
 * 发送帧: 0x55 0x55 | count+3 | 0x21 | count | ID1 ... IDn
 * 返回帧: 0x55 0x55 | count*3+3 | 0x21 | count | [ID, pos_lo, pos_hi] × n
 *
 * @param count     要读取的舵机数量
 * @param servo_ids 舵机 ID 数组
 * @param pos_out   输出：每个舵机的 ID 和位置值
 * @param timeout_ms 接收超时（毫秒）
 * @return true 成功读取, false 超时或校验失败
 */
bool Servo_MultPosRead(uint8_t count, const uint8_t* servo_ids,
                       servo_move_param_t* pos_out, uint32_t timeout_ms)
{
	uint8_t idx = 0;
	uint8_t expected_rx_len;

	if (count == 0 || count > SERVO_MAX_COUNT || servo_ids == NULL || pos_out == NULL)
		return false;

	/* header(2) + length(1) + cmd(1) + count(1) + count×3(data) */
	expected_rx_len = 5 + count * 3;

	s_uart_rx_complete = false;
	memset(s_uart_rx_buf, 0, sizeof(s_uart_rx_buf));

	R_SCI_UART_Read(&g_serial_servo_uart_ctrl, s_uart_rx_buf, expected_rx_len);

	memset(servo_frame_buf, 0, sizeof(servo_frame_buf));
	servo_frame_buf[idx++] = SERVO_FRAME_HEADER;
	servo_frame_buf[idx++] = SERVO_FRAME_HEADER;
	servo_frame_buf[idx++] = count + 3;
	servo_frame_buf[idx++] = CMD_MULT_SERVO_POS_READ;
	servo_frame_buf[idx++] = count;

	for (uint8_t i = 0; i < count; i++)
	{
		servo_frame_buf[idx++] = servo_ids[i];
	}

	R_SCI_UART_Write(&g_serial_servo_uart_ctrl, servo_frame_buf, idx);

	uint32_t elapsed = 0;
	while (!s_uart_rx_complete && elapsed < timeout_ms)
	{
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		elapsed++;
	}

	if (!s_uart_rx_complete)
		return false;

	if (s_uart_rx_buf[0] != SERVO_FRAME_HEADER || s_uart_rx_buf[1] != SERVO_FRAME_HEADER)
		return false;
	if (s_uart_rx_buf[3] != CMD_MULT_SERVO_POS_READ)
		return false;

	uint8_t rx_count = s_uart_rx_buf[4];
	if (rx_count != count)
		return false;

	for (uint8_t i = 0; i < rx_count; i++)
	{
		uint8_t base = 5 + i * 3;
		pos_out[i].servo_id = s_uart_rx_buf[base];
		pos_out[i].position = (uint16_t)s_uart_rx_buf[base + 1] |
		                      ((uint16_t)s_uart_rx_buf[base + 2] << 8);
	}

	return true;
}