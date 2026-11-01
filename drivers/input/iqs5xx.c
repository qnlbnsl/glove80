/*
 * Azoteq IQS5xx-B000 trackpad controller (IQS550/IQS572/IQS525; TPS43/TPS65 modules).
 *
 * Written from the IQS5xx-B000 register map. The controller runs its own scan cycle and
 * raises RDY when a report is ready; we read one block (gestures, finger count, relative
 * movement and up to 5 absolute finger positions), close the comms window by writing to
 * 0xEEEE, and turn the block into input events:
 *
 *   1 finger            cursor (INPUT_REL_X/Y)
 *   tap / 2-finger tap  left / right click (INPUT_BTN_0 / INPUT_BTN_1)  [controller gestures]
 *   2-finger drag       scroll (INPUT_REL_WHEEL / INPUT_REL_HWHEEL)     [controller gesture]
 *   pinch               INPUT_KEY_KPPLUS / INPUT_KEY_KPMINUS steps      [controller gesture]
 *   3-finger tap        middle click (INPUT_BTN_2)                      [ours]
 *   3-finger swipe      INPUT_KEY_LEFT/RIGHT/UP/DOWN, once per touch    [ours]
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT azoteq_iqs5xx

#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(iqs5xx, CONFIG_INPUT_LOG_LEVEL);

/* Registers (16-bit addresses, big-endian data). */
#define REG_PRODUCT_NUMBER 0x0000
#define REG_DATA_START 0x000C /* previous cycle time, then the report block */
#define REG_SYSTEM_CONTROL_0 0x0431
#define REG_REPORT_RATE_ACTIVE 0x057A
#define REG_REPORT_RATE_IDLE_TOUCH 0x057C
#define REG_SYSTEM_CONFIG_0 0x058E
#define REG_SYSTEM_CONFIG_1 0x058F
#define REG_XY_CONFIG_0 0x0669
#define REG_SINGLE_FINGER_GESTURES 0x06B7
#define REG_END_COMMS 0xEEEE

#define PRODUCT_IQS550 40
#define PRODUCT_IQS525 52
#define PRODUCT_IQS572 58

#define MAX_FINGERS 5
#define FINGER_BLOCK 7 /* abs X (2), abs Y (2), strength (2), area (1) */
#define DATA_LEN (10 + MAX_FINGERS * FINGER_BLOCK)

/* Report block offsets (relative to REG_DATA_START). */
#define OFF_GESTURES_0 1
#define OFF_GESTURES_1 2
#define OFF_SYSTEM_INFO_0 3
#define OFF_NUM_FINGERS 5
#define OFF_REL_X 6
#define OFF_REL_Y 8
#define OFF_FINGERS 10

#define GE0_SINGLE_TAP BIT(0)
#define GE0_PRESS_AND_HOLD BIT(1)
#define GE1_TWO_FINGER_TAP BIT(0)
#define GE1_SCROLL BIT(1)
#define GE1_ZOOM BIT(2)
#define SI0_SHOW_RESET BIT(7)

#define SC0_ACK_RESET BIT(7)
#define CFG0_REATI BIT(2)
#define CFG1_EVENT_MODE BIT(0)
#define CFG1_GESTURE_EVENT BIT(1)
#define CFG1_TP_EVENT BIT(2)
#define XY_FLIP_X BIT(0)
#define XY_FLIP_Y BIT(1)
#define XY_SWAP BIT(2)
#define XY_PALM_REJECT BIT(3)

#define TAP_MAX_MOVE 80 /* controller units a 3-finger tap may wander */

struct iqs5xx_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec rdy;
    struct gpio_dt_spec reset;
    uint16_t report_rate_ms;
    bool event_mode, flip_x, flip_y, swap_xy, palm_reject, hold_to_drag, natural_scroll;
    uint16_t scroll_divisor, zoom_divisor, swipe_distance, three_finger_tap_ms;
};

struct iqs5xx_data {
    const struct device *dev;
    struct gpio_callback rdy_cb;
    struct k_work read_work;
    struct k_work_delayable poll_work; /* used when RDY is not wired */
    struct k_work_delayable init_work;
    bool ready;

    /* Gesture state for the current touch (first finger down until all lifted). */
    bool touching;
    uint8_t max_fingers;
    bool dragging;
    bool three_active, three_swiped;
    int32_t three_x0, three_y0, three_move;
    int64_t three_t0;
    int64_t touch_t0;
    bool right_pending;
    int32_t scroll_acc_x, scroll_acc_y, zoom_acc;
};

