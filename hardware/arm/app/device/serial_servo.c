#include "serial_servo.h"
#include "global.h"

void Servo_Init(servo_ctrl_t* servo_ctrl)
{
	// 打开串口（用于舵机通信）
//	R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);

	memset(&servo_ctrl->servo_ctrl_tx, 0, sizeof(servo_frame_t));
	memset(&servo_ctrl->servo_ctrl_rx, 0, sizeof(servo_frame_t));
}

/* 静态缓冲区：UART 驱动异步发送时只保存指针，在 TXI 中断中按指针读取，
 * 必须保证数据在发送完成前有效，不能用栈上的局部变量 */
static uint8_t servo_frame_buf[25];  // 最大帧：2+1+1+1+2+6×3 = 25字节

void Servo_CmdFrameSend(servo_frame_t* servo_frame)
{
	uint8_t idx = 0;
	uint8_t i;

	// 清零静态缓冲区，避免残留数据干扰
	memset(servo_frame_buf, 0, sizeof(servo_frame_buf));

	// 帧头（2字节）
	servo_frame_buf[idx++] = servo_frame->header[0];  // 0x55
	servo_frame_buf[idx++] = servo_frame->header[1];  // 0x55

	// 长度（1字节）：个数×3 + 5
	servo_frame_buf[idx++] = servo_frame->length;

	// 指令（1字节）：0x03
	servo_frame_buf[idx++] = servo_frame->cmd;

	// 控制舵机的个数（1字节）
	servo_frame_buf[idx++] = servo_frame->count;

	// 时间（2字节）：低八位在前，高八位在后
	servo_frame_buf[idx++] = (uint8_t)(servo_frame->duration);
	servo_frame_buf[idx++] = (uint8_t)(servo_frame->duration >> 8);

	// 舵机参数（每个舵机3字节：ID + 位置低 + 位置高）
	for (i = 0; i < servo_frame->count; i++)
	{
		servo_frame_buf[idx++] = servo_frame->params[i].servo_id;
		servo_frame_buf[idx++] = (uint8_t)(servo_frame->params[i].position);
		servo_frame_buf[idx++] = (uint8_t)(servo_frame->params[i].position >> 8);
	}

	// 发送数据帧（新协议无校验和）
	R_SCI_UART_Write(&g_serial_servo_uart_ctrl, servo_frame_buf, idx);
}

void Servo_PositionSet(servo_ctrl_t* servo_ctrl, uint8_t servo_id, uint16_t position, uint16_t duration)
{
	// 限制位置值范围
	if(position > 1000)
	{
		position = 1000;
	}

	// 填充帧结构（新协议 CMD_SERVO_MOVE，指令0x03）
	servo_ctrl->servo_ctrl_tx.header[0] = SERVO_FRAME_HEADER;  // 帧头1：0x55
	servo_ctrl->servo_ctrl_tx.header[1] = SERVO_FRAME_HEADER;  // 帧头2：0x55
	servo_ctrl->servo_ctrl_tx.length = 8;                       // 长度：1×3+5 = 8
	servo_ctrl->servo_ctrl_tx.cmd = CMD_SERVO_MOVE;               // 指令：0x03
	servo_ctrl->servo_ctrl_tx.count = 1;                        // 控制1个舵机
	servo_ctrl->servo_ctrl_tx.duration = duration;               // 运动时间
	servo_ctrl->servo_ctrl_tx.params[0].servo_id = servo_id;     // 舵机ID
	servo_ctrl->servo_ctrl_tx.params[0].position = position;     // 目标位置

	// 通过帧发送函数发送数据
	Servo_CmdFrameSend(&servo_ctrl->servo_ctrl_tx);
}