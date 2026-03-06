# AquaGarden 机械臂调试总结与API使用指南

## 一、调试过程总结

### 1.1 初始问题
MCU复位后只发送一次舵机数据，随后串口助手收不到任何数据，程序似乎卡死。

### 1.2 问题排查与解决

#### 问题1：舵机ID映射错误
**现象**：发送的舵机命令与机械臂实际动作不符

**原因**：AquaGarden代码中的舵机ID定义与LeArm硬件不一致

| 功能 | 错误ID | 正确ID |
|------|--------|--------|
| 夹爪 | 5 | **1** |
| 旋转 | 6 | **2** |
| 腕部俯仰 | 4 | **3** |
| 肘部俯仰 | 3 | **4** |
| 肩部俯仰 | 2 | **5** |
| 基座旋转 | 1 | **6** |

**解决**：修改 `arm_control.h` 中的ID定义

```c
// 舵机ID定义（与LeArm硬件保持一致）
#define SERVO_ID_GRIPPER    1   // 夹爪舵机（ID 1）
#define SERVO_ID_ROTATION   2   // 旋转舵机（ID 2）
#define SERVO_ID_WRIST      3   // 腕部俯仰舵机（ID 3）
#define SERVO_ID_ELBOW      4   // 肘部俯仰舵机（ID 4）
#define SERVO_ID_SHOULDER   5   // 肩部俯仰舵机（ID 5）
#define SERVO_ID_BASE       6   // 基座旋转舵机（ID 6）
```

#### 问题2：关节角度限制过严
**现象**：逆运动学解算返回失败（KIN_STATUS_INVALID）

**原因**：角度限制为±90°，而实际机械臂需要更大的活动范围

**解决**：放宽角度限制
```c
// kinematatics.h
#define MIN_JOINT2_ANGLE                           -150.0f
#define MAX_JOINT2_ANGLE                            150.0f
#define MIN_JOINT3_ANGLE                           -150.0f
#define MAX_JOINT3_ANGLE                            150.0f
```

#### 问题3：坐标超出工作空间
**现象**：目标坐标(15, 0, 2)无法解算

**原因**：
- 连杆长度：L2=10.43, L3=8.9, L4=17.7
- 最小可达距离 = |10.43-8.9-17.7| = 16.17cm
- 坐标(15, 0, 2)的距离d = 15.03cm < 16.17cm

**解决**：使用在工作空间内的坐标，如(18, 0, 2)或(20, 0, 10)

#### 问题4：角度转换缺少LeArm兼容性
**现象**：直接角度控制时，机械臂运动方向与预期相反

**原因**：LeArm的theta2servo函数中，肩部有90°偏移和反向转换
```c
// LeArm的转换
target_angle[1] = 90.0f - self->knot[1].theta;  // 肩部反向
```

**解决**：在 `ArmControl_ServoPositionFromAngle` 中添加LeArm兼容转换
```c
// 关节角度转换为舵机位置值
if (joint_index == 1)  // 肩部
{
    converted_angle = 90.0f - joint_angle;  // 反向
}
```

#### 问题5：复位角度与实际舵机位置不匹配
**现象**：关节角度控制时，位置值被限制在边界

**原因**：假设的复位角度与实际舵机位置不符

**解决**：根据实际复位舵机位置反推复位角度
```c
// 复位位置对应的关节角度
static const float reset_angles[4] = {
    0.0f,    // 关节0：基座0度
    112.0f,  // 关节1：肩部112度
    -89.0f,  // 关节2：肘部-89度
    -77.0f   // 关节3：腕部-77度
};
```

---

## 二、API使用说明

### 2.1 核心数据结构

```c
// 机械臂控制对象
arm_control_t g_arm_ctrl;

// 运动学状态
kin_status_t g_kin_status;
```

### 2.2 初始化API

#### `ArmControl_Init`
**功能**：初始化机械臂控制对象

**参数**：
- `arm_ctrl`: 机械臂控制对象指针

**示例**：
```c
arm_control_t g_arm_ctrl;

void hal_entry(void)
{
    // 初始化
    ArmControl_Init(&g_arm_ctrl);
}
```

