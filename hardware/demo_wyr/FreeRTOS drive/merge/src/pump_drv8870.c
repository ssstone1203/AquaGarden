/*******************************************************************************************************************//**
 * DRV8870 brushed DC pump driver — hardware notes
 *
 * Datasheet ZHCSE25 (Chinese) / SLVSCY8 — critical for “no spin” that is not a loose wire:
 *   • §7.3.2 Sleep: power-up with IN1=IN2=low → sleep immediately; after sleep, INx must be high ≥5 µs then allow tON
 *     (typ 50 µs) before the H-bridge is operational. stop() holds both inputs low → next run must include that wake time.
 *   • §7.4.2 / §7.3.3: if not using current regulation, ISEN must tie to PCB GND. VREF is still 0.3–5 V. If ITRIP
 *     (from VREF and R_sense) is below the pump’s stall/start current, the chip current-chops and the rotor may never
 *     appear to run — check the carrier board’s VREF network and sense resistor, not just IN1/IN2 routing.
 *   • §6.3: VM ≥ 6.5 V (typ UVLO ~6.1–6.4 V falling); below that the bridge is off.
 *   • Logic PWM on IN1/IN2: fPWM ≤ 100 kHz recommended.
 *
 * Power (12 V typical):
 *   - Use a DC barrel jack or screw terminal for 12 V input to your power board.
 *   - A buck regulator (e.g. 12 V -> 5 V or 3.3 V) powers the RA6E2 only; do NOT feed 12 V to the MCU.
 *   - DRV8870 VM pin: 6.5–45 V per datasheet; tie pump + to VM domain, grounds common (star/join at one point).
 *   - Local 0.1 µF + bulk cap at VM; pump leads short and twisted if possible.
 *   - AG 板：PUMP_VREF (P206) → DRV8870 VREF；R_sense 110 mΩ。固件须保持 VREF≈3.3 V，否则 ITRIP≈0、泵不转。
 *
 * Water plumbing (aquarium bench test):
 *   - Submersible: fully submerge pump, inlet screen unobstructed; outlet hose rises then returns to tank (closed loop).
 *   - Inline: suction hose from tank (weighted strainer, no air leaks); discharge back to tank; prime before run.
 *   - Keep first tests at low power; verify direction; no dry-run beyond the pump’s rated limit.
 **********************************************************************************************************************/

#include "pump_drv8870.h"
#include "Pump_Task.h"

#include "hal_data.h"
#include "common_data.h"

/** AG 板：P206 / net PUMP_VREF -> DRV8870 VREF（与 ag_driver bsp_pin_cfg 一致） */
#define PUMP_VREF_PIN   BSP_IO_PORT_02_PIN_06

/** RASC user label PUMP_IN1 on P302 -> DRV8870 IN1 */
#define PUMP_IN1_PIN    BSP_IO_PORT_03_PIN_02

/** GPT output pin select for IN2 (P301 / GTIOC4B) */
#define PUMP_PWM_PIN    GPT_IO_PIN_GTIOCB

/** §7.3.2 tON typ 50 µs after wake; margin for 5 µs minimum high before tON. */
#define PUMP_DRV8870_WAKE_DELAY_US    200U

static void pump_drv8870_after_drive_asserted(void)
{
    R_BSP_SoftwareDelay(PUMP_DRV8870_WAKE_DELAY_US, BSP_DELAY_UNITS_MICROSECONDS);
}

static uint32_t pump_period_counts(void)
{
    return g_pump_timer_cfg.period_counts;
}

/**
 * IN1=1、IN2=1 慢衰减制动（DRV8870 表），不拉双低 → 不会进休眠。
 * 用 GPT 让 GTIOCB 整周期为高；若你板子 GPT 仍异常，请用 hal_entry 里全程 run_forward_dc。
 */
static void pump_drv8870_brake_holding(void)
{
    /*
     * 同 TIMER_MODE_PERIODIC + GTIOC B：计数前端为低、比较后至高。制动需 IN2≈高 → 比较值取极小，
     * 仅周期开头极短为低，其余为高（IN1=1 时为慢衰减制动）。勿再用 period-1，那会几乎全程低 = 正转。
     */
    uint32_t const brake_min_low_counts = 1U;

    (void) g_ioport.p_api->pinWrite(&g_ioport_ctrl, PUMP_IN1_PIN, BSP_IO_LEVEL_HIGH);
    (void) g_pump_timer.p_api->dutyCycleSet(g_pump_timer.p_ctrl, brake_min_low_counts, PUMP_PWM_PIN);
    (void) g_pump_timer.p_api->start(g_pump_timer.p_ctrl);
    pump_drv8870_after_drive_asserted();
}

