#include "hal_data.h"
#include "arm_control.h"
#include "serial_servo.h"
#include "comm.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

uint8_t g_status = 0;

/* ── 手眼标定位姿表（与 tools/algorithm/step2_collect_data.py 中 CALIB_POSES 完全一致）── */
typedef struct { float x; float y; float z; float pitch; } CalibPose_t;

#define CALIB_POSES_COUNT  21
static const CalibPose_t s_calib_poses[CALIB_POSES_COUNT] = {
    { 16.0f,  0.0f, -3.2f, -76.1f },  /* 01 中心（ArUco 在画面正中心） */
    { 14.0f,  0.0f, -3.2f, -76.1f },  /* 02 x-2  */
    { 18.0f,  0.0f, -3.2f, -76.1f },  /* 03 x+2  */
    { 16.0f, -3.0f, -3.2f, -76.1f },  /* 04 y-3  */
    { 16.0f,  3.0f, -3.2f, -76.1f },  /* 05 y+3  */
    { 16.0f,  0.0f, -1.0f, -76.1f },  /* 06 z+2  */
    { 16.0f,  0.0f, -5.5f, -76.1f },  /* 07 z-2  */
    { 16.0f,  0.0f, -3.2f, -71.0f },  /* 08 pitch+5 */
    { 16.0f,  0.0f, -3.2f, -81.0f },  /* 09 pitch-5 */
    { 14.0f, -3.0f, -3.2f, -76.1f },  /* 10 x-2 y-3 */
    { 14.0f,  3.0f, -3.2f, -76.1f },  /* 11 x-2 y+3 */
    { 18.0f, -3.0f, -3.2f, -76.1f },  /* 12 x+2 y-3 */
    { 18.0f,  3.0f, -3.2f, -76.1f },  /* 13 x+2 y+3 */
    { 14.0f,  0.0f, -1.0f, -71.0f },  /* 14 x-2 z+ pitch+ */
    { 18.0f,  0.0f, -5.5f, -81.0f },  /* 15 x+2 z- pitch- */
    { 16.0f, -3.0f, -1.0f, -71.0f },  /* 16 y-3 z+ pitch+ */
    { 16.0f,  3.0f, -5.5f, -81.0f },  /* 17 y+3 z- pitch- */
    { 14.0f, -3.0f, -1.0f, -71.0f },  /* 18 x-2 y-3 z+ pitch+ */
    { 18.0f,  3.0f, -1.0f, -71.0f },  /* 19 x+2 y+3 z+ pitch+ */
    { 14.0f,  3.0f, -5.5f, -81.0f },  /* 20 x-2 y+3 z- pitch- */
    { 18.0f, -3.0f, -5.5f, -81.0f },  /* 21 x+2 y-3 z- pitch- */
};

/* 当前标定位置下标（CALIB_RESET 后从 1 开始，因 RESET 已执行位置 0） */
static uint8_t s_calib_idx = 0;

/**
 * @brief 驱动机械臂到指定标定位置，等待完成后读取实际关节角并回传。
 *
 * 回传格式（含 '\n'）：
 *   成功且读位置成功：  "OK j0.dd,j1.dd,j2.dd,j3.dd\n"  （实际关节角，单位度）
 *   成功但读位置失败：  "OK j0.dd,j1.dd,j2.dd,j3.dd\n"  （回退到运动学命令角）
 *   运动学无解：        "ERR\n"
 */
static void s_calib_move_and_report(const CalibPose_t *pose)
{
    char     reply[72];
    uint16_t positions[ARM_MAX_SERVOS_NUM] = {0};
    float    joints[4]                     = {0};

    /* 1. 计算 IK 并驱动舵机 */
    g_status = ArmControl_CoordinateSet(
        pose->x, pose->y, pose->z, pose->pitch, -90.0f, 90.0f, 1000);

    if (!g_status)
    {
        Comm_SendStr("ERR\n");
        return;
    }

    /* 2. 等待运动完成（duration=1000ms，再留 500ms 余量） */
    R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);

    /* 3. 读取舵机实际位置 → 关节角
     *    成功：使用真实反馈；失败：回退到运动学命令角（g_kin_obj） */
    if (ArmControl_ReadAllPositions(positions, 500))
    {
        ArmControl_PositionsToJointAngles(positions, joints);
    }
    else
    {
        /* 读取失败，使用 CoordinateSet 内部存储的运动学解作为后备 */
        joints[0] = g_kin_obj.joint[0].theta;
        joints[1] = g_kin_obj.joint[1].theta;
        joints[2] = g_kin_obj.joint[2].theta;
        joints[3] = g_kin_obj.joint[3].theta;
    }

    /* 4. 格式化回传：OK j0.dd,j1.dd,j2.dd,j3.dd\n */
    snprintf(reply, sizeof(reply),
             "OK %.2f,%.2f,%.2f,%.2f\n",
             joints[0], joints[1], joints[2], joints[3]);
    Comm_SendStr(reply);
}

