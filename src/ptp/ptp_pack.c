/*
 * Touchpad frame encoding for the split link (see ptp.h).
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <string.h>
#include <zephyr/sys/util.h>

#include <ptp.h>

struct bits {
    uint8_t *buf;
    size_t pos; /* in bits */
};

static void put(struct bits *b, uint32_t v, int n) {
    for (int i = 0; i < n; i++, b->pos++) {
        if (v & BIT(i)) {
            b->buf[b->pos / 8] |= BIT(b->pos % 8);
        }
    }
}

static uint32_t get(struct bits *b, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; i++, b->pos++) {
        if (b->buf[b->pos / 8] & BIT(b->pos % 8)) {
            v |= BIT(i);
        }
    }
    return v;
}

size_t ptp_frame_pack(const struct ptp_frame *frame, uint8_t out[PTP_PACKED_MAX]) {
    uint8_t count = MIN(frame->count, PTP_MAX_CONTACTS);
    memset(out, 0, PTP_PACKED_MAX);
    out[0] = count | ((frame->buttons & 0x07) << 3);

    struct bits b = {.buf = &out[1]};
    for (int i = 0; i < count; i++) {
        const struct ptp_contact *c = &frame->contacts[i];
        put(&b, c->id & 0x07, 3);
        put(&b, c->tip ? 1 : 0, 1);
        put(&b, MIN(c->x, PTP_COORD_MAX), 12);
        put(&b, MIN(c->y, PTP_COORD_MAX), 12);
    }
    return 1 + DIV_ROUND_UP(b.pos, 8);
}

int ptp_frame_unpack(const uint8_t *buf, size_t len, struct ptp_frame *frame) {
    if (len < 1) {
        return -EINVAL;
    }
    uint8_t count = buf[0] & 0x07;
    if (count > PTP_MAX_CONTACTS || len < 1 + DIV_ROUND_UP(count * 28, 8)) {
        return -EINVAL;
    }
    memset(frame, 0, sizeof(*frame));
    frame->count = count;
    frame->buttons = (buf[0] >> 3) & 0x07;

    struct bits b = {.buf = (uint8_t *)&buf[1]};
    for (int i = 0; i < count; i++) {
        struct ptp_contact *c = &frame->contacts[i];
        c->id = get(&b, 3);
        c->tip = get(&b, 1);
        c->x = get(&b, 12);
        c->y = get(&b, 12);
    }
    return 0;
}
