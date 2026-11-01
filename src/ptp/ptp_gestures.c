/*
 * Helper-pad gestures (dongle side). The left trackpad never moves the cursor; its frames come
 * here and turn into mouse buttons and scrolling for the right pad (docs/trackpads-and-screen.md):
 *
 *   1 finger resting      arms the left button; pressed once the right pad is touched (drag)
 *   2 fingers resting     same with the right button
 *   quick tap             left click, sent TAP_WAIT_MS later once it can't be a double tap
 *   double tap            drag lock: left button latched down until the next tap
 *   circling              vertical scroll (clockwise = down), speed follows the circling
 *
 * Buttons and scroll go out through ZMK's mouse report, not the touchpad: hosts merge buttons
 * from all pointing devices, so this works on Windows, Linux and macOS.
 *
 * Everything runs on the system work queue (left frames are queued from the BT thread, right
 * pad activity comes from the touchpad TX work), so no locking is needed here.
 *
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/endpoints.h>
#include <zmk/hid.h>
#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
#include <zmk/pointing/resolution_multipliers.h>
#endif

#include <ptp.h>
#include "ptp_gestures.h"

LOG_MODULE_DECLARE(ptp_split, CONFIG_PTP_LOG_LEVEL);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define REST_MS 40          /* shorter contacts are ignored (brushes) */
#define TAP_MAX_MS 200      /* down..up for a tap */
#define TAP_WAIT_MS 250     /* wait for a second tap before sending a click */
#define TAP_MAX_MOVE 160    /* pad units (~2.5 mm on a TPS65) */
#define CENTER_X (CONFIG_PTP_LOGICAL_X / 2)
#define CENTER_Y (CONFIG_PTP_LOGICAL_Y / 2)
#define DEAD_RADIUS 400     /* angle is too noisy near the centre */
#define SCROLL_START_RAD (45.0f * (float)M_PI / 180.0f)
#define DETENTS_PER_TURN 12.0f

#define BTN_LEFT 0
#define BTN_RIGHT 1

enum state {
    IDLE,    /* no finger on the left pad */
    PENDING, /* finger down, younger than REST_MS */
    ARMED,   /* resting: armed_button is pressed while the right pad is touched */
    SCROLL,  /* circling */
};

static enum state state = IDLE;
static int64_t down_ms;
static uint16_t start_x, start_y;
static int max_move;
static uint8_t fingers_max;
static int armed_button = -1;
static int pressed_button = -1; /* currently held through ZMK, or -1 */
static bool locked;             /* drag lock: pressed_button stays down after lift */
static bool lock_this_touch;    /* the lock was started by the current touch */
static bool second_touch;       /* this touch started within TAP_WAIT_MS of a tap */
static bool right_touching;
static bool click_pending;      /* a tap's click is waiting in click_work */

static bool have_prev;
static float prev_angle, turned, scroll_rad;

static void send(void) { zmk_endpoint_send_mouse_report(); }

static void press(int button) {
    if (pressed_button == button) {
        return;
    }
    if (pressed_button >= 0) {
        zmk_hid_mouse_button_release(pressed_button);
    }
    zmk_hid_mouse_button_press(button);
    pressed_button = button;
    send();
}

static void release(void) {
    if (pressed_button < 0) {
        return;
    }
    zmk_hid_mouse_button_release(pressed_button);
    pressed_button = -1;
    send();
}

static void click_work_cb(struct k_work *work) {
    click_pending = false;
    if (pressed_button >= 0) {
        return; /* something else is holding a button; don't fight it */
    }
    zmk_hid_mouse_button_press(BTN_LEFT);
    send();
    zmk_hid_mouse_button_release(BTN_LEFT);
    send();
}

static K_WORK_DELAYABLE_DEFINE(click_work, click_work_cb);

static int wheel_multiplier(void) {
#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
    /* Feature value 0..15 means 1..16 steps per detent. */
    return zmk_pointing_resolution_multipliers_get_current_profile().wheel + 1;
#else
    return 1;
#endif
}

/* Send whole scroll steps out of the accumulated angle, keep the remainder. */
static void emit_scroll(void) {
    float per_rad = DETENTS_PER_TURN * wheel_multiplier() / (2.0f * (float)M_PI);
    int steps = (int)(scroll_rad * per_rad);
    if (steps == 0) {
        return;
    }
    scroll_rad -= steps / per_rad;
    /* Pad y grows downwards, so a growing angle is clockwise; clockwise scrolls down, and a
     * positive HID wheel value scrolls up. */
    zmk_hid_mouse_scroll_set(0, CLAMP(-steps, INT16_MIN, INT16_MAX));
    send();
    zmk_hid_mouse_scroll_set(0, 0);
}

