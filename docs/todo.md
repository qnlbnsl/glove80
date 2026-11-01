# Glove80 rework: TODO

Plan: finish everything on the current base (qnlbnsl/zmk `rgb-layer-25.08-peripherals`,
Zephyr 3.5). Once it all works on hardware, move to Zephyr 4.1 (last section).
Background, decisions and findings: `docs/rework-notes.md`. Known gotchas: `docs/gotchas.md`.

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
- [x] Phase 2: status row (module dd13b44): Ctrl/Alt/Super + Shift (white held, cyan Caps Word,
      amber sticky Shift / Caps Lock); OS logo between the battery cells; macOS layer shown as
      the OS logo instead of the layer name; dongle HID indicators on
- [ ] Phase 3 (deferred by user): runtime OS switching from Magic, icon follows. Leaning
      layer-set swap, but the current GACS -> CAGS mod swap is hard to relearn; undecided
- [x] Right half booted dark: LED supply restored "off" from flash (zmk 2119beb re-asserts it)
- [ ] Left half "iffy" connecting after reboot (seen once, not since): capture a log if it recurs
- [x] Halves show Base LEDs at boot (zmk 42e4769: unread battery 0% was treated as empty)
- [x] LED power saving: Base/macOS no fade, other layers fade after 3 s; idle off after 10 s

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
- [x] Touchpad (PTP) mode on the USB rig, written as our own module (`src/ptp/`, no ZMK fork
      change): HID_1 with Windows PTP descriptor + feature reports; Linux hid-multitouch
      switches it on. Hardware-confirmed: cursor, gestures, 3-finger workspace swipes
- [x] Driver hands one frame per scan to `ptp_send()` (transport-agnostic, for the split link)
- [~] X/Y resolution axis fix (cursor speed per axis); confirm on the rig
- [ ] Clicks in PTP reports (patch has them as TODO)
- [ ] Runtime switch mouse mode <-> touchpad mode (Magic layer; later tied to the OS setting)
- [ ] Test: Linux (libinput / Hyprland gestures), Windows (certification blob), macOS
      fallback to mouse mode

## Trackpads: hardware and integration
- [x] Decide A vs B: **B** (pads on their own XIAO). User's power plan: one ~3000 mAh LiPo
      per half (stock cells removed): cell -> protection + charger board (own USB-C) -> output
      pads -> one ideal diode (e.g. LM66100) per load -> Glove80 half BAT, trackpad XIAO BAT.
      Per-load diodes stop either board's USB charger back-feeding the pack or the other board.
      Open: charger with power path, battery % calibration
- [ ] Solder the 6-pin breakout (VCC, GND, SDA, SCL, RDY, RST) for the next rig tests
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
