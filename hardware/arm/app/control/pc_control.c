#include "pc_control.h"
#include "hal_data.h"
#include "arm_control.h"
#include "serial_servo.h"
#include "comm.h"
#include "ultrasound.h"
#include "kinematics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

static uint8_t s_calib_idx = 0;

/**
 * @brief 驱动机械臂到指定标定位置，等待完成后读取实际关节角并回传。
 *
 * 回传格式（含 '\n'）：
 *   成功且读位置成功：  "OK j0.dd,j1.dd,j2.dd,j3.dd\n"
 *   成功但读位置失败：  "OK j0.dd,j1.dd,j2.dd,j3.dd\n"（回退到运动学命令角）
 *   运动学无解：        "ERR\n"
 */
static void s_calib_move_and_report(const CalibPose_t *pose)
{
    char     reply[72];
    uint16_t positions[ARM_MAX_SERVOS_NUM] = {0};
    float    joints[4]                     = {0};
    uint8_t  status;

    status = ArmControl_CoordinateSet(
        pose->x, pose->y, pose->z, pose->pitch, -90.0f, 90.0f, 1000);

    if (!status)
    {
        Comm_SendStr("ERR\n");
        return;
    }

    R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);

    if (ArmControl_ReadAllPositions(positions, 500))
    {
        ArmControl_PositionsToJointAngles(positions, joints);
    }
    else
    {
        joints[0] = g_kin_obj.joint[0].theta;
        joints[1] = g_kin_obj.joint[1].theta;
        joints[2] = g_kin_obj.joint[2].theta;
        joints[3] = g_kin_obj.joint[3].theta;
    }

    snprintf(reply, sizeof(reply),
             "OK %.2f,%.2f,%.2f,%.2f\n",
             joints[0], joints[1], joints[2], joints[3]);
    Comm_SendStr(reply);
}

void PcControl_Init(void)
{
    R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);
    ArmControl_Init();
    Comm_Init();
    Ultrasound_Init();

    R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
}

void PcControl_Run(void)
{
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
            char    *p   = line + 5;
            char    *end = p;
            float    x        = strtof(p, &end); p = end + 1;
            float    y        = strtof(p, &end); p = end + 1;
            float    z        = strtof(p, &end); p = end + 1;
            float    pitch    = strtof(p, &end); p = end + 1;
            float    min_p    = strtof(p, &end); p = end + 1;
            float    max_p    = strtof(p, &end); p = end + 1;
            uint16_t duration = (uint16_t)strtol(p, NULL, 10);

            if (ArmControl_CoordinateSet(x, y, z, pitch, min_p, max_p, duration))
            {
                R_BSP_SoftwareDelay(duration + 300, BSP_DELAY_UNITS_MILLISECONDS);
                Comm_SendStr("OK\n");
            }
            else
            {
                Comm_SendStr("ERR\n");
            }
        }
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
//        else if (strcmp(line, "DIST") == 0)
//        {
//            float dist = Ultrasound_GetDistance();
//            if (dist < 0.0f)
//            {
//                Comm_SendStr("ERR\n");
//            }
//            else
//            {
//                char reply[16];
//                snprintf(reply, sizeof(reply), "%.2f\n", dist);
//                Comm_SendStr(reply);
//            }
//        }
        else if (strcmp(line, "CALIB_RESET") == 0)
        {
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
                s_calib_move_and_report(&s_calib_poses[s_calib_idx++]);
            }
        }
    }
}
