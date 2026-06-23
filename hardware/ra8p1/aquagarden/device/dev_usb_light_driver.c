#include "dev_usb_light_driver.h"
#include "../ra_gen/main_service.h"
#include <string.h>

#if (2 == BSP_CFG_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define USB_LIGHT_FRAME_LEN          (8U)
#define CH340_BRINGUP_DELAY_LOOPS    (100U)
#define CH340_CTRL_TIMEOUT_MS        (2000U)
#define CH340_BAUD_TARGET            (4800U)
#define CH341_CLKRATE                (48000000U)
#define CH341_CLK_DIV(ps, fact)      (1U << (12U - (3U * (ps)) - (fact)))

#define CH341_REQ_READ_VERSION       (0x5FU)
#define CH341_REQ_WRITE_REG          (0x9AU)
#define CH341_REQ_MODEM_CTRL         (0xA4U)
#define CH341_REQ_SERIAL_INIT        (0xA1U)
#define CH341_REG_DIVISOR            (0x13U)
#define CH341_REG_PRESCALER          (0x12U)
#define CH341_REG_LCR                (0x18U)
#define CH341_REG_LCR2               (0x25U)
#define CH341_LCR_ENABLE_RX          (0x80U)
#define CH341_LCR_ENABLE_TX          (0x40U)
#define CH341_LCR_CS8                (0x03U)
#define CH341_BIT_RTS                (0x20U)
#define CH341_BIT_DTR                (0x10U)

#define CH340_VENDOR_OUT_TYPE        ((uint16_t) (USB_HOST_TO_DEV | USB_VENDOR | USB_DEVICE))
#define CH340_VENDOR_IN_TYPE         ((uint16_t) (USB_DEV_TO_HOST | USB_VENDOR | USB_DEVICE))

static const uint8_t s_light_frames[][USB_LIGHT_FRAME_LEN] =
{
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x11U, 0xE8U, 0x3AU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x12U, 0xA8U, 0x3BU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x13U, 0x69U, 0xFBU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x14U, 0x28U, 0x39U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x15U, 0xE9U, 0xF9U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x16U, 0xA9U, 0xF8U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x17U, 0x68U, 0x38U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x18U, 0x28U, 0x3CU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x21U, 0xE9U, 0xFEU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x22U, 0xA9U, 0xFFU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x23U, 0x68U, 0x3FU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x24U, 0x29U, 0xFDU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x25U, 0xE8U, 0x3DU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x26U, 0xA8U, 0x3CU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x27U, 0x69U, 0xFCU},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x28U, 0x29U, 0xF8U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x31U, 0xE8U, 0x37U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x32U, 0xA8U, 0x36U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x33U, 0x69U, 0xF6U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x34U, 0x28U, 0x34U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x35U, 0xE9U, 0xF4U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x36U, 0xA9U, 0xF5U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x37U, 0x68U, 0x35U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x38U, 0x28U, 0x31U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x40U, 0x29U, 0xE2U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x41U, 0xE8U, 0x22U},
    {0x01U, 0x06U, 0x00U, 0xC2U, 0x00U, 0x60U, 0xE9U, 0xEBU},
};

static usb_instance_ctrl_t s_usb_event_ctrl;
static bool               s_usb_opened;
static volatile bool      s_ready;
static volatile bool      s_need_bringup;
static volatile uint8_t   s_device_address;
static uint8_t            s_bulk_out_pipe;
static uint8_t            s_chip_version;
static uint8_t            s_mode = 0xFFU;
static volatile uint16_t  s_bringup_delay;
static fsp_err_t          s_last_error = FSP_ERR_NOT_INITIALIZED;

#if (2 == BSP_CFG_RTOS)
static usb_cfg_t s_usb_cfg_rtos;
#endif

static void usb_ctrl_prepare(void)
{
    s_usb_event_ctrl.module_number = g_basic0_cfg.module_number;
    s_usb_event_ctrl.type = USB_CLASS_HVND;
}

#if (2 == BSP_CFG_RTOS)
/* In RTOS mode the FSP USB stack delivers events through this registered
 * callback (running in the USB manager task). A NULL callback here was the
 * root cause of the Default_Handler crash during enumeration. */
static void dev_usb_light_usb_callback(usb_event_info_t * p_event, usb_hdl_t task_hdl, usb_onoff_t onoff)
{
    FSP_PARAMETER_NOT_USED(onoff);

    if (NULL == p_event)
    {
        return;
    }

    /* Control transfer finished: wake whichever task issued it (main_service). */
    if ((USB_STATUS_REQUEST_COMPLETE == p_event->event) && (NULL != task_hdl))
    {
        xTaskNotifyGive(task_hdl);
    }

    switch (p_event->event)
    {
        case USB_STATUS_CONFIGURED:
        {
            if (0U != p_event->device_address)
            {
                s_device_address = p_event->device_address;
                s_ready          = false;
                s_need_bringup   = true;
                s_bringup_delay  = 0U;
            }
            break;
        }

        case USB_STATUS_DETACH:
        {
            s_ready          = false;
            s_need_bringup   = false;
            s_device_address = 0U;
            s_bulk_out_pipe  = 0U;
            break;
        }

        default:
        {
            break;
        }
    }
}
#endif

