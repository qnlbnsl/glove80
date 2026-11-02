# Trackpads and the dongle screen: decisions

Decided 2026-11-01. Mockups: https://claude.ai/artifact/7NVWtvHsdBnupHMSsFTco8 (option B chosen).

## Fixed sides (done, tested 2026-11-01)
- Pad firmware is built per side: `tps65_pad_lh` / `tps65_pad_rh` (`CONFIG_PTP_SPLIT_PAD_SIDE_LEFT/RIGHT`).
  The BLE name follows: "TPS65 Pad L" / "TPS65 Pad R".
- The pad exposes its side as a read characteristic (`PTP_SPLIT_SIDE_UUID`). The dongle reads it
  before subscribing and sends left -> USB touchpad 1, right -> USB touchpad 2, whatever the
  connection order. Pads without it (older firmware) keep connection order.

## Gestures
Roles:
- **Right pad = the main trackpad.** It behaves like a normal laptop touchpad, with the host's
  own gestures: 1-finger tap = click, 2-finger tap = right click, 2-finger scroll, and so on. The
  dongle passes it through unchanged unless the left pad is helping.
- **Left pad = helper only**, for the things the small TPS65 is bad at: holding a button while
  dragging, drag lock and fine scrolling. It never moves the cursor and is not passed to the
  host as a touchpad; the dongle uses its frames for gestures only. (Using it as a normal
  trackpad on its own is in the backlog, see todo.md.)

Decided and built 2026-11-01 (branch `gestures`). All ten checks passed on hardware: no cursor from the
left pad, tap click, 1- and 2-finger rest drags, double-tap drag lock on and off, circle scroll
both ways, rest without the right pad does nothing, brushes do nothing.

| Left pad | Result |
|---|---|
| 1 finger resting, right pad moving | left-button drag |
| 2 fingers resting, right pad moving | right-button drag |
| quick tap | left click (sent ~250 ms after the tap, once it can't be a double tap) |
| double tap | drag lock: left button latched down; the next tap releases it |
| circling | vertical scroll, clockwise = down, speed follows the circling (smooth scroll) |

How it works:
- **Where:** the dongle (`src/ptp/ptp_gestures.c`) reads the left pad's frames. They never go to
  the computer, and the computer sees one USB touchpad (the right pad).
- **How it reaches the computer:** buttons and scroll go out through ZMK's mouse report, not the
  touchpad. Operating systems merge buttons from all pointing devices, so a mouse button held
  while the touchpad moves is a drag on Windows, Linux and macOS.
- **Resting doesn't press yet.** A resting finger only "arms" the button. It is pressed once the
  right pad is touched, so circling or a stray touch on the left pad never clicks.
- **Circling:** angle around the pad centre (points near the centre are ignored). After 45 degrees
  of turning it becomes scrolling and disarms the button. Then 1 turn = 12 detents, sent in
  1/multiplier steps (ZMK smooth scrolling, `CONFIG_ZMK_POINTING_SMOOTH_SCROLLING`).
- **Ignored touches:** contacts under 40 ms.

Still open:
- **Drag lock safety:** should a forgotten drag lock release itself after some idle time?
- **macOS:** clicks, drag lock and scroll work there (mouse report). The right pad still needs
  mouse mode over the split link to move the cursor at all (see todo.md).

## Screen (done 2026-11-01, prospector-zmk-module zephyr-4.1)
Built as below, except the OS logo went to the bottom left and "wpm" sits on one line with a
small graph. Spacing is even by visible extents (8 px edges, 20 px gaps), documented in
`layouts/glove80/status_screen.c`. Pad tiles poll `ptp_split_pad_status()` from the split
central. Not done yet: pad battery % (needs per-pad batteries).

Original plan:
- **Top:** modifier row, as now. The OS logo moves up beside it.
- **Middle:** layer name with its underline right below it (no gap).
- **Tile grid,** mirrored like the hardware:
  - Row 1: `LH`, `RH`. Link dot plus battery %.
  - Row 2: `Pad L`, `Pad R`. Off (red) or on. While the pad is in use, it shows the tracking
    interval in ms (from the frames' scan times on the dongle). While on but idle, it shows the
    pad's battery % once the pads have their own batteries.
- **Row 3:** typing speed. ZMK's WPM (`CONFIG_ZMK_WPM`, both halves combined on the dongle) as a
  number plus a small history graph.
- **Later:** with one shared battery (planned fast-charge board), the halves' battery numbers
  collapse into one reading.

## Haptics and force click (scoped 2026-11-02, not building yet)
Idea: MacBook-style clickpad. Load cells measure press force, a haptic actuator plays the click.
- **Haptics:** one DRV2605L + one LRA per pad (one is enough if the pad sits on a stiff plate that
  floats). Zephyr 4.1 has the driver (`drivers/haptics/drv2605.c`, ROM effect library). On I2C
  (0x5A, no clash with the IQS550 at 0x74), 3V3. Glue the LRA under the plate; an X-axis
  (side-to-side) LRA disturbs the load cells less than a coin LRA (else ignore force ~20 ms after
  each buzz).
- **Force:** 4 mini load cells at the corners (current mount; a single cell would need a hinge and
  feels uneven), summed in hardware into one NAU7802 (I2C 0x2A, up to 320 SPS). Board: SparkFun
  Qwiic Scale (breaks out the data-ready pin -> D0). No Zephyr NAU7802 driver; small own driver.
  Per-corner readings (palm reject, press position) would need a 4-channel ADC (ADS131M04).
  Stiff backing plate (~1.5 mm aluminium), cells preloaded, re-zero while no finger touches.
- **Firmware plan:** clicks decided on the pad (press ~150 g down + click buzz, release ~100 g up
  + lighter buzz; optional deep press), button bit in the PTP report (host clickpad behaviour),
  buzz played locally (no BLE delay). Left helper could use press-to-hold instead of rest-to-arm.
  Dongle can also send effects for its gestures (tap, drag lock, scroll ticks).
- Free XIAO pins: D0, D1, D6-D10. Start with the right pad.
