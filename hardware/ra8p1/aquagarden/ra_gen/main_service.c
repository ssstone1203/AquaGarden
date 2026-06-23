/* generated thread source file - do not edit */
#include "main_service.h"


/* Macros to tie dynamic ELC links to adc_b_trigger_sync_elc option in adc_b_trigger_t. */
                     #define ADC_B_TRIGGER_ADC_B0        ADC_B_TRIGGER_SYNC_ELC
                     #define ADC_B_TRIGGER_ADC_B0_B      ADC_B_TRIGGER_SYNC_ELC
                     #define ADC_B_TRIGGER_ADC_B1        ADC_B_TRIGGER_SYNC_ELC
                     #define ADC_B_TRIGGER_ADC_B1_B      ADC_B_TRIGGER_SYNC_ELC
#if 1
                static StaticTask_t main_service_memory;
                #if defined(__ARMCC_VERSION)           /* AC6 compiler */
                static uint8_t main_service_stack[4096] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.thread") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #else
                static uint8_t main_service_stack[4096] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.main_service") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #endif
                #endif
                TaskHandle_t main_service;
                void main_service_create(void);
                static void main_service_func(void * pvParameters);
                void rtos_startup_err_callback(void * p_instance, void * p_data);
                void rtos_startup_common_init(void);
sci_b_uart_instance_ctrl_t     g_uart0_ctrl;

            sci_b_baud_setting_t               g_uart0_baud_setting =
            {
                /* Baud rate calculated with 0.160% error. */ .baudrate_bits_b.abcse = 0, .baudrate_bits_b.abcs = 0, .baudrate_bits_b.bgdm = 1, .baudrate_bits_b.cks = 0, .baudrate_bits_b.brr = 64, .baudrate_bits_b.mddr = (uint8_t) 256, .baudrate_bits_b.brme = false
            };

            /** UART extended configuration for UARTonSCI HAL driver */
            const sci_b_uart_extended_cfg_t g_uart0_cfg_extend =
            {
                .clock                = SCI_B_UART_CLOCK_INT,
                .rx_edge_start          = SCI_B_UART_START_BIT_FALLING_EDGE,
                .noise_cancel         = SCI_B_UART_NOISE_CANCELLATION_DISABLE,
                .rx_fifo_trigger        = SCI_B_UART_RX_FIFO_TRIGGER_MAX,
                .p_baud_setting         = &g_uart0_baud_setting,
                .flow_control           = SCI_B_UART_FLOW_CONTROL_RTS,
                #if 0xFF != 0xFF
                .flow_control_pin       = BSP_IO_PORT_FF_PIN_0xFF,
                #else
                .flow_control_pin       = (bsp_io_port_pin_t) UINT16_MAX,
                #endif
                .rs485_setting          = {
                    .enable = SCI_B_UART_RS485_DISABLE,
                    .polarity = SCI_B_UART_RS485_DE_POLARITY_HIGH,
                    .assertion_time = 1,
                    .negation_time = 1,
                },
                .delay_cycles = 0,
            };

            /** UART interface configuration */
            const uart_cfg_t g_uart0_cfg =
            {
                .channel             = 4,
                .data_bits           = UART_DATA_BITS_8,
                .parity              = UART_PARITY_OFF,
                .stop_bits           = UART_STOP_BITS_1,
                .p_callback          = NULL,
                .p_context           = NULL,
                .p_extend            = &g_uart0_cfg_extend,
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
                .rxi_ipl             = (12),
                .txi_ipl             = (12),
                .tei_ipl             = (12),
                .eri_ipl             = (12),
#if defined(VECTOR_NUMBER_SCI4_RXI)
                .rxi_irq             = VECTOR_NUMBER_SCI4_RXI,
#else
                .rxi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI4_TXI)
                .txi_irq             = VECTOR_NUMBER_SCI4_TXI,
#else
                .txi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI4_TEI)
                .tei_irq             = VECTOR_NUMBER_SCI4_TEI,
#else
                .tei_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI4_ERI)
                .eri_irq             = VECTOR_NUMBER_SCI4_ERI,
#else
                .eri_irq             = FSP_INVALID_VECTOR,
#endif
            };

/* Instance structure to use this module. */
const uart_instance_t g_uart0 =
{
    .p_ctrl        = &g_uart0_ctrl,
    .p_cfg         = &g_uart0_cfg,
    .p_api         = &g_uart_on_sci_b
};
usb_instance_ctrl_t g_basic0_ctrl;

#if !defined(NULL)
extern usb_descriptor_t NULL;
#endif
#define RA_NOT_DEFINED (1)
            const usb_cfg_t g_basic0_cfg =
            {
                .usb_mode  = USB_MODE_HOST,
                .usb_speed = USB_SPEED_HS,
                .module_number = 1,
                .type = USB_CLASS_HVND,
#if defined(NULL)
                .p_usb_reg = NULL,
#else
                .p_usb_reg = &NULL,
#endif
                .usb_complience_cb = NULL,
#if defined(VECTOR_NUMBER_USBFS_INT)
                .irq       = VECTOR_NUMBER_USBFS_INT,
#else
                .irq       = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_RESUME)
                .irq_r     = VECTOR_NUMBER_USBFS_RESUME,
#else
                .irq_r     = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_FIFO_0)
                .irq_d0    = VECTOR_NUMBER_USBFS_FIFO_0,
#else
                .irq_d0    = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_FIFO_1)
                .irq_d1    = VECTOR_NUMBER_USBFS_FIFO_1,
#else
                .irq_d1    = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_USB_INT_RESUME)
                .hsirq     = VECTOR_NUMBER_USBHS_USB_INT_RESUME,
#else
                .hsirq     = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_FIFO_0)
                .hsirq_d0  = VECTOR_NUMBER_USBHS_FIFO_0,
#else
                .hsirq_d0  = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_FIFO_1)
                .hsirq_d1  = VECTOR_NUMBER_USBHS_FIFO_1,
#else
                .hsirq_d1  = FSP_INVALID_VECTOR,
#endif
                .ipl       = (12),
                .ipl_r     = (12),
                .ipl_d0    = (12),
                .ipl_d1    = (12),
                .hsipl     = (12),
                .hsipl_d0  = (12),
                .hsipl_d1  = (12),
#if (BSP_CFG_RTOS == 0) && defined(USB_CFG_HMSC_USE)
                .p_usb_apl_callback = NULL,
#else
                .p_usb_apl_callback = NULL,
#endif
#if defined(NULL)
                .p_context = NULL,
#else
                .p_context = (void *) &NULL,
#endif
#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
#else
                .p_transfer_tx = &RA_NOT_DEFINED,
#endif
#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
#else
                .p_transfer_rx = &RA_NOT_DEFINED,
#endif
            };
#undef RA_NOT_DEFINED

