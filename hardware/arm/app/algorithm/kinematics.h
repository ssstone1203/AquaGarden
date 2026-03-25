#ifndef __KINEMATICS_H__
#define __KINEMATICS_H__

#include "stdint.h"
#include "math.h"
#include "string.h"
#include "stdbool.h"

#define PI 3.1415926f
 
/* 连杆序号按照从底部向上排序 单位：cm*/
#define LINKAGE_1    				 	2.89f
#define LINKAGE_2					 	10.43f
#define LINKAGE_3		 				8.9f
#define LINKAGE_4	 					17.7f

// 关节角度限制定义（与joint[4]数组索引对应）
// 根据实际机械臂结构调整，允许向后倾斜
#define MIN_JOINT0_ANGLE                           -180.0f// 关节0（基座旋转）最小角度
#define MAX_JOINT0_ANGLE                            180.0f// 关节0（基座旋转）最大角度
#define MIN_JOINT1_ANGLE                           0.0f// 关节1（肩部俯仰）最小角度
#define MAX_JOINT1_ANGLE                           180.0f// 关节1（肩部俯仰）最大角度
#define MIN_JOINT2_ANGLE                           -150.0f // 关节2（肘部俯仰）最小角度（放宽到-150）
#define MAX_JOINT2_ANGLE                            150.0f // 关节2（肘部俯仰）最大角度（放宽到150）
#define MIN_JOINT3_ANGLE                           -100.0f // 关节3（腕部俯仰）最小角度（放宽到-150）
#define MAX_JOINT3_ANGLE                            90.0f // 关节3（腕部俯仰）最大角度（放宽到150）

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

void Kin_Init(kin_obj_t* kin_obj);
kin_status_t Kin_Inverse(kin_obj_t* kin_obj);
void Kin_Forward(kin_obj_t* kin_obj);
bool PitchRange_Set(kin_obj_t* kin_obj,kin_vec_t* kin_vec, float alpha1, float alpha2);

#endif
