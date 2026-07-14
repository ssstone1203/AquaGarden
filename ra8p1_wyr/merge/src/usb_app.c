/**
 * USB Host event loop + CH340 bring-up + demo sequence.
 */
#include "usb_app.h"
#include "ch340_host.h"
#include "usb_light.h"
#include "hal_data.h"
#include <string.h>

#if (2 == BSP_CFG_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

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
static bool                g_usb_stack_ready;
static uint16_t            g_bringup_delay_loops;
static uint16_t            g_demo_loop_counter;

#if (2 == BSP_CFG_RTOS)
static usb_cfg_t g_usb_cfg_rtos;
#endif

volatile usb_app_debug_t g_usb_app_debug;

static void usb_event_ctrl_prepare(void)
{
    g_usb_event_ctrl.module_number = g_basic0_cfg.module_number;
    g_usb_event_ctrl.type          = USB_CLASS_HVND;
}

static void usb_app_handle_event(const usb_event_info_t * p_event)
{
    g_usb_event_ctrl.module_number  = p_event->module_number;
    g_usb_event_ctrl.device_address = p_event->device_address;
    g_usb_event_ctrl.event          = p_event->event;
    g_usb_app_debug.last_usb_event  = (uint16_t) p_event->event;

    switch (p_event->event)
    {
        case USB_STATUS_CONFIGURED:
        {
            if (0U != p_event->device_address)
            {
                g_usb_app_debug.configured_seen = 1U;
                g_usb_app_debug.configured_addr = p_event->device_address;
                g_pending_addr                  = p_event->device_address;
                g_need_ch340_bringup            = true;
                g_bringup_delay_loops           = 0U;
                ch340_host_on_configured(p_event->device_address);
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

#if (2 == BSP_CFG_RTOS)
static void usb_app_rtos_callback(usb_event_info_t * p_event, usb_hdl_t task_hdl, usb_onoff_t onoff)
{
    FSP_PARAMETER_NOT_USED(onoff);

    g_usb_app_debug.usb_callback_count++;

    if ((USB_STATUS_REQUEST_COMPLETE == p_event->event) && (NULL != task_hdl))
    {
        xTaskNotifyGive(task_hdl);
    }

    usb_app_handle_event(p_event);
}
#endif

fsp_err_t usb_app_init(void)
{
    fsp_err_t err;

    g_usb_stack_ready = false;

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

#if (2 == BSP_CFG_RTOS)
    g_usb_cfg_rtos                     = g_basic0_cfg;
    g_usb_cfg_rtos.p_usb_apl_callback  = usb_app_rtos_callback;
    err                                = R_USB_Open(&g_basic0_ctrl, &g_usb_cfg_rtos);
#else
    err = R_USB_Open(&g_basic0_ctrl, &g_basic0_cfg);
#endif

    g_usb_app_debug.usb_open_err = err;
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    usb_event_ctrl_prepare();
    (void) R_USB_VbusSet(&g_usb_event_ctrl, USB_ON);
    R_BSP_SoftwareDelay(200U, BSP_DELAY_UNITS_MILLISECONDS);

    g_usb_stack_ready              = true;
    g_usb_app_debug.stack_ready    = 1U;
    return FSP_SUCCESS;
}

void usb_app_poll(void)
{
#if (0 == BSP_CFG_RTOS)
    usb_status_t event = USB_STATUS_NONE;

    while (FSP_SUCCESS == R_USB_EventGet(&g_usb_event_ctrl, &event))
    {
        usb_event_info_t info;

        if (USB_STATUS_NONE == event)
        {
            break;
        }

        memset(&info, 0, sizeof(info));
        info.module_number  = g_usb_event_ctrl.module_number;
        info.device_address = g_usb_event_ctrl.device_address;
        info.event          = event;
        usb_app_handle_event(&info);
    }
#endif

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