static void track_angle(const struct ptp_contact *c) {
    int dx = (int)c->x - CENTER_X, dy = (int)c->y - CENTER_Y;
    if (dx * dx + dy * dy < DEAD_RADIUS * DEAD_RADIUS) {
        have_prev = false;
        return;
    }
    float angle = atan2f((float)dy, (float)dx);
    if (have_prev) {
        float d = angle - prev_angle;
        if (d > (float)M_PI) {
            d -= 2.0f * (float)M_PI;
        } else if (d < -(float)M_PI) {
            d += 2.0f * (float)M_PI;
        }
        turned += d;
        if (state == SCROLL) {
            scroll_rad += d;
            emit_scroll();
        }
    }
    prev_angle = angle;
    have_prev = true;
}

static void touch_down(const struct ptp_contact *c, int64_t now) {
    state = PENDING;
    down_ms = now;
    start_x = c->x;
    start_y = c->y;
    max_move = 0;
    fingers_max = 0;
    have_prev = false;
    turned = 0;
    scroll_rad = 0;
    lock_this_touch = false;
}

static void touch_lift(int64_t now) {
    bool tap = state != SCROLL && now - down_ms < TAP_MAX_MS && max_move < TAP_MAX_MOVE;

    if (lock_this_touch) {
        /* Double tap (or tap then rest): keep the button latched. */
        LOG_INF("gesture: drag lock on");
    } else if (locked) {
        if (tap) {
            locked = false;
            release();
            LOG_INF("gesture: drag lock off");
        }
    } else if (tap && second_touch) {
        /* A double tap too quick to reach REST_MS: latch now. */
        locked = true;
        press(BTN_LEFT);
        LOG_INF("gesture: drag lock on");
    } else {
        /* If the button was already pressed during this touch (right pad touched while
         * resting), lifting is the click; don't send another one. */
        bool was_pressed = pressed_button >= 0;
        release();
        if (tap && !was_pressed) {
            k_work_reschedule(&click_work, K_MSEC(TAP_WAIT_MS));
            click_pending = true;
        }
    }
    state = IDLE;
    armed_button = -1;
}

void ptp_gestures_left_frame(const struct ptp_frame *frame) {
    int64_t now = k_uptime_get();
    const struct ptp_contact *first = NULL;
    uint8_t touching = 0;
    for (int i = 0; i < frame->count; i++) {
        if (frame->contacts[i].tip) {
            if (!first) {
                first = &frame->contacts[i];
            }
            touching++;
        }
    }

    if (!touching) {
        if (state != IDLE) {
            touch_lift(now);
        }
        return;
    }

    if (state == IDLE) {
        /* A click still waiting from a tap just before: this is the second tap of a double
         * tap, so the click is dropped and the button gets latched instead. */
        second_touch = click_pending;
        if (click_pending) {
            k_work_cancel_delayable(&click_work);
            click_pending = false;
        }
        touch_down(first, now);
    }

    fingers_max = MAX(fingers_max, touching);
    max_move = MAX(max_move, abs((int)first->x - start_x) + abs((int)first->y - start_y));
    if (touching == 1) {
        track_angle(first);
    } else {
        have_prev = false; /* circling is one finger */
    }

    if (state != SCROLL && fabsf(turned) > SCROLL_START_RAD) {
        state = SCROLL;
        scroll_rad = 0;
        if (lock_this_touch) {
            locked = false;
            lock_this_touch = false;
        }
        if (!locked) {
            release();
        }
        armed_button = -1;
        LOG_DBG("gesture: scrolling");
        return;
    }

    if (state == PENDING && now - down_ms >= REST_MS) {
        state = ARMED;
        if (second_touch && !locked) {
            /* Second tap of a double tap: latch the left button now. */
            locked = true;
            lock_this_touch = true;
            press(BTN_LEFT);
        } else if (!locked) {
            armed_button = fingers_max >= 2 ? BTN_RIGHT : BTN_LEFT;
            if (right_touching) {
                press(armed_button);
            }
        }
    } else if (state == ARMED && !locked && fingers_max >= 2 && armed_button == BTN_LEFT) {
        /* Second finger landed a little after the first. */
        armed_button = BTN_RIGHT;
        if (pressed_button >= 0) {
            press(BTN_RIGHT);
        }
    }
}

void ptp_gestures_right_frame(const struct ptp_frame *frame) {
    bool touching = false;
    for (int i = 0; i < frame->count; i++) {
        touching |= frame->contacts[i].tip;
    }
    right_touching = touching;
    if (touching && state == ARMED && armed_button >= 0 && pressed_button < 0) {
        press(armed_button);
    }
}

void ptp_gestures_left_gone(void) {
    /* Left pad disconnected: let go of everything it was holding. */
    k_work_cancel_delayable(&click_work);
    click_pending = false;
    locked = false;
    lock_this_touch = false;
    release();
    state = IDLE;
    armed_button = -1;
}
