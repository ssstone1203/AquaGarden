#ifndef SERIAL_SERVO_H
#define SERIAL_SERVO_H

#include "string.h"
#include "hal_data.h"

#define SERVO_FRAME_HEADER         0x55
#define SERVO_MOVE_TIME_WRITE      1
#define SERVO_MOVE_TIME_READ       2
#define SERVO_MOVE_TIME_WAIT_WRITE 7
#define SERVO_MOVE_TIME_WAIT_READ  8
#define SERVO_MOVE_START           11
#define SERVO_MOVE_STOP            12

#define SERVO_ID_WRITE             13
#define SERVO_ID_READ              14
#define SERVO_ANGLE_OFFSET_ADJUST  17
#define SERVO_ANGLE_OFFSET_WRITE   18
#define SERVO_ANGLE_OFFSET_READ    19
#define SERVO_ANGLE_LIMIT_WRITE    20
#define SERVO_ANGLE_LIMIT_READ     21
#define SERVO_VIN_LIMIT_WRITE      22
#define SERVO_VIN_LIMIT_READ       23
#define SERVO_TEMP_MAX_LIMIT_WRITE 24
#define SERVO_TEMP_MAX_LIMIT_READ  25
#define SERVO_TEMP_READ            26
#define SERVO_VIN_READ             27
#define SERVO_POS_READ             28
#define SERVO_OR_MOTOR_MODE_WRITE  29
#define SERVO_OR_MOTOR_MODE_READ   30
#define SERVO_LOAD_OR_UNLOAD_WRITE 31
#define SERVO_LOAD_OR_UNLOAD_READ  32
#define SERVO_LED_CTRL_WRITE       33
#define SERVO_LED_CTRL_READ        34
#define SERVO_LED_ERROR_WRITE      35
#define SERVO_LED_ERROR_READ       36

#define CMD_SERVO_MOVE 0x03

#pragma pack(1)		//设置结构体成员按1字节对齐，也就是让这个结构体严格占13个字节

typedef struct  	//舵机指令包帧格式
{
	uint8_t servo_header[2];
	union
	{
		struct
		{
			uint8_t servo_id;
			uint8_t servo_length;
			uint8_t servo_cmd;
			uint8_t servo_args[8];
		}servo_element;
		uint8_t servo_data_raw[11];
	};
}servo_frame_t;

#pragma pack()		//恢复原来的对齐方式

typedef struct 
{
	servo_frame_t servo_ctrl_tx;
	servo_frame_t servo_ctrl_rx;
}servo_ctrl_t;

void Servo_Init(servo_ctrl_t* servo_ctrl);
void Servo_CmdFrameFill(servo_frame_t* servo_frame, uint8_t id, uint8_t length, uint8_t cmd);
uint8_t Servo_ChecksumCalc(servo_frame_t* servo_frame);
void Servo_CmdFrameSend(servo_frame_t* servo_frame);
void Servo_PositionSet(servo_ctrl_t* servo_ctrl, uint8_t servo_id, uint16_t position, uint16_t duration);

#endif