---

### 2.3 复位API

#### `ArmControl_Reset`
**功能**：控制所有6个舵机复位到初始位置

**参数**：
- `arm_ctrl`: 机械臂控制对象指针
- `duration`: 运动时间（毫秒）

**示例**：
```c
// 复位到初始位置，用时2000ms
ArmControl_Reset(&g_arm_ctrl, 2000);
R_BSP_SoftwareDelay(2500, BSP_DELAY_UNITS_MILLISECONDS);  // 等待完成
```

**发送的舵机顺序**：
1. ID 6（基座）→ SERIAL_SERVO6_RESET_DUTY (500)
2. ID 5（肩部）→ SERIAL_SERVO5_RESET_DUTY (408)
3. ID 4（肘部）→ SERIAL_SERVO4_RESET_DUTY (129)
4. ID 3（腕部）→ SERIAL_SERVO3_RESET_DUTY (177)
5. ID 2（旋转）→ SERIAL_SERVO2_RESET_DUTY (500)
6. ID 1（夹爪）→ SERIAL_SERVO1_RESET_DUTY (226)

---

### 2.4 关节角度控制API

#### `ArmControl_JointAngleSet`
**功能**：设置单个关节角度

**参数**：
- `arm_ctrl`: 机械臂控制对象指针
- `joint_index`: 关节索引（0-3）
  - 0: 基座旋转
  - 1: 肩部俯仰
  - 2: 肘部俯仰
  - 3: 腕部俯仰
- `angle`: 目标角度（度）
- `duration`: 运动时间（毫秒）

**返回值**：`kin_status_t`
- `KIN_STATUS_OK`: 成功
- `KIN_STATUS_INVALID`: 失败（索引错误或超出范围）

**示例**：
```c
// 测试基座旋转（关节0）：左右摆动
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 0, -30.0f, 1000);  // 左转30度
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 0, 30.0f, 1000);   // 右转30度
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 0, 0.0f, 1000);    // 回到中心
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);

// 测试肩部俯仰（关节1）
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 1, 112.0f, 1000);  // 肩部复位角度
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 1, 90.0f, 1000);   // 稍微抬起
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);

// 测试肘部俯仰（关节2）
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 2, -89.0f, 1000);   // 肘部复位角度
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 2, -70.0f, 1000);   // 稍微放松
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);

// 测试腕部俯仰（关节3）
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 3, -77.0f, 1000);   // 腕部复位角度
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
g_kin_status = ArmControl_JointAngleSet(&g_arm_ctrl, 3, -60.0f, 1000);   // 稍微调整
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
```

**角度参考范围**：
- 关节0（基座）：-90° ~ 90°
- 关节1（肩部）：-90° ~ 180°
- 关节2（肘部）：-150° ~ 150°
- 关节3（腕部）：-150° ~ 150°

---

#### `ArmControl_AllJointsSet`
**功能**：同时控制所有4个关节角度

**参数**：
- `arm_ctrl`: 机械臂控制对象指针
- `angles`: 4个关节的角度数组（度）
  - angles[0]: 基座旋转
  - angles[1]: 肩部俯仰
  - angles[2]: 肘部俯仰
  - angles[3]: 腕部俯仰
- `duration`: 运动时间（毫秒）

**示例**：
```c
// 复位姿态
float pose_reset[4] = {0.0f, 112.0f, -89.0f, -77.0f};
ArmControl_AllJointsSet(&g_arm_ctrl, pose_reset, 1000);
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);

// 稍微抬起
float pose_lift[4] = {0.0f, 90.0f, -70.0f, -60.0f};
ArmControl_AllJointsSet(&g_arm_ctrl, pose_lift, 1000);
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);

// 向侧面移动（基座旋转20度）
float pose_side[4] = {20.0f, 100.0f, -80.0f, -70.0f};
ArmControl_AllJointsSet(&g_arm_ctrl, pose_side, 1000);
R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
```

