/* generated thread header file - do not edit */
#ifndef COMMUNICATE_TASK_H_
#define COMMUNICATE_TASK_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void Communicate_Task_entry(void * pvParameters);
                #else
                extern void Communicate_Task_entry(void * pvParameters);
                #endif
#include "r_sci_uart.h"
            #include "r_uart_api.h"
#include "r_dmac.h"
#include "r_transfer_api.h"
#include "r_spi.h"
FSP_HEADER
/** UART on SCI Instance. */
            extern const uart_instance_t      g_com_uart0;

            /** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
            extern sci_uart_instance_ctrl_t     g_com_uart0_ctrl;
            extern const uart_cfg_t g_com_uart0_cfg;
            extern const sci_uart_extended_cfg_t g_com_uart0_cfg_extend;

            #ifndef NULL
            void NULL(uart_callback_args_t * p_args);
            #endif
/* Transfer on DMAC Instance. */
extern const transfer_instance_t g_com_spi_rx;

/** Access the DMAC instance using these structures when calling API functions directly (::p_api is not used). */
extern dmac_instance_ctrl_t g_com_spi_rx_ctrl;
extern const transfer_cfg_t g_com_spi_rx_cfg;

#ifndef g_com_spi_rx_transfer_callback
void g_com_spi_rx_transfer_callback(transfer_callback_args_t * p_args);
#endif
/* Transfer on DMAC Instance. */
extern const transfer_instance_t g_com_spi_tx;

/** Access the DMAC instance using these structures when calling API functions directly (::p_api is not used). */
extern dmac_instance_ctrl_t g_com_spi_tx_ctrl;
extern const transfer_cfg_t g_com_spi_tx_cfg;

#ifndef g_com_spi_tx_transfer_callback
void g_com_spi_tx_transfer_callback(transfer_callback_args_t * p_args);
#endif
/** SPI on SPI Instance. */
extern const spi_instance_t g_com_spi;

/** Access the SPI instance using these structures when calling API functions directly (::p_api is not used). */
extern spi_instance_ctrl_t g_com_spi_ctrl;
extern const spi_cfg_t g_com_spi_cfg;

/** Callback used by SPI Instance. */
#ifndef Com_SPI_Callback
void Com_SPI_Callback(spi_callback_args_t * p_args);
#endif


#define RA_NOT_DEFINED (1)
#if (RA_NOT_DEFINED == g_com_spi_tx)
    #define g_com_spi_P_TRANSFER_TX (NULL)
#else
    #define g_com_spi_P_TRANSFER_TX (&g_com_spi_tx)
#endif
#if (RA_NOT_DEFINED == g_com_spi_rx)
    #define g_com_spi_P_TRANSFER_RX (NULL)
#else
    #define g_com_spi_P_TRANSFER_RX (&g_com_spi_rx)
#endif
#undef RA_NOT_DEFINED
FSP_FOOTER
#endif /* COMMUNICATE_TASK_H_ */
