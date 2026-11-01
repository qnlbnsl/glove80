/*
 * Central (dongle) side of the touchpad frame service: finds the service on each split
 * peripheral that has one (the trackpads; the keyboard halves don't), subscribes to its
 * frames and forwards them to that pad's USB touchpad interface. Each pad also learns whether
 * the host has its interface in touchpad mode, so it can fall back to mouse gestures.
 *
 * Pads are numbered in the order they connect (pad 0 -> HID_1, pad 1 -> HID_2).
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

static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(PTP_SPLIT_SERVICE_UUID);
static struct bt_uuid_128 frames_uuid = BT_UUID_INIT_128(PTP_SPLIT_FRAMES_UUID);
static struct bt_uuid_128 mode_uuid = BT_UUID_INIT_128(PTP_SPLIT_MODE_UUID);

struct pad {
    struct bt_conn *conn;
    struct k_work_delayable discover_work;
    struct bt_gatt_discover_params disc;
    struct bt_gatt_subscribe_params sub;
    uint16_t svc_end, frames_handle, mode_handle;
    bool subscribed;
};

static struct pad pads[CONFIG_PTP_USB_PADS];

struct frame_item {
    uint8_t pad;
    struct ptp_frame frame;
};

K_MSGQ_DEFINE(frame_q, sizeof(struct frame_item), 16, 4);

static void tx_work_cb(struct k_work *work) {
    struct frame_item item;
    while (k_msgq_get(&frame_q, &item, K_NO_WAIT) == 0) {
        ptp_usb_pad_send(item.pad, &item.frame);
    }
}

static K_WORK_DEFINE(tx_work, tx_work_cb);

static int pad_index(struct pad *pad) { return pad - pads; }

static struct pad *pad_for_conn(struct bt_conn *conn) {
    for (int i = 0; i < ARRAY_SIZE(pads); i++) {
        if (pads[i].conn == conn) {
            return &pads[i];
        }
    }
    return NULL;
}

static void release(struct pad *pad) {
    k_work_cancel_delayable(&pad->discover_work);
    if (pad->conn) {
        bt_conn_unref(pad->conn);
    }
    pad->conn = NULL;
    pad->subscribed = false;
    pad->frames_handle = pad->mode_handle = 0;
}

static void send_mode(struct pad *pad) {
    if (!pad->conn || !pad->subscribed || !pad->mode_handle) {
        return;
    }
    uint8_t mode = ptp_usb_pad_active(pad_index(pad)) ? 1 : 0;
    int err = bt_gatt_write_without_response(pad->conn, pad->mode_handle, &mode, 1, false);
    LOG_INF("pad %d: told peripheral %s mode (err %d)", pad_index(pad),
            mode ? "touchpad" : "mouse", err);
}

static void mode_work_cb(struct k_work *work) {
    for (int i = 0; i < ARRAY_SIZE(pads); i++) {
        send_mode(&pads[i]);
    }
}

static K_WORK_DEFINE(mode_work, mode_work_cb);

static void usb_mode_changed(uint8_t pad, bool active) { k_work_submit(&mode_work); }

static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length) {
    struct pad *pad = CONTAINER_OF(params, struct pad, sub);
    if (!data) {
        LOG_INF("pad %d: unsubscribed", pad_index(pad));
        pad->subscribed = false;
        return BT_GATT_ITER_STOP;
    }
    struct frame_item item = {.pad = pad_index(pad)};
    if (ptp_frame_unpack(data, length, &item.frame) == 0) {
        if (k_msgq_put(&frame_q, &item, K_NO_WAIT) != 0) {
            LOG_WRN("pad %d: frame queue full, dropping", item.pad);
        }
        k_work_submit(&tx_work);
    }
    return BT_GATT_ITER_CONTINUE;
}

static void subscribed_cb(struct bt_conn *conn, uint8_t err,
                          struct bt_gatt_subscribe_params *params) {
    struct pad *pad = CONTAINER_OF(params, struct pad, sub);
    if (err) {
        LOG_ERR("pad %d: subscribe failed (%d)", pad_index(pad), err);
        return;
    }
    pad->subscribed = true;
    LOG_INF("pad %d: subscribed to frames", pad_index(pad));
    k_work_submit(&mode_work);
}

static void subscribe(struct pad *pad) {
    pad->sub = (struct bt_gatt_subscribe_params){
        .notify = notify_cb,
        .subscribe = subscribed_cb,
        .value_handle = pad->frames_handle,
        .ccc_handle = pad->frames_handle + 1, /* CCC follows the value (our service layout) */
        .value = BT_GATT_CCC_NOTIFY,
    };
    /* Drop it on disconnect even though we're bonded: we rediscover on every connection. */
    atomic_set_bit(pad->sub.flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);
    int err = bt_gatt_subscribe(pad->conn, &pad->sub);
    if (err && err != -EALREADY) {
        LOG_ERR("pad %d: subscribe error %d", pad_index(pad), err);
    }
}

