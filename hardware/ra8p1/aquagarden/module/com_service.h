#ifndef COM_SERVICE_H_
#define COM_SERVICE_H_

/* UART transport for the AquaGarden protocol.
 * Driven entirely from the 10 ms main service task:
 *  - RX bytes are collected in the SCI char callback (ISR) into a frame buffer.
 *  - Downlink frames are parsed/applied in task context.
 *  - An uplink frame is sent every 250 ms via the FSP interrupt-driven write. */

void com_service_init(void);
void com_service_process_10ms(void);

#endif /* COM_SERVICE_H_ */
