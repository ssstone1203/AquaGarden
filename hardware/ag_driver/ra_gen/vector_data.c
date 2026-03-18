/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = iic_b_master_rxi_isr, /* IICB0 RXI (Receive) */
            [1] = iic_b_master_txi_isr, /* IICB0 TXI (Transmit) */
            [2] = iic_b_master_tei_isr, /* IICB0 TEI (Transmit end) */
            [3] = iic_b_master_eri_isr, /* IICB0 ERI (Error) */
            [4] = sci_uart_rxi_isr, /* SCI9 RXI (Receive data full) */
            [5] = sci_uart_txi_isr, /* SCI9 TXI (Transmit data empty) */
            [6] = sci_uart_tei_isr, /* SCI9 TEI (Transmit end) */
            [7] = sci_uart_eri_isr, /* SCI9 ERI (Receive error) */
            [8] = adc_scan_end_isr, /* ADC0 SCAN END (End of A/D scanning operation) */
            [9] = spi_tei_isr, /* SPI1 TEI (Transmission complete event) */
            [10] = spi_eri_isr, /* SPI1 ERI (Error) */
            [11] = dmac_int_isr, /* DMAC0 INT (DMAC0 transfer end) */
            [12] = dmac_int_isr, /* DMAC1 INT (DMAC1 transfer end) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_IICB0_RXI,GROUP0), /* IICB0 RXI (Receive) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_IICB0_TXI,GROUP1), /* IICB0 TXI (Transmit) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_IICB0_TEI,GROUP2), /* IICB0 TEI (Transmit end) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_IICB0_ERI,GROUP3), /* IICB0 ERI (Error) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_SCI9_RXI,GROUP4), /* SCI9 RXI (Receive data full) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TXI,GROUP5), /* SCI9 TXI (Transmit data empty) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TEI,GROUP6), /* SCI9 TEI (Transmit end) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_SCI9_ERI,GROUP7), /* SCI9 ERI (Receive error) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_ADC0_SCAN_END,GROUP0), /* ADC0 SCAN END (End of A/D scanning operation) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_SPI1_TEI,GROUP1), /* SPI1 TEI (Transmission complete event) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_SPI1_ERI,GROUP2), /* SPI1 ERI (Error) */
            [11] = BSP_PRV_VECT_ENUM(EVENT_DMAC0_INT,GROUP3), /* DMAC0 INT (DMAC0 transfer end) */
            [12] = BSP_PRV_VECT_ENUM(EVENT_DMAC1_INT,GROUP4), /* DMAC1 INT (DMAC1 transfer end) */
        };
        #endif
        #endif