static int reg_read(const struct device *dev, uint16_t reg, uint8_t *buf, size_t len) {
    const struct iqs5xx_config *cfg = dev->config;
    uint8_t addr[2];
    sys_put_be16(reg, addr);
    return i2c_write_read_dt(&cfg->i2c, addr, sizeof(addr), buf, len);
}

static int reg_write(const struct device *dev, uint16_t reg, const uint8_t *val, size_t len) {
    const struct iqs5xx_config *cfg = dev->config;
    uint8_t buf[2 + 4];
    if (len > 4) {
        return -EINVAL;
    }
    sys_put_be16(reg, buf);
    memcpy(&buf[2], val, len);
    return i2c_write_dt(&cfg->i2c, buf, 2 + len);
}

static int reg_write_u8(const struct device *dev, uint16_t reg, uint8_t val) {
    return reg_write(dev, reg, &val, 1);
}

/*
 * Without RDY we cannot see the comms window: the controller NACKs its address outside it.
 * Retry for up to ~2 report cycles.
 */
static int retry(const struct device *dev, int (*op)(const struct device *, uint16_t, uint8_t *,
                                                    size_t),
                 uint16_t reg, uint8_t *buf, size_t len) {
    const struct iqs5xx_config *cfg = dev->config;
    int tries = cfg->rdy.port ? 1 : 2 * cfg->report_rate_ms + 20;
    int err;
    for (int i = 0; i < tries; i++) {
        err = op(dev, reg, buf, len);
        if (err != -EIO) {
            return err;
        }
        k_msleep(1);
    }
    return err;
}

static int reg_write_nc(const struct device *dev, uint16_t reg, uint8_t *val, size_t len) {
    return reg_write(dev, reg, val, len);
}

#define RD(reg, buf, len) retry(dev, reg_read, reg, buf, len)
#define WR(reg, buf, len) retry(dev, reg_write_nc, reg, buf, len)

static int wr_u8(const struct device *dev, uint16_t reg, uint8_t v) {
    return WR(reg, &v, 1);
}

static int wr_be16(const struct device *dev, uint16_t reg, uint16_t v) {
    uint8_t b[2];
    sys_put_be16(v, b);
    return WR(reg, b, 2);
}

static int end_comms(const struct device *dev) { return reg_write_u8(dev, REG_END_COMMS, 0x01); }

static void click(const struct device *dev, uint16_t code) {
    input_report_key(dev, code, 1, true, K_FOREVER);
    input_report_key(dev, code, 0, true, K_FOREVER);
}

/* Average position of the fingers present in this report. */
static bool centroid(const uint8_t *buf, int32_t *x, int32_t *y) {
    int32_t sx = 0, sy = 0, n = 0;
    for (int i = 0; i < MAX_FINGERS; i++) {
        const uint8_t *f = &buf[OFF_FINGERS + i * FINGER_BLOCK];
        if (sys_get_be16(&f[4]) == 0) { /* touch strength 0 = slot empty */
            continue;
        }
        sx += sys_get_be16(&f[0]);
        sy += sys_get_be16(&f[2]);
        n++;
    }
    if (n == 0) {
        return false;
    }
    *x = sx / n;
    *y = sy / n;
    return true;
}

