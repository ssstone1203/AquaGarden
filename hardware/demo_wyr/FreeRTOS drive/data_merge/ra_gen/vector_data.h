/* generated vector header file - do not edit */
        #ifndef VECTOR_DATA_H
        #define VECTOR_DATA_H
        #ifdef __cplusplus
        extern "C" {
        #endif
                /* Number of interrupts allocated */
        #ifndef VECTOR_DATA_IRQ_COUNT
        #define VECTOR_DATA_IRQ_COUNT    (17)
        #endif
        /* ISR prototypes */
        void iic_b_master_rxi_isr(void);
        void iic_b_master_txi_isr(void);
        void iic_b_master_tei_isr(void);
        void iic_b_master_eri_isr(void);
        void sci_uart_rxi_isr(void);
        void sci_uart_txi_isr(void);
        void sci_uart_tei_isr(void);
        void sci_uart_eri_isr(void);
        void adc_scan_end_isr(void);
        void spi_tei_isr(void);
        void spi_eri_isr(void);
        void dmac_int_isr(void);

        /* Vector table allocations */
        #define VECTOR_NUMBER_IICB0_RXI ((IRQn_Type) 0) /* IICB0 RXI (Receive) */
        #define IICB0_RXI_IRQn          ((IRQn_Type) 0) /* IICB0 RXI (Receive) */
        #define VECTOR_NUMBER_IICB0_TXI ((IRQn_Type) 1) /* IICB0 TXI (Transmit) */
        #define IICB0_TXI_IRQn          ((IRQn_Type) 1) /* IICB0 TXI (Transmit) */
        #define VECTOR_NUMBER_IICB0_TEI ((IRQn_Type) 2) /* IICB0 TEI (Transmit end) */
        #define IICB0_TEI_IRQn          ((IRQn_Type) 2) /* IICB0 TEI (Transmit end) */
        #define VECTOR_NUMBER_IICB0_ERI ((IRQn_Type) 3) /* IICB0 ERI (Error) */
        #define IICB0_ERI_IRQn          ((IRQn_Type) 3) /* IICB0 ERI (Error) */
        #define VECTOR_NUMBER_SCI9_RXI ((IRQn_Type) 4) /* SCI9 RXI (Receive data full) */
        #define SCI9_RXI_IRQn          ((IRQn_Type) 4) /* SCI9 RXI (Receive data full) */
        #define VECTOR_NUMBER_SCI9_TXI ((IRQn_Type) 5) /* SCI9 TXI (Transmit data empty) */
        #define SCI9_TXI_IRQn          ((IRQn_Type) 5) /* SCI9 TXI (Transmit data empty) */
        #define VECTOR_NUMBER_SCI9_TEI ((IRQn_Type) 6) /* SCI9 TEI (Transmit end) */
        #define SCI9_TEI_IRQn          ((IRQn_Type) 6) /* SCI9 TEI (Transmit end) */
        #define VECTOR_NUMBER_SCI9_ERI ((IRQn_Type) 7) /* SCI9 ERI (Receive error) */
        #define SCI9_ERI_IRQn          ((IRQn_Type) 7) /* SCI9 ERI (Receive error) */
        #define VECTOR_NUMBER_ADC0_SCAN_END ((IRQn_Type) 8) /* ADC0 SCAN END (End of A/D scanning operation) */
        #define ADC0_SCAN_END_IRQn          ((IRQn_Type) 8) /* ADC0 SCAN END (End of A/D scanning operation) */
        #define VECTOR_NUMBER_SPI1_TEI ((IRQn_Type) 9) /* SPI1 TEI (Transmission complete event) */
        #define SPI1_TEI_IRQn          ((IRQn_Type) 9) /* SPI1 TEI (Transmission complete event) */
        #define VECTOR_NUMBER_SPI1_ERI ((IRQn_Type) 10) /* SPI1 ERI (Error) */
        #define SPI1_ERI_IRQn          ((IRQn_Type) 10) /* SPI1 ERI (Error) */
        #define VECTOR_NUMBER_DMAC0_INT ((IRQn_Type) 11) /* DMAC0 INT (DMAC0 transfer end) */
        #define DMAC0_INT_IRQn          ((IRQn_Type) 11) /* DMAC0 INT (DMAC0 transfer end) */
        #define VECTOR_NUMBER_DMAC1_INT ((IRQn_Type) 12) /* DMAC1 INT (DMAC1 transfer end) */
        #define DMAC1_INT_IRQn          ((IRQn_Type) 12) /* DMAC1 INT (DMAC1 transfer end) */
        #define VECTOR_NUMBER_SCI0_RXI ((IRQn_Type) 13) /* SCI0 RXI (Receive data full) */
        #define SCI0_RXI_IRQn          ((IRQn_Type) 13) /* SCI0 RXI (Receive data full) */
        #define VECTOR_NUMBER_SCI0_TXI ((IRQn_Type) 14) /* SCI0 TXI (Transmit data empty) */
        #define SCI0_TXI_IRQn          ((IRQn_Type) 14) /* SCI0 TXI (Transmit data empty) */
        #define VECTOR_NUMBER_SCI0_TEI ((IRQn_Type) 15) /* SCI0 TEI (Transmit end) */
        #define SCI0_TEI_IRQn          ((IRQn_Type) 15) /* SCI0 TEI (Transmit end) */
        #define VECTOR_NUMBER_SCI0_ERI ((IRQn_Type) 16) /* SCI0 ERI (Receive error) */
        #define SCI0_ERI_IRQn          ((IRQn_Type) 16) /* SCI0 ERI (Receive error) */
        /* The number of entries required for the ICU vector table. */
        #define BSP_ICU_VECTOR_NUM_ENTRIES (17)

        #ifdef __cplusplus
        }
        #endif
        #endif /* VECTOR_DATA_H */