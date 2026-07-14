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
        void usbfs_interrupt_handler(void);
        void usbfs_resume_handler(void);
        void usbfs_d0fifo_handler(void);
        void usbfs_d1fifo_handler(void);
        void usbhs_interrupt_handler(void);
        void usbhs_d0fifo_handler(void);
        void usbhs_d1fifo_handler(void);
        void iic_master_rxi_isr(void);
        void iic_master_txi_isr(void);
        void iic_master_tei_isr(void);
        void iic_master_eri_isr(void);
        void adc_b_calend0_isr(void);
        void adc_b_calend1_isr(void);
        void sci_b_uart_rxi_isr(void);
        void sci_b_uart_txi_isr(void);
        void sci_b_uart_tei_isr(void);
        void sci_b_uart_eri_isr(void);

        /* Vector table allocations */
        #define VECTOR_NUMBER_USBFS_INT ((IRQn_Type) 0) /* USBFS INT (USBFS interrupt) */
        #define USBFS_INT_IRQn          ((IRQn_Type) 0) /* USBFS INT (USBFS interrupt) */
        #define VECTOR_NUMBER_USBFS_RESUME ((IRQn_Type) 1) /* USBFS RESUME (USBFS resume interrupt) */
        #define USBFS_RESUME_IRQn          ((IRQn_Type) 1) /* USBFS RESUME (USBFS resume interrupt) */
        #define VECTOR_NUMBER_USBFS_FIFO_0 ((IRQn_Type) 2) /* USBFS FIFO 0 (DMA/DTC transfer request 0) */
        #define USBFS_FIFO_0_IRQn          ((IRQn_Type) 2) /* USBFS FIFO 0 (DMA/DTC transfer request 0) */
        #define VECTOR_NUMBER_USBFS_FIFO_1 ((IRQn_Type) 3) /* USBFS FIFO 1 (DMA/DTC transfer request 1) */
        #define USBFS_FIFO_1_IRQn          ((IRQn_Type) 3) /* USBFS FIFO 1 (DMA/DTC transfer request 1) */
        #define VECTOR_NUMBER_USBHS_USB_INT_RESUME ((IRQn_Type) 4) /* USBHS USB INT RESUME (USBHS interrupt) */
        #define USBHS_USB_INT_RESUME_IRQn          ((IRQn_Type) 4) /* USBHS USB INT RESUME (USBHS interrupt) */
        #define VECTOR_NUMBER_USBHS_FIFO_0 ((IRQn_Type) 5) /* USBHS FIFO 0 (DMA transfer request 0) */
        #define USBHS_FIFO_0_IRQn          ((IRQn_Type) 5) /* USBHS FIFO 0 (DMA transfer request 0) */
        #define VECTOR_NUMBER_USBHS_FIFO_1 ((IRQn_Type) 6) /* USBHS FIFO 1 (DMA transfer request 1) */
        #define USBHS_FIFO_1_IRQn          ((IRQn_Type) 6) /* USBHS FIFO 1 (DMA transfer request 1) */
        #define VECTOR_NUMBER_IIC2_RXI ((IRQn_Type) 7) /* IIC2 RXI (Receive data full) */
        #define IIC2_RXI_IRQn          ((IRQn_Type) 7) /* IIC2 RXI (Receive data full) */
        #define VECTOR_NUMBER_IIC2_TXI ((IRQn_Type) 8) /* IIC2 TXI (Transmit data empty) */
        #define IIC2_TXI_IRQn          ((IRQn_Type) 8) /* IIC2 TXI (Transmit data empty) */
        #define VECTOR_NUMBER_IIC2_TEI ((IRQn_Type) 9) /* IIC2 TEI (Transmit end) */
        #define IIC2_TEI_IRQn          ((IRQn_Type) 9) /* IIC2 TEI (Transmit end) */
        #define VECTOR_NUMBER_IIC2_ERI ((IRQn_Type) 10) /* IIC2 ERI (Transfer error) */
        #define IIC2_ERI_IRQn          ((IRQn_Type) 10) /* IIC2 ERI (Transfer error) */
        #define VECTOR_NUMBER_ADC_CALEND0 ((IRQn_Type) 11) /* ADC CALEND0 (End of calibration of A/D converter unit 0) */
        #define ADC_CALEND0_IRQn          ((IRQn_Type) 11) /* ADC CALEND0 (End of calibration of A/D converter unit 0) */
        #define VECTOR_NUMBER_ADC_CALEND1 ((IRQn_Type) 12) /* ADC CALEND1 (End of calibration of A/D converter unit 1) */
        #define ADC_CALEND1_IRQn          ((IRQn_Type) 12) /* ADC CALEND1 (End of calibration of A/D converter unit 1) */
        #define VECTOR_NUMBER_SCI9_RXI ((IRQn_Type) 13) /* SCI9 RXI (Receive data full) */
        #define SCI9_RXI_IRQn          ((IRQn_Type) 13) /* SCI9 RXI (Receive data full) */
        #define VECTOR_NUMBER_SCI9_TXI ((IRQn_Type) 14) /* SCI9 TXI (Transmit data empty) */
        #define SCI9_TXI_IRQn          ((IRQn_Type) 14) /* SCI9 TXI (Transmit data empty) */
        #define VECTOR_NUMBER_SCI9_TEI ((IRQn_Type) 15) /* SCI9 TEI (Transmit end) */
        #define SCI9_TEI_IRQn          ((IRQn_Type) 15) /* SCI9 TEI (Transmit end) */
        #define VECTOR_NUMBER_SCI9_ERI ((IRQn_Type) 16) /* SCI9 ERI (Receive error) */
        #define SCI9_ERI_IRQn          ((IRQn_Type) 16) /* SCI9 ERI (Receive error) */
        /* The number of entries required for the ICU vector table. */
        #define BSP_ICU_VECTOR_NUM_ENTRIES (17)

        #ifdef __cplusplus
        }
        #endif
        #endif /* VECTOR_DATA_H */