static void process(const struct device *dev, const uint8_t *buf) {
    const struct iqs5xx_config *cfg = dev->config;
    struct iqs5xx_data *data = dev->data;

    uint8_t ge0 = buf[OFF_GESTURES_0];
    uint8_t ge1 = buf[OFF_GESTURES_1];
    uint8_t fingers = MIN(buf[OFF_NUM_FINGERS], MAX_FINGERS);
    int16_t rel_x = (int16_t)sys_get_be16(&buf[OFF_REL_X]);
    int16_t rel_y = (int16_t)sys_get_be16(&buf[OFF_REL_Y]);
    int64_t now = k_uptime_get();

    if (fingers > 0 && !data->touching) {
        data->touching = true;
        data->touch_t0 = now;
        data->right_pending = false;
        data->max_fingers = 0;
        data->three_active = false;
        data->three_swiped = false;
        data->scroll_acc_x = data->scroll_acc_y = data->zoom_acc = 0;
    }
    data->max_fingers = MAX(data->max_fingers, fingers);

    /* 3+ fingers: our own swipe / tap detection on the finger centroid. */
    if (fingers >= 3) {
        int32_t x, y;
        if (centroid(buf, &x, &y)) {
            if (!data->three_active) {
                data->three_active = true;
                data->three_x0 = x;
                data->three_y0 = y;
                data->three_move = 0;
                data->three_t0 = now;
            } else if (!data->three_swiped) {
                int32_t dx = x - data->three_x0, dy = y - data->three_y0;
                data->three_move = MAX(data->three_move, MAX(abs(dx), abs(dy)));
                uint16_t code = 0;
                if (abs(dx) >= cfg->swipe_distance && abs(dx) > 2 * abs(dy)) {
                    code = dx > 0 ? INPUT_KEY_RIGHT : INPUT_KEY_LEFT;
                } else if (abs(dy) >= cfg->swipe_distance && abs(dy) > 2 * abs(dx)) {
                    code = dy > 0 ? INPUT_KEY_DOWN : INPUT_KEY_UP;
                }
                if (code) {
                    LOG_DBG("3-finger swipe %d", code);
                    click(dev, code);
                    data->three_swiped = true;
                }
            }
        }
    }

    /* Controller gestures only count for 1- and 2-finger touches. */
    if (data->max_fingers <= 2) {
        if (ge0 & GE0_SINGLE_TAP) {
            click(dev, INPUT_BTN_0);
        } else if ((ge0 & GE0_PRESS_AND_HOLD) && cfg->hold_to_drag && !data->dragging) {
            data->dragging = true;
            input_report_key(dev, INPUT_BTN_0, 1, true, K_FOREVER);
        } else if (ge1 & GE1_TWO_FINGER_TAP) {
            /* Decide at lift-off: a 3-finger tap whose third finger landed late must not
             * become a right click. */
            if (data->touching) {
                data->right_pending = true;
            } else if (data->max_fingers <= 2) {
                click(dev, INPUT_BTN_1);
            }
        } else if (ge1 & GE1_SCROLL) {
            LOG_DBG("scroll %d/%d", rel_x, rel_y);
            int32_t sign = cfg->natural_scroll ? -1 : 1;
            data->scroll_acc_x += rel_x;
            data->scroll_acc_y += rel_y;
            int32_t h = data->scroll_acc_x / cfg->scroll_divisor;
            int32_t v = data->scroll_acc_y / cfg->scroll_divisor;
            data->scroll_acc_x -= h * cfg->scroll_divisor;
            data->scroll_acc_y -= v * cfg->scroll_divisor;
            if (h) {
                input_report_rel(dev, INPUT_REL_HWHEEL, sign * h, !v, K_FOREVER);
            }
            if (v) {
                input_report_rel(dev, INPUT_REL_WHEEL, -sign * v, true, K_FOREVER);
            }
        } else if (ge1 & GE1_ZOOM) {
            LOG_DBG("zoom %d", rel_x);
            data->zoom_acc += rel_x;
            while (data->zoom_acc >= cfg->zoom_divisor) {
                click(dev, INPUT_KEY_KPPLUS);
                data->zoom_acc -= cfg->zoom_divisor;
            }
            while (data->zoom_acc <= -cfg->zoom_divisor) {
                click(dev, INPUT_KEY_KPMINUS);
                data->zoom_acc += cfg->zoom_divisor;
            }
        } else if (fingers == 1 && data->max_fingers == 1 && (rel_x || rel_y)) {
            input_report_rel(dev, INPUT_REL_X, rel_x, false, K_FOREVER);
            input_report_rel(dev, INPUT_REL_Y, rel_y, true, K_FOREVER);
        }
    }

    if (fingers == 0 && data->touching) {
        /* 3-finger tap: measured over the whole touch (fingers rarely land or lift together). */
        if (data->max_fingers >= 3 && !data->three_swiped &&
            now - data->touch_t0 <= cfg->three_finger_tap_ms && data->three_move < TAP_MAX_MOVE) {
            LOG_DBG("3-finger tap");
            click(dev, INPUT_BTN_2);
        } else if (data->right_pending && data->max_fingers <= 2) {
            click(dev, INPUT_BTN_1);
        }
        data->right_pending = false;
        if (data->dragging) {
            input_report_key(dev, INPUT_BTN_0, 0, true, K_FOREVER);
            data->dragging = false;
        }
        data->touching = false;
    }
}

static int read_report(const struct device *dev) {
    struct iqs5xx_data *data = dev->data;
    uint8_t buf[DATA_LEN];

    int err = reg_read(dev, REG_DATA_START, buf, sizeof(buf));
    if (err) {
        return err; /* outside the comms window (NACK) */
    }
    end_comms(dev);
    if (buf[OFF_SYSTEM_INFO_0] & SI0_SHOW_RESET) {
        LOG_WRN("controller reset, reconfiguring");
        data->ready = false;
        k_work_schedule(&data->init_work, K_MSEC(10));
        return 0;
    }
    process(dev, buf);
    return 0;
}

