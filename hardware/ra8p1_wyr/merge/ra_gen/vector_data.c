/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = usbfs_interrupt_handler, /* USBFS INT (USBFS interrupt) */
            [1] = usbfs_resume_handler, /* USBFS RESUME (USBFS resume interrupt) */
            [2] = usbfs_d0fifo_handler, /* USBFS FIFO 0 (DMA/DTC transfer request 0) */
            [3] = usbfs_d1fifo_handler, /* USBFS FIFO 1 (DMA/DTC transfer request 1) */
            [4] = usbhs_interrupt_handler, /* USBHS USB INT RESUME (USBHS interrupt) */
            [5] = usbhs_d0fifo_handler, /* USBHS FIFO 0 (DMA transfer request 0) */
            [6] = usbhs_d1fifo_handler, /* USBHS FIFO 1 (DMA transfer request 1) */
            [7] = iic_master_rxi_isr, /* IIC2 RXI (Receive data full) */
            [8] = iic_master_txi_isr, /* IIC2 TXI (Transmit data empty) */
            [9] = iic_master_tei_isr, /* IIC2 TEI (Transmit end) */
            [10] = iic_master_eri_isr, /* IIC2 ERI (Transfer error) */
            [11] = adc_b_calend0_isr, /* ADC CALEND0 (End of calibration of A/D converter unit 0) */
            [12] = adc_b_calend1_isr, /* ADC CALEND1 (End of calibration of A/D converter unit 1) */
            [13] = sci_b_uart_rxi_isr, /* SCI9 RXI (Receive data full) */
            [14] = sci_b_uart_txi_isr, /* SCI9 TXI (Transmit data empty) */
            [15] = sci_b_uart_tei_isr, /* SCI9 TEI (Transmit end) */
            [16] = sci_b_uart_eri_isr, /* SCI9 ERI (Receive error) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_USBFS_INT,GROUP0), /* USBFS INT (USBFS interrupt) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_USBFS_RESUME,GROUP1), /* USBFS RESUME (USBFS resume interrupt) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_USBFS_FIFO_0,GROUP2), /* USBFS FIFO 0 (DMA/DTC transfer request 0) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_USBFS_FIFO_1,GROUP3), /* USBFS FIFO 1 (DMA/DTC transfer request 1) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_USBHS_USB_INT_RESUME,GROUP4), /* USBHS USB INT RESUME (USBHS interrupt) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_USBHS_FIFO_0,GROUP5), /* USBHS FIFO 0 (DMA transfer request 0) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_USBHS_FIFO_1,GROUP6), /* USBHS FIFO 1 (DMA transfer request 1) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_IIC2_RXI,GROUP7), /* IIC2 RXI (Receive data full) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_IIC2_TXI,GROUP0), /* IIC2 TXI (Transmit data empty) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_IIC2_TEI,GROUP1), /* IIC2 TEI (Transmit end) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_IIC2_ERI,GROUP2), /* IIC2 ERI (Transfer error) */
            [11] = BSP_PRV_VECT_ENUM(EVENT_ADC_CALEND0,GROUP3), /* ADC CALEND0 (End of calibration of A/D converter unit 0) */
            [12] = BSP_PRV_VECT_ENUM(EVENT_ADC_CALEND1,GROUP4), /* ADC CALEND1 (End of calibration of A/D converter unit 1) */
            [13] = BSP_PRV_VECT_ENUM(EVENT_SCI9_RXI,GROUP5), /* SCI9 RXI (Receive data full) */
            [14] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TXI,GROUP6), /* SCI9 TXI (Transmit data empty) */
            [15] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TEI,GROUP7), /* SCI9 TEI (Transmit end) */
            [16] = BSP_PRV_VECT_ENUM(EVENT_SCI9_ERI,GROUP0), /* SCI9 ERI (Receive error) */
        };
        #endif
        #endif