/*
 * Touchpad frames from a trackpad split peripheral to the central (dongle).
 *
 * A small GATT service next to ZMK's split service:
 *   frames  notify   one packed frame (ptp_frame_pack) per controller report
 *   mode    write    1 byte from the central: 1 = its host has this pad in touchpad mode
 *   side    read     1 byte: PTP_SPLIT_SIDE_LEFT or _RIGHT (CONFIG_PTP_SPLIT_PAD_SIDE)
 *
 * Until the central has subscribed and reported touchpad mode, ptp_active() is false and the
 * driver keeps its own mouse-mode gestures.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <ptp.h>
#include "ptp_split.h"

LOG_MODULE_REGISTER(ptp_split, CONFIG_PTP_LOG_LEVEL);

static bool notify_enabled;
static bool central_touchpad_mode;
static uint32_t frames_sent, frames_failed;
static uint16_t last_scan; /* time-byte state, matches the central's (ptp_frame_pack) */

static void frames_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    notify_enabled = value == BT_GATT_CCC_NOTIFY;
    last_scan = 0;
    if (!notify_enabled) {
        central_touchpad_mode = false;
    }
    LOG_INF("central %s frames", notify_enabled ? "subscribed to" : "unsubscribed from");
}

static ssize_t mode_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                          uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset != 0 || len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    central_touchpad_mode = ((const uint8_t *)buf)[0] != 0;
    LOG_INF("central reports %s mode", central_touchpad_mode ? "touchpad" : "mouse");
    return len;
}

static ssize_t side_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                         uint16_t len, uint16_t offset) {
    const uint8_t side = IS_ENABLED(CONFIG_PTP_SPLIT_PAD_SIDE_RIGHT) ? PTP_SPLIT_SIDE_RIGHT
                                                                     : PTP_SPLIT_SIDE_LEFT;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &side, sizeof(side));
}

BT_GATT_SERVICE_DEFINE(
    ptp_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(PTP_SPLIT_SERVICE_UUID)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(PTP_SPLIT_FRAMES_UUID), BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(frames_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(PTP_SPLIT_MODE_UUID),
                           BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE_ENCRYPT, NULL, mode_write, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(PTP_SPLIT_SIDE_UUID), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, side_read, NULL, NULL), );

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    /* The CCC of a bonded central is restored on reconnect, but its mode must be re-sent. */
    central_touchpad_mode = false;
}

BT_CONN_CB_DEFINE(ptp_split_conn_cb) = {
    .disconnected = disconnected,
};

bool ptp_active(void) { return notify_enabled && central_touchpad_mode; }

int ptp_send(const struct ptp_frame *frame) {
    if (!ptp_active()) {
        return -EAGAIN;
    }
    uint8_t buf[PTP_PACKED_MAX];
    size_t len = ptp_frame_pack(frame, &last_scan, buf);
    /* attrs[1] = frames characteristic declaration */
    int err = bt_gatt_notify(NULL, &ptp_svc.attrs[1], buf, len);
    if (err) {
        frames_failed++;
    } else {
        frames_sent++;
    }
    if ((frames_sent + frames_failed) % 500 == 1) {
        LOG_INF("frames sent %u failed %u (last err %d, %u bytes)", frames_sent, frames_failed,
                err, len);
    }
    return err;
}