/* Instance structure to use this module. */
const usb_instance_t g_basic0 =
{
    .p_ctrl        = &g_basic0_ctrl,
    .p_cfg         = &g_basic0_cfg,
    .p_api         = &g_usb_on_usb,
};
gpt_instance_ctrl_t g_pump_timer_ctrl;
#if 0
const gpt_extended_pwm_cfg_t g_pump_timer_pwm_extend =
{
    .trough_ipl             = (BSP_IRQ_DISABLED),
#if defined(VECTOR_NUMBER_GPT1_COUNTER_UNDERFLOW)
    .trough_irq             = VECTOR_NUMBER_GPT1_COUNTER_UNDERFLOW,
#else
    .trough_irq             = FSP_INVALID_VECTOR,
#endif
    .poeg_link              = GPT_POEG_LINK_POEG0,
    .output_disable         = (gpt_output_disable_t) ( GPT_OUTPUT_DISABLE_NONE),
    .adc_trigger            = (gpt_adc_trigger_t) ( GPT_ADC_TRIGGER_NONE),
    .dead_time_count_up     = 0,
    .dead_time_count_down   = 0,
    .adc_a_compare_match    = 0,
    .adc_b_compare_match    = 0,
    .interrupt_skip_source  = GPT_INTERRUPT_SKIP_SOURCE_NONE,
    .interrupt_skip_count   = GPT_INTERRUPT_SKIP_COUNT_0,
    .interrupt_skip_adc     = GPT_INTERRUPT_SKIP_ADC_NONE,
    .gtioca_disable_setting = GPT_GTIOC_DISABLE_PROHIBITED,
    .gtiocb_disable_setting = GPT_GTIOC_DISABLE_PROHIBITED,
};
#endif
const gpt_extended_cfg_t g_pump_timer_extend =
{
    .gtioca = { .output_enabled = true,
                .stop_level     = GPT_PIN_LEVEL_LOW
              },
    .gtiocb = { .output_enabled = false,
                .stop_level     = GPT_PIN_LEVEL_LOW
              },
    .start_source        = (gpt_source_t) ( GPT_SOURCE_NONE),
    .stop_source         = (gpt_source_t) ( GPT_SOURCE_NONE),
    .clear_source        = (gpt_source_t) ( GPT_SOURCE_NONE),
    .count_up_source     = (gpt_source_t) ( GPT_SOURCE_NONE),
    .count_down_source   = (gpt_source_t) ( GPT_SOURCE_NONE),
    .capture_a_source    = (gpt_source_t) ( GPT_SOURCE_NONE),
    .capture_b_source    = (gpt_source_t) ( GPT_SOURCE_NONE),
    .capture_a_ipl       = (BSP_IRQ_DISABLED),
    .capture_b_ipl       = (BSP_IRQ_DISABLED),
    .compare_match_c_ipl = (BSP_IRQ_DISABLED),
    .compare_match_d_ipl = (BSP_IRQ_DISABLED),
    .compare_match_e_ipl = (BSP_IRQ_DISABLED),
    .compare_match_f_ipl = (BSP_IRQ_DISABLED),
#if defined(VECTOR_NUMBER_GPT1_CAPTURE_COMPARE_A)
    .capture_a_irq         = VECTOR_NUMBER_GPT1_CAPTURE_COMPARE_A,
#else
    .capture_a_irq         = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT1_CAPTURE_COMPARE_B)
    .capture_b_irq         = VECTOR_NUMBER_GPT1_CAPTURE_COMPARE_B,
#else
    .capture_b_irq         = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT1_COMPARE_C)
    .compare_match_c_irq   = VECTOR_NUMBER_GPT1_COMPARE_C,
#else
    .compare_match_c_irq   = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT1_COMPARE_D)
    .compare_match_d_irq   = VECTOR_NUMBER_GPT1_COMPARE_D,
#else
    .compare_match_d_irq   = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT1_COMPARE_E)
    .compare_match_e_irq   = VECTOR_NUMBER_GPT1_COMPARE_E,
#else
    .compare_match_e_irq   = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT1_COMPARE_F)
    .compare_match_f_irq   = VECTOR_NUMBER_GPT1_COMPARE_F,
#else
    .compare_match_f_irq   = FSP_INVALID_VECTOR,
#endif
     .compare_match_value = { (uint32_t)0x0, /* CMP_A */(uint32_t)0x0, /* CMP_B */(uint32_t)0x0, /* CMP_C */(uint32_t)0x0, /* CMP_D */(uint32_t)0x0, /* CMP_E */(uint32_t)0x0, /* CMP_F */ }, .compare_match_status = ((0U << 5U) | (0U << 4U) | (0U << 3U) | (0U << 2U) | (0U << 1U) | 0U),
    .capture_filter_gtioca = GPT_CAPTURE_FILTER_NONE,
    .capture_filter_gtiocb = GPT_CAPTURE_FILTER_NONE,
#if 0
    .p_pwm_cfg             = &g_pump_timer_pwm_extend,
#else
    .p_pwm_cfg             = NULL,
#endif
#if 0
    .gtior_setting.gtior_b.gtioa  = (0U << 4U) | (0U << 2U) | (0U << 0U),
    .gtior_setting.gtior_b.oadflt = (uint32_t) GPT_PIN_LEVEL_LOW,
    .gtior_setting.gtior_b.oahld  = 0U,
    .gtior_setting.gtior_b.oae    = (uint32_t) true,
    .gtior_setting.gtior_b.oadf   = (uint32_t) GPT_GTIOC_DISABLE_PROHIBITED,
    .gtior_setting.gtior_b.nfaen  = ((uint32_t) GPT_CAPTURE_FILTER_NONE & 1U),
    .gtior_setting.gtior_b.nfcsa  = ((uint32_t) GPT_CAPTURE_FILTER_NONE >> 1U),
    .gtior_setting.gtior_b.gtiob  = (0U << 4U) | (0U << 2U) | (0U << 0U),
    .gtior_setting.gtior_b.obdflt = (uint32_t) GPT_PIN_LEVEL_LOW,
    .gtior_setting.gtior_b.obhld  = 0U,
    .gtior_setting.gtior_b.obe    = (uint32_t) false,
    .gtior_setting.gtior_b.obdf   = (uint32_t) GPT_GTIOC_DISABLE_PROHIBITED,
    .gtior_setting.gtior_b.nfben  = ((uint32_t) GPT_CAPTURE_FILTER_NONE & 1U),
    .gtior_setting.gtior_b.nfcsb  = ((uint32_t) GPT_CAPTURE_FILTER_NONE >> 1U),
#else
    .gtior_setting.gtior = 0U,
#endif

    .gtioca_polarity = GPT_GTIOC_POLARITY_NORMAL,
    .gtiocb_polarity = GPT_GTIOC_POLARITY_NORMAL,
};

