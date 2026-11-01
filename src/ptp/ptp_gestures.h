/*
 * Helper-pad gestures on the split central (CONFIG_PTP_SPLIT_LEFT_HELPER). Call from the system
 * work queue only.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <ptp.h>

void ptp_gestures_left_frame(const struct ptp_frame *frame);
void ptp_gestures_right_frame(const struct ptp_frame *frame);
void ptp_gestures_left_gone(void);
