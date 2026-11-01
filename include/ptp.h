/*
 * Precision touchpad (Windows PTP / Linux hid-multitouch) frame API.
 *
 * A trackpad driver hands over one frame per scan: every finger currently on the pad, plus a
 * final tip = 0 entry for each finger that just lifted. Where the frame goes depends on the
 * build:
 *   CONFIG_PTP_USB               this device's own USB touchpad interface (USB rig, dongle)
 *   CONFIG_PTP_SPLIT_PERIPHERAL  one BLE notification per frame to the split central
 * On the central, CONFIG_PTP_SPLIT_CENTRAL forwards each pad's frames to its own USB
 * touchpad interface (pad 0 = HID_1, pad 1 = HID_2).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PTP_MAX_CONTACTS 5
#define PTP_COORD_MAX 4095 /* coordinates are 12 bits on the split link */

struct ptp_contact {
    uint8_t id;  /* stable for the lifetime of the touch, 0..7 */
    uint8_t tip; /* 1 = touching, 0 = lifted (sent once) */
    uint16_t x;  /* 0..CONFIG_PTP_LOGICAL_X */
    uint16_t y;  /* 0..CONFIG_PTP_LOGICAL_Y */
};

struct ptp_frame {
    uint8_t count;   /* entries used in contacts[] */
    uint8_t buttons; /* bit 0..2 = button 1..3 */
    struct ptp_contact contacts[PTP_MAX_CONTACTS];
};

/* For the local trackpad driver: true while the host has the touchpad in touchpad mode
 * (Input Mode feature = 3). Hosts that never set it (macOS) keep the driver's mouse output. */
bool ptp_active(void);
int ptp_send(const struct ptp_frame *frame);

/* USB touchpad interfaces, one per pad (CONFIG_PTP_USB). */
bool ptp_usb_pad_active(uint8_t pad);
int ptp_usb_pad_send(uint8_t pad, const struct ptp_frame *frame);
/* Called (from the USB stack / event context) when a pad's touchpad mode turns on or off. */
void ptp_usb_set_mode_callback(void (*cb)(uint8_t pad, bool active));

/* Split link encoding: header byte (count | buttons << 3), then 28 bits per contact
 * (id:3 tip:1 x:12 y:12), LSB first. 5 contacts fit in 19 bytes (default ATT MTU: 20). */
#define PTP_PACKED_MAX (1 + (PTP_MAX_CONTACTS * 28 + 7) / 8)
size_t ptp_frame_pack(const struct ptp_frame *frame, uint8_t out[PTP_PACKED_MAX]);
int ptp_frame_unpack(const uint8_t *buf, size_t len, struct ptp_frame *frame);