const timer_cfg_t g_pump_timer_cfg =
{
    .mode                = TIMER_MODE_PERIODIC,
    /* Actual period: 0.000262144 seconds. Actual duty: 50%. */ .period_counts = (uint32_t) 0x10000, .duty_cycle_counts = 0x8000, .source_div = (timer_source_div_t)0,
    .channel             = 1,
    .p_callback          = NULL,
    /** If NULL then do not add & */
#if defined(NULL)
    .p_context           = NULL,
#else
    .p_context           = (void *) &NULL,
#endif
    .p_extend            = &g_pump_timer_extend,
    .cycle_end_ipl       = (BSP_IRQ_DISABLED),
#if defined(VECTOR_NUMBER_GPT1_COUNTER_OVERFLOW)
    .cycle_end_irq       = VECTOR_NUMBER_GPT1_COUNTER_OVERFLOW,
#else
    .cycle_end_irq       = FSP_INVALID_VECTOR,
#endif
};
/* Instance structure to use this module. */
const timer_instance_t g_pump_timer =
{
    .p_ctrl        = &g_pump_timer_ctrl,
    .p_cfg         = &g_pump_timer_cfg,
    .p_api         = &g_timer_on_gpt
};
#define RA_NOT_DEFINED (0)

                               #if (1) // Define Virtual Channel 0 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_0_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_0,

                                   .channel_cfg_bits.group           = (1),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_1,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (1) // Define Virtual Channel 1 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_1_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_1,

                                   .channel_cfg_bits.group           = (1),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_2,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 2 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_2_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_2,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 3 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_3_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_3,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 4 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_4_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_4,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 5 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_5_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_5,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 6 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_6_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_6,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 7 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_7_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_7,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 8 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_8_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_8,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 9 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_9_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_9,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 10 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_10_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_10,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 11 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_11_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_11,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 12 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_12_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_12,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 13 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_13_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_13,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 14 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_14_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_14,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 15 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_15_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_15,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 16 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_16_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_16,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 17 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_17_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_17,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 18 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_18_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_18,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 19 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_19_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_19,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 20 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_20_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_20,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 21 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_21_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_21,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 22 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_22_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_22,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 23 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_23_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_23,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 24 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_24_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_24,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 25 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_25_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_25,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 26 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_26_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_26,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 27 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_27_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_27,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 28 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_28_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_28,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 29 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_29_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_29,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 30 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_30_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_30,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 31 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_31_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_31,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if (0) // Define Virtual Channel 32 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_32_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_32,

                                   .channel_cfg_bits.group           = (0),
                                   .channel_cfg_bits.channel         = ADC_CHANNEL_0,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = ADC_B_SAMPLING_STATE_TABLE_0,

                                   .channel_control_a_bits.digital_filter_id = 0x0,
                                   .channel_control_a_bits.offset_table_id = ADC_B_USER_OFFSET_TABLE_SELECTION_DISABLED,
                                   .channel_control_a_bits.gain_table_id = ADC_B_USER_GAIN_TABLE_SELECTION_DISABLED,

                                   .channel_control_b_bits.addition_average_mode = (ADC_B_ADD_AVERAGE_OFF),
                                   .channel_control_b_bits.addition_average_count = ADC_B_ADD_AVERAGE_1,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = ADC_B_LIMIT_CLIP_TABLE_SELECTION_NONE,
                                   .channel_control_c_bits.channel_data_format = (ADC_B_DATA_FORMAT_12_BIT),
                                   .channel_control_c_bits.data_is_unsigned = true,
                               };
                               #endif

                               #if RA_NOT_DEFINED // Define Virtual Channel 33 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_33_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_33,

                                   .channel_cfg_bits.group           = RA_NOT_DEFINED,
                                   .channel_cfg_bits.channel         = RA_NOT_DEFINED,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = RA_NOT_DEFINED,

                                   .channel_control_a_bits.digital_filter_id = RA_NOT_DEFINED,
                                   .channel_control_a_bits.offset_table_id = RA_NOT_DEFINED,
                                   .channel_control_a_bits.gain_table_id = RA_NOT_DEFINED,

                                   .channel_control_b_bits.addition_average_mode = RA_NOT_DEFINED,
                                   .channel_control_b_bits.addition_average_count = RA_NOT_DEFINED,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = RA_NOT_DEFINED,
                                   .channel_control_c_bits.channel_data_format = RA_NOT_DEFINED,
                                   .channel_control_c_bits.data_is_unsigned = false,
                               };
                               #endif

                               #if RA_NOT_DEFINED // Define Virtual Channel 34 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_34_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_34,

                                   .channel_cfg_bits.group           = RA_NOT_DEFINED,
                                   .channel_cfg_bits.channel         = RA_NOT_DEFINED,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = RA_NOT_DEFINED,

                                   .channel_control_a_bits.digital_filter_id = RA_NOT_DEFINED,
                                   .channel_control_a_bits.offset_table_id = RA_NOT_DEFINED,
                                   .channel_control_a_bits.gain_table_id = RA_NOT_DEFINED,

                                   .channel_control_b_bits.addition_average_mode = RA_NOT_DEFINED,
                                   .channel_control_b_bits.addition_average_count = RA_NOT_DEFINED,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = RA_NOT_DEFINED,
                                   .channel_control_c_bits.channel_data_format = RA_NOT_DEFINED,
                                   .channel_control_c_bits.data_is_unsigned = false,
                               };
                               #endif

                               #if RA_NOT_DEFINED // Define Virtual Channel 35 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_35_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_35,

                                   .channel_cfg_bits.group           = RA_NOT_DEFINED,
                                   .channel_cfg_bits.channel         = RA_NOT_DEFINED,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = RA_NOT_DEFINED,

                                   .channel_control_a_bits.digital_filter_id = RA_NOT_DEFINED,
                                   .channel_control_a_bits.offset_table_id = RA_NOT_DEFINED,
                                   .channel_control_a_bits.gain_table_id = RA_NOT_DEFINED,

                                   .channel_control_b_bits.addition_average_mode = RA_NOT_DEFINED,
                                   .channel_control_b_bits.addition_average_count = RA_NOT_DEFINED,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = RA_NOT_DEFINED,
                                   .channel_control_c_bits.channel_data_format = RA_NOT_DEFINED,
                                   .channel_control_c_bits.data_is_unsigned = false,
                               };
                               #endif

                               #if RA_NOT_DEFINED // Define Virtual Channel 36 if it's assigned to a scan group.
                               const adc_b_virtual_channel_cfg_t g_adc0_virtual_channel_36_cfg =
                               {
                                   .channel_id             = ADC_B_VIRTUAL_CHANNEL_36,

                                   .channel_cfg_bits.group           = RA_NOT_DEFINED,
                                   .channel_cfg_bits.channel         = RA_NOT_DEFINED,
                                   .channel_cfg_bits.differential    = 0,
                                   .channel_cfg_bits.sample_table_id = RA_NOT_DEFINED,

                                   .channel_control_a_bits.digital_filter_id = RA_NOT_DEFINED,
                                   .channel_control_a_bits.offset_table_id = RA_NOT_DEFINED,
                                   .channel_control_a_bits.gain_table_id = RA_NOT_DEFINED,

                                   .channel_control_b_bits.addition_average_mode = RA_NOT_DEFINED,
                                   .channel_control_b_bits.addition_average_count = RA_NOT_DEFINED,
                                   .channel_control_b_bits.compare_match_enable = false,

                                   .channel_control_c_bits.limiter_clip_table_id = RA_NOT_DEFINED,
                                   .channel_control_c_bits.channel_data_format = RA_NOT_DEFINED,
                                   .channel_control_c_bits.data_is_unsigned = false,
                               };
                               #endif
#if (((1) == 1)||((1) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||(RA_NOT_DEFINED == 1)||(RA_NOT_DEFINED == 1)||(RA_NOT_DEFINED == 1)||(RA_NOT_DEFINED == 1))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_0_virtual_channels[] = {
                                  #if ((1) == 1)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 1)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 1)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 1)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 1)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 1)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 1)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif
#if (((1) == 2)||((1) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||(RA_NOT_DEFINED == 2)||(RA_NOT_DEFINED == 2)||(RA_NOT_DEFINED == 2)||(RA_NOT_DEFINED == 2))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_1_virtual_channels[] = {
                                  #if ((1) == 2)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 2)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 2)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 2)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 2)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 2)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 2)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif
