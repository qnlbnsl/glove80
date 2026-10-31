# Glove80 rework: working notes / session handoff

Last updated 2026-10-31. Repo: github.com/qnlbnsl/glove80. Read this first when resuming.
Claims below were verified in the 2026-10-31 session unless marked otherwise; re-check
current state (git log, `gh run list -R qnlbnsl/glove80`) before relying on them.

## Hardware / setup

- Glove80 halves (glove80_lh / glove80_rh) run as **BLE peripherals**; a **Seeed XIAO nRF52840
  dongle** with a **Prospector** display (shields `glove80_dongle prospector_adapter`) is the
  central. All builds (dongle + halves) compile `config/glove80.keymap`.
- ZMK: fork `qnlbnsl/zmk` branch `rgb-layer-25.08-peripherals` (darknao's per-key
  `underglow-layer` RGB + central->peripheral layer-state sync). Display module:
  `qnlbnsl/prospector-zmk-module@main`. See `config/west.yml`.
- CI uses the fork's workflow: `.github/workflows/build.yml` ->
  `qnlbnsl/zmk/.github/workflows/build-user-config.yml@rgb-layer-25.08-peripherals`.
  (Upstream zmkfirmware workflow @main fails with `KeyError: 'qualifiers'` against the fork.)
- Used with Linux (Omarchy/Hyprland), macOS and Windows.
- Commit messages must NOT contain AI attribution / model names (commit-msg hook rejects them).

## Branches / commits

- `main`: still red (dongle overlay experiments from 2025-10-09). Not touched.
- `setup/my-config` @ e8a0665: restored last-green dongle overlay + build.yaml, fork workflow.
  CI 41232444561 green. Dongle-driven layer colours were dropped in that restore.
- `keymap/sunaku-latest` (current work, branched from setup/my-config):
  - e2816a0 Update keymap to Sunaku glove80-keymaps main (f3594ac, post-v52). CI 41238107186 green.
  - 72b5b5b Drop Factory and System layers; merge System into Magic; delete
    config/keymap.dts + config/keymaps_layers.h. CI 41240799708 (check result).
- `origin/wip/build-scripts-2026-10-30`: local build.sh / build_seed.sh (west build). Not merged.
- Untracked `trainer/` (tools/parse-keymap.mjs -> trainer/keymap.json, a JSON dump of the keymap
  layers). Purpose unknown; ask the user before changing it.

## Keymap pipeline (how config/glove80.keymap is made)

