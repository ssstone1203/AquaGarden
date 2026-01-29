// <<< Use Configuration Wizard in Context Menu >>>
#ifndef GLOBAL_H
#define GLOBAL_H

/* 对应坐标(15,0,2) */
#define SERIAL_SERVO1_RESET_DUTY		   			  226
#define SERIAL_SERVO2_RESET_DUTY		   			  500 
#define SERIAL_SERVO3_RESET_DUTY		   			  177 
#define SERIAL_SERVO4_RESET_DUTY		   			  129 
#define SERIAL_SERVO5_RESET_DUTY		   			  408 
#define SERIAL_SERVO6_RESET_DUTY		   			  500 

// <o>Arm Type
//  <i>Select servo type
//  <0=> PWM Servos Arm
//  <1=> Serial Servos Arm
#define ARM_SELECT				1
#if (ARM_SELECT == 0)
	#define SERVO_TYPE			1
#elif (ARM_SELECT == 1)
	#define SERVO_TYPE			2
#endif

#if (SERVO_TYPE == 1)
	#define PS2_SET_MAX_DUTY							 2500
	#define PS2_SET_MIN_DUTY							  500
#elif (SERVO_TYPE == 2)
	#define PS2_SET_MAX_DUTY							  875
	#define PS2_SET_MIN_DUTY							  125
#endif

// <o>Control Mode
//  <i>Select control mode
//  <0=> PC Control
//  <1=> Bluetooth Control
#define CONTROL_MODE		0
#if (CONTROL_MODE == 0)
	#define PC_CONTROL
#elif (CONTROL_MODE == 1)
	#define BLUETOOTH_CONTROL	
#endif

#endif
