/**
 * CH340 USB-serial bridge – host-side init (Linux ch341.c protocol) + bulk OUT.
 */
#include "ch340_host.h"
#include "hal_data.h"
#include <string.h>

#if (2 == BSP_CFG_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define CH341_REQ_READ_VERSION    (0x5FU)
#define CH341_REQ_WRITE_REG       (0x9AU)
#define CH341_REQ_MODEM_CTRL      (0xA4U)
#define CH341_REQ_SERIAL_INIT     (0xA1U)

#define CH341_REG_DIVISOR         (0x13U)
#define CH341_REG_PRESCALER       (0x12U)
#define CH341_REG_LCR             (0x18U)
#define CH341_REG_LCR2            (0x25U)

#define CH341_LCR_ENABLE_RX       (0x80U)
#define CH341_LCR_ENABLE_TX       (0x40U)
#define CH341_LCR_CS8             (0x03U)

#define CH341_BIT_RTS             (0x20U)
#define CH341_BIT_DTR             (0x10U)

#define CH341_CLKRATE             (48000000U)
#define CH341_CLK_DIV(ps, fact)   (1U << (12U - (3U * (uint32_t) (ps)) - (fact)))

#define CH340_BAUD_TARGET         (4800U)

#define CH340_CTRL_TIMEOUT_MS     (2000U)

#define CH340_PIPE_SCAN_START     (USB_PIPE1)
#define CH340_PIPE_SCAN_END       (USB_PIPE9)

#define CH340_VENDOR_OUT_TYPE     ((uint16_t) (USB_HOST_TO_DEV | USB_VENDOR | USB_DEVICE))
#define CH340_VENDOR_IN_TYPE      ((uint16_t) (USB_DEV_TO_HOST | USB_VENDOR | USB_DEVICE))

static usb_instance_ctrl_t g_usb_event_ctrl;

static uint8_t  g_device_address;
static uint8_t  g_bulk_out_pipe;
static uint8_t  g_chip_version;
static bool     g_ready;
static fsp_err_t g_last_error;

static void ch340_usb_ctrl_prepare(void)
{
    g_usb_event_ctrl.module_number = g_basic0_cfg.module_number;
    g_usb_event_ctrl.type          = USB_CLASS_HVND;
}

static uint16_t ch340_setup_type(uint8_t request, bool host_to_device)
{
    uint16_t type = host_to_device ? CH340_VENDOR_OUT_TYPE : CH340_VENDOR_IN_TYPE;

    return (uint16_t) (((uint16_t) request << 8) | type);
}

static int ch341_get_divisor(uint32_t speed)
{
    uint32_t fact = 1U;
    int      ps;

    if ((speed < 46U) || (speed > 3000000U))
    {
        return -1;
    }

    for (ps = 3; ps >= 0; ps--)
    {
        uint32_t min_rate = CH341_CLKRATE / (CH341_CLK_DIV(ps, 1U) * 512U);
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
    uint32_t div     = CH341_CLKRATE / (clk_div * speed);

    if ((div < 9U) || (div > 255U))
    {
        div     /= 2U;
        clk_div *= 2U;
        fact     = 0U;
    }

    if (div < 2U)
    {
        return -1;
    }

    if ((16U * CH341_CLKRATE / (clk_div * div)) - (16U * speed) >=
        (16U * speed) - (16U * CH341_CLKRATE / (clk_div * (div + 1U))))
    {
        div++;
    }

    if ((1U == fact) && (0U == (div % 2U)))
    {
        div  /= 2U;
        fact  = 0U;
    }

    return (int) (((0x100U - div) << 8) | (fact << 2) | (uint32_t) ps);
}

static fsp_err_t usb_wait_event(usb_status_t expect, uint32_t timeout_ms)
{
#if (2 == BSP_CFG_RTOS)
    FSP_PARAMETER_NOT_USED(expect);

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) > 0U)
    {
        return FSP_SUCCESS;
    }

    return FSP_ERR_TIMEOUT;
#else
    uint32_t elapsed = 0U;

    while (elapsed < timeout_ms)
    {
        usb_status_t ev = USB_STATUS_NONE;

        (void) R_USB_EventGet(&g_usb_event_ctrl, &ev);
        if (expect == ev)
        {
            return FSP_SUCCESS;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        elapsed++;
    }

    return FSP_ERR_TIMEOUT;
#endif
}

static fsp_err_t ch340_vendor_transfer(uint8_t request, uint16_t value, uint16_t index,
                                       uint8_t * buf, uint16_t length, bool host_to_device)
{
    usb_setup_t setup;
    fsp_err_t   err;

    setup.request_type   = ch340_setup_type(request, host_to_device);
    setup.request_value  = value;
    setup.request_index  = index;
    setup.request_length = length;

    ch340_usb_ctrl_prepare();
    g_usb_event_ctrl.device_address = g_device_address;

    err = R_USB_HostControlTransfer(&g_usb_event_ctrl, &setup, buf, g_device_address);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return usb_wait_event(USB_STATUS_REQUEST_COMPLETE, CH340_CTRL_TIMEOUT_MS);
}

