/*
 * Precision touchpad over USB: a second HID interface (HID_1) next to ZMK's keyboard/mouse one.
 *
 * Report descriptor follows Microsoft's Windows Precision Touchpad collection
 * (learn.microsoft.com/windows-hardware/design/component-guidelines/
 * touchpad-windows-precision-touchpad-collection), which Linux's hid-multitouch also drives:
 * it switches the device into touchpad mode through the Input Mode feature report on probe.
 * Until a host does that, ptp_active() is false and the driver keeps sending mouse reports.
 *
 * Layout and report fields were cross-checked against FluxWav's zmk-zephyr41-ptp patch (MIT);
 * the PTPHQA certification blob is Microsoft's published sample blob, as used there.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zmk/usb.h>
#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>

#include <ptp.h>

LOG_MODULE_REGISTER(ptp_usb, CONFIG_PTP_USB_LOG_LEVEL);

#define RID_INPUT 0x01
#define RID_CAPS 0x02
#define RID_HQA 0x03
#define RID_MODE 0x04
#define RID_SELECTIVE 0x05

#define INPUT_MODE_MOUSE 0
#define INPUT_MODE_TOUCHPAD 3

#define PAD_TYPE_DEPRESSIBLE 0
#define PAD_TYPE_PRESSURE 1
#define PAD_TYPE_NON_CLICKABLE 2

#define LE16(v) ((v) & 0xFF), (((v) >> 8) & 0xFF)

/* One finger: confidence + tip (1 bit each), contact id (4), pad (2), X (16), Y (16). */
#define FINGER_DESC                                                                                    0x05, 0x0D,       /*   Usage Page (Digitizers) */                                                    0x09, 0x22,   /*   Usage (Finger) */                                                             0xA1, 0x02,   /*   Collection (Logical) */                                                       0x15, 0x00,   /*     Logical Min (0) */                                                          0x25, 0x01,   /*     Logical Max (1) */                                                          0x09, 0x47,   /*     Usage (Confidence) */                                                       0x09, 0x42,   /*     Usage (Tip Switch) */                                                       0x95, 0x02,   /*     Report Count (2) */                                                         0x75, 0x01,   /*     Report Size (1) */                                                          0x81, 0x02,   /*     Input (Data, Var, Abs) */                                                   0x25, 0x0F,   /*     Logical Max (15) */                                                         0x09, 0x51,   /*     Usage (Contact Identifier) */                                               0x95, 0x01,   /*     Report Count (1) */                                                         0x75, 0x04,   /*     Report Size (4) */                                                          0x81, 0x02,   /*     Input (Data, Var, Abs) */                                                   0x75, 0x01,   /*     Report Size (1) */                                                          0x95, 0x02,   /*     Report Count (2) */                                                         0x81, 0x03,   /*     Input (Const) padding */                                                    0x05, 0x01,   /*     Usage Page (Generic Desktop) */                                             0x75, 0x10,   /*     Report Size (16) */                                                         0x95, 0x01,   /*     Report Count (1) */                                                         0x55, 0x0E,   /*     Unit Exponent (-2) */                                                       0x65, 0x11,   /*     Unit (cm) */                                                                0x35, 0x00,   /*     Physical Min (0) */                                                         0x26, LE16(CONFIG_PTP_LOGICAL_X),     /* Logical Max */                                          0x46, LE16(CONFIG_PTP_PHYSICAL_X),    /* Physical Max (0.1 mm) */                                0x09, 0x30,   /*     Usage (X) */                                                                0x81, 0x02,   /*     Input (Data, Var, Abs) */                                                   0x26, LE16(CONFIG_PTP_LOGICAL_Y),     /* Logical Max */                                          0x46, LE16(CONFIG_PTP_PHYSICAL_Y),    /* Physical Max (0.1 mm) */                                0x09, 0x31,   /*     Usage (Y) */                                                                0x81, 0x02,   /*     Input (Data, Var, Abs) */                                                   0xC0          /*   End Collection */

