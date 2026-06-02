#ifndef USB_APP_H_
#define USB_APP_H_

#include "bsp_api.h"

#define USB_APP_DEMO_INTERVAL_MS    (3000U)
#define USB_APP_POLL_SLICE_MS       (1U)
#define USB_APP_POLL_SLICES         (10U)
#define USB_APP_DEMO_PERIOD_LOOPS     (USB_APP_DEMO_INTERVAL_MS / (USB_APP_POLL_SLICE_MS * USB_APP_POLL_SLICES))

/** Watch in debugger when the light does not respond. */
typedef struct st_usb_app_debug
{
    uint16_t     last_usb_event;       /**< (uint16_t) usb_status_t, see USB_STATUS_* */
    uint8_t      configured_addr;
    uint8_t      ch340_ready;
    uint8_t      configured_seen;
    fsp_err_t    ch340_bringup_err;
    fsp_err_t    last_light_err;
} usb_app_debug_t;

extern volatile usb_app_debug_t g_usb_app_debug;

fsp_err_t usb_app_init(void);

void usb_app_poll(void);

void usb_app_process(void);

bool usb_app_is_ready(void);

void usb_app_run_demo_step(void);

#endif /* USB_APP_H_ */