**与单关节控制的区别**：
- `ArmControl_JointAngleSet`：每次只控制一个关节，其他关节保持当前状态
- `ArmControl_AllJointsSet`：同时控制4个关节，可以实现更流畅的协调运动

---

### 2.5 逆运动学控制API

#### `ArmControl_EndPositionSet`
**功能**：通过逆运动学设置末端执行器的空间位置

**参数**：
- `arm_ctrl`: 机械臂控制对象指针
- `x`: 目标X坐标（cm），正方向为前方
- `y`: 目标Y坐标（cm），正方向为右侧
- `z`: 目标Z坐标（cm），正方向为上方
- `pitch`: 末端俯仰角（度），0°为水平
- `duration`: 运动时间（毫秒）

**返回值**：`kin_status_t`
- `KIN_STATUS_OK`: 解算成功，已发送运动命令
- `KIN_STATUS_INVALID`: 解算失败（超出工作空间或角度限制）

**示例**：
```c
// 正前方位置（在工作空间内）
g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 18.0f, 0.0f, 2.0f, 0.0f, 1000);
if (g_kin_status == KIN_STATUS_OK)
{
    R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
}
else
{
    // 解算失败，坐标可能超出工作空间
    R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);
}

// 右侧位置
g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 17.0f, 5.0f, 3.0f, 0.0f, 1000);
if (g_kin_status == KIN_STATUS_OK)
{
    R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
}

// 较高位置
g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 15.0f, 0.0f, 15.0f, 0.0f, 1000);
if (g_kin_status == KIN_STATUS_OK)
{
    R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
}

// 回到复位位置
ArmControl_Reset(&g_arm_ctrl, 1000);
R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
```

**工作空间说明**：
- 最小可达距离：约16.17cm（基于连杆参数计算）
- 最大可达距离：约37.03cm
- 推荐坐标范围：
  - X: 18~25cm（前后）
  - Y: -10~10cm（左右）
  - Z: 2~20cm（高度）

**与直接角度控制的区别**：
- `ArmControl_JointAngleSet/AllJointsSet`：直接控制关节角度，简单直接
- `ArmControl_EndPositionSet`：控制末端位置，需要逆运动学解算，更适合空间定位任务

---

#### `ArmControl_EndPositionSetWithPitchRange`
**功能**：在指定的俯仰角范围内求解逆运动学

**参数**：
- `arm_ctrl`: 机械臂控制对象指针
- `x`: 目标X坐标（cm）
- `y`: 目标Y坐标（cm）
- `z`: 目标Z坐标（cm）
- `pitch`: 目标俯仰角（度）
- `min_pitch`: 最小俯仰角限制（度）
- `max_pitch`: 最大俯仰角限制（度）
- `duration`: 运动时间（毫秒）

**返回值**：`kin_status_t`

**示例**：
```c
// 尝试在-30°到30°范围内找到最优解
g_kin_status = ArmControl_EndPositionSetWithPitchRange(
    &g_arm_ctrl, 
    18.0f, 0.0f, 5.0f,    // 目标位置
    0.0f,                 // 目标俯仰角：水平
    -30.0f, 30.0f,        // 允许范围：-30°到30°
    1000);
if (g_kin_status == KIN_STATUS_OK)
{
    R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
}
```

**使用场景**：
- 当固定俯仰角无解时，可以在范围内自动调整
- 优先选择最接近目标俯仰角的解

---

### 2.6 夹爪控制API

#### `ArmControl_GripperControl`
**功能**：控制夹爪开合

**参数**：
- `arm_ctrl`: 机械臂控制对象指针
- `open`: `true`表示打开，`false`表示关闭
- `duration`: 运动时间（毫秒）

**示例**：
```c
// 打开夹爪
ArmControl_GripperControl(&g_arm_ctrl, true, 500);
R_BSP_SoftwareDelay(600, BSP_DELAY_UNITS_MILLISECONDS);

// 关闭夹爪
ArmControl_GripperControl(&g_arm_ctrl, false, 500);
R_BSP_SoftwareDelay(600, BSP_DELAY_UNITS_MILLISECONDS);
```

---

## 三、完整使用示例

