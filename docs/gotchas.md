# Gotchas

Behaviour that looks like a bug but isn't, plus traps hit during the rework.
Details and history: `docs/rework-notes.md`.

## Keymap
- **Sticky Shift drops after ~0.5 s.** The outer Shift keys on Base are `&sticky_key_modtap`
  (tap = one-shot Shift, hold = normal Shift). The one-shot uses `sticky_key_quickrel` with
  `release-after-ms = STICKY_1SHOT_DECAY`, which defaults to 500 ms (Sunaku's value). The
  dongle's Shift icon goes amber for that window, then off. To lengthen it, `#define
  STICKY_1SHOT_DECAY <ms>` before that block; only the halves need reflashing. Kept at 500
  on purpose.
- **A key's own modifier is released by the next key press** (ZMK implicit modifiers), e.g. the
  Ctrl in the Wispr key `&kp LC(F19)`. Holding it and typing cancels Wispr; double-tap
  (hands-free mode) avoids it.

## LEDs
- **Layer colours fade after 3 s; Base/macOS stay lit.** Non-base layer maps have
  `fade-delay = <3>`, then the LEDs fade and their supply switches off until the next layer
  change. Base and macOS have no fade. Each half powers its LEDs off after 10 s without a key
  press on that half (`ZMK_IDLE_TIMEOUT=10000`, halves only).
- The LED supply (ext-power) saves every on/off to flash and restores it at boot, so a half
  that last faded out boots with the supply off. The RGB code re-asserts power after settings
  load (zmk 2119beb); without that, one half could boot dark.

## Display (Prospector dongle)
- Modifier icons only show explicit modifiers, so `&kp LC(F19)` does not light Ctrl.
- Sticky Shift is detected as "Shift active and no key physically down"; it can flick white
  for a moment while the next key is held.
- `ZMK_DISPLAY_WIDGET_LISTENER` keeps only the latest event and calls its state function with
  NULL at init. Use per-slot state and one work item on `zmk_display_work_q()` instead.

## Trackpad
- Gesture keys sent through `zmk,input-processor-behaviors` run on the input thread. Zephyr's
  512 B default stack overflows and freezes the device: set `CONFIG_INPUT_THREAD_STACK_SIZE=2048`.
- Without RDY wired, the IQS550 NACKs outside its comms window; the driver retries every 1 ms.
- 3-finger up/down sends PgUp/PgDn, which a focused text box swallows (looks like swipes do
  nothing on e.g. ChatGPT).

- **keyd grabs ZMK devices by USB ID (1d50:615e), including the trackpad.** It then re-sends
  the touchpad as an absolute "keyd virtual pointer": the pad maps to the whole screen and
  multi-touch is lost. Any keyd config must not match the touchpad (match keyboards only), and
  `sudo keyd reload` after removing a config, or the running keyd keeps the old one.
- **Zephyr 3.5 (zmk-fixes) GATT client: a request that gets an error response never completes.**
  `att_error_rsp()` keeps `att_change_security()`'s result in a `uint8_t`, so `>= 0` is always
  true and every error (e.g. "attribute not found", the normal end of a discovery) is taken as a
  security retry and dropped. The request then blocks that connection's ATT queue until
  disconnect. Workaround in `src/ptp/ptp_split_central.c`: tell pads from halves by GAP
  appearance (always readable), and stop every discovery/read as soon as it has what it needs.
  Upstream 4.1 has it right (`int ret; if (ret == 0 || ret == -EBUSY)`).
- Split pad choppiness had three causes, fixed in this order: ZMK drops idle split links to
  22.5 ms (pad frames aren't "activity"), the dongle stamped Scan Time on arrival (host speed
  comes from it), and BLE's 7.5 ms connection events bunch ~11.5 ms frames (7.5/15 ms
  arrivals). Fixes: per-pad fast link, pad-side scan time, 10 ms jitter buffer on the dongle.

- Zephyr 4.1: `CONFIG_I2C_LOG_LEVEL_OFF=y` breaks the link (`log_const_i2c_nrfx_twi[m]`
  undefined: the shared nrfx I2C code still references the log module). Don't turn I2C logging
  fully off. A split peripheral with `BT_BUF_ACL_TX_COUNT` raised also needs
  `BT_BUF_EVT_RX_COUNT` above it (build-time assert).

## Builds and tooling
- Builds are CI only; download with `gh run download <id> -n firmware -D firmware`.
- The shell is zsh: an unmatched glob (`rm firmware/*` on an empty dir) aborts the whole command
  chain. Use `find firmware -mindepth 1 -delete`.
- `CONFIG_RESET_ON_FATAL_ERROR` does not exist in this Zephyr (3.5) and fails the build.