static void read_work_cb(struct k_work *work) {
    struct iqs5xx_data *data = CONTAINER_OF(work, struct iqs5xx_data, read_work);
    int err = read_report(data->dev);
    if (err) {
        LOG_WRN("report read failed (%d)", err);
    }
}

static void poll_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct iqs5xx_data *data = CONTAINER_OF(dwork, struct iqs5xx_data, poll_work);
    const struct iqs5xx_config *cfg = data->dev->config;

    if (!data->ready) {
        return;
    }
    /* Read as soon as the next window opens: on NACK try again in 1 ms; after a report,
     * the next window is about one report interval away. */
    int err = read_report(data->dev);
    k_work_schedule(dwork, K_MSEC(err ? 1 : MAX(cfg->report_rate_ms - 2, 1)));
}

static void rdy_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins) {
    struct iqs5xx_data *data = CONTAINER_OF(cb, struct iqs5xx_data, rdy_cb);
    if (data->ready) {
        k_work_submit(&data->read_work);
    }
}

/* The controller only talks during its comms window (RDY high); wait for it. */
static void wait_rdy(const struct device *dev, int timeout_ms) {
    const struct iqs5xx_config *cfg = dev->config;
    if (!cfg->rdy.port) {
        return; /* no RDY: the controller clock-stretches until its window opens */
    }
    for (int i = 0; i < timeout_ms && !gpio_pin_get_dt(&cfg->rdy); i++) {
        k_msleep(1);
    }
}

static int configure(const struct device *dev) {
    const struct iqs5xx_config *cfg = dev->config;
    uint8_t val[2];
    int err;

    wait_rdy(dev, 500);

    err = RD(REG_PRODUCT_NUMBER, val, 2);
    if (err) {
        LOG_ERR("no response from controller (%d)", err);
        return err;
    }
    uint16_t product = sys_get_be16(val);
    if (product != PRODUCT_IQS550 && product != PRODUCT_IQS572 && product != PRODUCT_IQS525) {
        LOG_WRN("unexpected product number %u", product);
    }
    LOG_INF("product number %u (%s)", product,
            product == PRODUCT_IQS550   ? "IQS550"
            : product == PRODUCT_IQS572 ? "IQS572"
            : product == PRODUCT_IQS525 ? "IQS525"
                                        : "unknown");

    err = wr_u8(dev, REG_SYSTEM_CONTROL_0, SC0_ACK_RESET);
    err |= wr_be16(dev, REG_REPORT_RATE_ACTIVE, cfg->report_rate_ms);
    err |= wr_be16(dev, REG_REPORT_RATE_IDLE_TOUCH, cfg->report_rate_ms);

    err |= RD(REG_SYSTEM_CONFIG_0, val, 1);
    err |= wr_u8(dev, REG_SYSTEM_CONFIG_0, val[0] | CFG0_REATI);

    bool event_mode = cfg->event_mode && cfg->rdy.port; /* polling needs stream mode */
    uint8_t cfg1 = CFG1_GESTURE_EVENT | CFG1_TP_EVENT | (event_mode ? CFG1_EVENT_MODE : 0);
    err |= wr_u8(dev, REG_SYSTEM_CONFIG_1, cfg1);

    /* Flips/swap toggle the module's factory orientation. */
    err |= RD(REG_XY_CONFIG_0, val, 1);
    uint8_t xy = val[0];
    xy ^= (cfg->flip_x ? XY_FLIP_X : 0) | (cfg->flip_y ? XY_FLIP_Y : 0) |
          (cfg->swap_xy ? XY_SWAP : 0);
    xy = cfg->palm_reject ? (xy | XY_PALM_REJECT) : (xy & ~XY_PALM_REJECT);
    err |= wr_u8(dev, REG_XY_CONFIG_0, xy);

    /* Single-finger: tap (+ press-and-hold for drag). Multi: 2-finger tap, scroll, zoom. */
    val[0] = BIT(0) | (cfg->hold_to_drag ? BIT(1) : 0);
    val[1] = BIT(0) | BIT(1) | BIT(2);
    err |= WR(REG_SINGLE_FINGER_GESTURES, val, 2);

    end_comms(dev);
    if (err) {
        LOG_ERR("configuration failed");
        return -EIO;
    }
    return 0;
}

static void init_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct iqs5xx_data *data = CONTAINER_OF(dwork, struct iqs5xx_data, init_work);
    const struct device *dev = data->dev;

