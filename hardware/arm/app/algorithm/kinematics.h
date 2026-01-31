#ifndef __KINEMATICS_H__
#define __KINEMATICS_H__

#include "stdint.h"
#include "math.h"
#include "string.h"

#define PI 3.1415926f
 
/* 连杆序号按照从底部向上排序 单位：cm*/
#define LINKAGE_1    				 	2.89f
#define LINKAGE_2					 	10.43f
#define LINKAGE_3		 				8.9f
#define LINKAGE_4	 					17.7f

// 关节角度限制定义（与joint[4]数组索引对应）
#define MIN_JOINT0_ANGLE							-90.0f	// 关节0（基座旋转）最小角度
#define MAX_JOINT0_ANGLE							 90.0f	// 关节0（基座旋转）最大角度
#define MIN_JOINT1_ANGLE							  0.0f	// 关节1（肩部俯仰）最小角度
#define MAX_JOINT1_ANGLE							180.0f	// 关节1（肩部俯仰）最大角度
#define MIN_JOINT2_ANGLE							-90.0f	// 关节2（肘部俯仰）最小角度
#define MAX_JOINT2_ANGLE							 90.0f	// 关节2（肘部俯仰）最大角度
#define MIN_JOINT3_ANGLE							-90.0f	// 关节3（腕部俯仰）最小角度
#define MAX_JOINT3_ANGLE							 90.0f	// 关节3（腕部俯仰）最大角度

// 运动学解算状态枚举
typedef enum
{
	KIN_STATUS_OK = 1,		// 解算成功
	KIN_STATUS_INVALID = 0	// 解算失败（无解或超出工作空间）
}kin_status_t;

typedef struct
{
	float x;	// X轴坐标（单位：cm）
	float y;	// Y轴坐标（单位：cm）
	float z;	// Z轴坐标（单位：cm）
}kin_vec_t;		//表示机械臂末端执行器在三维空间中的位置（笛卡尔坐标）。

typedef struct
{
	float rad;		// 弧度值（radians）
	float theta;	// 角度值（degreeθ）
	
}kin_joint_val_t;

typedef struct  
{
	float alpha_pitch;	// 末端俯仰角（pitch angle）
	kin_vec_t vector;	// 末端位置 (x, y, z)
	kin_joint_val_t joint[4];      // 4个关节的角度数组
	
}kin_obj_t;


void Kinematics_Init(kin_obj_t* kin_obj);

/**
 * @brief 正运动学解算
 * 根据4个关节角度计算末端执行器位置
 * 
 * @param joint0_theta 从下至上第1个关节角度（度）- 基座旋转
 * @param joint1_theta 从下至上第2个关节角度（度）- 肩部俯仰
 * @param joint2_theta 从下至上第3个关节角度（度）- 肘部俯仰
 * @param joint3_theta 从下至上第4个关节角度（度）- 腕部俯仰
 * @return kin_vec_t 末端位置向量 (x, y, z)
 */
kin_vec_t Kinematics_ForwardKinematicsCalc(float joint0_theta, float joint1_theta, float joint2_theta, float joint3_theta);

/**
 * @brief 逆运动学解算（Inverse Kinematics）
 * 根据末端位置和俯仰角计算4个关节角度
 * 
 * @param kin_obj 运动学对象指针（输入：vector和alpha_pitch，输出：joint[0..3]）
 * @return kin_status_t KIN_STATUS_OK表示有解，KIN_STATUS_INVALID表示无解
 */
kin_status_t Kinematics_InverseKinematicsCalc(kin_obj_t* kin_obj);

#endif