### 3.1 主函数框架
```c
#include "hal_data.h"
#include "arm_control.h"
#include "serial_servo.h"

arm_control_t g_arm_ctrl;
kin_status_t g_kin_status;

void hal_entry(void)
{
    // 1. 打开串口
    R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);
    
    // 2. 初始化机械臂
    ArmControl_Init(&g_arm_ctrl);
    
    // 3. 等待稳定
    R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);
    
    // 4. 复位到初始位置
    ArmControl_Reset(&g_arm_ctrl, 2000);
    R_BSP_SoftwareDelay(2500, BSP_DELAY_UNITS_MILLISECONDS);
    
    // 5. 进入主循环
    while(1)
    {
        // 在这里调用机械臂控制API
        // ...
    }
}
```

### 3.2 循环测试示例
```c
while(1)
{
    // 移动到正前方位置
    g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 20.0f, 0.0f, 5.0f, 0.0f, 1000);
    if (g_kin_status == KIN_STATUS_OK)
    {
        R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
    }
    
    // 打开夹爪
    ArmControl_GripperControl(&g_arm_ctrl, true, 500);
    R_BSP_SoftwareDelay(600, BSP_DELAY_UNITS_MILLISECONDS);
    
    // 移动到抓取位置（较低）
    g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 18.0f, 0.0f, 2.0f, -20.0f, 800);
    if (g_kin_status == KIN_STATUS_OK)
    {
        R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);
    }
    
    // 关闭夹爪（抓取）
    ArmControl_GripperControl(&g_arm_ctrl, false, 500);
    R_BSP_SoftwareDelay(600, BSP_DELAY_UNITS_MILLISECONDS);
    
    // 抬起
    g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 18.0f, 0.0f, 10.0f, 0.0f, 1000);
    if (g_kin_status == KIN_STATUS_OK)
    {
        R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
    }
    
    // 移动到放置位置（右侧）
    g_kin_status = ArmControl_EndPositionSet(&g_arm_ctrl, 17.0f, 8.0f, 5.0f, 0.0f, 1000);
    if (g_kin_status == KIN_STATUS_OK)
    {
        R_BSP_SoftwareDelay(1200, BSP_DELAY_UNITS_MILLISECONDS);
    }
    
    // 打开夹爪（放置）
    ArmControl_GripperControl(&g_arm_ctrl, true, 500);
    R_BSP_SoftwareDelay(600, BSP_DELAY_UNITS_MILLISECONDS);
    
    // 复位
    ArmControl_Reset(&g_arm_ctrl, 1000);
    R_BSP_SoftwareDelay(1500, BSP_DELAY_UNITS_MILLISECONDS);
}
```

---

## 四、连杆参数

```c
// 单位：cm
#define LINKAGE_1     2.89f   // 基座高度
#define LINKAGE_2    10.43f   // 肩部连杆
#define LINKAGE_3     8.90f   // 肘部连杆
#define LINKAGE_4    17.70f   // 腕部连杆
```

---

## 五、调试技巧

### 5.1 通过串口数据判断问题
| 数据特征 | 问题 |
|---------|------|
| 只有复位数据，后续无数据 | 程序卡死或进入HardFault |
| 6帧数据后停止 | 可能逆运动学失败 |
| 4帧数据（ID 6,5,4,3） | 正常角度控制 |
| 6帧数据（ID 6,5,4,3,2,1） | 正常复位 |

### 5.2 常见错误代码
- `KIN_STATUS_INVALID`：
  - 坐标超出工作空间
  - 解算出的角度超出限制
  - 关节索引错误

### 5.3 推荐的测试流程
1. 先测试 `ArmControl_Reset` 确保硬件连接正常
2. 测试 `ArmControl_JointAngleSet` 单个关节，确认ID映射正确
3. 测试 `ArmControl_AllJointsSet` 多个关节协调运动
4. 最后测试 `ArmControl_EndPositionSet` 逆运动学

---

**文档版本**：v1.0  
**创建日期**：2026-03-07  
**适用硬件**：LeArm机械臂 + RA6M5 MCU
