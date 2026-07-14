#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"

/* Keil Watch: inspect these after a fault to see the real cause (not Default_Handler). */
volatile uint32_t g_fault_type;
volatile uint32_t g_fault_cfsr;
volatile uint32_t g_fault_hfsr;
volatile uint32_t g_fault_bfar;
volatile uint32_t g_fault_mmfar;
volatile uint32_t g_fault_shcsr;
volatile uint32_t g_fault_lr;
volatile uint32_t g_fault_sp;
volatile uint32_t g_fault_pc;
volatile uint32_t g_fault_exc_return;
volatile uint32_t g_fault_stacked_lr;

typedef struct
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
} fault_stack_frame_t;

static uint32_t fault_get_lr(void)
{
    uint32_t lr;
    __asm volatile ("MOV %0, LR" : "=r" (lr));
    return lr;
}

static uint32_t fault_get_msp(void)
{
    uint32_t sp;
    __asm volatile ("MRS %0, MSP" : "=r" (sp));
    return sp;
}

static uint32_t fault_get_psp(void)
{
    uint32_t sp;
    __asm volatile ("MRS %0, PSP" : "=r" (sp));
    return sp;
}

static fault_stack_frame_t * fault_get_frame(uint32_t exc_lr, uint32_t * p_sp)
{
    uint32_t sp = (0U != (exc_lr & 0x4U)) ? fault_get_psp() : fault_get_msp();

    if (0U == (exc_lr & 0x10U))
    {
        /* Cortex-M85 FPU extended stack frame: 18 extra words below the basic frame. */
        sp += 72U;
    }

    *p_sp = sp;
    return (fault_stack_frame_t *) sp;
}

static void fault_capture(uint32_t type)
{
    uint32_t              exc_lr = fault_get_lr();
    uint32_t              sp     = 0U;
    fault_stack_frame_t * frame  = fault_get_frame(exc_lr, &sp);

    g_fault_type        = type;
    g_fault_lr          = exc_lr;
    g_fault_exc_return  = exc_lr;
    g_fault_sp          = sp;
    g_fault_pc          = frame->pc;
    g_fault_stacked_lr  = frame->lr;
    g_fault_cfsr        = SCB->CFSR;
    g_fault_hfsr        = SCB->HFSR;
    g_fault_bfar        = SCB->BFAR;
    g_fault_mmfar       = SCB->MMFAR;
    g_fault_shcsr       = SCB->SHCSR;

    __BKPT(0);
    while (1)
    {
        __NOP();
    }
}

void HardFault_Handler(void)
{
    fault_capture(1U);
}

void MemManage_Handler(void)
{
    fault_capture(2U);
}

void BusFault_Handler(void)
{
    fault_capture(3U);
}

void UsageFault_Handler(void)
{
    fault_capture(4U);
}

void SecureFault_Handler(void)
{
    fault_capture(5U);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char * pcTaskName)
{
    FSP_PARAMETER_NOT_USED(xTask);
    FSP_PARAMETER_NOT_USED(pcTaskName);
    g_fault_type = 99U;
    __BKPT(0);
    for (;;)
    {
        __NOP();
    }
}