#if (((1) == 3)||((1) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||(RA_NOT_DEFINED == 3)||(RA_NOT_DEFINED == 3)||(RA_NOT_DEFINED == 3)||(RA_NOT_DEFINED == 3))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_2_virtual_channels[] = {
                                  #if ((1) == 3)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 3)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 3)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 3)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 3)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 3)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 3)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif
#if (((1) == 4)||((1) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||(RA_NOT_DEFINED == 4)||(RA_NOT_DEFINED == 4)||(RA_NOT_DEFINED == 4)||(RA_NOT_DEFINED == 4))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_3_virtual_channels[] = {
                                  #if ((1) == 4)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 4)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 4)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 4)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 4)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 4)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 4)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif
#if (((1) == 5)||((1) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||(RA_NOT_DEFINED == 5)||(RA_NOT_DEFINED == 5)||(RA_NOT_DEFINED == 5)||(RA_NOT_DEFINED == 5))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_4_virtual_channels[] = {
                                  #if ((1) == 5)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 5)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 5)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 5)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 5)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 5)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 5)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif
#if (((1) == 6)||((1) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||(RA_NOT_DEFINED == 6)||(RA_NOT_DEFINED == 6)||(RA_NOT_DEFINED == 6)||(RA_NOT_DEFINED == 6))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_5_virtual_channels[] = {
                                  #if ((1) == 6)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 6)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 6)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 6)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 6)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 6)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 6)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif
#if (((1) == 7)||((1) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||(RA_NOT_DEFINED == 7)||(RA_NOT_DEFINED == 7)||(RA_NOT_DEFINED == 7)||(RA_NOT_DEFINED == 7))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_6_virtual_channels[] = {
                                  #if ((1) == 7)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 7)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 7)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 7)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 7)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 7)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 7)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif
#if (((1) == 8)||((1) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||(RA_NOT_DEFINED == 8)||(RA_NOT_DEFINED == 8)||(RA_NOT_DEFINED == 8)||(RA_NOT_DEFINED == 8))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_7_virtual_channels[] = {
                                  #if ((1) == 8)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 8)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 8)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 8)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 8)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 8)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 8)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif
#if (((1) == 9)||((1) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||(RA_NOT_DEFINED == 9)||(RA_NOT_DEFINED == 9)||(RA_NOT_DEFINED == 9)||(RA_NOT_DEFINED == 9))
const adc_b_virtual_channel_cfg_t *const g_adc0_group_8_virtual_channels[] = {
                                  #if ((1) == 9)
                                      &g_adc0_virtual_channel_0_cfg,
                                  #endif

                                  #if ((1) == 9)
                                      &g_adc0_virtual_channel_1_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_2_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_3_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_4_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_5_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_6_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_7_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_8_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_9_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_10_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_11_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_12_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_13_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_14_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_15_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_16_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_17_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_18_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_19_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_20_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_21_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_22_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_23_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_24_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_25_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_26_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_27_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_28_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_29_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_30_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_31_cfg,
                                  #endif

                                  #if ((0) == 9)
                                      &g_adc0_virtual_channel_32_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 9)
                                      &g_adc0_virtual_channel_33_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 9)
                                      &g_adc0_virtual_channel_34_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 9)
                                      &g_adc0_virtual_channel_35_cfg,
                                  #endif

                                  #if (RA_NOT_DEFINED == 9)
                                      &g_adc0_virtual_channel_36_cfg,
                                  #endif
                                  };
                                  #endif

                     #if (0) // Define Scan Group 0 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_0_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_0,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 1)+((1) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+((0) == 1)+(RA_NOT_DEFINED == 1)+(RA_NOT_DEFINED == 1)+(RA_NOT_DEFINED == 1)+(RA_NOT_DEFINED == 1)),
#if (((1) == 1)||((1) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||((0) == 1)||(RA_NOT_DEFINED == 1)||(RA_NOT_DEFINED == 1)||(RA_NOT_DEFINED == 1)||(RA_NOT_DEFINED == 1))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_0_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif

                     #if (0) // Define Scan Group 1 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_1_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_1,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 2)+((1) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+((0) == 2)+(RA_NOT_DEFINED == 2)+(RA_NOT_DEFINED == 2)+(RA_NOT_DEFINED == 2)+(RA_NOT_DEFINED == 2)),
#if (((1) == 2)||((1) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||((0) == 2)||(RA_NOT_DEFINED == 2)||(RA_NOT_DEFINED == 2)||(RA_NOT_DEFINED == 2)||(RA_NOT_DEFINED == 2))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_1_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif

                     #if (0) // Define Scan Group 2 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_2_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_2,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 3)+((1) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+((0) == 3)+(RA_NOT_DEFINED == 3)+(RA_NOT_DEFINED == 3)+(RA_NOT_DEFINED == 3)+(RA_NOT_DEFINED == 3)),
#if (((1) == 3)||((1) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||((0) == 3)||(RA_NOT_DEFINED == 3)||(RA_NOT_DEFINED == 3)||(RA_NOT_DEFINED == 3)||(RA_NOT_DEFINED == 3))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_2_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif

                     #if (0) // Define Scan Group 3 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_3_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_3,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 4)+((1) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+((0) == 4)+(RA_NOT_DEFINED == 4)+(RA_NOT_DEFINED == 4)+(RA_NOT_DEFINED == 4)+(RA_NOT_DEFINED == 4)),
#if (((1) == 4)||((1) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||((0) == 4)||(RA_NOT_DEFINED == 4)||(RA_NOT_DEFINED == 4)||(RA_NOT_DEFINED == 4)||(RA_NOT_DEFINED == 4))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_3_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif

                     #if (0) // Define Scan Group 4 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_4_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_4,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 5)+((1) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+((0) == 5)+(RA_NOT_DEFINED == 5)+(RA_NOT_DEFINED == 5)+(RA_NOT_DEFINED == 5)+(RA_NOT_DEFINED == 5)),
#if (((1) == 5)||((1) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||((0) == 5)||(RA_NOT_DEFINED == 5)||(RA_NOT_DEFINED == 5)||(RA_NOT_DEFINED == 5)||(RA_NOT_DEFINED == 5))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_4_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif

                     #if (0) // Define Scan Group 5 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_5_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_5,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 6)+((1) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+((0) == 6)+(RA_NOT_DEFINED == 6)+(RA_NOT_DEFINED == 6)+(RA_NOT_DEFINED == 6)+(RA_NOT_DEFINED == 6)),
#if (((1) == 6)||((1) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||((0) == 6)||(RA_NOT_DEFINED == 6)||(RA_NOT_DEFINED == 6)||(RA_NOT_DEFINED == 6)||(RA_NOT_DEFINED == 6))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_5_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif

                     #if (0) // Define Scan Group 6 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_6_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_6,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 7)+((1) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+((0) == 7)+(RA_NOT_DEFINED == 7)+(RA_NOT_DEFINED == 7)+(RA_NOT_DEFINED == 7)+(RA_NOT_DEFINED == 7)),
