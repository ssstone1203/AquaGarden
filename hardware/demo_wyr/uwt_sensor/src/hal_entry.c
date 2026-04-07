#include "hal_data.h"
#include "ds18b20.h"

/*******************************************************************************************************************//**
 * Keil / µVision Debug：在 Watch 窗口添加下列符号观察（建议 Decimal 看温度，Hex 看 err/raw）。
 * - g_uwt_temp_c         ：float，摄氏度（主要结果）
 * - g_uwt_temp_millic    ：int32，毫摄氏度，例如 23680 = 23.680 °C
 * - g_uwt_temp_raw       ：int16，器件原始温度字（二进制补码，数值单位 = 1/16 °C）
 * - g_uwt_last_err       ：fsp_err_t，0 成功；34 无应答(FSP_ERR_NOT_FOUND)；37 CRC(FSP_ERR_INVALID_DATA)；20 超时
 **********************************************************************************************************************/

volatile float     g_uwt_temp_c;
volatile int32_t   g_uwt_temp_millic;
volatile int16_t   g_uwt_temp_raw;
volatile fsp_err_t g_uwt_last_err;

void hal_entry(void)
{
    float     temp_c;
    int16_t   raw_reg;
    fsp_err_t err;

#if BSP_TZ_SECURE_BUILD
    R_BSP_NonSecureEnter();
#endif

    DS18B20_Init();

    for (;;)
    {
        err = DS18B20_ReadTemperatureC(&temp_c, &raw_reg);
        g_uwt_last_err = err;
        if (FSP_SUCCESS == err)
        {
            g_uwt_temp_c       = temp_c;
            g_uwt_temp_millic  = (int32_t) (temp_c * 1000.0f);
            g_uwt_temp_raw     = raw_reg;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_SECONDS);
    }
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
