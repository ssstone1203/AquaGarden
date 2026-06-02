/**
 * CH340 (1A86:7523) USB Host init and bulk OUT for Modbus frames.
 */
#ifndef CH340_HOST_H_
#define CH340_HOST_H_

#include "bsp_api.h"
#include "r_usb_basic_api.h"

#define CH340_HOST_FRAME_LEN    (8U)

fsp_err_t ch340_host_init(void);

/** Called after USB_STATUS_CONFIGURED (defers heavy init). */
void ch340_host_on_configured(uint8_t device_address);

fsp_err_t ch340_host_bringup(void);

void ch340_host_on_detach(void);

bool ch340_host_is_ready(void);

fsp_err_t ch340_host_get_last_error(void);

/** Fire-and-forget bulk OUT (8 bytes). */
fsp_err_t ch340_host_write(const uint8_t * frame, uint32_t length);

#endif /* CH340_HOST_H_ */
