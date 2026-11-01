/*
 * Central (dongle) side of the touchpad frame service: finds the service on each split
 * peripheral that has one (the trackpads; the keyboard halves don't), subscribes to its
 * frames and forwards them to that pad's USB touchpad interface. Each pad also learns whether
 * the host has its interface in touchpad mode, so it can fall back to mouse gestures.
 *
 * Each connection takes a slot in pads[]; its frames go to the USB touchpad for its side
 * (left -> HID_1, right -> HID_2), read from the pad's side characteristic. Pads without one
 * (older firmware) take the first free touchpad, i.e. connection order.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <ptp.h>
#include "ptp_split.h"

LOG_MODULE_REGISTER(ptp_split, CONFIG_PTP_LOG_LEVEL);

static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(PTP_SPLIT_SERVICE_UUID);
static struct bt_uuid_128 frames_uuid = BT_UUID_INIT_128(PTP_SPLIT_FRAMES_UUID);
static struct bt_uuid_128 mode_uuid = BT_UUID_INIT_128(PTP_SPLIT_MODE_UUID);
static struct bt_uuid_128 side_uuid = BT_UUID_INIT_128(PTP_SPLIT_SIDE_UUID);

struct pad {
    struct bt_conn *conn;
    struct k_work_delayable discover_work;
    struct k_work_delayable watchdog_work;
    const char *step;
    struct bt_gatt_read_params read;
    struct bt_gatt_discover_params disc;
    struct bt_gatt_subscribe_params sub;
    struct k_work fast_work;
    int64_t fast_checked;
    uint16_t last_scan; /* time-byte state, matches the peripheral's (ptp_frame_unpack) */
    /* Jitter buffer timeline: pad time (us, extended) and its offset onto our clock. */
    uint16_t prev_scan;
    int64_t pad_us, offset_us, last_rx_us;
    uint16_t svc_end, frames_handle, mode_handle, side_handle;
    uint8_t side; /* PTP_SPLIT_SIDE_* */
    uint8_t read_tries;
    uint8_t usb;  /* USB touchpad interface (and frame queue) this pad's frames go to */
    bool usb_assigned;
    bool subscribed;
    /* For the display (ptp_split_pad_status). */
    int64_t last_touch_ms;
    uint32_t interval_avg_us; /* exponential average, 1/8 weight per frame */
};

static struct pad pads[CONFIG_PTP_USB_PADS];

BUILD_ASSERT(PTP_SIDE_LEFT == PTP_SPLIT_SIDE_LEFT && PTP_SIDE_RIGHT == PTP_SPLIT_SIDE_RIGHT,
             "ptp.h and ptp_split.h side numbers must match");

/*
 * Jitter buffer. The pad produces a frame every ~11-12 ms but BLE delivers only at 7.5 ms
 * connection events, so arrivals alternate 7.5 / 15 ms apart and the cursor visibly lurches,
 * even with correct Scan Time. Each frame is released to USB at its pad read time (scan_time,
 * mapped onto our clock) plus a fixed delay, so the host sees the pad's own even cadence.
 */
struct frame_item {
    int64_t due_us;
    struct ptp_frame frame;
};

K_MSGQ_DEFINE(frame_q0, sizeof(struct frame_item), 16, 8);
#if CONFIG_PTP_USB_PADS > 1
K_MSGQ_DEFINE(frame_q1, sizeof(struct frame_item), 16, 8);
#endif
static struct k_msgq *const frame_qs[] = {
    &frame_q0,
#if CONFIG_PTP_USB_PADS > 1
    &frame_q1,
#endif
};

static struct k_spinlock due_lock;
static int64_t scheduled_due_us = INT64_MAX;

static int64_t now_us(void) { return k_ticks_to_us_floor64(k_uptime_ticks()); }

