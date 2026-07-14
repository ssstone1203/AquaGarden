/* generated vector header file - do not edit */
        #ifndef VECTOR_DATA_H
        #define VECTOR_DATA_H
        #ifdef __cplusplus
        extern "C" {
        #endif
                /* Number of interrupts allocated */
        #ifndef VECTOR_DATA_IRQ_COUNT
        #define VECTOR_DATA_IRQ_COUNT    (2)
        #endif
        /* ISR prototypes */
        void adc_b_calend0_isr(void);
        void adc_b_calend1_isr(void);

        /* Vector table allocations */
        #define VECTOR_NUMBER_ADC_CALEND0 ((IRQn_Type) 0) /* ADC CALEND0 (End of calibration of A/D converter unit 0) */
        #define ADC_CALEND0_IRQn          ((IRQn_Type) 0) /* ADC CALEND0 (End of calibration of A/D converter unit 0) */
        #define VECTOR_NUMBER_ADC_CALEND1 ((IRQn_Type) 1) /* ADC CALEND1 (End of calibration of A/D converter unit 1) */
        #define ADC_CALEND1_IRQn          ((IRQn_Type) 1) /* ADC CALEND1 (End of calibration of A/D converter unit 1) */
        /* The number of entries required for the ICU vector table. */
        #define BSP_ICU_VECTOR_NUM_ENTRIES (2)

        #ifdef __cplusplus
        }
        #endif
        #endif /* VECTOR_DATA_H */