/**
* @par Copyright (C): 2018-2028, Shenzhen Yahboom Tech
* @file         // main.c
* @author       // lly
* @version      // V1.0
* @date         // 240628
* @brief        // 程序入口
* @details      
* @par History  // 修改历史记录列表，每条修改记录应包括修改日期、修改者及
*               // 修改内容简述  
*/
/**
* @par Copyright (C): 2018-2028, Shenzhen Yahboom Tech
* @file // main.c
* @author // lly
* @version // V1.0
* @date // 240628
* @brief // Program entry
* @details
* @par History // Modification history list, each modification record should include modification date, modifier and
* // Brief description of modification content
*/
#include "AllHeader.h"


Model_state model = Cat_Dog_Model;


int main(void)
{	
	bsp_init();
	
	TIM3_Init();
	
	//等待摄像头正常启动 Wait for the camera to start normally
	delay_ms(1000);
	delay_ms(1000);

	
	
	IIC_Camera_Init();//摄像头i2c通信初始化 Camera i2c communication initialization
	
	set_ai_mode(model); //设置AI模式 Setting AI Mode
	delay_ms(500); //如果不是同一个模式的话，可能要加长等待的时间 If it is not the same mode, you may have to wait longer.
	 

//	delay_ms(500);
//	set_level_mode(0);
//	delay_ms(500);
//	set_vertical_mode(1);//

//	i2c_set_Virtual_key(KEY_MENU);

	printf("pelase wait !!!\r\n");
	
	while(1)
	{

		if(model != Normal)//只要不是正常模式 As long as it's not normal mode
		{
			deal_camera_data();
			printf("mx:%d\t my:%d\t area:%d\t id:%d\t \r\n",g_model_data.middle_x,g_model_data.middle_y,g_model_data.area,g_model_data.id);
		}

	}
	
}