static void tx_work_cb(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(tx_work, tx_work_cb);

static void schedule_tx(int64_t due_us) {
    k_spinlock_key_t key = k_spin_lock(&due_lock);
    if (due_us < scheduled_due_us) {
        scheduled_due_us = due_us;
        int64_t wait = MAX(due_us - now_us(), 0);
        k_work_reschedule(&tx_work, K_USEC(wait));
    }
    k_spin_unlock(&due_lock, key);
}

static void tx_work_cb(struct k_work *work) {
    k_spinlock_key_t key = k_spin_lock(&due_lock);
    scheduled_due_us = INT64_MAX;
    k_spin_unlock(&due_lock, key);

    int64_t next = INT64_MAX;
    for (int i = 0; i < ARRAY_SIZE(frame_qs); i++) {
        struct frame_item item;
        while (k_msgq_peek(frame_qs[i], &item) == 0) {
            if (item.due_us > now_us() + 300) {
                next = MIN(next, item.due_us);
                break;
            }
            k_msgq_get(frame_qs[i], &item, K_NO_WAIT);
            ptp_usb_pad_send(i, &item.frame);
        }
    }
    if (next != INT64_MAX) {
        schedule_tx(next);
    }
}

static int pad_index(struct pad *pad) { return pad - pads; }

/* Pick the USB touchpad for a pad whose side is now known: its side's interface unless another
 * connected pad already has it, otherwise the first free one. */
static void assign_usb(struct pad *pad) {
    bool taken[ARRAY_SIZE(pads)] = {0};
    for (int i = 0; i < ARRAY_SIZE(pads); i++) {
        if (&pads[i] != pad && pads[i].conn && pads[i].usb_assigned) {
            taken[pads[i].usb] = true;
        }
    }
    int want = pad->side == PTP_SPLIT_SIDE_RIGHT ? 1 : 0;
    if (pad->side == PTP_SPLIT_SIDE_UNKNOWN || want >= ARRAY_SIZE(pads) || taken[want]) {
        for (want = 0; want < ARRAY_SIZE(pads) && taken[want]; want++) {
        }
    }
    pad->usb = want < ARRAY_SIZE(pads) ? want : 0;
    pad->usb_assigned = true;
    static const char *const names[] = {"unknown", "left", "right"};
    LOG_INF("pad %d: %s side -> USB touchpad %d", pad_index(pad),
            names[MIN(pad->side, ARRAY_SIZE(names) - 1)], pad->usb);
}

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
    k_work_cancel_delayable(&pad->watchdog_work);
    if (pad->conn) {
        bt_conn_unref(pad->conn);
    }
    pad->conn = NULL;
    pad->subscribed = false;
    pad->frames_handle = pad->mode_handle = pad->side_handle = 0;
    pad->side = PTP_SPLIT_SIDE_UNKNOWN;
    pad->usb_assigned = false;
    pad->read_tries = 0;
}

