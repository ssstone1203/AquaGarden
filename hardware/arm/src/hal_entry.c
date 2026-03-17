#include "hal_data.h"
#include "arm_control.h"
#include "serial_servo.h"

//#define SERVO_TEST
//#define ARM_SINGLE_JOINT_TEST
//#define ARM_ALL_JOINT_TEST
//#define ARM_IKINE_TEST

//servo_ctrl_t g_servo_ctrl;
// 机械臂控制对象（全局变量）
//arm_control_t g_arm_ctrl;
uint8_t g_status = 0;

void hal_entry(void)
{
//	R_IOPORT_Open(&g_ioport_ctrl, &g_bsp_pin_cfg);
    // 1. 打开串口（舵机通信）
    R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);
	ArmControl_Init();
    
    // 2. 初始化机械臂控制
//	R_IOPORT_PinWrite(&g_ioport_ctrl, BUS_EN, BSP_IO_LEVEL_LOW);
    
    
    // 等待系统稳定
    R_BSP_SoftwareDelay(3000, BSP_DELAY_UNITS_MILLISECONDS);

	while(1)
	{
//		g_status = ArmControl_CoordinateSet(15, 0, 2, 0, -90, 90, 1000);  // 复位位置
//		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
//		g_status = ArmControl_CoordinateSet(18,3,5,0,-90,90,1000);
//		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
		g_status = ArmControl_CoordinateSet(15,0,20,0,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
		g_status = ArmControl_CoordinateSet(3,-12,20,0,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
		g_status = ArmControl_CoordinateSet(23,0,20,0,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
		g_status = ArmControl_CoordinateSet(30,0,13,0,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
		g_status = ArmControl_CoordinateSet(30,0,13,15,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
		g_status = ArmControl_CoordinateSet(30,0,13,0,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
		g_status = ArmControl_CoordinateSet(33,0,13,0,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
		
    }
	
#if BSP_TZ_SECURE_BUILD
    /* Enter non-secure code */
    R_BSP_NonSecureEnter();
#endif
}

#if BSP_TZ_SECURE_BUILD

FSP_CPP_HEADER
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();

/* Trustzone Secure Projects require at least one nonsecure callable function in order to build (Remove this if it is not required to build). */
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ()
{

}
FSP_CPP_FOOTER

#endif
