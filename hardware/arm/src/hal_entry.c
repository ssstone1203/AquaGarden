#include "hal_data.h"
#include "arm_control.h"
#include "serial_servo.h"

uint8_t g_status = 0;

void hal_entry(void)
{
    // 1. 打开串口（舵机通信）
    R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);
	
	// 2. 初始化机械臂控制
	ArmControl_Init();
    
    // 等待系统稳定
    R_BSP_SoftwareDelay(3000, BSP_DELAY_UNITS_MILLISECONDS);

	/* ===== 示教模式：卸力后用手掰动机械臂，实时观察末端坐标 =====
	 * 在 Keil 调试器的 Watch 窗口添加 g_teach_data 即可查看实时 XYZ。
	 * 每 100ms 读取一次舵机位置并计算正向运动学。
	 */
//	ArmControl_TeachMode(100);

	/* ===== 常规运动模式（与示教模式互斥，需注释掉上面的 TeachMode）=====*/
	while(1)
	{
		g_status = ArmControl_CoordinateSet(15,-6,20,0,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
//		g_status = ArmControl_CoordinateSet(30,0,13,15,-90,90,1000);
//		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
//		g_status = ArmControl_CoordinateSet(30,0,13,30,-90,90,1000);
//		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
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
