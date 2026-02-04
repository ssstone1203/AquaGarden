#include "serial_servo.h"
#include "global.h"

void Servo_Init(servo_ctrl_t* servo_ctrl)
{
	memset(&servo_ctrl->servo_ctrl_tx, 0, sizeof(servo_frame_t));
	memset(&servo_ctrl->servo_ctrl_rx, 0, sizeof(servo_frame_t));
}
/**
  * @brief Universal format for filling command frames
  * @param servo_frame structure ptr
  * @param servo_id
  * @param data length
  * @param servo_cmd
  */
void Servo_CmdFrameFill(servo_frame_t* servo_frame, uint8_t id, uint8_t length, uint8_t cmd)
{
	servo_frame->servo_header[0] = SERVO_FRAME_HEADER;
	servo_frame->servo_header[1] = SERVO_FRAME_HEADER;
	servo_frame->servo_element.servo_id = id;
	servo_frame->servo_element.servo_length = length;
	servo_frame->servo_element.servo_cmd = cmd;
}

/**
  * @brief 计算舵机帧校验和
  * @param servo_frame 帧结构指针
  * @return 校验和（~sum 低8位）
  */
uint8_t Servo_ChecksumCalc(servo_frame_t* servo_frame)
{
	uint16_t sum = 0;
	uint8_t  len = servo_frame->servo_element.servo_length;

	sum += servo_frame->servo_element.servo_id;
	sum += servo_frame->servo_element.servo_length;
	sum += servo_frame->servo_element.servo_cmd;
	for (uint8_t i = 0; i < len - 3; i++)
	{
		sum += servo_frame->servo_element.servo_args[i];
	}
	return (uint8_t)(~sum);
}

/* 静态缓冲区：UART 驱动异步发送时只保存指针，在 TXI 中断中按指针读取，
 * 必须保证数据在发送完成前有效，不能用栈上的局部变量 */
static uint8_t servo_frame_buf[16];

void Servo_CmdFrameSend(servo_frame_t* servo_frame)
{
	uint8_t  len = servo_frame->servo_element.servo_length;
	uint32_t bytes = (uint32_t)(len + 3);

	servo_frame_buf[0] = servo_frame->servo_header[0];
	servo_frame_buf[1] = servo_frame->servo_header[1];
	servo_frame_buf[2] = servo_frame->servo_element.servo_id;
	servo_frame_buf[3] = servo_frame->servo_element.servo_length;
	servo_frame_buf[4] = servo_frame->servo_element.servo_cmd;
	for (uint8_t i = 0; i < len - 2; i++)
	{
		servo_frame_buf[5 + i] = servo_frame->servo_element.servo_args[i];
	}

	R_SCI_UART_Write(&g_serial_servo_uart_ctrl, servo_frame_buf, bytes);
}

void Servo_PositionSet(servo_ctrl_t* servo_ctrl, uint8_t servo_id, uint16_t position, uint16_t duration)
{
	if(position > 1000)
	{
		position = 1000;
	}
	Servo_CmdFrameFill(&servo_ctrl->servo_ctrl_tx, servo_id, 7, SERVO_MOVE_TIME_WRITE);
	
	servo_ctrl->servo_ctrl_tx.servo_element.servo_args[0] = (uint8_t)position;
	servo_ctrl->servo_ctrl_tx.servo_element.servo_args[1] = position >> 8;
	servo_ctrl->servo_ctrl_tx.servo_element.servo_args[2] = (uint8_t)duration;
	servo_ctrl->servo_ctrl_tx.servo_element.servo_args[3] = duration >> 8;
	servo_ctrl->servo_ctrl_tx.servo_element.servo_args[4] = Servo_ChecksumCalc(&servo_ctrl->servo_ctrl_tx);
	
	Servo_CmdFrameSend(&servo_ctrl->servo_ctrl_tx);
}