static const uint8_t report_desc[] = {
    0x05, 0x0D, /* Usage Page (Digitizers) */
    0x09, 0x05, /* Usage (Touch Pad) */
    0xA1, 0x01, /* Collection (Application) */
    0x85, RID_INPUT,
    FINGER_DESC, FINGER_DESC, FINGER_DESC, FINGER_DESC, FINGER_DESC,

    /* Scan time: 16 bits, 100 us units. */
    0x05, 0x0D,                   /* Usage Page (Digitizers) */
    0x55, 0x0C,                   /* Unit Exponent (-4) */
    0x66, 0x01, 0x10,             /* Unit (seconds) */
    0x47, 0xFF, 0xFF, 0x00, 0x00, /* Physical Max (65535) */
    0x27, 0xFF, 0xFF, 0x00, 0x00, /* Logical Max (65535) */
    0x75, 0x10,                   /* Report Size (16) */
    0x95, 0x01,                   /* Report Count (1) */
    0x09, 0x56,                   /* Usage (Scan Time) */
    0x81, 0x02,                   /* Input (Data, Var, Abs) */

    /* Contact count (4 bits), buttons 1..3, 1 bit padding. */
    0x55, 0x00,       /* Unit Exponent (0) */
    0x65, 0x00,       /* Unit (none) */
    0x45, 0x00,       /* Physical Max (0) */
    0x25, PTP_MAX_CONTACTS, /* Logical Max */
    0x09, 0x54,       /* Usage (Contact Count) */
    0x75, 0x04,       /* Report Size (4) */
    0x95, 0x01,       /* Report Count (1) */
    0x81, 0x02,       /* Input (Data, Var, Abs) */
    0x05, 0x09,       /* Usage Page (Button) */
    0x19, 0x01,       /* Usage Min (1) */
    0x29, 0x03,       /* Usage Max (3) */
    0x25, 0x01,       /* Logical Max (1) */
    0x75, 0x01,       /* Report Size (1) */
    0x95, 0x03,       /* Report Count (3) */
    0x81, 0x02,       /* Input (Data, Var, Abs) */
    0x95, 0x01,       /* Report Count (1) */
    0x81, 0x03,       /* Input (Const) padding */

    /* Feature: device capabilities (max contacts, pad type). */
    0x05, 0x0D,       /* Usage Page (Digitizers) */
    0x85, RID_CAPS,
    0x09, 0x55,       /* Usage (Contact Count Maximum) */
    0x09, 0x59,       /* Usage (Pad Type) */
    0x25, 0x0F,       /* Logical Max (15) */
    0x75, 0x04,       /* Report Size (4) */
    0x95, 0x02,       /* Report Count (2) */
    0xB1, 0x02,       /* Feature (Data, Var, Abs) */

    /* Feature: PTPHQA certification blob (vendor page 0xFF00, usage 0xC5). Its presence is
     * also what makes Linux treat the device as a Windows 8 multitouch device. */
    0x06, 0x00, 0xFF, /* Usage Page (Vendor 0xFF00) */
    0x85, RID_HQA,
    0x09, 0xC5,       /* Usage (0xC5) */
    0x15, 0x00,       /* Logical Min (0) */
    0x26, 0xFF, 0x00, /* Logical Max (255) */
    0x75, 0x08,       /* Report Size (8) */
    0x96, 0x00, 0x01, /* Report Count (256) */
    0xB1, 0x02,       /* Feature (Data, Var, Abs) */
    0xC0,             /* End Collection */

    /* Configuration collection: input mode + selective reporting. */
    0x05, 0x0D, /* Usage Page (Digitizers) */
    0x09, 0x0E, /* Usage (Device Configuration) */
    0xA1, 0x01, /* Collection (Application) */
    0x85, RID_MODE,
    0x09, 0x22, /*   Usage (Finger) */
    0xA1, 0x02, /*   Collection (Logical) */
    0x09, 0x52, /*     Usage (Device Mode) */
    0x15, 0x00, /*     Logical Min (0) */
    0x25, 0x0A, /*     Logical Max (10) */
    0x75, 0x08, /*     Report Size (8) */
    0x95, 0x01, /*     Report Count (1) */
    0xB1, 0x02, /*     Feature (Data, Var, Abs) */
    0xC0,       /*   End Collection */
    0x09, 0x22, /*   Usage (Finger) */
    0xA1, 0x00, /*   Collection (Physical) */
    0x85, RID_SELECTIVE,
    0x09, 0x57, /*     Usage (Surface Switch) */
    0x09, 0x58, /*     Usage (Button Switch) */
    0x75, 0x01, /*     Report Size (1) */
    0x95, 0x02, /*     Report Count (2) */
    0x25, 0x01, /*     Logical Max (1) */
    0xB1, 0x02, /*     Feature (Data, Var, Abs) */
    0x95, 0x06, /*     Report Count (6) */
    0xB1, 0x03, /*     Feature (Const) padding */
    0xC0,       /*   End Collection */
    0xC0,       /* End Collection */
};

#define FINGER_BYTES 5
#define INPUT_REPORT_LEN (1 + PTP_MAX_CONTACTS * FINGER_BYTES + 2 + 1)

BUILD_ASSERT(INPUT_REPORT_LEN <= CONFIG_HID_INTERRUPT_EP_MPS,
             "PTP input report doesn't fit one packet: raise CONFIG_HID_INTERRUPT_EP_MPS");