static uint8_t discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params) {
    struct pad *pad = CONTAINER_OF(params, struct pad, disc);

    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        if (!attr) {
            /* Not a trackpad (e.g. a keyboard half): free the slot. */
            LOG_DBG("pad %d: no touchpad service on this peripheral", pad_index(pad));
            release(pad);
            return BT_GATT_ITER_STOP;
        }
        const struct bt_gatt_service_val *svc = attr->user_data;
        pad->svc_end = svc->end_handle;
        pad->disc.uuid = NULL;
        pad->disc.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        pad->disc.start_handle = attr->handle + 1;
        pad->disc.end_handle = pad->svc_end;
        int err = bt_gatt_discover(conn, &pad->disc);
        if (err) {
            LOG_ERR("pad %d: characteristic discovery failed (%d)", pad_index(pad), err);
        }
        return BT_GATT_ITER_STOP;
    }

    if (attr) {
        const struct bt_gatt_chrc *chrc = attr->user_data;
        if (bt_uuid_cmp(chrc->uuid, &frames_uuid.uuid) == 0) {
            pad->frames_handle = chrc->value_handle;
        } else if (bt_uuid_cmp(chrc->uuid, &mode_uuid.uuid) == 0) {
            pad->mode_handle = chrc->value_handle;
        }
        return BT_GATT_ITER_CONTINUE;
    }

    /* Characteristic discovery finished. */
    if (!pad->frames_handle || !pad->mode_handle) {
        LOG_ERR("pad %d: touchpad service incomplete", pad_index(pad));
        release(pad);
        return BT_GATT_ITER_STOP;
    }
    LOG_INF("pad %d: touchpad service found (frames %u, mode %u)", pad_index(pad),
            pad->frames_handle, pad->mode_handle);
    subscribe(pad);
    return BT_GATT_ITER_STOP;
}

static void discover_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pad *pad = CONTAINER_OF(dwork, struct pad, discover_work);
    if (!pad->conn) {
        return;
    }
    pad->disc = (struct bt_gatt_discover_params){
        .uuid = &svc_uuid.uuid,
        .func = discover_cb,
        .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
        .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
        .type = BT_GATT_DISCOVER_PRIMARY,
    };
    int err = bt_gatt_discover(pad->conn, &pad->disc);
    if (err) {
        LOG_WRN("pad %d: discovery failed to start (%d), retrying", pad_index(pad), err);
        k_work_schedule(dwork, K_MSEC(500));
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    struct bt_conn_info info;
    if (err || level < BT_SECURITY_L2 || bt_conn_get_info(conn, &info) ||
        info.role != BT_CONN_ROLE_CENTRAL || pad_for_conn(conn)) {
        return;
    }
    struct pad *pad = pad_for_conn(NULL);
    if (!pad) {
        return; /* all pad slots taken by connected trackpads */
    }
    pad->conn = bt_conn_ref(conn);
    /* Let ZMK's own split discovery go first. */
    k_work_schedule(&pad->discover_work, K_MSEC(300));
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    struct pad *pad = pad_for_conn(conn);
    if (pad) {
        LOG_INF("pad %d: disconnected (reason %d)", pad_index(pad), reason);
        release(pad);
    }
}

BT_CONN_CB_DEFINE(ptp_split_central_conn_cb) = {
    .security_changed = security_changed,
    .disconnected = disconnected,
};

static int ptp_split_central_init(void) {
    for (int i = 0; i < ARRAY_SIZE(pads); i++) {
        k_work_init_delayable(&pads[i].discover_work, discover_work_cb);
    }
    ptp_usb_set_mode_callback(usb_mode_changed);
    return 0;
}

SYS_INIT(ptp_split_central_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
