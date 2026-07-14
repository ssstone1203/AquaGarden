/**
 * USB Host event loop + CH340 bring-up + demo sequence.
 */
#include "usb_app.h"
#include "ch340_host.h"
#include "usb_light.h"
#include "hal_data.h"
#include <string.h>

#define CH340_BRINGUP_DELAY_LOOPS    (100U)

static const usb_light_mode_t g_demo_sequence[] =
{
    USB_LIGHT_RED_ON,
    USB_LIGHT_GREEN_ON,
    USB_LIGHT_BLUE_ON,
    USB_LIGHT_RED_SLOW,
    USB_LIGHT_RED_FAST_HORN,
    USB_LIGHT_OFF,
};

static usb_instance_ctrl_t g_usb_event_ctrl;
static uint8_t             g_demo_index;
static uint8_t             g_pending_addr;
static bool                g_need_ch340_bringup;
static uint16_t            g_bringup_delay_loops;
static uint16_t            g_demo_loop_counter;

volatile usb_app_debug_t g_usb_app_debug;

static void usb_event_ctrl_prepare(void)
{
    g_usb_event_ctrl.module_number = g_basic0_cfg.module_number;
    g_usb_event_ctrl.type          = USB_CLASS_HVND;
}

fsp_err_t usb_app_init(void)
{
    fsp_err_t err;

    memset((void *) &g_usb_app_debug, 0, sizeof(g_usb_app_debug));
    g_demo_index           = 0U;
    g_pending_addr         = 0U;
    g_need_ch340_bringup   = false;
    g_bringup_delay_loops  = 0U;
    g_demo_loop_counter    = 0U;

    memset(&g_usb_event_ctrl, 0, sizeof(g_usb_event_ctrl));
    usb_event_ctrl_prepare();

    err = ch340_host_init();
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = R_USB_Open(&g_basic0_ctrl, &g_basic0_cfg);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    usb_event_ctrl_prepare();
    (void) R_USB_VbusSet(&g_usb_event_ctrl, USB_ON);
    R_BSP_SoftwareDelay(200U, BSP_DELAY_UNITS_MILLISECONDS);

    return FSP_SUCCESS;
}

void usb_app_poll(void)
{
    usb_status_t event = USB_STATUS_NONE;

    while (FSP_SUCCESS == R_USB_EventGet(&g_usb_event_ctrl, &event))
    {
        if (USB_STATUS_NONE == event)
        {
            break;
        }

        g_usb_app_debug.last_usb_event = (uint16_t) event;

        switch (event)
        {
            case USB_STATUS_CONFIGURED:
            {
                if (0U != g_usb_event_ctrl.device_address)
                {
                    g_usb_app_debug.configured_seen = 1U;
                    g_usb_app_debug.configured_addr = g_usb_event_ctrl.device_address;
                    g_pending_addr                  = g_usb_event_ctrl.device_address;
                    g_need_ch340_bringup            = true;
                    g_bringup_delay_loops           = 0U;
                    ch340_host_on_configured(g_usb_event_ctrl.device_address);
                }

                break;
            }

            case USB_STATUS_DETACH:
            {
                ch340_host_on_detach();
                g_demo_index                = 0U;
                g_pending_addr              = 0U;
                g_need_ch340_bringup        = false;
                g_demo_loop_counter         = 0U;
                g_usb_app_debug.ch340_ready = 0U;
                break;
            }

            default:
            {
                break;
            }
        }
    }

    if (g_need_ch340_bringup)
    {
        if (g_bringup_delay_loops < CH340_BRINGUP_DELAY_LOOPS)
        {
            g_bringup_delay_loops++;
        }
        else
        {
            g_need_ch340_bringup              = false;
            g_usb_app_debug.ch340_bringup_err = ch340_host_bringup();
            g_usb_app_debug.ch340_ready       = ch340_host_is_ready() ? 1U : 0U;
        }
    }
}

void usb_app_process(void)
{
    for (uint8_t i = 0U; i < USB_APP_POLL_SLICES; i++)
    {
        usb_app_poll();
        R_BSP_SoftwareDelay(USB_APP_POLL_SLICE_MS, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if (!usb_app_is_ready())
    {
        g_demo_loop_counter = 0U;
        return;
    }

    g_demo_loop_counter++;
    if (g_demo_loop_counter >= USB_APP_DEMO_PERIOD_LOOPS)
    {
        g_demo_loop_counter = 0U;
        usb_app_run_demo_step();
    }
}

bool usb_app_is_ready(void)
{
    return ch340_host_is_ready();
}

void usb_app_run_demo_step(void)
{
    fsp_err_t err;

    if (!ch340_host_is_ready())
    {
        return;
    }

    if (g_demo_index >= (sizeof(g_demo_sequence) / sizeof(g_demo_sequence[0])))
    {
        g_demo_index = 0U;
    }

    err = usb_light_set(g_demo_sequence[g_demo_index]);
    g_usb_app_debug.last_light_err = err;
    if (FSP_SUCCESS == err)
    {
        g_demo_index++;
    }
}