static void send_mode(struct pad *pad) {
    if (!pad->conn || !pad->subscribed || !pad->mode_handle) {
        return;
    }
    uint8_t mode = ptp_usb_pad_active(pad->usb) ? 1 : 0;
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

/* ZMK slows every split link to the idle parameters when the keyboard sees no key activity;
 * touchpad frames don't count as activity. While a pad is sending, put its link (only) back to
 * the active parameters. */
static void fast_work_cb(struct k_work *work) {
    struct pad *pad = CONTAINER_OF(work, struct pad, fast_work);
    struct bt_conn_info info;
    if (!pad->conn || bt_conn_get_info(pad->conn, &info)) {
        return;
    }
    if (info.le.interval <= CONFIG_ZMK_SPLIT_BLE_PREF_INT) {
        return;
    }
    int err = bt_conn_le_param_update(
        pad->conn, BT_LE_CONN_PARAM(CONFIG_ZMK_SPLIT_BLE_PREF_INT, CONFIG_ZMK_SPLIT_BLE_PREF_INT,
                                    CONFIG_ZMK_SPLIT_BLE_PREF_LATENCY,
                                    CONFIG_ZMK_SPLIT_BLE_PREF_TIMEOUT));
    LOG_INF("pad %d: link interval %u while in use, requesting %u (%d)", pad_index(pad),
            info.le.interval, CONFIG_ZMK_SPLIT_BLE_PREF_INT, err);
}

static void usb_mode_changed(uint8_t pad, bool active) { k_work_submit(&mode_work); }

static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length) {
    struct pad *pad = CONTAINER_OF(params, struct pad, sub);
    if (!data) {
        LOG_INF("pad %d: unsubscribed", pad_index(pad));
        pad->subscribed = false;
        return BT_GATT_ITER_STOP;
    }
    int64_t now = k_uptime_get();
    if (now - pad->fast_checked > 500) {
        pad->fast_checked = now;
        k_work_submit(&pad->fast_work);
    }
    struct frame_item item;
    if (ptp_frame_unpack(data, length, &pad->last_scan, &item.frame) != 0) {
        return BT_GATT_ITER_CONTINUE;
    }

    int64_t rx = now_us();
    uint32_t scan_delta_us = (uint16_t)(item.frame.scan_time - pad->prev_scan) * 100;
    if (item.frame.count > 0) {
        if (scan_delta_us > 0 && scan_delta_us < 50000 && rx - pad->last_rx_us < 50000) {
            pad->interval_avg_us = pad->interval_avg_us
                                       ? (pad->interval_avg_us * 7 + scan_delta_us) / 8
                                       : scan_delta_us;
        }
        pad->last_touch_ms = now;
    }
    pad->pad_us += scan_delta_us;
    pad->prev_scan = item.frame.scan_time;
    const int64_t delay = CONFIG_PTP_SPLIT_JITTER_US;
    int64_t due = pad->pad_us + pad->offset_us + delay;
    if (rx - pad->last_rx_us > 50000 || due > rx + 3 * delay) {
        /* New touch (or lost track): map pad time onto ours from this frame. */
        pad->offset_us = rx - pad->pad_us;
        due = rx + delay;
    } else if (due < rx) {
        /* Later than the buffer allows: shift the timeline (adds latency). */
        pad->offset_us += rx - due;
        due = rx;
    } else if (due > rx + delay) {
        /* More buffered than needed (an earlier late frame, clock drift): ease back. */
        pad->offset_us -= 100;
        due -= 100;
    }
    pad->last_rx_us = rx;
    item.due_us = due;

    if (k_msgq_put(frame_qs[pad->usb], &item, K_NO_WAIT) != 0) {
        LOG_WRN("pad %d: frame queue full, dropping", pad_index(pad));
    }
    schedule_tx(due);
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
    pad->last_scan = 0;
    pad->step = "done";
    LOG_INF("pad %d: subscribed to frames", pad_index(pad));
    k_work_submit(&mode_work);
}

static void subscribe(struct pad *pad) {
    assign_usb(pad); /* before frames can arrive */
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

static uint8_t side_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params,
                       const void *data, uint16_t length) {
    struct pad *pad = CONTAINER_OF(params, struct pad, read);
    if (!err && data && length >= 1 && ((const uint8_t *)data)[0] <= PTP_SPLIT_SIDE_RIGHT) {
        pad->side = ((const uint8_t *)data)[0];
    } else {
        LOG_WRN("pad %d: side read failed (err %u, len %u)", pad_index(pad), err, length);
    }
    pad->step = "subscribe";
    subscribe(pad);
    return BT_GATT_ITER_STOP;
}

static void read_side(struct pad *pad) {
    pad->read = (struct bt_gatt_read_params){
        .func = side_cb,
        .handle_count = 1,
        .single = {.handle = pad->side_handle, .offset = 0},
    };
    pad->step = "side read";
    int err = bt_gatt_read(pad->conn, &pad->read);
    if (err) {
        LOG_ERR("pad %d: side read failed to start (%d)", pad_index(pad), err);
        pad->step = "subscribe";
        subscribe(pad);
    }
}