#if (((1) == 7)||((1) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||((0) == 7)||(RA_NOT_DEFINED == 7)||(RA_NOT_DEFINED == 7)||(RA_NOT_DEFINED == 7)||(RA_NOT_DEFINED == 7))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_6_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif

                     #if (0) // Define Scan Group 7 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_7_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_7,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 8)+((1) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+((0) == 8)+(RA_NOT_DEFINED == 8)+(RA_NOT_DEFINED == 8)+(RA_NOT_DEFINED == 8)+(RA_NOT_DEFINED == 8)),
#if (((1) == 8)||((1) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||((0) == 8)||(RA_NOT_DEFINED == 8)||(RA_NOT_DEFINED == 8)||(RA_NOT_DEFINED == 8)||(RA_NOT_DEFINED == 8))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_7_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif

                     #if (0) // Define Scan Group 8 if it's enabled
                     const adc_b_group_cfg_t g_adc0_group_8_cfg =
                     {
                         .scan_group_id                   = ADC_GROUP_ID_8,
                         .converter_selection             = (adc_b_unit_id_t)(0),
                         .scan_group_enable               = (0),
                         .scan_end_interrupt_enable       = (1),
                         .external_trigger_enable_mask    = ( ADC_B_EXTERNAL_TRIGGER_NONE),
                         .elc_trigger_enable_mask         = (elc_peripheral_t)( 0x00),
                         .gpt_trigger_enable_mask         = ( ADC_B_GPT_TRIGGER_NONE),

                         .self_diagnosis_mask             = (ADC_B_SELF_DIAGNOSIS_DISABLED << R_ADC_B0_ADSGDCR0_DIAGVAL_Pos),

                         .limit_clip_interrupt_enable     = (1),
                         .virtual_channel_count           = 0 + (
                         ((1) == 9)+((1) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+((0) == 9)+(RA_NOT_DEFINED == 9)+(RA_NOT_DEFINED == 9)+(RA_NOT_DEFINED == 9)+(RA_NOT_DEFINED == 9)),
#if (((1) == 9)||((1) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||((0) == 9)||(RA_NOT_DEFINED == 9)||(RA_NOT_DEFINED == 9)||(RA_NOT_DEFINED == 9)||(RA_NOT_DEFINED == 9))

                         .p_virtual_channels = (adc_b_virtual_channel_cfg_t**)g_adc0_group_8_virtual_channels,
                         #else
                         .p_virtual_channels = NULL,
                         #endif
                     };
                     #endif
#if ((0)||(0)||(0)||(0)||(0)||(0)||(0)||(0)||(0))
const adc_b_group_cfg_t * const g_adc0_scan_cfg_groups[] = {
                           #if (0 != (0))
                           &g_adc0_group_0_cfg,
                           #endif

                           #if (0 != (0))
                           &g_adc0_group_1_cfg,
                           #endif

                           #if (0 != (0))
                           &g_adc0_group_2_cfg,
                           #endif

                           #if (0 != (0))
                           &g_adc0_group_3_cfg,
                           #endif

                           #if (0 != (0))
                           &g_adc0_group_4_cfg,
                           #endif

                           #if (0 != (0))
                           &g_adc0_group_5_cfg,
                           #endif

                           #if (0 != (0))
                           &g_adc0_group_6_cfg,
                           #endif

                           #if (0 != (0))
                           &g_adc0_group_7_cfg,
                           #endif

                           #if (0 != (0))
                           &g_adc0_group_8_cfg,
                           #endif
                           };
                           #endif

                       const adc_b_scan_cfg_t g_adc0_scan_cfg =
                       {
                       .group_count = ( 0 +
                       (0 != (0)) + (0 != (0)) + (0 != (0)) + (0 != (0)) + (0 != (0)) + (0 != (0)) + (0 != (0)) + (0 != (0)) + (0 != (0))),
#if ((0 != (0))||(0 != (0))||(0 != (0))||(0 != (0))||(0 != (0))||(0 != (0))||(0 != (0))||(0 != (0))||(0 != (0)))
                       .p_adc_groups = (adc_b_group_cfg_t**)g_adc0_scan_cfg_groups,
                       #else
                       .p_adc_groups = NULL,
                       #endif
                       };

     const adc_b_isr_cfg_t g_adc0_isr_cfg =
{
    .calibration_end_ipl_adc_0 =  (12),
    .calibration_end_ipl_adc_1 =  (12),
    .limit_clip_ipl           =  (12),
    .conversion_error_ipl_adc_0 = (12),
    .conversion_error_ipl_adc_1 = (12),
    .overflow_error_ipl_adc_0 =   (12),
    .overflow_error_ipl_adc_1 =   (12),

    .scan_end_ipl_group_0 = (12),
    .scan_end_ipl_group_1 = (12),
    .scan_end_ipl_group_2 = (12),
    .scan_end_ipl_group_3 = (12),
    .scan_end_ipl_group_4 = (12),
    .scan_end_ipl_group_5 = (BSP_IRQ_DISABLED),
    .scan_end_ipl_group_6 = (BSP_IRQ_DISABLED),
    .scan_end_ipl_group_7 = (BSP_IRQ_DISABLED),
    .scan_end_ipl_group_8 = (BSP_IRQ_DISABLED),
    .scan_end_ipl_group_5678 = (BSP_IRQ_DISABLED),
    .fifo_overflow_ipl = (12),
    .fifo_read_ipl_group_0 = (12),
    .fifo_read_ipl_group_1 = (12),
    .fifo_read_ipl_group_2 = (12),
    .fifo_read_ipl_group_3 = (12),
    .fifo_read_ipl_group_4 = (12),
    .fifo_read_ipl_group_5 = (BSP_IRQ_DISABLED),
    .fifo_read_ipl_group_6 = (BSP_IRQ_DISABLED),
    .fifo_read_ipl_group_7 = (BSP_IRQ_DISABLED),
    .fifo_read_ipl_group_8 = (BSP_IRQ_DISABLED),
    .fifo_read_ipl_group_5678 = (BSP_IRQ_DISABLED),
#if defined(VECTOR_NUMBER_ADC_CALEND0) && ( (12) != BSP_IRQ_DISABLED )
    .calibration_end_irq_adc_0 = VECTOR_NUMBER_ADC_CALEND0,
#else
    .calibration_end_irq_adc_0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_CALEND1) && ( (12) != BSP_IRQ_DISABLED )
    .calibration_end_irq_adc_1 = VECTOR_NUMBER_ADC_CALEND1,
#else
    .calibration_end_irq_adc_1 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_LIMCLPI) && ( (12) != BSP_IRQ_DISABLED )
    .limit_clip_irq = VECTOR_NUMBER_ADC_LIMCLPI,
#else
    .limit_clip_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ERR0) && ( (12) != BSP_IRQ_DISABLED )
    .conversion_error_irq_adc_0 = VECTOR_NUMBER_ADC_ERR0,
#else
    .conversion_error_irq_adc_0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ERR1) && ( (12) != BSP_IRQ_DISABLED )
    .conversion_error_irq_adc_1 = VECTOR_NUMBER_ADC_ERR1,
#else
    .conversion_error_irq_adc_1 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_RESOVF0) && ( (12) != BSP_IRQ_DISABLED )
    .overflow_error_irq_adc_0 = VECTOR_NUMBER_ADC_RESOVF0,
#else
    .overflow_error_irq_adc_0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_RESOVF1) && ( (12) != BSP_IRQ_DISABLED )
    .overflow_error_irq_adc_1 = VECTOR_NUMBER_ADC_RESOVF1,
