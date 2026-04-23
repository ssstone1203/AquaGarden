/* generated thread source file - do not edit */
#include "Communicate_Task.h"

#if 1
                static StaticTask_t Communicate_Task_memory;
                #if defined(__ARMCC_VERSION)           /* AC6 compiler */
                static uint8_t Communicate_Task_stack[1024] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.thread") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #else
                static uint8_t Communicate_Task_stack[1024] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.Communicate_Task") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #endif
                #endif
                TaskHandle_t Communicate_Task;
                void Communicate_Task_create(void);
                static void Communicate_Task_func(void * pvParameters);
                void rtos_startup_err_callback(void * p_instance, void * p_data);
                void rtos_startup_common_init(void);
dmac_instance_ctrl_t g_com_spi_rx_ctrl;
transfer_info_t g_com_spi_rx_info =
{
    .transfer_settings_word_b.dest_addr_mode = TRANSFER_ADDR_MODE_INCREMENTED,
    .transfer_settings_word_b.repeat_area    = TRANSFER_REPEAT_AREA_DESTINATION,
    .transfer_settings_word_b.irq            = TRANSFER_IRQ_END,
    .transfer_settings_word_b.chain_mode     = TRANSFER_CHAIN_MODE_DISABLED,
    .transfer_settings_word_b.src_addr_mode  = TRANSFER_ADDR_MODE_FIXED,
    .transfer_settings_word_b.size           = TRANSFER_SIZE_2_BYTE,
    .transfer_settings_word_b.mode           = TRANSFER_MODE_NORMAL,
    .p_dest                                  = (void *) NULL,
    .p_src                                   = (void const *) NULL,
    .num_blocks                              = 0,
    .length                                  = 0,
};
const dmac_extended_cfg_t g_com_spi_rx_extend =
{
    .offset              = 1,
    .src_buffer_size     = 1,
#if defined(VECTOR_NUMBER_DMAC1_INT)
    .irq                 = VECTOR_NUMBER_DMAC1_INT,
#else
    .irq                 = FSP_INVALID_VECTOR,
#endif
    .ipl                 = (8),
    .channel             = 1,
    .p_callback          = g_com_spi_rx_transfer_callback,
    .p_context           = NULL,
    .activation_source   = ELC_EVENT_SPI1_RXI,
};
const transfer_cfg_t g_com_spi_rx_cfg =
{
    .p_info              = &g_com_spi_rx_info,
    .p_extend            = &g_com_spi_rx_extend,
};
/* Instance structure to use this module. */
const transfer_instance_t g_com_spi_rx =
{
    .p_ctrl        = &g_com_spi_rx_ctrl,
    .p_cfg         = &g_com_spi_rx_cfg,
    .p_api         = &g_transfer_on_dmac
};
dmac_instance_ctrl_t g_com_spi_tx_ctrl;
transfer_info_t g_com_spi_tx_info =
{
    .transfer_settings_word_b.dest_addr_mode = TRANSFER_ADDR_MODE_FIXED,
    .transfer_settings_word_b.repeat_area    = TRANSFER_REPEAT_AREA_SOURCE,
    .transfer_settings_word_b.irq            = TRANSFER_IRQ_END,
    .transfer_settings_word_b.chain_mode     = TRANSFER_CHAIN_MODE_DISABLED,
    .transfer_settings_word_b.src_addr_mode  = TRANSFER_ADDR_MODE_INCREMENTED,
    .transfer_settings_word_b.size           = TRANSFER_SIZE_2_BYTE,
    .transfer_settings_word_b.mode           = TRANSFER_MODE_NORMAL,
    .p_dest                                  = (void *) NULL,
    .p_src                                   = (void const *) NULL,
    .num_blocks                              = 0,
    .length                                  = 0,
};
const dmac_extended_cfg_t g_com_spi_tx_extend =
{
    .offset              = 1,
    .src_buffer_size     = 1,
#if defined(VECTOR_NUMBER_DMAC0_INT)
    .irq                 = VECTOR_NUMBER_DMAC0_INT,
#else
    .irq                 = FSP_INVALID_VECTOR,
#endif
    .ipl                 = (8),
    .channel             = 0,
    .p_callback          = g_com_spi_tx_transfer_callback,
    .p_context           = NULL,
    .activation_source   = ELC_EVENT_SPI1_TXI,
};
const transfer_cfg_t g_com_spi_tx_cfg =
{
    .p_info              = &g_com_spi_tx_info,
    .p_extend            = &g_com_spi_tx_extend,
};
/* Instance structure to use this module. */
const transfer_instance_t g_com_spi_tx =
{
    .p_ctrl        = &g_com_spi_tx_ctrl,
    .p_cfg         = &g_com_spi_tx_cfg,
    .p_api         = &g_transfer_on_dmac
};
#define RA_NOT_DEFINED (UINT32_MAX)
#if (RA_NOT_DEFINED) != (1)

/* If the transfer module is DMAC, define a DMAC transfer callback. */
#include "r_dmac.h"
extern void spi_tx_dmac_callback(spi_instance_ctrl_t const * const p_ctrl);