static uint8_t discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params) {
    struct pad *pad = CONTAINER_OF(params, struct pad, disc);

    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        if (!attr) {
            /* Not a trackpad (e.g. a keyboard half): free the slot. */
            LOG_INF("pad %d: no touchpad service on this peripheral, freeing slot",
                    pad_index(pad));
            release(pad);
            return BT_GATT_ITER_STOP;
        }
        const struct bt_gatt_service_val *svc = attr->user_data;
        pad->svc_end = svc->end_handle;
        LOG_INF("pad %d: service at %u..%u, discovering characteristics", pad_index(pad),
                attr->handle, pad->svc_end);
        pad->step = "characteristic discovery";
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
        LOG_INF("pad %d: characteristic at %u (value %u)", pad_index(pad), attr->handle,
                chrc->value_handle);
        if (bt_uuid_cmp(chrc->uuid, &frames_uuid.uuid) == 0) {
            pad->frames_handle = chrc->value_handle;
        } else if (bt_uuid_cmp(chrc->uuid, &mode_uuid.uuid) == 0) {
            pad->mode_handle = chrc->value_handle;
        } else if (bt_uuid_cmp(chrc->uuid, &side_uuid.uuid) == 0) {
            pad->side_handle = chrc->value_handle;
        }
        if (!pad->frames_handle || !pad->mode_handle || !pad->side_handle) {
            return BT_GATT_ITER_CONTINUE;
        }
        /* Stop once everything is found: on Zephyr 3.5 (zmk-fixes) walking on to the end of
         * the service ends in an error response that the ATT client never completes. */
        LOG_INF("pad %d: touchpad service found (frames %u, mode %u, side %u)", pad_index(pad),
                pad->frames_handle, pad->mode_handle, pad->side_handle);
        read_side(pad);
        return BT_GATT_ITER_STOP;
    }

    if (pad->frames_handle && pad->mode_handle) {
        /* Older pad firmware without the side characteristic. */
        LOG_INF("pad %d: touchpad service found (frames %u, mode %u), no side", pad_index(pad),
                pad->frames_handle, pad->mode_handle);
        pad->step = "subscribe";
        subscribe(pad);
        return BT_GATT_ITER_STOP;
    }
    LOG_ERR("pad %d: touchpad service incomplete", pad_index(pad));
    release(pad);
    return BT_GATT_ITER_STOP;
}

static void discover_service(struct pad *pad) {
    pad->disc = (struct bt_gatt_discover_params){
        .uuid = &svc_uuid.uuid,
        .func = discover_cb,
        .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
        .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
        .type = BT_GATT_DISCOVER_PRIMARY,
    };
    pad->step = "service discovery";
    int err = bt_gatt_discover(pad->conn, &pad->disc);
    if (err) {
        LOG_ERR("pad %d: service discovery failed to start (%d)", pad_index(pad), err);
    }
}

/* Every peripheral has a GAP appearance, so this read always succeeds; only trackpads go on to
 * service discovery (which would hang on a keyboard half: "not found" is an error response). */
static uint8_t appearance_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params,
                             const void *data, uint16_t length) {
    struct pad *pad = CONTAINER_OF(params, struct pad, read);
    if (err || !data || length < 2) {
        /* Seen once right after a reconnect (err 14, "unlikely error"). Giving up here would
         * leave a connected pad unused until it reconnects, so retry a few times first. */
        if (++pad->read_tries < 4) {
            LOG_WRN("pad %d: appearance read failed (err %u, len %u), retrying", pad_index(pad),
                    err, length);
            k_work_schedule(&pad->discover_work, K_MSEC(1000));
        } else {
            LOG_WRN("pad %d: appearance read failed (err %u, len %u), freeing slot",
                    pad_index(pad), err, length);
            release(pad);
        }
        return BT_GATT_ITER_STOP;
    }
    uint16_t appearance = sys_get_le16(data);
    if (appearance != CONFIG_PTP_SPLIT_PAD_APPEARANCE) {
        LOG_INF("pad %d: appearance %u, not a trackpad, freeing slot", pad_index(pad), appearance);
        release(pad);
        return BT_GATT_ITER_STOP;
    }
    LOG_INF("pad %d: trackpad (appearance %u)", pad_index(pad), appearance);
    discover_service(pad);
    /* Stop: a by-UUID read would otherwise continue to the end of the table (error response). */
    return BT_GATT_ITER_STOP;
}

