#include "hal_data.h"
#include "arm_control.h"
#include "serial_servo.h"

/* ── 模式选择 ────────────────────────────────────────────────────
 * 手眼标定时开启 HAND_EYE_CALIB_MODE
 * 正常使用时注释掉该宏
 * ─────────────────────────────────────────────────────────────── */
#define HAND_EYE_CALIB_MODE

uint8_t g_status = 0;

/* ── 手眼标定位置表 ───────────────────────────────────────────────
 * 与 tools/algorithm/kinematics.py 中 CALIB_POSITIONS 完全一致
 * 格式：{x, y, z, pitch, min_pitch, max_pitch, move_time_ms}
 * 单位：cm / 度 / ms
 * ─────────────────────────────────────────────────────────────── */
typedef struct { float x, y, z, pitch, min_p, max_p; uint16_t t; } calib_pose_t;

static const calib_pose_t CALIB_POSES[] = {
    {20,   0,  20,   0, -90, 90, 1500},  /* 01 */
    {20,  -8,  20,   0, -90, 90, 1500},  /* 02 */
    {20,   8,  20,   0, -90, 90, 1500},  /* 03 */
    {20,  -8,  15, -15, -90, 90, 1500},  /* 04 */
    {20,   8,  15,  15, -90, 90, 1500},  /* 05 */
    {25,   0,  15,  -5, -90, 90, 1500},  /* 06 */
    {25,  -6,  15, -10, -90, 90, 1500},  /* 07 */
    {25,   6,  15,  10, -90, 90, 1500},  /* 08 */
    {15,   0,  20,   5, -90, 90, 1500},  /* 09 */
    {15,  -8,  18,  -5, -90, 90, 1500},  /* 10 */
    {15,   8,  18,   5, -90, 90, 1500},  /* 11 */
    {22,   0,  22,  10, -90, 90, 1500},  /* 12 */
    {22, -10,  18,   0, -90, 90, 1500},  /* 13 */
    {22,  10,  18,   0, -90, 90, 1500},  /* 14 */
    {18,   0,  15, -20, -90, 90, 1500},  /* 15 */
    {18,  -8,  22,  15, -90, 90, 1500},  /* 16 */
    {18,   8,  22,  15, -90, 90, 1500},  /* 17 */
    {23,  -8,  12, -15, -90, 90, 1500},  /* 18 */
};
#define CALIB_POSES_NUM  (sizeof(CALIB_POSES) / sizeof(CALIB_POSES[0]))

/* 每个位置在到达后的保持时间（ms）
 * PC端 Python 脚本在此期间按空格键采集图像
 * 如果机械臂运动较慢可以适当加大 */
#define CALIB_HOLD_MS   5000U

void hal_entry(void)
{
    R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);
    ArmControl_Init();
    R_BSP_SoftwareDelay(3000, BSP_DELAY_UNITS_MILLISECONDS);

#ifdef HAND_EYE_CALIB_MODE
    /* ── 手眼标定模式：循环遍历所有标定位置 ── */
    while (1)
    {
        for (uint8_t i = 0; i < CALIB_POSES_NUM; i++)
        {
            const calib_pose_t *p = &CALIB_POSES[i];
            g_status = ArmControl_CoordinateSet(
                p->x, p->y, p->z, p->pitch, p->min_p, p->max_p, p->t);
            /* 等待机械臂运动完成 + 保持静止供拍照 */
            R_BSP_SoftwareDelay(p->t + CALIB_HOLD_MS, BSP_DELAY_UNITS_MILLISECONDS);
        }
        /* 一轮结束后复位，再开始下一轮 */
        ArmControl_CoordinateSet(15, 0, 20, 0, -90, 90, 1500);
        R_BSP_SoftwareDelay(3000, BSP_DELAY_UNITS_MILLISECONDS);
    }

#else
    /* ── 正常测试模式（标定完成后切换回来）── */
    while (1)
    {
        g_status = ArmControl_CoordinateSet(15, 0, 20,  0, -90, 90, 1000);
        R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
        g_status = ArmControl_CoordinateSet(23, 0, 20,  0, -90, 90, 1000);
        R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
        g_status = ArmControl_CoordinateSet(30, 0, 13,  0, -90, 90, 1000);
        R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
        g_status = ArmControl_CoordinateSet(30, 0, 13, 15, -90, 90, 1000);
        R_BSP_SoftwareDelay(2000, BSP_DELAY_UNITS_MILLISECONDS);
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