#else
    .overflow_error_irq_adc_1 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI0) && ( (12) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_0 = VECTOR_NUMBER_ADC_ADI0,
#else
    .scan_end_irq_group_0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI1) && ( (12) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_1 = VECTOR_NUMBER_ADC_ADI1,
#else
    .scan_end_irq_group_1 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI2) && ( (12) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_2 = VECTOR_NUMBER_ADC_ADI2,
#else
    .scan_end_irq_group_2 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI3) && ( (12) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_3 = VECTOR_NUMBER_ADC_ADI3,
#else
    .scan_end_irq_group_3 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI4) && ( (12) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_4 = VECTOR_NUMBER_ADC_ADI4,
#else
    .scan_end_irq_group_4 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI5) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_5 = VECTOR_NUMBER_ADC_ADI5,
#else
    .scan_end_irq_group_5 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI6) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_6 = VECTOR_NUMBER_ADC_ADI6,
#else
    .scan_end_irq_group_6 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI7) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_7 = VECTOR_NUMBER_ADC_ADI7,
#else
    .scan_end_irq_group_7 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI8) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_8 = VECTOR_NUMBER_ADC_ADI8,
#else
    .scan_end_irq_group_8 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_ADI5678) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .scan_end_irq_group_5678 = VECTOR_NUMBER_ADC_ADI5678,
#else
    .scan_end_irq_group_5678 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOOVF) && ( (12) != BSP_IRQ_DISABLED )
    .fifo_overflow_irq = VECTOR_NUMBER_ADC_FIFOOVF,
#else
    .fifo_overflow_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ0) && ( (12) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_0 = VECTOR_NUMBER_ADC_FIFOREQ0,
#else
    .fifo_read_irq_group_0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ1) && ( (12) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_1 = VECTOR_NUMBER_ADC_FIFOREQ1,
#else
    .fifo_read_irq_group_1 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ2) && ( (12) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_2 = VECTOR_NUMBER_ADC_FIFOREQ2,
#else
    .fifo_read_irq_group_2 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ3) && ( (12) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_3 = VECTOR_NUMBER_ADC_FIFOREQ3,
#else
    .fifo_read_irq_group_3 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ4) && ( (12) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_4 = VECTOR_NUMBER_ADC_FIFOREQ4,
#else
    .fifo_read_irq_group_4 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ5) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_5 = VECTOR_NUMBER_ADC_FIFOREQ5,
#else
    .fifo_read_irq_group_5 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ6) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_6 = VECTOR_NUMBER_ADC_FIFOREQ6,
#else
    .fifo_read_irq_group_6 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ7) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_7 = VECTOR_NUMBER_ADC_FIFOREQ7,
#else
    .fifo_read_irq_group_7 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ8) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_8 = VECTOR_NUMBER_ADC_FIFOREQ8,
#else
    .fifo_read_irq_group_8 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_ADC_FIFOREQ5678) && ( (BSP_IRQ_DISABLED) != BSP_IRQ_DISABLED )
    .fifo_read_irq_group_5678 = VECTOR_NUMBER_ADC_FIFOREQ5678,
#else
    .fifo_read_irq_group_5678 = FSP_INVALID_VECTOR,
#endif
};