void hal_entry(void)
{
    // 1. 打开串口（舵机通信）
    R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);

    // 2. 初始化机械臂控制
    ArmControl_Init();

#ifdef PC_CONTROL
    // 3. 初始化 PC 通信串口（g_com_uart）
    Comm_Init();
#endif

    // 等待系统稳定
    R_BSP_SoftwareDelay(3000, BSP_DELAY_UNITS_MILLISECONDS);

	/* ===== 示教模式：卸力后用手掰动机械臂，实时观察末端坐标 =====
	 * 在 Keil 调试器的 Watch 窗口添加 g_teach_data 即可查看实时 XYZ。
	 * 每 100ms 读取一次舵机位置并计算正向运动学。
	 */
//	ArmControl_TeachMode(100);

#ifdef PC_CONTROL
	/*
	 * PC 控制模式：循环读取上位机发来的文本行并执行
	 *
	 * 支持的指令（均以 \n 或 \r\n 结尾）：
	 *
	 *   PING
	 *       → 返回 "PONG\n"，用于连接测试
	 *
	 *   MOVE x y z pitch min_pitch max_pitch duration
	 *       → 调用 ArmControl_CoordinateSet，返回 "OK\n" 或 "ERR\n"
	 *       → 示例：MOVE 16 0 -3.2 -76.1 -90 90 1000
	 *
	 *   RESET
	 *       → 机械臂复位到出厂初始位置，返回 "OK\n"
	 *
	 *   CALIB_RESET
	 *       → 手眼标定：重置序号，机械臂移动到位置 01（中心），返回 "OK\n" 或 "ERR\n"
	 *       → 上位机在启动 step2_collect_data.py 时自动发送
	 *
	 *   1
	 *       → 手眼标定：机械臂移动到下一个标定位置，返回 "OK\n" 或 "ERR\n"
	 *       → 全部 21 个位置执行完后返回 "DONE\n"
	 *       → 上位机每次按空格采集完数据后自动发送
	 */
	char line[COMM_RX_BUF_SIZE];

	while (1)
	{
		if (!Comm_HasLine())
			continue;

		Comm_ReadLine(line, sizeof(line));

		if (strcmp(line, "PING") == 0)
		{
			Comm_SendStr("PONG\n");
		}
		else if (strcmp(line, "RESET") == 0)
		{
			ArmControl_Reset(&g_arm_ctrl, 1000);
			Comm_SendStr("OK\n");
		}
		else if (strncmp(line, "MOVE ", 5) == 0)
		{
			char *p   = line + 5;
			char *end = p;

			float    x        = strtof(p, &end); p = end + 1;
			float    y        = strtof(p, &end); p = end + 1;
			float    z        = strtof(p, &end); p = end + 1;
			float    pitch    = strtof(p, &end); p = end + 1;
			float    min_p    = strtof(p, &end); p = end + 1;
			float    max_p    = strtof(p, &end); p = end + 1;
			uint16_t duration = (uint16_t)strtol(p, NULL, 10);

			g_status = ArmControl_CoordinateSet(x, y, z, pitch, min_p, max_p, duration);
			Comm_SendStr(g_status == 0 ? "OK\n" : "ERR\n");
		}
		/*
		 * 手眼标定专用指令
		 *
		 *   CALIB_RESET
		 *       → 重置位置序号，机械臂移动到第 1 个标定位置（中心），返回 "OK\n"
		 *
		 *   1
		 *       → 机械臂移动到下一个标定位置，返回 "OK\n"
		 *       → 所有位置已完成时返回 "DONE\n"
		 */
		else if (strcmp(line, "CALIB_RESET") == 0)
		{
			/* 重置序号，移动到位置 01（中心），回传实际关节角 */
			s_calib_idx = 0;
			s_calib_move_and_report(&s_calib_poses[s_calib_idx++]);
		}
		else if (strcmp(line, "1") == 0)
		{
			if (s_calib_idx >= CALIB_POSES_COUNT)
			{
				Comm_SendStr("DONE\n");
			}
			else
			{
				/* 移动到下一个标定位置，回传实际关节角 */
				s_calib_move_and_report(&s_calib_poses[s_calib_idx++]);
			}
		}
	}

#else
	/* ===== 常规运动模式（与示教模式互斥，需注释掉上面的 TeachMode）=====*/
	while(1)
	{
		g_status = ArmControl_CoordinateSet(16,0,(float)-3.2,(float)-76.1,-90,90,1000);
		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
//		g_status = ArmControl_CoordinateSet(15,-6,20,0,-90,90,1000);
//		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
//		g_status = ArmControl_CoordinateSet(30,0,13,15,-90,90,1000);
//		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
//		g_status = ArmControl_CoordinateSet(30,0,13,30,-90,90,1000);
//		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
    }
#endif
	
	
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
