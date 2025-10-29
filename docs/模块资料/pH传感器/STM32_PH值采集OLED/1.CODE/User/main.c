/********************(C) COPRIGHT 2019 Crownto electronic **************************
 * 文件名  ：main.c*/
 //---------------------------------------------------
 //OLED  >> STM32F103C8T6芯片的硬件I2C
 // SCL  -- PB6
 // SDA  -- PB7
 // VCC  -- 3.3V
 // GND  -- GND
 //---------------------------------------------------
 //传感器模块的  PO_ADC >> STM32F103C8T6开发板的--PA0
 //传感器模块的  v+     >> -----------------------5V 
 //传感器模块的  GND		>> -----------------------GND
//**********************************************************************************
#include "stm32f10x.h"
#include <string.h>
#include "delay.h"
#include "bsp_SysTick.h"
#include "math.h"
#include "bsp_adc.h"
#include "ds18b20.h"

#include "OLED_I2C.h"
#include "timer.h"
#include "bsp_usart1.h"
#include "bsp_usart2.h"


volatile uint32_t time = 0; // ms 计时变量 

////定义变量


 
GPIO_InitTypeDef  GPIO_InitStructure; 
unsigned char AD_CHANNEL=0;


float PH=0.0,PH_voltage;
float PH_value=0.0,PH_10value=0.0,voltage_value;
float compensationCoefficient=1.0;//温度校准系数
float compensationVolatge;
float kValue=1.67;
float TEMP_Value=0.0;







char  TEMP_Buff[5];   //温度存放数组
char  PH_Buff[6];   //PH存放数组


extern  u8 SET_Flag,SET_Count; //设置标志位
extern u8 CLC_Flag; //清屏标志位
extern u8 Warning_flag;
u8 Warning_count=0;




// ADC1转换的电压值通过MDA方式传到SRAM
extern __IO uint16_t ADC_ConvertedValue[4];

// 用于保存转换计算后的电压值 	 
float ADC_ConvertedValueLocal[4]; 


 
void GPIO_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/* Enable the GPIO  Clock */					 		
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC| RCC_APB2Periph_AFIO,ENABLE);
	
	//GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable,ENABLE);		//屏蔽所有作为JTAG口的GPIO口
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);		//屏蔽PB口上IO口JTAG功能


}

/**************温度采集函数***************/
void TEMP_Value_Conversion()
{
	
	  TEMP_Value=DS18B20_Get_Temp();
	
	  TEMP_Buff[0]=(int)(TEMP_Value)%1000/100+'0';	
	  TEMP_Buff[1]=(int)(TEMP_Value)%100/10+'0';
	  TEMP_Buff[2]='.';
	  TEMP_Buff[3]=(int)(TEMP_Value)%10+'0';
}




/**************PH值采集函数***************/
void PH_Value_Conversion()
{
	  int i;
		for(i=0;i<52;i++)//采集52次取平均值
		{
				ADC_ConvertedValueLocal[0]=(float)ADC_ConvertedValue[0]/4096*3.3; //AD转换
				PH_value=-5.6342*ADC_ConvertedValueLocal[0]+16.413;
				if((PH_value<=0)){PH_value=0;}
				if((PH_value>14.0)){PH_value=14.0;}
				PH_10value = PH_10value+PH_value;
		}
	PH_value = PH_10value/52;
  PH_10value = 0.0;		
	/*显示EC*/
	PH_Buff[0]=(int)(PH_value*100)/1000+'0';
	PH_Buff[1]=(int)(PH_value*100)%1000/100+'0';
	PH_Buff[2]='.';
	PH_Buff[3]=(int)(PH_value*100)%100/10+'0';	
	PH_Buff[4]=(int)(PH_value*100)%10+'0';

}
///*************数据显示函数***************/
void Display_Data()
{
	OLED_ShowStr(24,2,TEMP_Buff,2);//测试6*8字符
	OLED_ShowStr(36,4,PH_Buff,2);//测试6*8字符
}

/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{	 
	
		GPIO_Configuration(); //IO口配置
    /* 配置USART1 */
    USART1_Config();
	
    /* 配置USART2 */
    USART2_Config();
    
    /* 初始化系统定时器 */
    SysTick_Init();
    
	
	  TIM3_Init();                 //初始化配置TIM
	
		I2C_Configuration(); //I2C初始化
	  OLED_Init();  //OLED初始化
	  ADCx_Init();		// ADC 初始化

	
		DS18B20_Init();//DS18B20初始化 	
		
    OLED_CLS();//清屏
		

		OLED_ShowStr(0,2,"T:",2);
		OLED_ShowStr(0,4,"PH:",2);
		OLED_ShowStr(56,2,"C",2);


  while(1)
	{	

			TEMP_Value_Conversion();//温度数据转换
			PH_Value_Conversion();//PH
			Display_Data();	//显示数据
			delay_ms(500);
		
	}	
}


/*********************************************END OF FILE**********************/
