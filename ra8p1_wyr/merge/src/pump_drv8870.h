/*******************************************************************************************************************//**
 * DRV8870/DRV8871 H-bridge pump control (RA8P1 + FSP).
 *
 * Wiring (CPKHMI-RA8P1 expansion board, must match RASC symbolic names):
 *   P206 (PUMP_VREF) -> DRV8870 VREF when present (GPIO out high ≈3.3 V)
 *   P105 (PUMP_IN1)  -> driver IN1 (GPIO, logic 3.3 V)
 *   P104 (PUMP_IN2)  -> driver IN2 (GPT1 GTIOC1B PWM)
 *
 * DRV8870/DRV8871 VM / GND / MOTOR terminals go to pump supply and DC pump (see .c comment block).
 **********************************************************************************************************************/
#ifndef PUMP_DRV8870_H_
#define PUMP_DRV8870_H_

#include <stdint.h>

void pump_drv8870_init(void);

/** Stops pump and puts driver inputs low (coast / sleep after ~1 ms per DRV8870). */
void pump_drv8870_stop(void);

/**
 * Forward at ~100% without running the timer: IN1=H, IN2=L (DRV8870 table), GPT stopped so GTIOCB = stop level low.
 * Use to prove MCU->driver->motor path; if this does not spin, check VM/UVLO, VREF/ISEN/ITRIP (datasheet §7.4.2), OUT, and INx.
 */
void pump_drv8870_run_forward_dc(void);

/**
 * True PWM on IN2 (GPT). Some driver boards / wiring make this unreliable even when run_forward_dc() works.
 *
 * @param[in] power_percent  1–100，数值越大 IN2 上“正转(低)”占空比越高、转速越快；0 请用 pump_drv8870_stop。
 */
void pump_drv8870_set_power(uint8_t power_percent);

/**
 * “软调速”但不靠 GPT：在 duration 内交替 全速正转 / 停，用时间比例近似占空比（只用到 run_forward_dc + stop）。
 * 适合板子直流能转、PWM 段不转的情况。
 *
 * @param[in] power_percent  1–99 平均出力；100 等同全程直流正转；0 等同全程停。
 * @param[in] duration_ms    这一段总时长。
 */
void pump_drv8870_run_dc_time_average(uint8_t power_percent, uint32_t duration_ms);

#endif /* PUMP_DRV8870_H_ */
