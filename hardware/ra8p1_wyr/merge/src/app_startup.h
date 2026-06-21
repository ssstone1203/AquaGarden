#ifndef APP_STARTUP_H_
#define APP_STARTUP_H_

#include <stdint.h>

/* Keil Watch: non-zero means the task loop has run at least once. */
extern volatile uint32_t g_task_hb_sensor;
extern volatile uint32_t g_task_hb_actuator;

#endif /* APP_STARTUP_H_ */
