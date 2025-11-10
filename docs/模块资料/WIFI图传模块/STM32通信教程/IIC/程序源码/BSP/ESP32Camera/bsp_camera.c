#include "bsp_camera.h"
 
#define delaytime 2 //根据自己的需求调整时间 Adjust the time according to your needs

uint8_t g_iicdata[8];
Model_Data g_model_data;
extern Model_state model;//引入外部变量 Introducing external variables

void IIC_Camera_Init(void)
{			
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); //使能PB端口时钟 Enable PB port clock
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10|GPIO_Pin_11;	//端口配置 Port Configuration
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;      //推挽输出 Push-pull output
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;     //50M 
  GPIO_Init(GPIOB, &GPIO_InitStructure);					      //根据设定参数初始化GPIOB  Initialize GPIOB according to the set parameters
}


//设置水平翻转 Set horizontal flip
void set_level_mode(uint8_t data)
{
	i2cWrite(ESP32CameraADDR,level_reg,1,&data);
}

//设置垂直翻转 Set Vertical Flip
void set_vertical_mode(uint8_t data)
{
	i2cWrite(ESP32CameraADDR,vertical_reg,1,&data);
}

//设置AI模式 Set AI mode
void set_ai_mode(uint8_t model)
{
	i2cWrite(ESP32CameraADDR,Model_reg,1,&model);
}

//虚拟按键 Virtual buttons
void i2c_set_Virtual_key(uint8_t key_data)
{ 
	for(uint8_t i = 0;i<5;i++)
	{
		i2cWrite(ESP32CameraADDR,Virtual_key_reg,1,&key_data);
		delay_ms(500);
	}
	
}



void read_camera_data(uint8_t *buf)
{
	//每个数据读两次，确保准确性 Read each data twice to ensure accuracy
	i2cRead(ESP32CameraADDR, Middle_X_Hreg, 1, &buf[0]);
	delay_time(delaytime);
	i2cRead(ESP32CameraADDR, Middle_X_Hreg, 1, &buf[0]);
	delay_time(delaytime);
	
	i2cRead(ESP32CameraADDR, Middle_X_Lreg, 1, &buf[1]);
	delay_time(delaytime);
	i2cRead(ESP32CameraADDR, Middle_X_Lreg, 1, &buf[1]);
	delay_time(delaytime);
	
	i2cRead(ESP32CameraADDR, Middle_Y_Hreg, 1, &buf[2]);
	delay_time(delaytime);
	i2cRead(ESP32CameraADDR, Middle_Y_Hreg, 1, &buf[2]);
	delay_time(delaytime);
	
	i2cRead(ESP32CameraADDR, Middle_Y_Lreg, 1, &buf[3]);
	delay_time(delaytime);
	i2cRead(ESP32CameraADDR, Middle_Y_Lreg, 1, &buf[3]);
	delay_time(delaytime);
	
	i2cRead(ESP32CameraADDR, AREA_Hreg, 1, &buf[4]);
	delay_time(delaytime);
	i2cRead(ESP32CameraADDR, AREA_Hreg, 1, &buf[4]);
	delay_time(delaytime);
	
	i2cRead(ESP32CameraADDR, AREA_Lreg, 1, &buf[5]);
	delay_time(delaytime);
	i2cRead(ESP32CameraADDR, AREA_Lreg, 1, &buf[5]);
	delay_time(delaytime);
	
	
		
	
	
	if(model == Face_identify)//只有此模式才去读 Only read in this mode
	{
		i2cRead(ESP32CameraADDR, Face_ID_Hreg, 1, &buf[6]);
		delay_time(delaytime);
		i2cRead(ESP32CameraADDR, Face_ID_Hreg, 1, &buf[6]);
		delay_time(delaytime);
		i2cRead(ESP32CameraADDR, Face_ID_Lreg, 1, &buf[7]);
		delay_time(delaytime);
		i2cRead(ESP32CameraADDR, Face_ID_Lreg, 1, &buf[7]);
		delay_time(delaytime);
	}
	

}


void deal_camera_data(void)
{
	read_camera_data(g_iicdata); //获取iic数据 Obtain IIC data
//	for(u8 i=0;i<8;i++)
//	{
//		printf("%d:%x\r\n",i,g_iicdata[i]);
//	}
	
	g_model_data.middle_x = g_iicdata[0]<<8 | g_iicdata[1];
	g_model_data.middle_y = g_iicdata[2]<<8 | g_iicdata[3];
	
	g_model_data.area = g_iicdata[4]<<8 | g_iicdata[5]; //面积 area
	
	if(model == Face_identify)//只有此识别模式才有此功能 Only this recognition mode has this function
	{
		g_model_data.id = g_iicdata[6]<<8 | g_iicdata[7];
	}
	else
	{
		g_model_data.id = 0;
	}
}



