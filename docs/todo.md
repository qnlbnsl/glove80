# Glove80 rework: TODO

Plan: finish everything on the current base (qnlbnsl/zmk `rgb-layer-25.08-peripherals`,
Zephyr 3.5). Once it all works on hardware, move to Zephyr 4.1 (last section).
Background, decisions and findings: `docs/rework-notes.md`.

Legend: `[x]` done (hardware-confirmed), `[~]` built but not yet confirmed, `[ ]` open.

## Done
- [x] Vim HJKL on the Cursor layer
- [x] Interactive layer map (`layermap/`, served with `python3 -m http.server -d layermap`)
- [x] Omarchy ("WM") layer on RH T2, Omarchy + Magic LED maps
- [x] LEDs with the dongle as central (`&rgb_ug` forward stub, halves start in layer mode)
- [x] Dongle display phase 1: big layer name + colour bar, fade, no roller
- [x] Split reconnect after dongle reset (peripheral advertising retry)
- [x] Dongle TX power +8 dBm (fixed flaky battery readings)
- [x] Battery widget: per-slot state, left/right swap
- [x] Wispr key: leave as Ctrl+F19 (double-tap = hands-free mode)

## Dongle display
- [~] Phase 2: status row (module 496fee5): OS icon, Ctrl/Alt/Super/Shift, caps lock, caps word;
      macOS layer shown as the OS icon instead of the layer name; dongle HID indicators on
- [ ] Phase 3: runtime OS switching (Linux/macOS/Windows) from Magic, icon follows
      (design needed: shortcuts and unicode entry are compile-time today)
- [ ] Halves show Base LEDs at boot without waiting for the first layer update

## Trackpads: driver and mouse mode (XIAO + TPS65 USB rig)
- [x] IQS5xx driver: polling without RDY, NACK retry, reset recovery, reset-cause log
- [x] Cursor, tap, 2-finger tap (decided at lift-off), 3-finger tap
- [x] Own 2-finger scroll + pinch from absolute positions (mode locked per touch)
- [x] 3-finger swipes (input thread stack 2 KB fixes the freeze)
- [ ] Tune: cursor speed/acceleration, scroll divisor, swipe distance, palm reject
- [ ] Wire RDY (and NRST) from the module pads; switch to interrupt + event mode, check power
- [ ] Use absolute positions for the cursor too (no lost movement on missed windows)
- [ ] Real gesture mapping per OS (workspace prev/next, overview, show desktop) instead of
      the test keys
- [ ] Press-and-hold drag on a single pad (optional, `hold-to-drag`)

## Trackpads: touchpad (PTP digitizer) mode
- [ ] Port the touchpad part of FluxWav's `zmk-zephyr41-ptp` patch to the 3.5 fork: second
      USB HID interface, PTP descriptor, feature reports (capabilities, PTPHQA blob, mode,
      selective reporting); drop the old `ZMK_MOUSE` parts that clash with `ZMK_POINTING`
- [ ] Driver emits per-finger events (ABS_MT_SLOT / ABS_X / ABS_Y / BTN_TOUCH) in PTP mode
- [ ] Clicks in PTP reports (patch has them as TODO)
- [ ] Runtime switch mouse mode <-> touchpad mode (Magic layer; later tied to the OS setting)
- [ ] Test: Linux (libinput / Hyprland gestures), Windows (certification blob), macOS
      fallback to mouse mode

## Trackpads: hardware and integration
- [ ] Decide A (pad on each half's GPIO header) vs B (pad on its own XIAO + LiPo)
- [ ] A: per half 3.3 V LDO with enable from VDDH, 4-ch level shifter to VEXT, wiring per
      `rework-notes.md`; bigger LiPo
- [ ] Second breakout board for the second TPS65 (current spare is for a TPS43 / RP2040)
- [ ] Pads report to the dongle over the split link (mouse mode: deltas + touch state)
- [ ] Touchpad mode over split: batch each finger frame into one BLE message (input-split
      sends one notification per event, too much at 100 Hz x 5 fingers)
- [ ] Dongle gesture engine across both pads: left = scroll, right = cursor; resting finger
      on one pad holds the button while the other drags (two fingers resting = right drag)
- [ ] Input thread stack 2 KB on the dongle/halves once gestures emit keys there
- [ ] Optional: haptics (e.g. DRV2605L on the same I2C bus), touch-to-wake

## Other
- [ ] Commit or ignore `.gitignore` (firmware/) and `trainer/` (user's WIP game, leave untouched)
- [ ] Trainer game (user's own project)

## Later: move to Zephyr 4.1 (after everything above works)
- [ ] Rebase the fork onto upstream ZMK main (Zephyr 4.1, `moergo/glove80` HWMv2 board):
      darknao per-key underglow layers (no 4.1 port exists yet), central->peripheral layer
      sync, `&rgb_ug` forward stub, Kconfig tweaks; drop the advertising-retry fix (upstream
      uses the `recycled` callback)
- [ ] Prospector module: rebase our widgets (layer name, battery bar, status icons) onto
      upstream's 4.1 / LVGL 9 version
- [ ] Trackpad driver and PTP mode on 4.1 (FluxWav's patch targets 4.1 directly)
- [ ] Re-test everything on hardware: layers, LEDs, display, reconnect, battery, trackpads
