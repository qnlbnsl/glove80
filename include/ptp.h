/*
 * Precision touchpad (Windows PTP / Linux hid-multitouch) frame API.
 *
 * A trackpad driver hands over one frame per scan: every finger currently on the pad, plus a
 * final tip = 0 entry for each finger that just lifted. The transport behind ptp_send() is
 * separate from the driver so the same frames can later travel over the split link (one
 * message per frame) instead of USB.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PTP_MAX_CONTACTS 5

struct ptp_contact {
    uint8_t id;  /* stable for the lifetime of the touch, 0..15 */
    uint8_t tip; /* 1 = touching, 0 = lifted (sent once) */
    uint16_t x;  /* 0..CONFIG_PTP_LOGICAL_X */
    uint16_t y;  /* 0..CONFIG_PTP_LOGICAL_Y */
};

struct ptp_frame {
    uint8_t count;   /* entries used in contacts[] */
    uint8_t buttons; /* bit 0..2 = button 1..3 */
    struct ptp_contact contacts[PTP_MAX_CONTACTS];
};

/* True while the host has switched the device into touchpad mode (Input Mode feature = 3).
 * Hosts that never do (macOS) keep getting the driver's mouse output instead. */
bool ptp_active(void);

int ptp_send(const struct ptp_frame *frame);