static uint16_t ch340_setup_type(uint8_t request, bool host_to_device)
{
    uint16_t type = host_to_device ? CH340_VENDOR_OUT_TYPE : CH340_VENDOR_IN_TYPE;
    return (uint16_t) (((uint16_t) request << 8) | type);
}

static int ch341_get_divisor(uint32_t speed)
{
    uint32_t fact = 1U;
    int ps;

    for (ps = 3; ps >= 0; ps--)
    {
        uint32_t min_rate = CH341_CLKRATE / (CH341_CLK_DIV((uint32_t) ps, 1U) * 512U);
        if (speed > min_rate)
        {
            break;
        }
    }
    if (ps < 0)
    {
        return -1;
    }

    uint32_t clk_div = CH341_CLK_DIV((uint32_t) ps, fact);
    uint32_t div = CH341_CLKRATE / (clk_div * speed);
    if ((div < 9U) || (div > 255U))
    {
        div /= 2U;
        fact = 0U;
    }
    if (div < 2U)
    {
        return -1;
    }

    return (int) (((0x100U - div) << 8) | (fact << 2) | (uint32_t) ps);
}

static fsp_err_t wait_usb_event(usb_status_t expected)
{
#if (2 == BSP_CFG_RTOS)
    FSP_PARAMETER_NOT_USED(expected);

    /* Block the calling task until the USB callback signals completion. */
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CH340_CTRL_TIMEOUT_MS)) > 0U)
    {
        return FSP_SUCCESS;
    }

    return FSP_ERR_TIMEOUT;
