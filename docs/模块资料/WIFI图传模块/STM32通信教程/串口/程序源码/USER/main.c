#include "stm32f10x.h"
#include "delay.h"
#include "bsp_usart.h"
#include "esp32_wifi.h"


extern u8 data_buff[50];
extern uint8_t cmd_flag; //发送命令的标志 0:没发送  1:以发送  2:进入网络透传  //Send command flag 0: Not sent 1: Sent 2: Enter network transparent transmission


#define AI_set_mode QR_AI  //设置AI模式 Setting AI Mode



void mode_chage()
{
  
  if(runmode == Nornal_AI) 
  {
    cmd_flag = 2;//进入透传模式 Enter transparent mode
  }
  else if(runmode == REFACE_AI )
  {
    cmd_flag = 3;//解析人脸识别数据   Parsing facial recognition data
  }
  else if(runmode == QR_AI )
  {
    cmd_flag = 4;//解析二维码数据   Parsing QR code data
  }
  else
  {
    cmd_flag = 5;//其它AI模式数据   Other AI mode data
   }
  
}


int main()
{
	
	SystemInit();
	
	delay_init();
	USART1_init(115200);//接PC的串口PA9(TX)  PA10(RX) Connect to PC's serial port PA9 (TX) PA10 (RX)
	USART2_init(115200);//接摄像头模块的 PA2(TX)  PA3(RX) Connect to the camera module's PA2(TX) and PA3(RX)
	
	FUN_delay_s(2);//等待摄像头正常开启 Wait for the camera to start normally
	
	printf("waiting for wifi start!\r\n");
	SET_ESP_WIFI_MODE();//模式配置 Mode Configuration
	SET_STA_WIFI();//配置STA  Configuring STA
	SET_AP_WIFI();//配置AP Configuring APs
	SET_ESP_AI_MODE(AI_set_mode);
	
	//Get_Version();
	Get_APIP();
	FUN_delay_s(2);
	IP_identify(data_buff);
	
	
	Get_STAIP();
	FUN_delay_s(2);
	IP_identify(data_buff);
	
	mode_chage();//根据模式选择解析数据办法 Choose the method to parse data according to the mode
  delay_ms(100);

	while(1)
	{
		
	}
	
}

