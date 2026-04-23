/* generated thread source file - do not edit */
#include "THS_Task.h"

#if 1
                static StaticTask_t THS_Task_memory;
                #if defined(__ARMCC_VERSION)           /* AC6 compiler */
                static uint8_t THS_Task_stack[1024] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.thread") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #else
                static uint8_t THS_Task_stack[1024] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.THS_Task") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #endif
                #endif
                TaskHandle_t THS_Task;
                void THS_Task_create(void);
                static void THS_Task_func(void * pvParameters);
                void rtos_startup_err_callback(void * p_instance, void * p_data);
                void rtos_startup_common_init(void);
iic_b_master_instance_ctrl_t g_ths_i2c_master_ctrl;
const iic_b_master_extended_cfg_t g_ths_i2c_master_extend =
{
    .timeout_mode             = IIC_B_MASTER_TIMEOUT_MODE_SHORT,
    .timeout_scl_low          = IIC_B_MASTER_TIMEOUT_SCL_LOW_ENABLED,
    .smbus_operation         = 0,
    /* Actual calculated bitrate: 99800. Actual calculated duty cycle: 50%. */ .clock_settings.brl_value = 244, .clock_settings.brh_value = 243, .clock_settings.cks_value = 1, .clock_settings.sdod_value = 0, .clock_settings.sdodcs_value = 0, .iic_clock_freq = 50000000,
};
const i2c_master_cfg_t g_ths_i2c_master_cfg =
{
    .channel             = 0,
    .rate                = I2C_MASTER_RATE_STANDARD,
    .slave               = 0x44,
    .addr_mode           = I2C_MASTER_ADDR_MODE_7BIT,
#define RA_NOT_DEFINED (1)
#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
                .p_transfer_tx       = NULL,
#else
                .p_transfer_tx       = &RA_NOT_DEFINED,
#endif
#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
                .p_transfer_rx       = NULL,
#else
                .p_transfer_rx       = &RA_NOT_DEFINED,
#endif
#undef RA_NOT_DEFINED
    .p_callback          = THS_I2CMaster_CpltCallback,
    .p_context           = NULL,
#if defined(VECTOR_NUMBER_IICB0_RXI)
    .rxi_irq             = VECTOR_NUMBER_IICB0_RXI,
#else
    .rxi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_TXI)
    .txi_irq             = VECTOR_NUMBER_IICB0_TXI,
#else
    .txi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_TEND)
    .tei_irq             = VECTOR_NUMBER_IICB0_TEND,
#elif defined(VECTOR_NUMBER_IICB0_TEI)
    .tei_irq             = VECTOR_NUMBER_IICB0_TEI,
#else
    .tei_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_EEI)
    .eri_irq             = VECTOR_NUMBER_IICB0_EEI,
#elif defined(VECTOR_NUMBER_IICB0_ERI)
    .eri_irq             = VECTOR_NUMBER_IICB0_ERI,
#else
    .eri_irq             = FSP_INVALID_VECTOR,
#endif
    .ipl                 = (12),
    .p_extend            = &g_ths_i2c_master_extend,
};
/* Instance structure to use this module. */
const i2c_master_instance_t g_ths_i2c_master =
{
    .p_ctrl        = &g_ths_i2c_master_ctrl,
    .p_cfg         = &g_ths_i2c_master_cfg,
    .p_api         = &g_i2c_master_on_iic_b
};
extern uint32_t g_fsp_common_thread_count;

                const rm_freertos_port_parameters_t THS_Task_parameters =
                {
                    .p_context = (void *) NULL,
                };

                void THS_Task_create (void)
                {
                    /* Increment count so we will know the number of threads created in the RA Configuration editor. */
                    g_fsp_common_thread_count++;

                    /* Initialize each kernel object. */
                    

                    #if 1
                    THS_Task = xTaskCreateStatic(
                    #else
                    BaseType_t THS_Task_create_err = xTaskCreate(
                    #endif
                        THS_Task_func,
                        (const char *)"THS_Measure",
                        1024/4, // In words, not bytes
                        (void *) &THS_Task_parameters, //pvParameters
                        1,
                        #if 1
                        (StackType_t *)&THS_Task_stack,
                        (StaticTask_t *)&THS_Task_memory
                        #else
                        & THS_Task
                        #endif
                    );

                    #if 1
                    if (NULL == THS_Task)
                    {
                        rtos_startup_err_callback(THS_Task, 0);
                    }
                    #else
                    if (pdPASS != THS_Task_create_err)
                    {
                        rtos_startup_err_callback(THS_Task, 0);
                    }
                    #endif
                }
                static void THS_Task_func (void * pvParameters)
                {
                    /* Initialize common components */
                    rtos_startup_common_init();

                    /* Initialize each module instance. */
                    

                    #if (1 == BSP_TZ_NONSECURE_BUILD) && (1 == 1)
                    /* When FreeRTOS is used in a non-secure TrustZone application, portALLOCATE_SECURE_CONTEXT must be called prior
                     * to calling any non-secure callable function in a thread. The parameter is unused in the FSP implementation.
                     * If no slots are available then configASSERT() will be called from vPortSVCHandler_C(). If this occurs, the
                     * application will need to either increase the value of the "Process Stack Slots" Property in the rm_tz_context
                     * module in the secure project or decrease the number of threads in the non-secure project that are allocating
                     * a secure context. Users can control which threads allocate a secure context via the Properties tab when
                     * selecting each thread. Note that the idle thread in FreeRTOS requires a secure context so the application
                     * will need at least 1 secure context even if no user threads make secure calls. */
                     portALLOCATE_SECURE_CONTEXT(0);
                    #endif

                    /* Enter user code for this thread. Pass task handle. */
                    THS_Task_entry(pvParameters);
                }
