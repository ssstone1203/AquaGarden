#include "hal_data.h"
#include "arm_control.h"
#include "serial_servo.h"

//#define SERVO_TEST
//#define ARM_SINGLE_JOINT_TEST
//#define ARM_ALL_JOINT_TEST
#define ARM_IKINE_TEST


// 机械臂控制对象（全局变量）
arm_control_t g_arm_ctrl;
kin_status_t g_kin_status;

void hal_entry(void)
{
    // 1. 打开串口（舵机通信）
    R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);
    
    // 2. 初始化机械臂控制
    ArmControl_Init(&g_arm_ctrl);
    
    // 等待系统稳定
    R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);
    
    // 3. 先复位到初始位置（坐标 15, 0, 2）
    ArmControl_Reset(&g_arm_ctrl, 2000);
    R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);

	while(1)
	{
		#ifdef ARM_SINGLE_JOINT_TEST
		// ========== 测试1：单个关节角度控制 ==========
		// 关节索引：0=基座旋转, 1=肩部俯仰, 2=肘部俯仰, 3=腕部俯仰

		// 测试基座旋转（关节0）：左右摆动
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 0, -30.0f, 1000);  // 左转30度
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 0, 30.0f, 1000);   // 右转30度
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 0, 0.0f, 1000);    // 回到中心
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);

		// 测试肩部俯仰（关节1）：前后摆动
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 1, 120.0f, 1000);  // 向前
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 1, 60.0f, 1000);   // 向后
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 1, 90.0f, 1000);   // 回到水平
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);

		// 测试肘部俯仰（关节2）
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 2, -45.0f, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 2, 45.0f, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 2, 0.0f, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);

		// 测试腕部俯仰（关节3）
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 3, -45.0f, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 3, 45.0f, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 3, 0.0f, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		#endif
		#ifdef ARM_ALL_JOINT_TEST
		// ========== 测试2：同时控制所有4个关节角度 ==========
		// 使用 ArmControl_AllJointsSet() API
		// 参数顺序：{基座, 肩部, 肘部, 腕部}

		float pose1[4] = {0.0f, 90.0f, 0.0f, 0.0f};    // 初始姿态
		float pose2[4] = {20.0f, 110.0f, -30.0f, 30.0f};  // 右侧伸展
		float pose3[4] = {-20.0f, 70.0f, 30.0f, -30.0f};  // 左侧伸展
		float pose4[4] = {0.0f, 60.0f, 45.0f, -45.0f};    // 向前下方

		ArmControl_AllJointsSet(&g_arm_ctrl, pose1, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		ArmControl_AllJointsSet(&g_arm_ctrl, pose2, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		ArmControl_AllJointsSet(&g_arm_ctrl, pose3, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		ArmControl_AllJointsSet(&g_arm_ctrl, pose4, 1000);
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		ArmControl_AllJointsSet(&g_arm_ctrl, pose1, 1000);  // 回到初始
		R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
		#endif
		#ifdef ARM_IKINE_TEST
		// 【逆运动学测试】

		// 测试1：正前方 (18, 0, 2)，与当前姿态相近
		g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 18.0f, 0.0f, 2.0f, 0.0f, 1000);
		if (g_kin_status == KIN_STATUS_OK)
		{
			// 成功：ID 2 应该转到 800
			R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
		}
		else
		{
			// 失败：观察 ID 2 位置（100/150/170/190/200）
			R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);
		}

		// 测试2：右侧位置 (17, 5, 3)
		g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 17.0f, 5.0f, 3.0f, 0.0f, 1000);
		if (g_kin_status == KIN_STATUS_OK)
		{
			R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
		}
		else
		{
			R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);
		}

		// 复位
		ArmControl_Reset(&g_arm_ctrl, 1000);
		R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
		#endif
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
