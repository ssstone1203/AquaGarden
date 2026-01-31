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

void Servo_CmdFrameSend(servo_frame_t* servo_frame)
{
	uint8_t  len = servo_frame->servo_element.servo_length;
	uint32_t bytes = (uint32_t)(len + 3);
	uint8_t  frame_to_send[16];   /* 最大 3+13，用固定数组避免 VLA */

	frame_to_send[0] = servo_frame->servo_header[0];
	frame_to_send[1] = servo_frame->servo_header[1];
	frame_to_send[2] = servo_frame->servo_element.servo_id;
	frame_to_send[3] = servo_frame->servo_element.servo_length;
	frame_to_send[4] = servo_frame->servo_element.servo_cmd;
	for (uint8_t i = 0; i < len - 2; i++)
	{
		frame_to_send[5 + i] = servo_frame->servo_element.servo_args[i];
	}

	R_SCI_UART_Write(&g_serial_servo_uart_ctrl, frame_to_send, bytes);
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