static const uint8_t hqa_report[1 + 256] = {
    RID_HQA,
    0xfc, 0x28, 0xfe, 0x84, 0x40, 0xcb, 0x9a, 0x87, 0x0d, 0xbe, 0x57, 0x3c,
    0xb6, 0x70, 0x09, 0x88, 0x07, 0x97, 0x2d, 0x2b, 0xe3, 0x38, 0x34, 0xb6,
    0x6c, 0xed, 0xb0, 0xf7, 0xe5, 0x9c, 0xf6, 0xc2, 0x2e, 0x84, 0x1b, 0xe8,
    0xb4, 0x51, 0x78, 0x43, 0x1f, 0x28, 0x4b, 0x7c, 0x2d, 0x53, 0xaf, 0xfc,
    0x47, 0x70, 0x1b, 0x59, 0x6f, 0x74, 0x43, 0xc4, 0xf3, 0x47, 0x18, 0x53,
    0x1a, 0xa2, 0xa1, 0x71, 0xc7, 0x95, 0x0e, 0x31, 0x55, 0x21, 0xd3, 0xb5,
    0x1e, 0xe9, 0x0c, 0xba, 0xec, 0xb8, 0x89, 0x19, 0x3e, 0xb3, 0xaf, 0x75,
    0x81, 0x9d, 0x53, 0xb9, 0x41, 0x57, 0xf4, 0x6d, 0x39, 0x25, 0x29, 0x7c,
    0x87, 0xd9, 0xb4, 0x98, 0x45, 0x7d, 0xa7, 0x26, 0x9c, 0x65, 0x3b, 0x85,
    0x68, 0x89, 0xd7, 0x3b, 0xbd, 0xff, 0x14, 0x67, 0xf2, 0x2b, 0xf0, 0x2a,
    0x41, 0x54, 0xf0, 0xfd, 0x2c, 0x66, 0x7c, 0xf8, 0xc0, 0x8f, 0x33, 0x13,
    0x03, 0xf1, 0xd3, 0xc1, 0x0b, 0x89, 0xd9, 0x1b, 0x62, 0xcd, 0x51, 0xb7,
    0x80, 0xb8, 0xaf, 0x3a, 0x10, 0xc1, 0x8a, 0x5b, 0xe8, 0x8a, 0x56, 0xf0,
    0x8c, 0xaa, 0xfa, 0x35, 0xe9, 0x42, 0xc4, 0xd8, 0x55, 0xc3, 0x38, 0xcc,
    0x2b, 0x53, 0x5c, 0x69, 0x52, 0xd5, 0xc8, 0x73, 0x02, 0x38, 0x7c, 0x73,
    0xb6, 0x41, 0xe7, 0xff, 0x05, 0xd8, 0x2b, 0x79, 0x9a, 0xe2, 0x34, 0x60,
    0x8f, 0xa3, 0x32, 0x1f, 0x09, 0x78, 0x62, 0xbc, 0x80, 0xe3, 0x0f, 0xbd,
    0x65, 0x20, 0x08, 0x13, 0xc1, 0xe2, 0xee, 0x53, 0x2d, 0x86, 0x7e, 0xa7,
    0x5a, 0xc5, 0xd3, 0x7d, 0x98, 0xbe, 0x31, 0x48, 0x1f, 0xfb, 0xda, 0xaf,
    0xa2, 0xa8, 0x6a, 0x89, 0xd6, 0xbf, 0xf2, 0xd3, 0x32, 0x2a, 0x9a, 0xe4,
    0xcf, 0x17, 0xb7, 0xb8, 0xf4, 0xe1, 0x33, 0x08, 0x24, 0x8b, 0xc4, 0x43,
    0xa5, 0xe5, 0x24, 0xc2,
};

static const struct device *hid_dev;
static K_SEM_DEFINE(in_sem, 1, 1);

static uint8_t input_report[INPUT_REPORT_LEN] = {RID_INPUT};
static uint8_t caps_report[2] = {RID_CAPS, PTP_MAX_CONTACTS | (PAD_TYPE_NON_CLICKABLE << 4)};
static uint8_t mode_report[2] = {RID_MODE, INPUT_MODE_MOUSE};
static uint8_t selective_report[2] = {RID_SELECTIVE, 0x03}; /* surface + buttons on */

bool ptp_active(void) {
    return hid_dev && mode_report[1] == INPUT_MODE_TOUCHPAD && (selective_report[1] & BIT(0)) &&
           zmk_usb_is_hid_ready();
}