void g_com_spi_tx_transfer_callback (dmac_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
    spi_tx_dmac_callback(&g_com_spi_ctrl);
}
#endif

#if (RA_NOT_DEFINED) != (1)

/* If the transfer module is DMAC, define a DMAC transfer callback. */
#include "r_dmac.h"
extern void spi_rx_dmac_callback(spi_instance_ctrl_t const * const p_ctrl);

void g_com_spi_rx_transfer_callback (dmac_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
    spi_rx_dmac_callback(&g_com_spi_ctrl);
}
#endif
#undef RA_NOT_DEFINED

spi_instance_ctrl_t g_com_spi_ctrl;

/** SPI extended configuration for SPI HAL driver */
const spi_extended_cfg_t g_com_spi_ext_cfg =
{
    .spi_clksyn         = SPI_SSL_MODE_CLK_SYN,
    .spi_comm           = SPI_COMMUNICATION_FULL_DUPLEX,
    .ssl_polarity        = SPI_SSLP_LOW,
    .ssl_select          = SPI_SSL_SELECT_SSL0,
    .mosi_idle           = SPI_MOSI_IDLE_VALUE_FIXING_DISABLE,
    .parity              = SPI_PARITY_MODE_DISABLE,
    .byte_swap           = SPI_BYTE_SWAP_DISABLE,
    .spck_div            = {
        /* Actual calculated bitrate: 12500000. */ .spbr = 3, .brdv = 0
    },
    .spck_delay          = SPI_DELAY_COUNT_1,
    .ssl_negation_delay  = SPI_DELAY_COUNT_1,
    .next_access_delay   = SPI_DELAY_COUNT_1
 };

/** SPI configuration for SPI HAL driver */
const spi_cfg_t g_com_spi_cfg =
{
    .channel             = 1,

#if defined(VECTOR_NUMBER_SPI1_RXI)
    .rxi_irq             = VECTOR_NUMBER_SPI1_RXI,
#else
    .rxi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SPI1_TXI)
    .txi_irq             = VECTOR_NUMBER_SPI1_TXI,
#else
    .txi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SPI1_TEI)
    .tei_irq             = VECTOR_NUMBER_SPI1_TEI,
#else
    .tei_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SPI1_ERI)
    .eri_irq             = VECTOR_NUMBER_SPI1_ERI,
#else
    .eri_irq             = FSP_INVALID_VECTOR,
#endif

    .rxi_ipl             = (BSP_IRQ_DISABLED),
    .txi_ipl             = (BSP_IRQ_DISABLED),
    .tei_ipl             = (12),
    .eri_ipl             = (12),

    .operating_mode      = SPI_MODE_SLAVE,

    .clk_phase           = SPI_CLK_PHASE_EDGE_ODD,
    .clk_polarity        = SPI_CLK_POLARITY_LOW,

    .mode_fault          = SPI_MODE_FAULT_ERROR_DISABLE,
    .bit_order           = SPI_BIT_ORDER_MSB_FIRST,
    .p_transfer_tx       = g_com_spi_P_TRANSFER_TX,
    .p_transfer_rx       = g_com_spi_P_TRANSFER_RX,
    .p_callback          = Com_SPI_Callback,

    .p_context           = NULL,
    .p_extend            = (void *)&g_com_spi_ext_cfg,
};

/* Instance structure to use this module. */
const spi_instance_t g_com_spi =
{
    .p_ctrl        = &g_com_spi_ctrl,
    .p_cfg         = &g_com_spi_cfg,
    .p_api         = &g_spi_on_spi
};
extern uint32_t g_fsp_common_thread_count;

                const rm_freertos_port_parameters_t Communicate_Task_parameters =
                {
                    .p_context = (void *) NULL,
                };

                void Communicate_Task_create (void)
                {
                    /* Increment count so we will know the number of threads created in the RA Configuration editor. */
                    g_fsp_common_thread_count++;

                    /* Initialize each kernel object. */
                    

                    #if 1
                    Communicate_Task = xTaskCreateStatic(
                    #else
                    BaseType_t Communicate_Task_create_err = xTaskCreate(
                    #endif
                        Communicate_Task_func,
                        (const char *)"Com_Thread",
                        1024/4, // In words, not bytes
                        (void *) &Communicate_Task_parameters, //pvParameters
                        1,
                        #if 1
                        (StackType_t *)&Communicate_Task_stack,
                        (StaticTask_t *)&Communicate_Task_memory
                        #else
                        & Communicate_Task
                        #endif
                    );

                    #if 1
                    if (NULL == Communicate_Task)
                    {
                        rtos_startup_err_callback(Communicate_Task, 0);
                    }
                    #else
                    if (pdPASS != Communicate_Task_create_err)
                    {
                        rtos_startup_err_callback(Communicate_Task, 0);
                    }
                    #endif
                }
                static void Communicate_Task_func (void * pvParameters)
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
                    Communicate_Task_entry(pvParameters);
                }