The keymap is Sunaku's glove80-keymaps (Layout Editor export `keymap.zmk`) plus our changes.
`tools/sunaku/sync.sh [ref]` regenerates it (clones Sunaku at `tools/sunaku/REF`, runs
`customize.js`, Sunaku's `rake keymap.dtsi`, then `assemble.js`). Needs node, ruby + gems
`rake` and `erb` (`gem install --user-install rake erb`). Note: sync.sh itself was not run
in-repo yet (auto-mode blocked running external code); the same steps were run by hand.

- `customize.js`: dropped layers, per-position key overrides, config defines, RGB tweaks.
- `assemble.js`: splices regenerated dtsi into keymap.zmk, renumbers layer defines, rebuilds
  layer nodes, adds `display-name`s (map `DISPLAY_NAMES`).
- Edit those two files, not glove80.keymap, so Sunaku updates stay a re-run.

Previous base was Sunaku v42-rc5 (2025-08-08). Our changes carried over (3-way merge, no conflicts):
- Base: top row = Up Down F3-F8 Left Right; outer keys `=` (R2 left), `-` (R2 right),
  Tab (R3 left), `\` (R3 right), backtick (R6 left C5).
- Cursor: inverted-T arrows (+ white RGB highlight on them).
- `ENFORCE_BILATERAL` off. OPERATING_SYSTEM stays `'L'` (old file had `'W'`).

## Decisions made

- QWERTY only (Enthium/Dvorak/Colemak dropped). Keep home-row mods, Typing, Cursor, Number,
  Symbol, Function, Mouse (+ slow/fast/warp), Emoji, World, Gaming, Lower, Magic, macOS layers.
- Factory removed. System removed and merged into Magic: Magic right hand
  R3 = Menu PrtSc Pause ScrLk NumLk Caps, R4 = Lock Sleep Power SysRq; RGB "prev effect" dropped.
- Lower (Sunaku's version: layer locks via `tog`, sticky one-handed mods, numpad) KEPT on **LH T2**.
- **RH T2** (currently `&thumb LAYER_Lower INSERT`) will become the **Omarchy layer** hold.
  RH T1 is now plain Enter.
- Wispr Flow push-to-talk: **LH T3** (was PgUp) sends `Ctrl+F19` directly. On Mac/Windows set
  Wispr's hotkey to Ctrl+F19. On Linux, keyd still maps Glove80 (1d50:615e) Caps Lock ->
  Ctrl+F19; that rule can be removed now (Caps combo is T2+T6 in Sunaku's latest).
- OS: no auto-detection possible in ZMK. Build is Linux. GUI/Cmd stays on pinky (GACS).
  Sunaku's macOS toggle (Magic + C5R6) swaps to CAGS for trying Mac ordering. Emoji/World
  unicode entry is compile-time per OS (only correct on Linux with this build).
- Cursor: user may later want vim-style HJKL arrows instead of inverted-T. Not decided.

## Plan (agreed order)

1. ~~Update to Sunaku latest~~ (done) -> ~~layer cleanup~~ (done, verify CI 41240799708).
2. Possibly first: an **interactive layer map** page like Sunaku's
   https://sunaku.github.io/moergo-glove80-keyboard.html#layers, generated from our keymap
   (user idea, 2026-10-31). Not started.
3. **Prospector display fix** (then user flashes and tests). Diagnosis from reading
   `prospector-zmk-module/boards/shields/prospector_adapter/src/widgets/layer_roller.c` and
   `custom_status_screen.c`:
   - Names cut off: roller is 224px wide with 48pt FRAC font; long names don't fit.
   - Out of sync: shows `zmk_keymap_highest_layer_active()`, so home-row-mod layers
     (LeftPinky..RightPinky) and other helper layers flash through while typing; 100ms
     animation on a looping (LV_ROLLER_MODE_INFINITE) list of every layer -> lag / wrong row.
   - Fix: short display names (DISPLAY_NAMES in assemble.js), hide helper layers
     (HRM, macOS_*, Mouse_*), non-infinite roller, shorter/no animation, fit text.
     Changes go to qnlbnsl/prospector-zmk-module and the keymap.
4. **Omarchy layer** on RH T2: workspaces, window focus/move/swap/resize, launchers,
   screenshots, lock, menus. Source bindings: `omarchy menu keybindings --print` and
   `~/.config/hypr/bindings.lua`.
5. **LEDs (per-key, per-layer colours as a visual helper)**: e.g. Cursor layer lights arrows,
   Number lights numpad, other action groups get their own colours. Sunaku's `device.dtsi`
   already has per-layer `underglow-layer` colour maps, and the fork has darknao's per-key RGB.
   The missing piece in the dongle setup: halves must receive layer state from the dongle
   (fork has `split_central_update_layers` / `peripheral_layers.c`). Don't put a fake LED
   strip on the dongle (that's what broke CI in Oct 2025). Needs build + hardware test.
   Then retune colour maps per layer. Dongle build sets EXPERIMENTAL_RGB_LAYER=n (fine).
6. **Trackpads (later)**: two trackpads on the sidecar, each driven by a battery-powered
   Seeed XIAO nRF52840 as extra BLE peripherals of the dongle. User wants multi-touch
   gestures; stock ZMK only sends mouse move/scroll (no precision-touchpad HID), so this needs
   research. Fallback: keep using the Apple trackpad.

## Open questions

- What is `trainer/` for? Commit it?
- Interactive layer map: build it before the display fix? Where should it live (artifact
  page vs. GitHub Pages)?
- Omarchy layer contents and layout.
- HJKL Cursor layer?

## Other

- Founder OS: SKILL-FEEDBACK.md had 5 open rows (skill-sharpen); repo has no plugin selection.