#if IS_ENABLED(CONFIG_HWINFO)
    /* Why did we (re)boot? SOFTWARE = crash/fatal error, PIN = reset button,
     * POR/BROWNOUT = power dropped (e.g. a loose 3V3/GND contact). */
    static bool cause_logged;
    if (!cause_logged) {
        uint32_t cause = 0;
        hwinfo_get_reset_cause(&cause);
        LOG_INF("reset cause 0x%08x%s%s%s%s%s", cause, (cause & RESET_SOFTWARE) ? " SOFTWARE" : "",
                (cause & RESET_PIN) ? " PIN" : "", (cause & RESET_POR) ? " POR" : "",
                (cause & RESET_BROWNOUT) ? " BROWNOUT" : "", (cause & RESET_CPU_LOCKUP) ? " LOCKUP" : "");
        hwinfo_clear_reset_cause();
        cause_logged = true;
    }
#endif
    if (configure(dev) < 0) {
        k_work_schedule(dwork, K_SECONDS(1)); /* retry, e.g. pad not plugged in yet */
        return;
    }
    data->touching = false;
    data->ready = true;
    const struct iqs5xx_config *cfg = dev->config;
    LOG_INF("ready (%s)", cfg->rdy.port ? "RDY interrupt" : "polling");
    if (!cfg->rdy.port) {
        k_work_schedule(&data->poll_work, K_NO_WAIT);
    }
}

static int iqs5xx_init(const struct device *dev) {
    const struct iqs5xx_config *cfg = dev->config;
    struct iqs5xx_data *data = dev->data;
    int err;

    data->dev = dev;
    k_work_init(&data->read_work, read_work_cb);
    k_work_init_delayable(&data->poll_work, poll_work_cb);
    k_work_init_delayable(&data->init_work, init_work_cb);

    if (!i2c_is_ready_dt(&cfg->i2c)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    if (cfg->reset.port) {
        if (!gpio_is_ready_dt(&cfg->reset)) {
            return -ENODEV;
        }
        gpio_pin_configure_dt(&cfg->reset, GPIO_OUTPUT_ACTIVE);
        k_busy_wait(1000);
        gpio_pin_set_dt(&cfg->reset, 0);
    }

    if (cfg->rdy.port) {
        if (!gpio_is_ready_dt(&cfg->rdy)) {
            return -ENODEV;
        }
        err = gpio_pin_configure_dt(&cfg->rdy, GPIO_INPUT);
        if (err) {
            return err;
        }
        gpio_init_callback(&data->rdy_cb, rdy_isr, BIT(cfg->rdy.pin));
        err = gpio_add_callback(cfg->rdy.port, &data->rdy_cb);
        if (err) {
            return err;
        }
        err = gpio_pin_interrupt_configure_dt(&cfg->rdy, GPIO_INT_EDGE_TO_ACTIVE);
        if (err) {
            return err;
        }
    }

    /* Give the controller time to boot after power-up / reset. */
    k_work_schedule(&data->init_work, K_MSEC(100));
    return 0;
}

#define IQS5XX_DEFINE(n)                                                                           \
    static struct iqs5xx_data iqs5xx_data_##n;                                                     \
    static const struct iqs5xx_config iqs5xx_config_##n = {                                        \
        .i2c = I2C_DT_SPEC_INST_GET(n),                                                            \
        .rdy = GPIO_DT_SPEC_INST_GET_OR(n, rdy_gpios, {0}),                                                \
        .reset = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, {0}),                                    \
        .report_rate_ms = DT_INST_PROP(n, report_rate_ms),                                         \
        .event_mode = DT_INST_PROP(n, event_mode),                                                 \
        .flip_x = DT_INST_PROP(n, flip_x),                                                         \
        .flip_y = DT_INST_PROP(n, flip_y),                                                         \
        .swap_xy = DT_INST_PROP(n, swap_xy),                                                       \
        .palm_reject = DT_INST_PROP(n, palm_reject),                                               \
        .hold_to_drag = DT_INST_PROP(n, hold_to_drag),                                             \
        .natural_scroll = DT_INST_PROP(n, natural_scroll),                                         \
        .scroll_divisor = DT_INST_PROP(n, scroll_divisor),                                         \
        .zoom_divisor = DT_INST_PROP(n, zoom_divisor),                                             \
        .swipe_distance = DT_INST_PROP(n, swipe_distance),                                         \
        .three_finger_tap_ms = DT_INST_PROP(n, three_finger_tap_ms),                               \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, iqs5xx_init, NULL, &iqs5xx_data_##n, &iqs5xx_config_##n, POST_KERNEL, \
                          CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(IQS5XX_DEFINE)