int ptp_send(const struct ptp_frame *frame) {
    if (!ptp_active()) {
        return -EAGAIN;
    }

    uint8_t *p = &input_report[1];
    memset(p, 0, INPUT_REPORT_LEN - 1);
    uint8_t count = MIN(frame->count, PTP_MAX_CONTACTS);
    for (int i = 0; i < count; i++) {
        const struct ptp_contact *c = &frame->contacts[i];
        p[0] = BIT(0) /* confidence */ | (c->tip ? BIT(1) : 0) | ((c->id & 0x0F) << 2);
        sys_put_le16(MIN(c->x, CONFIG_PTP_LOGICAL_X), &p[1]);
        sys_put_le16(MIN(c->y, CONFIG_PTP_LOGICAL_Y), &p[3]);
        p += FINGER_BYTES;
    }
    p = &input_report[1 + PTP_MAX_CONTACTS * FINGER_BYTES];
    uint16_t scan_time = (uint16_t)(k_ticks_to_us_floor64(k_uptime_ticks()) / 100);
    sys_put_le16(scan_time, p);
    uint8_t buttons = (selective_report[1] & BIT(1)) ? (frame->buttons & 0x07) : 0;
    p[2] = count | (buttons << 4);

    if (zmk_usb_get_status() == USB_DC_SUSPEND) {
        return usb_wakeup_request();
    }
    k_sem_take(&in_sem, K_MSEC(30));
    int err = hid_int_ep_write(hid_dev, input_report, sizeof(input_report), NULL);
    if (err) {
        k_sem_give(&in_sem);
    }
    return err;
}

static void in_ready_cb(const struct device *dev) { k_sem_give(&in_sem); }

#define REPORT_TYPE(setup) ((setup)->wValue >> 8)
#define REPORT_ID(setup) ((setup)->wValue & 0xFF)
#define TYPE_INPUT 1
#define TYPE_FEATURE 3

static int get_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
    uint8_t type = REPORT_TYPE(setup), id = REPORT_ID(setup);

    if (type == TYPE_INPUT && id == RID_INPUT) {
        *data = input_report;
        *len = sizeof(input_report);
        return 0;
    }
    if (type != TYPE_FEATURE) {
        return -ENOTSUP;
    }
    switch (id) {
    case RID_CAPS:
        *data = caps_report;
        *len = sizeof(caps_report);
        return 0;
    case RID_HQA:
        *data = (uint8_t *)hqa_report;
        *len = sizeof(hqa_report);
        return 0;
    case RID_MODE:
        *data = mode_report;
        *len = sizeof(mode_report);
        return 0;
    case RID_SELECTIVE:
        *data = selective_report;
        *len = sizeof(selective_report);
        return 0;
    default:
        LOG_WRN("get feature %d: unknown", id);
        return -ENOTSUP;
    }
}

static int set_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
    if (REPORT_TYPE(setup) != TYPE_FEATURE || *len < 2) {
        return -ENOTSUP;
    }
    uint8_t *buf = *data;
    switch (REPORT_ID(setup)) {
    case RID_MODE:
        mode_report[1] = buf[1];
        LOG_INF("host set input mode %d (%s)", buf[1],
                buf[1] == INPUT_MODE_TOUCHPAD ? "touchpad" : "mouse");
        return 0;
    case RID_SELECTIVE:
        selective_report[1] = buf[1] & 0x03;
        LOG_INF("host set selective reporting: surface %d, buttons %d", buf[1] & 1,
                (buf[1] >> 1) & 1);
        return 0;
    default:
        return -ENOTSUP;
    }
}

static const struct hid_ops ops = {
    .int_in_ready = in_ready_cb,
    .get_report = get_report_cb,
    .set_report = set_report_cb,
};

/* A new host (or a re-plug) has to choose touchpad mode again. */
static int usb_listener(const zmk_event_t *eh) {
    if (!zmk_usb_is_hid_ready()) {
        mode_report[1] = INPUT_MODE_MOUSE;
        selective_report[1] = 0x03;
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(ptp_usb, usb_listener);
ZMK_SUBSCRIPTION(ptp_usb, zmk_usb_conn_state_changed);

static int ptp_usb_init(void) {
    hid_dev = device_get_binding("HID_1");
    if (!hid_dev) {
        LOG_ERR("HID_1 missing: set CONFIG_USB_HID_DEVICE_COUNT=2");
        return -ENODEV;
    }
    usb_hid_register_device(hid_dev, report_desc, sizeof(report_desc), &ops);
    return usb_hid_init(hid_dev);
}

/* Before ZMK enables the USB stack (CONFIG_ZMK_USB_INIT_PRIORITY). */
SYS_INIT(ptp_usb_init, APPLICATION, CONFIG_PTP_USB_INIT_PRIORITY);
