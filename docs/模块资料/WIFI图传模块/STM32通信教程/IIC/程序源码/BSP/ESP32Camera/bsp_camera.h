#ifndef __BSP_CAMERA_H_
#define __BSP_CAMERA_H_

#include "AllHeader.h"

#define ESP32CameraADDR    0x33

enum Camera_REG
{
	level_reg =0x01, //摄像头水平寄存器 Camera horizontal register
	vertical_reg = 0x02,//摄像头垂直寄存器 Camera vertical register
	Model_reg = 0x03, //模式选择寄存器 Mode selection register
	Reset_camera_reg = 0x04,//摄像头重启寄存器 Camera restart register
	
	Virtual_key_reg = 0x08, //虚拟按键寄存器 Virtual key register
	
	//图像识别结果坐标寄存器 Image recognition result coordinate register
	Middle_X_Hreg = 0x28,
	Middle_X_Lreg,
	Middle_Y_Hreg=0x2A,
	Middle_Y_Lreg,
	Face_ID_Hreg,
	Face_ID_Lreg,
	AREA_Hreg = 0x2E, //识别框的第一个面积 The first area of ??the identification box
	AREA_Lreg = 0x2F //识别框的第一个面积 The first area of ??the identification box
};


//模式选择 Model Select
typedef enum Model_state_t
{
	Normal=0x00, 
	Cat_Dog_Model,
	Face_Detection,
	Color_identify,
	Face_identify,
	QR_code,
	MAX_ERROR
}Model_state;

//虚拟按键 Virtual buttons
enum IIC_KEY
{
	KEY_MENU = 0x01,
	KEY_PLAY,
	KEY_UPUP,
	KEY_DOWN,
	KEY_ERROR	
	
};


typedef struct Model_Data_t
{
	uint16_t middle_x;
	uint16_t middle_y;
	int16_t  id;
	uint16_t  area;
}Model_Data;


extern Model_Data g_model_data;//给外部使用 For external use


void IIC_Camera_Init(void);
void set_ai_mode(uint8_t model);
void set_level_mode(uint8_t data);
void set_vertical_mode(uint8_t data);
void i2c_set_Virtual_key(uint8_t key_data);
void read_camera_data(uint8_t *buf);
void deal_camera_data(void);


#endif
