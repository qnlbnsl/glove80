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
- **Base is dark while typing, by design.** Each layer map has a `fade-delay` (Base 1 s, other
  layers 3 s); after it the LEDs fade and their power supply switches off. Holding a layer
  lights that layer again. Each half also powers its LEDs off after 10 s without a key press
  on that half (`ZMK_IDLE_TIMEOUT=10000`, halves only).
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

## Builds and tooling
- Builds are CI only; download with `gh run download <id> -n firmware -D firmware`.
- The shell is zsh: an unmatched glob (`rm firmware/*` on an empty dir) aborts the whole command
  chain. Use `find firmware -mindepth 1 -delete`.
- `CONFIG_RESET_ON_FATAL_ERROR` does not exist in this Zephyr (3.5) and fails the build.
