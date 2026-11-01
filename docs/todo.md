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
- [x] X/Y resolution axis fix (resolution registers are already in output axes)
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
- [x] RDY -> D2, NRST -> D3 soldered and enabled; driver runs on the RDY interrupt
- [ ] Second breakout board for the second TPS65 (current spare is for a TPS43 / RP2040)
- [x] Touchpad mode over split, hardware-confirmed smooth (`tps65_pad` peripheral -> dongle
      HID_1/HID_2): one packed 20-byte notification per frame with the pad's scan time;
      pads identified by GAP appearance 965; dongle keeps the pad's link at 7.5 ms while in
      use and releases frames through a 10 ms jitter buffer (PTP_SPLIT_JITTER_US). Pad rate
      measured 75-88 Hz (controller scan limit ~10-11 ms, below the 125 Hz PTP spec)
- [ ] Mouse-mode fallback over split (macOS): pad sends deltas/gestures when the dongle's
      host never sets touchpad mode
- [ ] Dongle gesture engine across both pads: left = scroll, right = cursor; resting finger
      on one pad holds the button while the other drags (two fingers resting = right drag)
- [ ] Input thread stack 2 KB on the dongle/halves once gestures emit keys there
- [ ] Optional: haptics (e.g. DRV2605L on the same I2C bus), touch-to-wake

## Other
- [x] `.gitignore` (firmware/) and `trainer/` committed
- [ ] Trainer game (user's own project)

## Later: move to Zephyr 4.1 (after everything above works)
Branches `zephyr-4.1` in glove80, qnlbnsl/zmk, qnlbnsl/prospector-zmk-module. All builds green
in CI (2026-11-01); not yet flashed. Push those branches over SSH (they carry workflow files).
- [x] Rebase the fork onto upstream ZMK main (Zephyr 4.1, `moergo/glove80` HWMv2 board):
      darknao per-key underglow layers (no 4.1 port exists yet), central->peripheral layer
      sync, `&rgb_ug` forward stub, Kconfig tweaks; drop the advertising-retry fix (upstream
      uses the `recycled` callback)
- [x] Prospector module (as layout `glove80` on upstream feat/new-status-screens): rebase our widgets (layer name, battery bar, status icons) onto
      upstream's 4.1 / LVGL 9 version
- [x] Trackpad driver and PTP mode on 4.1 (builds; TWIM, no I2C_LOG_LEVEL_OFF) (FluxWav's patch targets 4.1 directly). 4.1 fixes
      the ATT error-response hang (see gotchas); keep the workaround, it is harmless
- [ ] Re-test everything on hardware: layers, LEDs, display, reconnect, battery, trackpads

- Backlog: left trackpad as a normal trackpad on its own. For now it is helper-only (gestures
  for the right pad, see docs/trackpads-and-screen.md). Until the gesture code exists, the
  dongle still passes it through as USB touchpad 1.

- Connection quality (2026-11-01, gestures branch, logging dongle): occasional repeated key
  ("rrrrr") then the halves' LEDs flash and it recovers. No disconnects in the log. It lines up
  with ZMK's sleep/wake param swaps on all 4 links (22.5 <-> 7.5 ms), `opcode 0x2013 status 0x02`
  (LE conn update rejected) and "Failed to sleep split connection: -5". Next: test on the
  non-logging dongle; then stop ptp_split's fast_work from racing ZMK's updates; then consider a
  slightly longer interval for the halves with 4 links.
- Left 4-wire breakout: wiring was wrong (fixed). Same I2C setup as the right pad (no internal
  pull-ups, 400 kHz); only RDY/NRST are missing.