static fsp_err_t ch340_set_baudrate_lcr(uint8_t lcr)
{
    int       val;
    fsp_err_t err;

    val = ch341_get_divisor(CH340_BAUD_TARGET);
    if (val < 0)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (g_chip_version > 0x27U)
    {
        val |= 0x80;
    }

    err = ch340_vendor_transfer(CH341_REQ_WRITE_REG,
                                (uint16_t) ((CH341_REG_DIVISOR << 8) | CH341_REG_PRESCALER),
                                (uint16_t) val, NULL, 0U, true);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    if (g_chip_version < 0x30U)
    {
        return FSP_SUCCESS;
    }

    return ch340_vendor_transfer(CH341_REQ_WRITE_REG,
                                 (uint16_t) ((CH341_REG_LCR2 << 8) | CH341_REG_LCR),
                                 lcr, NULL, 0U, true);
}

static fsp_err_t ch340_configure(void)
{
    uint8_t   buf[2];
    fsp_err_t err;
    uint8_t   mcr = (uint8_t) (CH341_BIT_RTS | CH341_BIT_DTR);
    uint8_t   lcr = (uint8_t) (CH341_LCR_ENABLE_RX | CH341_LCR_ENABLE_TX | CH341_LCR_CS8);

    err = ch340_vendor_transfer(CH341_REQ_READ_VERSION, 0U, 0U, buf, (uint16_t) sizeof(buf), false);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    g_chip_version = buf[0];

    err = ch340_vendor_transfer(CH341_REQ_SERIAL_INIT, 0U, 0U, NULL, 0U, true);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = ch340_set_baudrate_lcr(lcr);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return ch340_vendor_transfer(CH341_REQ_MODEM_CTRL, (uint16_t) (~mcr), 0U, NULL, 0U, true);
}

static fsp_err_t ch340_find_bulk_out_pipe(void)
{
    uint16_t  used = 0U;
    fsp_err_t err;
    uint8_t   pipe;

    err = R_USB_UsedPipesGet(&g_usb_event_ctrl, &used, g_device_address);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    for (pipe = CH340_PIPE_SCAN_START; pipe <= CH340_PIPE_SCAN_END; pipe++)
    {
        usb_pipe_t info;

        if (0U == (used & (1U << pipe)))
        {
            continue;
        }

        err = R_USB_PipeInfoGet(&g_usb_event_ctrl, &info, pipe);
        if (FSP_SUCCESS != err)
        {
            continue;
        }

        if ((USB_TRANSFER_TYPE_BULK == info.transfer_type) &&
            (USB_EP_DIR_IN != (info.endpoint & USB_EP_DIR_IN)))
        {
            g_bulk_out_pipe = pipe;
            return FSP_SUCCESS;
        }
    }

    return FSP_ERR_NOT_FOUND;
}

void ch340_host_on_configured(uint8_t device_address)
{
    g_device_address = device_address;
    g_ready          = false;
    g_last_error     = FSP_SUCCESS;
    ch340_usb_ctrl_prepare();
}

fsp_err_t ch340_host_bringup(void)
{
    fsp_err_t err;

    if (0U == g_device_address)
    {
        g_last_error = FSP_ERR_INVALID_STATE;
        return g_last_error;
    }

    ch340_usb_ctrl_prepare();

    err = ch340_find_bulk_out_pipe();
    if (FSP_SUCCESS != err)
    {
        g_ready      = false;
        g_last_error = err;
        return err;
    }

    err = ch340_configure();
    g_ready      = (FSP_SUCCESS == err);
    g_last_error = err;

    return err;
}

fsp_err_t ch340_host_get_last_error(void)
{
    return g_last_error;
}

void ch340_host_on_detach(void)
{
    g_ready          = false;
    g_bulk_out_pipe  = 0U;
    g_device_address = 0U;
}

fsp_err_t ch340_host_init(void)
{
    g_ready          = false;
    g_bulk_out_pipe  = 0U;
    g_device_address = 0U;
    g_chip_version   = 0U;
    g_last_error     = FSP_SUCCESS;
    memset(&g_usb_event_ctrl, 0, sizeof(g_usb_event_ctrl));
    ch340_usb_ctrl_prepare();

    return FSP_SUCCESS;
}

bool ch340_host_is_ready(void)
{
    return g_ready;
}

fsp_err_t ch340_host_write(const uint8_t * frame, uint32_t length)
{
    static uint8_t tx_buf[CH340_HOST_FRAME_LEN] __attribute__((aligned(4)));

    if (!g_ready || (NULL == frame) || (CH340_HOST_FRAME_LEN != length))
    {
        return FSP_ERR_INVALID_STATE;
    }

    memcpy(tx_buf, frame, CH340_HOST_FRAME_LEN);

    ch340_usb_ctrl_prepare();
    g_usb_event_ctrl.device_address = g_device_address;

    /* Exactly 8 bytes on the wire — no CR/LF, no padding (same as serial HEX send). */
    return R_USB_PipeWrite(&g_usb_event_ctrl, tx_buf, CH340_HOST_FRAME_LEN, g_bulk_out_pipe);
}
