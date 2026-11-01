/*
 * GATT UUIDs for the touchpad frame service (split peripheral -> central).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/bluetooth/uuid.h>

#define PTP_SPLIT_SERVICE_UUID BT_UUID_128_ENCODE(0x6c1e0001, 0x5e3a, 0x4b8f, 0x9a64, 0x0b5d7c2f3e10)
#define PTP_SPLIT_FRAMES_UUID BT_UUID_128_ENCODE(0x6c1e0002, 0x5e3a, 0x4b8f, 0x9a64, 0x0b5d7c2f3e10)
#define PTP_SPLIT_MODE_UUID BT_UUID_128_ENCODE(0x6c1e0003, 0x5e3a, 0x4b8f, 0x9a64, 0x0b5d7c2f3e10)