static void discover_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pad *pad = CONTAINER_OF(dwork, struct pad, discover_work);
    if (!pad->conn) {
        return;
    }
    pad->read = (struct bt_gatt_read_params){
        .func = appearance_cb,
        .handle_count = 0,
        .by_uuid =
            {
                .uuid = BT_UUID_GAP_APPEARANCE,
                .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
                .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
            },
    };
    pad->step = "appearance read";
    int err = bt_gatt_read(pad->conn, &pad->read);
    LOG_INF("pad %d: reading appearance (%d)", pad_index(pad), err);
    if (err) {
        k_work_schedule(dwork, K_MSEC(500));
        return;
    }
    k_work_schedule(&pad->watchdog_work, K_SECONDS(5));
}

static void watchdog_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pad *pad = CONTAINER_OF(dwork, struct pad, watchdog_work);
    if (pad->conn && !pad->subscribed) {
        LOG_WRN("pad %d: still waiting on %s after 5 s", pad_index(pad), pad->step);
    }
}

/* Claim a pad slot for a new split peripheral connection and look for the touchpad service.
 * Discovery doesn't need encryption; subscribing to the (encrypted) frames CCC makes the stack
 * raise security itself. Keyboard halves have no touchpad service and give their slot back. */
static void claim(struct bt_conn *conn, const char *why) {
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) || info.role != BT_CONN_ROLE_CENTRAL ||
        pad_for_conn(conn)) {
        return;
    }
    struct pad *pad = pad_for_conn(NULL);
    if (!pad) {
        LOG_WRN("no free pad slot (%s)", why);
        return;
    }
    pad->conn = bt_conn_ref(conn);
    LOG_INF("pad %d: probing peripheral (%s)", pad_index(pad), why);
    /* Let ZMK's own split discovery go first. */
    k_work_schedule(&pad->discover_work, K_MSEC(1000));
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (!err) {
        claim(conn, "connected");
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    LOG_INF("security changed: level %d err %d", level, err);
    if (!err && level >= BT_SECURITY_L2) {
        claim(conn, "encrypted");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    struct pad *pad = pad_for_conn(conn);
    if (pad) {
        LOG_INF("pad %d: disconnected (reason %d)", pad_index(pad), reason);
        release(pad);
    }
}

BT_CONN_CB_DEFINE(ptp_split_central_conn_cb) = {
    .connected = connected,
    .security_changed = security_changed,
    .disconnected = disconnected,
};

void ptp_split_pad_status(uint8_t side, struct ptp_split_pad_status *out) {
    *out = (struct ptp_split_pad_status){0};
    for (int i = 0; i < ARRAY_SIZE(pads); i++) {
        struct pad *pad = &pads[i];
        if (!pad->conn || !pad->subscribed) {
            continue;
        }
        uint8_t pad_side = pad->side;
        if (pad_side == PTP_SPLIT_SIDE_UNKNOWN) {
            pad_side = pad->usb == 1 ? PTP_SPLIT_SIDE_RIGHT : PTP_SPLIT_SIDE_LEFT;
        }
        if (pad_side != side) {
            continue;
        }
        out->connected = true;
        out->touching = pad->last_touch_ms && k_uptime_get() - pad->last_touch_ms < 300;
        out->interval_us = MIN(pad->interval_avg_us, UINT16_MAX);
        return;
    }
}

static int ptp_split_central_init(void) {
    for (int i = 0; i < ARRAY_SIZE(pads); i++) {
        k_work_init_delayable(&pads[i].discover_work, discover_work_cb);
        k_work_init_delayable(&pads[i].watchdog_work, watchdog_work_cb);
        k_work_init(&pads[i].fast_work, fast_work_cb);
    }
    ptp_usb_set_mode_callback(usb_mode_changed);
    return 0;
}

SYS_INIT(ptp_split_central_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