#else
    for (uint32_t elapsed = 0U; elapsed < CH340_CTRL_TIMEOUT_MS; elapsed++)
    {
        usb_status_t event = USB_STATUS_NONE;
        (void) R_USB_EventGet(&s_usb_event_ctrl, &event);
        if (event == expected)
        {
            return FSP_SUCCESS;
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    return FSP_ERR_TIMEOUT;
#endif
}

static fsp_err_t ch340_vendor_transfer(uint8_t request, uint16_t value, uint16_t index,
                                       uint8_t * p_buf, uint16_t len, bool host_to_device)
{
    usb_setup_t setup;
    fsp_err_t err;

    setup.request_type = ch340_setup_type(request, host_to_device);
    setup.request_value = value;
    setup.request_index = index;
    setup.request_length = len;

    usb_ctrl_prepare();
    s_usb_event_ctrl.device_address = s_device_address;
    err = R_USB_HostControlTransfer(&s_usb_event_ctrl, &setup, p_buf, s_device_address);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return wait_usb_event(USB_STATUS_REQUEST_COMPLETE);
}

static fsp_err_t ch340_find_bulk_out_pipe(void)
{
    uint16_t used = 0U;
    fsp_err_t err = R_USB_UsedPipesGet(&s_usb_event_ctrl, &used, s_device_address);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    for (uint8_t pipe = USB_PIPE1; pipe <= USB_PIPE9; pipe++)
    {
        usb_pipe_t info;
        if (0U == (used & (1U << pipe)))
        {
            continue;
        }
        if (FSP_SUCCESS != R_USB_PipeInfoGet(&s_usb_event_ctrl, &info, pipe))
        {
            continue;
        }
        if ((USB_TRANSFER_TYPE_BULK == info.transfer_type) && (0U == (info.endpoint & USB_EP_DIR_IN)))
        {
            s_bulk_out_pipe = pipe;
            return FSP_SUCCESS;
        }
    }

    return FSP_ERR_NOT_FOUND;
}

static fsp_err_t ch340_bringup(void)
{
    uint8_t version[2] = {0U};
    uint8_t lcr = CH341_LCR_ENABLE_RX | CH341_LCR_ENABLE_TX | CH341_LCR_CS8;
    uint8_t mcr = CH341_BIT_RTS | CH341_BIT_DTR;
    int divisor;
    fsp_err_t err;

    err = ch340_vendor_transfer(CH341_REQ_READ_VERSION, 0U, 0U, version, sizeof(version), false);
    if (FSP_SUCCESS != err)
    {
        return err;
    }
    s_chip_version = version[0];

    err = ch340_vendor_transfer(CH341_REQ_SERIAL_INIT, 0U, 0U, NULL, 0U, true);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    divisor = ch341_get_divisor(CH340_BAUD_TARGET);
    if (divisor < 0)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    if (s_chip_version > 0x27U)
    {
        divisor |= 0x80;
    }

    err = ch340_vendor_transfer(CH341_REQ_WRITE_REG,
                                (uint16_t) ((CH341_REG_DIVISOR << 8) | CH341_REG_PRESCALER),
                                (uint16_t) divisor, NULL, 0U, true);
    if (FSP_SUCCESS != err)
    {
        return err;
    }
    if (s_chip_version >= 0x30U)
    {
        err = ch340_vendor_transfer(CH341_REQ_WRITE_REG,
                                    (uint16_t) ((CH341_REG_LCR2 << 8) | CH341_REG_LCR),
                                    lcr, NULL, 0U, true);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    }

    err = ch340_vendor_transfer(CH341_REQ_MODEM_CTRL, (uint16_t) (~mcr), 0U, NULL, 0U, true);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return ch340_find_bulk_out_pipe();
}

void dev_usb_light_init(void)
{
    memset(&s_usb_event_ctrl, 0, sizeof(s_usb_event_ctrl));
    usb_ctrl_prepare();

    s_ready          = false;
    s_need_bringup   = false;
    s_device_address = 0U;
    s_bulk_out_pipe  = 0U;

    if (!s_usb_opened)
    {
#if (2 == BSP_CFG_RTOS)
        /* Open with a cfg copy whose application callback is non-NULL, so the
         * USB stack can deliver enumeration/transfer events instead of calling
         * a NULL pointer. */
        s_usb_cfg_rtos                    = g_basic0_cfg;
        s_usb_cfg_rtos.p_usb_apl_callback = dev_usb_light_usb_callback;
        s_last_error = R_USB_Open(&g_basic0_ctrl, &s_usb_cfg_rtos);
#else
        s_last_error = R_USB_Open(&g_basic0_ctrl, &g_basic0_cfg);
#endif
        if (FSP_SUCCESS == s_last_error)
        {
            s_usb_opened = true;
            usb_ctrl_prepare();
            (void) R_USB_VbusSet(&s_usb_event_ctrl, USB_ON);
#if (2 == BSP_CFG_RTOS)
            /* Let VBUS settle before the host starts enumeration. */
            vTaskDelay(pdMS_TO_TICKS(200U));
#else
            R_BSP_SoftwareDelay(200U, BSP_DELAY_UNITS_MILLISECONDS);
#endif
        }
    }
}

void dev_usb_light_process(void)
{
    if (!s_usb_opened)
    {
        return;
    }

#if (0 == BSP_CFG_RTOS)
    /* Bare-metal fallback: pump events here. In RTOS mode events arrive via the
     * registered callback, so no polling is needed. */
    usb_status_t event = USB_STATUS_NONE;

    while (FSP_SUCCESS == R_USB_EventGet(&s_usb_event_ctrl, &event))
    {
        if (USB_STATUS_NONE == event)
        {
            break;
        }

        if (USB_STATUS_CONFIGURED == event)
        {
            s_device_address = s_usb_event_ctrl.device_address;
            if (0U != s_device_address)
            {
                s_ready = false;
                s_need_bringup = true;
                s_bringup_delay = 0U;
            }
        }
        else if (USB_STATUS_DETACH == event)
        {
            s_ready = false;
            s_need_bringup = false;
            s_device_address = 0U;
            s_bulk_out_pipe = 0U;
        }
    }
#endif

    if (s_need_bringup)
    {
        if (s_bringup_delay < CH340_BRINGUP_DELAY_LOOPS)
        {
            s_bringup_delay++;
        }
        else
        {
            s_need_bringup = false;
            s_last_error = ch340_bringup();
            s_ready = (FSP_SUCCESS == s_last_error);
        }
    }
}

fsp_err_t dev_usb_light_set_mode(uint8_t mode)
{
    uint8_t frame[USB_LIGHT_FRAME_LEN];
    fsp_err_t err;

    if (mode >= (sizeof(s_light_frames) / sizeof(s_light_frames[0])))
    {
        s_last_error = FSP_ERR_INVALID_ARGUMENT;
        return s_last_error;
    }
    if (!s_ready)
    {
        s_last_error = FSP_ERR_INVALID_STATE;
        return s_last_error;
    }

    memcpy(frame, s_light_frames[mode], sizeof(frame));
    usb_ctrl_prepare();
    s_usb_event_ctrl.device_address = s_device_address;
    err = R_USB_PipeWrite(&s_usb_event_ctrl, frame, sizeof(frame), s_bulk_out_pipe);
    if (FSP_SUCCESS == err)
    {
        s_mode = mode;
    }
    s_last_error = err;
    return err;
}

bool dev_usb_light_ready(void)
{
    return s_ready;
}

uint8_t dev_usb_light_get_mode(void)
{
    return s_mode;
}

fsp_err_t dev_usb_light_get_last_error(void)
{
    return s_last_error;
}
