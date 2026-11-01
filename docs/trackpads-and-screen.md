# Trackpads and the dongle screen: decisions

Decided 2026-11-01. Mockups: https://claude.ai/artifact/7NVWtvHsdBnupHMSsFTco8 (option B chosen).

## Fixed sides (done, untested)
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

Cross-pad (combined on the dongle, the only place that sees both pads):

| Left pad | Right pad | Result |
|---|---|---|
| 1 finger resting | moving | left-button drag (right pad's frames carry button 1) |
| 2 fingers resting | moving | right-button drag |
| circling clockwise | — | scroll down, speed follows the circling (like the Steam Deck); counter-clockwise scrolls up |
| — | normal use | normal touchpad |

Open:
- **Drag lock:** how to latch and release a drag. Maybe a left tap while dragging latches it,
  and the next left tap releases it.
- **Rest vs move:** how long a left finger has to stay still before it counts as "resting" and
  not the start of cursor movement (guess: 100–150 ms and under ~1 mm of travel).
- **Circular scroll output:** in touchpad mode the host does scrolling itself, so the dongle has
  to either fake a 2-finger swipe on a touchpad or send wheel events through ZMK's mouse report.
  The wheel is simpler; check whether ZMK's report supports high-resolution scrolling.
- **macOS:** none of this works until mouse mode works over the split link (see todo.md).

## Screen (option B + changes)
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

## Side question parked
Haptics driven from the nRF (pad XIAO or dongle): ask again after screen and gestures are done.