void pump_drv8870_init(void)
{
    /* Pins are already applied in R_BSP_WarmStart via R_IOPORT_Open. */
    (void) g_ioport.p_api->pinWrite(&g_ioport_ctrl, PUMP_VREF_PIN, BSP_IO_LEVEL_HIGH);
    (void) g_ioport.p_api->pinWrite(&g_ioport_ctrl, PUMP_IN1_PIN, BSP_IO_LEVEL_LOW);

    (void) g_pump_timer.p_api->open(g_pump_timer.p_ctrl, g_pump_timer.p_cfg);
    (void) g_pump_timer.p_api->stop(g_pump_timer.p_ctrl);
}

void pump_drv8870_stop(void)
{
    (void) g_pump_timer.p_api->stop(g_pump_timer.p_ctrl);
    (void) g_ioport.p_api->pinWrite(&g_ioport_ctrl, PUMP_IN1_PIN, BSP_IO_LEVEL_LOW);
}

void pump_drv8870_run_forward_dc(void)
{
    (void) g_pump_timer.p_api->stop(g_pump_timer.p_ctrl);
    (void) g_ioport.p_api->pinWrite(&g_ioport_ctrl, PUMP_IN1_PIN, BSP_IO_LEVEL_HIGH);
    pump_drv8870_after_drive_asserted();
}

void pump_drv8870_set_power(uint8_t power_percent)
{
    if (power_percent == 0U)
    {
        pump_drv8870_stop();
        return;
    }

    if (power_percent >= 100U)
    {
        pump_drv8870_run_forward_dc();
        return;
    }

    uint32_t const period = pump_period_counts();

    /*
     * 实测：本板 GPT GTIOC 与原先假设相反，linear(pct) 会使力度表现为 (100-pct)%；改用 period - linear(pct)。
     */
    uint32_t const duty_compare = period -
        (uint32_t) (((uint64_t) period * (uint32_t) power_percent) / 100U);

    (void) g_ioport.p_api->pinWrite(&g_ioport_ctrl, PUMP_IN1_PIN, BSP_IO_LEVEL_HIGH);
    (void) g_pump_timer.p_api->dutyCycleSet(g_pump_timer.p_ctrl, duty_compare, PUMP_PWM_PIN);
    (void) g_pump_timer.p_api->start(g_pump_timer.p_ctrl);
    pump_drv8870_after_drive_asserted();
}

void pump_drv8870_run_dc_time_average(uint8_t power_percent, uint32_t duration_ms)
{
    if (duration_ms == 0U)
    {
        return;
    }

    if (power_percent >= 100U)
    {
        pump_drv8870_run_forward_dc();
        R_BSP_SoftwareDelay(duration_ms, BSP_DELAY_UNITS_MILLISECONDS);
        pump_drv8870_stop();
        return;
    }

    if (power_percent == 0U)
    {
        pump_drv8870_stop();
        R_BSP_SoftwareDelay(duration_ms, BSP_DELAY_UNITS_MILLISECONDS);
        return;
    }

    /*
     * 关断段不能用 stop()（双低 >1 ms 会进 DRV8870 休眠，反复醒不来转）。
     * 关断段用制动 IN1=1 IN2=1，保持芯片醒着。
     */
    uint32_t const slice_ms = 40U;
    uint32_t       elapsed  = 0U;

    while (elapsed < duration_ms)
    {
        uint32_t chunk = duration_ms - elapsed;
        if (chunk > slice_ms)
        {
            chunk = slice_ms;
        }

        uint32_t const on_ms  = (chunk * (uint32_t) power_percent) / 100U;
        uint32_t const off_ms = chunk - on_ms;

        if (on_ms > 0U)
        {
            pump_drv8870_run_forward_dc();
            R_BSP_SoftwareDelay(on_ms, BSP_DELAY_UNITS_MILLISECONDS);
        }

        if (off_ms > 0U)
        {
            pump_drv8870_brake_holding();
            R_BSP_SoftwareDelay(off_ms, BSP_DELAY_UNITS_MILLISECONDS);
        }

        elapsed += chunk;
    }

    pump_drv8870_stop();
}
