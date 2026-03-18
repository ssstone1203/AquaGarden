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

#define CMD_SERVO_MOVE          0x03
#define CMD_MULT_SERVO_UNLOAD   0x14  // decimal 20, protocol doc "指令: 20"
#define CMD_MULT_SERVO_POS_READ 0x15  // decimal 21, protocol doc "指令: 21"

#define SERVO_MAX_COUNT         6

#pragma pack(1)		//设置结构体成员按1字节对齐

/**
 * @brief 新协议舵机控制参数（CMD_SERVO_MOVE，指令0x03）
 * 支持一帧控制多个舵机，每个舵机占用3字节（ID+位置低+位置高）
 */
typedef struct
{
	uint8_t servo_id;           // 舵机ID号
	uint16_t position;          // 目标位置（0-1000）
} servo_move_param_t;

/**
 * @brief 新协议帧结构（CMD_SERVO_MOVE，指令0x03）
 * 帧头(2B) + 长度(1B) + 指令(1B) + 个数(1B) + 时间(2B) + 参数(N×3B)
 */
typedef struct
{
	uint8_t header[2];           // 帧头：0x55 0x55
	uint8_t length;              // 数据长度：个数×3 + 5
	uint8_t cmd;                 // 指令：0x03（CMD_SERVO_MOVE）
	uint8_t count;               // 控制舵机的个数
	uint16_t duration;           // 运动时间（毫秒）
	servo_move_param_t params[6]; // 最多支持6个舵机参数
} servo_frame_t;

#pragma pack()		//恢复原来的对齐方式

typedef struct
{
	servo_frame_t servo_ctrl_tx;
	servo_frame_t servo_ctrl_rx;
}servo_ctrl_t;

void Servo_Init(servo_ctrl_t* servo_ctrl);
void Servo_CmdFrameSend(servo_frame_t* servo_frame);
void Servo_PositionSet(servo_ctrl_t* servo_ctrl, uint8_t servo_id, uint16_t position, uint16_t duration);

void Servo_MultUnload(uint8_t count, const uint8_t* servo_ids);
bool Servo_MultPosRead(uint8_t count, const uint8_t* servo_ids,
                       servo_move_param_t* pos_out, uint32_t timeout_ms);

void Servo_UartCallback(uart_callback_args_t* p_args);

extern servo_ctrl_t g_servo_ctrl;

#endif