const adc_b_extended_cfg_t g_adc0_cfg_extend =
{
    .clock_control_data     = ((ADC_B_CLOCK_SOURCE_ADC << R_ADC_B0_ADCLKCR_CLKSEL_Pos) |
                               (ADC_B_CLOCK_DIV_1 << R_ADC_B0_ADCLKCR_DIVR_Pos)),
    .sync_operation_control = (((1) << R_ADC_B0_ADSYCR_ADSYDIS0_Pos) |
                               ((1) << R_ADC_B0_ADSYCR_ADSYDIS1_Pos) |
                               (100 << R_ADC_B0_ADSYCR_ADSYCYC_Pos)),
    .adc_b_converter_mode[0] = {
                                .mode   =  (ADC_B_CONVERTER_MODE_CONTINUOUS_SCAN),
                                .method =  (ADC_B_CONVERSION_METHOD_SAR)
                               },
    .adc_b_converter_mode[1] = {
                                .mode   =  (ADC_B_CONVERTER_MODE_SINGLE_SCAN),
                                .method =  (ADC_B_CONVERSION_METHOD_SAR)
                               },
    .converter_selection_0 = (((0) << R_ADC_B0_ADSGCR0_SGADS0_Pos) |
                              ((0) << R_ADC_B0_ADSGCR0_SGADS1_Pos) |
                              ((0) << R_ADC_B0_ADSGCR0_SGADS2_Pos) |
                              ((0) << R_ADC_B0_ADSGCR0_SGADS3_Pos)),
    .converter_selection_1 = (((0) << R_ADC_B0_ADSGCR1_SGADS4_Pos) |
                              ((0) << R_ADC_B0_ADSGCR1_SGADS5_Pos) |
                              ((0) << R_ADC_B0_ADSGCR1_SGADS6_Pos) |
                              ((0) << R_ADC_B0_ADSGCR1_SGADS7_Pos)),
    .converter_selection_2 =  ((0) << R_ADC_B0_ADSGCR2_SGADS8_Pos),

    .fifo_enable_mask     = (((0) << R_ADC_B0_ADFIFOCR_FIFOEN0_Pos) |
                             ((0) << R_ADC_B0_ADFIFOCR_FIFOEN1_Pos) |
                             ((0) << R_ADC_B0_ADFIFOCR_FIFOEN2_Pos) |
                             ((0) << R_ADC_B0_ADFIFOCR_FIFOEN3_Pos) |
                             ((0) << R_ADC_B0_ADFIFOCR_FIFOEN4_Pos) |
                             ((0) << R_ADC_B0_ADFIFOCR_FIFOEN5_Pos) |
                             ((0) << R_ADC_B0_ADFIFOCR_FIFOEN6_Pos) |
                             ((0) << R_ADC_B0_ADFIFOCR_FIFOEN7_Pos) |
                             ((0) << R_ADC_B0_ADFIFOCR_FIFOEN8_Pos)),
    .fifo_interrupt_enable_mask = (((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE0_Pos) |
                                   ((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE1_Pos) |
                                   ((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE2_Pos) |
                                   ((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE3_Pos) |
                                   ((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE4_Pos) |
                                   ((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE5_Pos) |
                                   ((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE6_Pos) |
                                   ((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE7_Pos) |
                                   ((0) << R_ADC_B0_ADFIFOINTCR_FIFOIE8_Pos)),
    .fifo_interrupt_level0 = ((0 << R_ADC_B0_ADFIFOINTLR0_FIFOILV0_Pos) |
                              (0 << R_ADC_B0_ADFIFOINTLR0_FIFOILV1_Pos)),
    .fifo_interrupt_level1 = ((0 << R_ADC_B0_ADFIFOINTLR1_FIFOILV2_Pos) |
                              (0 << R_ADC_B0_ADFIFOINTLR1_FIFOILV3_Pos)),
    .fifo_interrupt_level2 = ((0 << R_ADC_B0_ADFIFOINTLR2_FIFOILV4_Pos) |
                              (0 << R_ADC_B0_ADFIFOINTLR2_FIFOILV5_Pos)),
    .fifo_interrupt_level3 = ((0 << R_ADC_B0_ADFIFOINTLR3_FIFOILV6_Pos) |
                              (0 << R_ADC_B0_ADFIFOINTLR3_FIFOILV7_Pos)),
    .fifo_interrupt_level4 = (0 << R_ADC_B0_ADFIFOINTLR4_FIFOILV8_Pos),

    .start_trigger_delay_0 = ((0 << R_ADC_B0_ADTRGDLR0_TRGDLY0_Pos) |
                              (0 << R_ADC_B0_ADTRGDLR0_TRGDLY1_Pos)),
    .start_trigger_delay_1 = ((0 << R_ADC_B0_ADTRGDLR1_TRGDLY2_Pos) |
                              (0 << R_ADC_B0_ADTRGDLR1_TRGDLY3_Pos)),
    .start_trigger_delay_2 = ((0 << R_ADC_B0_ADTRGDLR2_TRGDLY4_Pos) |
                              (0 << R_ADC_B0_ADTRGDLR2_TRGDLY5_Pos)),
    .start_trigger_delay_3 = ((0 << R_ADC_B0_ADTRGDLR3_TRGDLY6_Pos) |
                              (0 << R_ADC_B0_ADTRGDLR3_TRGDLY7_Pos)),
    .start_trigger_delay_4 = ((0 << R_ADC_B0_ADTRGDLR4_TRGDLY8_Pos)),
    .calibration_adc_state = ((95 << R_ADC_B0_ADCALSTCR_CALADSST_Pos) |
                              (5 << R_ADC_B0_ADCALSTCR_CALADCST_Pos)),
    .calibration_sample_and_hold = ((95 << R_ADC_B0_ADCALSHCR_CALSHSST_Pos) |
                                    (5 << R_ADC_B0_ADCALSHCR_CALSHHST_Pos)),
   .p_isr_cfg = &g_adc0_isr_cfg,
   .sampling_state_tables = {
                            ((95  << R_ADC_B0_ADSSTR0_SST0_Pos)  | (95  << R_ADC_B0_ADSSTR0_SST1_Pos)),
                            ((95  << R_ADC_B0_ADSSTR1_SST2_Pos)  | (95  << R_ADC_B0_ADSSTR1_SST3_Pos)),
                            ((95  << R_ADC_B0_ADSSTR2_SST4_Pos)  | (95  << R_ADC_B0_ADSSTR2_SST5_Pos)),
                            ((95  << R_ADC_B0_ADSSTR3_SST6_Pos)  | (95  << R_ADC_B0_ADSSTR3_SST7_Pos)),
                            ((95  << R_ADC_B0_ADSSTR4_SST8_Pos)  | (95  << R_ADC_B0_ADSSTR4_SST9_Pos)),
                            ((95 << R_ADC_B0_ADSSTR5_SST10_Pos) | (95 << R_ADC_B0_ADSSTR5_SST11_Pos)),
                            ((95 << R_ADC_B0_ADSSTR6_SST12_Pos) | (95 << R_ADC_B0_ADSSTR6_SST13_Pos)),
                            ((95 << R_ADC_B0_ADSSTR7_SST14_Pos) | (95 << R_ADC_B0_ADSSTR7_SST15_Pos)),
                            },
    .sample_and_hold_enable_mask = ( ADC_B_SAMPLE_AND_HOLD_MASK_NONE),
    .sample_and_hold_config_012 = ((95 << R_ADC_B0_ADSHSTR1_SHSST_Pos )|
                                   (5 << R_ADC_B0_ADSHSTR0_SHHST_Pos )),
    .sample_and_hold_config_456 = ((95 << R_ADC_B0_ADSHSTR1_SHSST_Pos) |
                                   (5 << R_ADC_B0_ADSHSTR1_SHHST_Pos)),
    .conversion_state = ((5 << R_ADC_B0_ADCNVSTR_CST0_Pos) |
                        (5 << R_ADC_B0_ADCNVSTR_CST1_Pos)),
    .user_offset_tables = {
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          },
    .user_gain_tables = {
                        ((1U << R_ADC_B0_ADUGTR0_UGAINI_Pos) + (uint32_t)(0 * (R_ADC_B0_ADUGTR0_UGAINF_Msk + 1))),
                        ((1U << R_ADC_B0_ADUGTR1_UGAINI_Pos) + (uint32_t)(0 * (R_ADC_B0_ADUGTR1_UGAINF_Msk + 1))),
                        ((1U << R_ADC_B0_ADUGTR2_UGAINI_Pos) + (uint32_t)(0 * (R_ADC_B0_ADUGTR2_UGAINF_Msk + 1))),
                        ((1U << R_ADC_B0_ADUGTR3_UGAINI_Pos) + (uint32_t)(0 * (R_ADC_B0_ADUGTR3_UGAINF_Msk + 1))),
                        ((1U << R_ADC_B0_ADUGTR4_UGAINI_Pos) + (uint32_t)(0 * (R_ADC_B0_ADUGTR4_UGAINF_Msk + 1))),
                        ((1U << R_ADC_B0_ADUGTR5_UGAINI_Pos) + (uint32_t)(0 * (R_ADC_B0_ADUGTR5_UGAINF_Msk + 1))),
                        ((1U << R_ADC_B0_ADUGTR6_UGAINI_Pos) + (uint32_t)(0 * (R_ADC_B0_ADUGTR6_UGAINF_Msk + 1))),
                        ((1U << R_ADC_B0_ADUGTR7_UGAINI_Pos) + (uint32_t)(0 * (R_ADC_B0_ADUGTR7_UGAINF_Msk + 1))),
                        },
    .limiter_clip_interrupt_enable_mask = ( 0x00),
    .limiter_clip_tables = {
                           (0 | 0 << R_ADC_B0_ADLIMTR0_LIMU_Pos),
                           (0 | 0 << R_ADC_B0_ADLIMTR1_LIMU_Pos),
                           (0 | 0 << R_ADC_B0_ADLIMTR2_LIMU_Pos),
                           (0 | 0 << R_ADC_B0_ADLIMTR3_LIMU_Pos),
                           (0 | 0 << R_ADC_B0_ADLIMTR4_LIMU_Pos),
                           (0 | 0 << R_ADC_B0_ADLIMTR5_LIMU_Pos),
                           (0 | 0 << R_ADC_B0_ADLIMTR6_LIMU_Pos),
                           (0 | 0 << R_ADC_B0_ADLIMTR7_LIMU_Pos),
                           },

    #if (1 == 0)
    .pga_gain[0] = ADC_B_PGA_GAIN_SINGLE_ENDED_2_500,
    #elif (2 == 0)
    .pga_gain[0] = ADC_B_PGA_GAIN_DIFFERENTIAL_1_500,
    #else
    .pga_gain[0] = ADC_B_PGA_GAIN_DISABLED,
    #endif

    #if (1 == 0)
    .pga_gain[1] = ADC_B_PGA_GAIN_SINGLE_ENDED_2_500,
    #elif (2 == 0)
    .pga_gain[1] = ADC_B_PGA_GAIN_DIFFERENTIAL_1_500,
    #else
    .pga_gain[1] = ADC_B_PGA_GAIN_DISABLED,
    #endif

    #if (1 == 0)
    .pga_gain[2] = ADC_B_PGA_GAIN_SINGLE_ENDED_2_500,
    #elif (2 == 0)
    .pga_gain[2] = ADC_B_PGA_GAIN_DIFFERENTIAL_1_500,
    #else
    .pga_gain[2] = ADC_B_PGA_GAIN_DISABLED,
    #endif

    #if (1 == 0)
    .pga_gain[3] = ADC_B_PGA_GAIN_SINGLE_ENDED_2_500,
    #elif (2 == 0)
    .pga_gain[3] = ADC_B_PGA_GAIN_DIFFERENTIAL_1_500,
    #else
    .pga_gain[3] = ADC_B_PGA_GAIN_DISABLED,
    #endif

    /* For debug only! Prolonged use of PGA Monitor function may deteriorate PGA characteristics. See user manual for more information.*/
    .pga_debug_monitor_mask_b.unit_0 = 0,
    .pga_debug_monitor_mask_b.unit_1 = 0,
    .pga_debug_monitor_mask_b.unit_2 = 0,
    .pga_debug_monitor_mask_b.unit_3 = 0,
    .adc_filter_selection[0].bits = ((R_ADC_B0_ADDFSR0_DFSEL0_Msk & (ADC_B_DIGITAL_FILTER_MODE_SINC3 << R_ADC_B0_ADDFSR0_DFSEL0_Pos)) |
                                    (R_ADC_B0_ADDFSR0_DFSEL1_Msk & (ADC_B_DIGITAL_FILTER_MODE_SINC3 << R_ADC_B0_ADDFSR0_DFSEL1_Pos)) |
                                    (R_ADC_B0_ADDFSR0_DFSEL2_Msk & (ADC_B_DIGITAL_FILTER_MODE_SINC3 << R_ADC_B0_ADDFSR0_DFSEL2_Pos)) |
                                    (R_ADC_B0_ADDFSR0_DFSEL3_Msk & (ADC_B_DIGITAL_FILTER_MODE_SINC3 << R_ADC_B0_ADDFSR0_DFSEL3_Pos))),
    .adc_filter_selection[1].bits = ((R_ADC_B0_ADDFSR1_DFSEL0_Msk & (ADC_B_DIGITAL_FILTER_MODE_SINC3 << R_ADC_B0_ADDFSR1_DFSEL0_Pos)) |
                                    (R_ADC_B0_ADDFSR1_DFSEL1_Msk & (ADC_B_DIGITAL_FILTER_MODE_SINC3 << R_ADC_B0_ADDFSR1_DFSEL1_Pos)) |
                                    (R_ADC_B0_ADDFSR1_DFSEL2_Msk & (ADC_B_DIGITAL_FILTER_MODE_SINC3 << R_ADC_B0_ADDFSR1_DFSEL2_Pos)) |
                                    (R_ADC_B0_ADDFSR1_DFSEL3_Msk & (ADC_B_DIGITAL_FILTER_MODE_SINC3 << R_ADC_B0_ADDFSR1_DFSEL3_Pos))),
};


const adc_cfg_t g_adc0_cfg =
{
    .unit                = 0xFFFC,
    .mode                = (adc_mode_t) 0, // Unused
    .resolution          = (adc_resolution_t) 0, // Unused
    .alignment           = ADC_ALIGNMENT_RIGHT,
    .trigger             = (adc_trigger_t) 0, // Unused
    .p_callback          = NULL,
#if defined(NULL)
    .p_context           = NULL,
#else
    .p_context           = (void *) &NULL,
#endif
    .p_extend            = &g_adc0_cfg_extend,

    .scan_end_irq        = FSP_INVALID_VECTOR,
    .scan_end_ipl        = BSP_IRQ_DISABLED,
    .scan_end_b_irq      = FSP_INVALID_VECTOR,
    .scan_end_b_ipl      = BSP_IRQ_DISABLED,
};


adc_b_instance_ctrl_t g_adc0_ctrl;


const adc_instance_t g_adc0 =
{
    .p_ctrl    = &g_adc0_ctrl,
    .p_cfg = &g_adc0_cfg,
    .p_channel_cfg = &g_adc0_scan_cfg,
    .p_api     = &g_adc_on_adc_b,
};


    #undef RA_NOT_DEFINED
iic_master_instance_ctrl_t g_i2c_master0_ctrl;
const iic_master_extended_cfg_t g_i2c_master0_extend =
{
    .timeout_mode             = IIC_MASTER_TIMEOUT_MODE_SHORT,
    .timeout_scl_low          = IIC_MASTER_TIMEOUT_SCL_LOW_ENABLED,
    .smbus_operation         = 0,
    /* Actual calculated bitrate: 97809. Actual calculated duty cycle: 49%. */ .clock_settings.brl_value = 17, .clock_settings.brh_value = 16, .clock_settings.cks_value = 4, .clock_settings.sddl_value = 0, .clock_settings.dlcs_value = 0,
};
const i2c_master_cfg_t g_i2c_master0_cfg =
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
    .p_callback          = NULL,
    .p_context           = NULL,
#if defined(VECTOR_NUMBER_IIC0_RXI)
    .rxi_irq             = VECTOR_NUMBER_IIC0_RXI,
#else
    .rxi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IIC0_TXI)
    .txi_irq             = VECTOR_NUMBER_IIC0_TXI,
#else
    .txi_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IIC0_TEI)
    .tei_irq             = VECTOR_NUMBER_IIC0_TEI,
#else
    .tei_irq             = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IIC0_ERI)
    .eri_irq             = VECTOR_NUMBER_IIC0_ERI,
#else
    .eri_irq             = FSP_INVALID_VECTOR,
#endif
    .ipl                 = (12),
    .p_extend            = &g_i2c_master0_extend,
};
/* Instance structure to use this module. */
const i2c_master_instance_t g_i2c_master0 =
{
    .p_ctrl        = &g_i2c_master0_ctrl,
    .p_cfg         = &g_i2c_master0_cfg,
    .p_api         = &g_i2c_master_on_iic
};
extern uint32_t g_fsp_common_thread_count;

                const rm_freertos_port_parameters_t main_service_parameters =
                {
                    .p_context = (void *) NULL,
                };

                void main_service_create (void)
                {
                    /* Increment count so we will know the number of threads created in the RA Configuration editor. */
                    g_fsp_common_thread_count++;

                    /* Initialize each kernel object. */
                    

                    #if 1
                    main_service = xTaskCreateStatic(
                    #else
                    BaseType_t main_service_create_err = xTaskCreate(
                    #endif
                        main_service_func,
                        (const char *)"main_thread",
                        4096/4, // In words, not bytes
                        (void *) &main_service_parameters, //pvParameters
                        2,
                        #if 1
                        (StackType_t *)&main_service_stack,
                        (StaticTask_t *)&main_service_memory
                        #else
                        & main_service
                        #endif
                    );

                    #if 1
                    if (NULL == main_service)
                    {
                        rtos_startup_err_callback(main_service, 0);
                    }
                    #else
                    if (pdPASS != main_service_create_err)
                    {
                        rtos_startup_err_callback(main_service, 0);
                    }
                    #endif
                }
                static void main_service_func (void * pvParameters)
                {
                    /* Initialize common components */
                    rtos_startup_common_init();

                    /* Initialize each module instance. */
                    

                    #if (1 == BSP_TZ_NONSECURE_BUILD) && (1 == 0)
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
                    main_service_entry(pvParameters);
                }
