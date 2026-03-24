#include "hal_data.h"
#include "pc_control.h"

void hal_entry(void)
{
	PcControl_Init();
	PcControl_Run();
	
#if BSP_TZ_SECURE_BUILD
    R_BSP_NonSecureEnter();
#endif	
}

#ifdef ULTRA_TEST
#include "ultrasound.h"

float g_ultra_dist = 0.0f;

void hal_entry(void)
{
	Ultrasound_Init();
	while(1)
	{
		g_ultra_dist = Ultrasound_GetDistance();
		R_BSP_SoftwareDelay(80, BSP_DELAY_UNITS_MILLISECONDS);
	}
#if BSP_TZ_SECURE_BUILD
    R_BSP_NonSecureEnter();
#endif
}
#endif

#if BSP_TZ_SECURE_BUILD

FSP_CPP_HEADER
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();

BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ()
{

}
FSP_CPP_FOOTER

#endif
