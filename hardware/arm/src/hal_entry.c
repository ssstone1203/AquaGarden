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

#define CALIB_POSES_COUNT  23
static const CalibPose_t s_calib_poses[CALIB_POSES_COUNT] = {
    { 13.0f,  0.0f, -3.0f, -68.0f },  /* 01 */
    { 13.0f,  0.0f, -6.5f, -83.0f },  /* 02 */
    { 16.0f,  0.0f, -2.0f, -62.0f },  /* 03 */
    { 16.0f,  0.0f, -4.5f, -76.0f },  /* 04 */
    { 16.0f,  0.0f, -7.5f, -88.0f },  /* 05 */
    { 19.0f,  0.0f, -3.0f, -64.0f },  /* 06 */
    { 19.0f,  0.0f, -6.0f, -78.0f },  /* 07 */
    { 13.0f, -5.0f, -3.5f, -72.0f },  /* 08 */
    { 13.0f, -5.0f, -6.5f, -83.0f },  /* 09 */
    { 16.0f, -6.0f, -3.0f, -68.0f },  /* 10 */
    { 16.0f, -6.0f, -6.0f, -80.0f },  /* 11 */
    { 16.0f, -8.0f, -4.0f, -76.0f },  /* 12 */
    { 13.0f,  5.0f, -3.5f, -72.0f },  /* 13 */
    { 13.0f,  5.0f, -6.5f, -83.0f },  /* 14 */
    { 16.0f,  6.0f, -3.0f, -68.0f },  /* 15 */
    { 16.0f,  6.0f, -6.0f, -80.0f },  /* 16 */
    { 16.0f,  8.0f, -4.0f, -76.0f },  /* 17 */
    { 15.0f,  0.0f, -8.5f, -88.0f },  /* 18 */
    { 18.0f,  0.0f, -1.5f, -58.0f },  /* 19 */
    { 18.0f,  0.0f, -9.0f, -88.0f },  /* 20 */
    { 12.0f,  0.0f, -3.5f, -78.0f },  /* 21 */
    { 12.0f, -4.0f, -5.0f, -80.0f },  /* 22 */
    { 12.0f,  4.0f, -5.0f, -80.0f },  /* 23 */
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
	 *       → 逆运动学解算并驱动舵机；等待 duration+300ms 运动完成后返回 "OK\n"
	 *       → 逆运动学无解时立即返回 "ERR\n"
	 *       → 示例：MOVE 16 0 -3.2 -76.1 -90 90 1000
	 *
	 *   RESET
	 *       → 机械臂复位到出厂初始位置，返回 "OK\n"（立即回复，运动 1000ms）
	 *
	 *   GRIPPER_OPEN [dur_ms]
	 *       → 打开夹爪，等待 dur_ms+100ms 后返回 "OK\n"（默认 dur=500）
	 *
	 *   GRIPPER_CLOSE [dur_ms]
	 *       → 关闭夹爪，等待 dur_ms+100ms 后返回 "OK\n"（默认 dur=500）
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
		/*
		 *   UNLOAD
		 *       → 所有舵机卸力（可用手自由掰动机械臂），返回 "OK\n"
		 *       → 再次发 RESET / MOVE 可重新上力
		 *
		 *   READ_POS
		 *       → 读取舵机当前位置，计算正向运动学后返回 "x,y,z,pitch\n"（cm/度）
		 *       → 读取失败返回 "ERR\n"
		 */
		else if (strcmp(line, "UNLOAD") == 0)
		{
			ArmControl_UnloadAll();
			Comm_SendStr("OK\n");
		}
		else if (strcmp(line, "READ_POS") == 0)
		{
			uint16_t positions[ARM_MAX_SERVOS_NUM] = {0};
			float    joints[4] = {0};
			char     reply[64];
			kin_obj_t fk_obj;

			if (ArmControl_ReadAllPositions(positions, 500))
			{
				ArmControl_PositionsToJointAngles(positions, joints);
				for (uint8_t i = 0; i < 4; i++)
				{
					fk_obj.joint[i].theta = joints[i];
					fk_obj.joint[i].rad   = joints[i] * PI / 180.0f;
				}
				Kin_Forward(&fk_obj);
				snprintf(reply, sizeof(reply),
						 "%.2f,%.2f,%.2f,%.2f\n",
						 fk_obj.vector.x, fk_obj.vector.y,
						 fk_obj.vector.z, fk_obj.alpha_pitch);
				Comm_SendStr(reply);
			}
			else
			{
				Comm_SendStr("ERR\n");
			}
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
			if (g_status)
			{
				/* 等待运动完成后再回复，确保上位机收到 OK 时机械臂已静止 */
				R_BSP_SoftwareDelay(duration + 300, BSP_DELAY_UNITS_MILLISECONDS);
				Comm_SendStr("OK\n");
			}
			else
			{
				Comm_SendStr("ERR\n");
			}
		}
		/*
		 *   GRIPPER_OPEN [dur_ms]
		 *       → 打开夹爪，等待运动完成后返回 "OK\n"
		 *       → dur_ms 默认 500，范围 100~5000
		 *
		 *   GRIPPER_CLOSE [dur_ms]
		 *       → 关闭夹爪，等待运动完成后返回 "OK\n"
		 */
		else if (strncmp(line, "GRIPPER_OPEN", 12) == 0)
		{
			uint16_t dur = (uint16_t)strtol(line + 12, NULL, 10);
			if (dur < 100 || dur > 5000) dur = 500;
			ArmControl_GripperControl(&g_arm_ctrl, true, dur);
			R_BSP_SoftwareDelay(dur + 100, BSP_DELAY_UNITS_MILLISECONDS);
			Comm_SendStr("OK\n");
		}
		else if (strncmp(line, "GRIPPER_CLOSE", 13) == 0)
		{
			uint16_t dur = (uint16_t)strtol(line + 13, NULL, 10);
			if (dur < 100 || dur > 5000) dur = 500;
			ArmControl_GripperControl(&g_arm_ctrl, false, dur);
			R_BSP_SoftwareDelay(dur + 100, BSP_DELAY_UNITS_MILLISECONDS);
			Comm_SendStr("OK\n");
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
//		g_status = ArmControl_CoordinateSet(16,0,(float)-3.2,(float)-76.1,-90,90,1000);
//		R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
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
