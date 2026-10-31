#!/usr/bin/env node
// Generate layermap/data.js (window.KEYMAP) from config/glove80.keymap for layermap/index.html.
// Usage: node tools/layermap/build.mjs [keymap] [out]
// Labels assume the Linux build (OPERATING_SYSTEM 'L', GACS home-row mods).
import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const keymapPath = resolve(process.argv[2] ?? `${here}/../../config/glove80.keymap`);
const outPath = resolve(process.argv[3] ?? `${here}/../../layermap/data.js`);

const src = readFileSync(keymapPath, "utf8")
  .replace(/\/\*[\s\S]*?\*\//g, "")
  .replace(/\/\/.*$/gm, "");

// ---- defines (first non-self definition wins; OS-specific names are handled in LABELS) ----
const defines = {};
for (const m of src.matchAll(/^\s*#define\s+(\w+)\s+(.+?)\s*$/gm)) {
  if (!(m[1] in defines) && m[2] !== m[1]) defines[m[1]] = m[2];
}
const deref = (t, n = 6) => (n && /^\w+$/.test(t) && defines[t] && !/^_/.test(t) ? deref(defines[t], n - 1) : t);

// ---- physical positions ----
const posName = [];
for (const [k, v] of Object.entries(defines)) {
  const m = /^POS_(LH|RH)_(C\dR\d|T\d)$/.exec(k);
  if (m && /^\d+$/.test(v)) posName[+v] = `${m[1]} ${m[2]}`;
}
if (posName.filter(Boolean).length !== 80) throw new Error("expected 80 POS_ defines");

// ---- layers ----
const kstart = src.search(/keymap\s*\{\s*compatible\s*=\s*"zmk,keymap"/);
if (kstart < 0) throw new Error("no zmk,keymap node");
const layers = [];
for (const m of src.slice(kstart).matchAll(/layer_(\w+)\s*\{([\s\S]*?)bindings\s*=\s*<([\s\S]*?)>\s*;/g)) {
  const bindings = m[3].split(/(?=&)/).map((s) => s.replace(/\s+/g, " ").trim()).filter(Boolean);
  if (bindings.length !== 80) throw new Error(`layer ${m[1]}: ${bindings.length} bindings`);
  const display = /display-name\s*=\s*"([^"]*)"/.exec(m[2])?.[1];
  layers.push({ id: m[1], name: display || m[1].replace(/_/g, " "), internal: !display, bindings });
}
const layerIds = layers.map((l) => l.id);
const layerRef = (t) => {
  t = deref(t);
  if (/^\d+$/.test(t)) return layerIds[+t];
  const m = /^LAYER_(\w+)$/.exec(t);
  return m && layerIds.includes(m[1]) ? m[1] : null;
};

// ---- key labels ----
const SYM = {
  EQUAL: "=", MINUS: "-", BSLH: "\\", BACKSLASH: "\\", SQT: "'", SINGLE_QUOTE: "'", APOS: "'",
  SEMI: ";", SEMICOLON: ";", COMMA: ",", DOT: ".", PERIOD: ".", FSLH: "/", SLASH: "/",
  GRAVE: "`", LBKT: "[", RBKT: "]",
};
const SHIFTED = {
  N1: "!", N2: "@", N3: "#", N4: "$", N5: "%", N6: "^", N7: "&", N8: "*", N9: "(", N0: ")",
  MINUS: "_", EQUAL: "+", LBKT: "{", RBKT: "}", BSLH: "|", SEMI: ":", SQT: "\"", SINGLE_QUOTE: "\"",
  COMMA: "<", DOT: ">", FSLH: "?", GRAVE: "~",
};
const NAMES = {
  TAB: "Tab", ENTER: "Enter", RET: "Enter", RETURN: "Enter", ESC: "Esc", ESCAPE: "Esc",
  BSPC: "Bksp", BACKSPACE: "Bksp", DEL: "Del", DELETE: "Del", INS: "Ins", INSERT: "Ins",
  SPACE: "Space", UP: "↑", DOWN: "↓", LEFT: "←", RIGHT: "→", HOME: "Home", END: "End",
  PG_UP: "PgUp", PG_DN: "PgDn", CAPS: "Caps", CAPSLOCK: "Caps", PSCRN: "PrtSc",
  PAUSE_BREAK: "Pause", SLCK: "ScrLk", KP_NUM: "NumLk", K_APP: "Menu",
  LSHFT: "Shift", LSFT: "Shift", RSHFT: "Shift", RSFT: "Shift", LCTRL: "Ctrl", LCTL: "Ctrl",
  RCTRL: "Ctrl", RCTL: "Ctrl", LALT: "Alt", RALT: "AltGr", LGUI: "Gui", RGUI: "Gui",
  C_PLAY: "Play", C_PP: "Play/Pause", C_PREV: "Prev", C_NEXT: "Next", C_STOP: "Stop",
  C_EJECT: "Eject", C_MUTE: "Mute", C_VOL_UP: "Vol+", C_VOL_DN: "Vol−", C_BRI_UP: "Bright+",
  C_BRI_DN: "Bright−", C_BRI_MAX: "Bright max", C_BRI_MIN: "Bright min", C_BRI_AUTO: "Bright auto",
  C_MEDIA_HOME: "Media", K_CALC: "Calc", K_WWW: "Browser", C_POWER: "Power", C_SLEEP: "Sleep",
  K_LOCK: "Lock", KP_MULTIPLY: "kp *", KP_SLASH: "kp /", KP_MINUS: "kp −", KP_PLUS: "kp +",
  KP_DOT: "kp .", KP_COMMA: "kp ,", KP_EQUAL: "kp =", KP_ENTER: "kp Enter", KP_CLEAR: "kp Clear",
  KP_LPAR: "kp (", KP_RPAR: "kp )", GLOBE: "Globe",
  // Sunaku's OS-portable shortcuts (Linux values).
  _UNDO: "Undo", _REDO: "Redo", _CUT: "Cut", _COPY: "Copy", _PASTE: "Paste", _FIND: "Find",
  _FIND_NEXT: "Find next", _FIND_PREV: "Find prev", _HOME: "Home", _END: "End", _LOCK: "Lock",
  _SLEEP: "Sleep", _POWER: "Power", _FILES: "Files", _EMOJI: "Emoji picker", _GLOBE: "Gui", _A_TAB: "Alt", _G_TAB: "Gui",
};
const FINGER_MOD = { PINKY: "Gui", RINGY: "Alt", MIDDY: "Ctrl", INDEX: "Shift" };
const MODFN = { LS: "Shift", RS: "Shift", LC: "Ctrl", RC: "Ctrl", LA: "Alt", RA: "AltGr", LG: "Gui", RG: "Gui", _C: "Ctrl" };

function key(t) {
  t = t.trim();
  if (t === "0") return "(nothing)"; // keycode 0, e.g. Typing layer's LH T2
  const fn = /^(\w+)\((.+)\)$/.exec(t);
  if (fn && fn[1] in MODFN) {
    const inner = fn[2].trim();
    if ((fn[1] === "LS" || fn[1] === "RS") && SHIFTED[inner]) return SHIFTED[inner];
    return `${MODFN[fn[1]]}+${key(inner)}`;
  }
  const fm = /^(LEFT|RIGHT)_(PINKY|RINGY|MIDDY|INDEX)_MOD$/.exec(t);
  if (fm) return FINGER_MOD[fm[2]];
  if (NAMES[t]) return NAMES[t];
  if (SYM[t]) return SYM[t];
  if (/^[A-Z]$/.test(t)) return t;
  let m;
  if ((m = /^N(\d)$/.exec(t))) return m[1];
  if ((m = /^KP_N(\d)$/.exec(t))) return `kp ${m[1]}`;
  if (/^F\d+$/.test(t)) return t;
  const d = deref(t);
  if (d !== t) return key(d);
  return t.replace(/^_/, "").replace(/_/g, " ").toLowerCase();
}
const human = (s) => s.replace(/_/g, " ");

const MOUSE = { LCLK: "L click", RCLK: "R click", MCLK: "M click", MB4: "Back", MB5: "Fwd" };
const ARROW = { UP: "↑", DOWN: "↓", LEFT: "←", RIGHT: "→" };
const RGB = { RGB_TOG: "RGB on/off", RGB_EFF: "RGB effect", RGB_HUI: "Hue+", RGB_HUD: "Hue−", RGB_SAI: "Sat+", RGB_SAD: "Sat−", RGB_BRI: "Bright+", RGB_BRD: "Bright−", RGB_SPI: "Speed+", RGB_SPD: "Speed−" };
const SPECIAL = {
  "&trans": { kind: "trans" }, "&none": { kind: "none" },
  "&parang_left": { tap: "( <" }, "&parang_right": { tap: ") >" },
  "&thums_up": { tap: "Scroll ↑" }, "&thums_down": { tap: "Scroll ↓" },
  "&dot_dot": { tap: "../" }, "&triple_backtick": { tap: "```" },
  "&caps_word": { tap: "Caps word" }, "&linux_magic_sysrq": { tap: "SysRq" },
  "&bootloader": { tap: "Bootloader", danger: true }, "&sys_reset": { tap: "Reset", danger: true },
  "&emoji_zwj": { tap: "ZWJ" },
};

// Returns { tap, hold?, layer?, action?, kind, danger? } for one binding.
function describe(b) {
  if (SPECIAL[b]) return { kind: "key", ...SPECIAL[b] };
  const [beh, ...rest] = b.split(" ");
  const arg = rest.join(" ");
  let m;
  switch (beh) {
    case "&kp": return { kind: "key", tap: key(arg) };
    case "&sk": return { kind: "key", tap: `${key(arg)}`, note: "sticky" };
    case "&mo": case "&tog": case "&to": case "&sl": {
      const layer = layerRef(arg);
      const action = { "&mo": "hold", "&tog": "toggle", "&to": "go to", "&sl": "sticky" }[beh];
      if (beh === "&mo" && !layer) return { kind: "key", tap: key(deref(arg)) }; // HRM pass-through thumb
      return { kind: "layer", action, layer, tap: null };
    }
    case "&thumb": case "&space": return { kind: "layer", action: "hold", layer: layerRef(rest[0]), tap: key(rest[1]) };
    case "&stumb": return { kind: "layer", action: "hold", layer: layerRef(rest[0]), tap: key(rest[1]), note: "sticky" };
    case "&magic": return { kind: "layer", action: "hold", layer: layerRef(rest[0]), tap: "RGB status" };
    case "&sticky_key_modtap": return { kind: "key", tap: key(rest[1]), note: "sticky", hold: key(rest[0]) };
    case "&mkp": return { kind: "key", tap: MOUSE[arg] ?? arg };
    case "&mmv": return { kind: "key", tap: `Mouse ${ARROW[arg.replace("MOVE_", "")] ?? arg}` };
    case "&msc": return { kind: "key", tap: `Scroll ${ARROW[arg.replace("MOUSE_SCROLL_", "")] ?? arg}` };
    case "&rgb_ug": return { kind: "key", tap: RGB[arg] ?? arg };
    case "&bt": return { kind: "key", tap: arg === "BT_CLR" ? "BT clear" : arg === "BT_CLR_ALL" ? "BT clear all" : arg, danger: true };
    case "&out": return { kind: "key", tap: arg === "OUT_USB" ? "USB out" : "BLE out" };
  }
  if ((m = /^&bt_(\d)$/.exec(beh))) return { kind: "key", tap: `BT ${m[1]}` };
  // Home-row mods: "&LeftPinky (A, LAYER_QWERTY)" and macOS "&LeftMiddy_layer0 (KEY_LH_C5R4)".
  if ((m = /^&(Left|Right)(Pinky|Ringy|Middy|Index)(_layer0)?$/.exec(beh))) {
    const tap = /^\((\w+)/.exec(arg)?.[1];
    return { kind: "hrm", tap: key(deref(tap)), hold: FINGER_MOD[m[2].toUpperCase()] };
  }
  if ((m = /^&(Left|Right)(Pinky|Ringy|Middy|Index)(Pinky|Ringy|Middy|Index)$/.exec(beh)))
    return { kind: "key", tap: `+${FINGER_MOD[m[3].toUpperCase()]}`, note: "chord" };
  if ((m = /^&(left|right)_(pinky|ringy|middy|index)_tap$/.exec(beh))) return { kind: "key", tap: key(deref(arg)) };
  if ((m = /^&(select|extend)_(\w+)$/.exec(beh))) return { kind: "key", tap: `${m[1]} ${human(m[2])}` };
  if ((m = /^&emoji_(\w+)$/.exec(beh))) return { kind: "key", tap: human(m[1]), note: "emoji" };
  if ((m = /^&world_(\w+?)(_base)?$/.exec(beh))) return { kind: "key", tap: human(m[1]), note: "unicode" };
  return { kind: "key", tap: b.replace(/^&/, "") };
}

// ---- LED colours (underglow-layer) ----
const hexOf = {};
for (const [k, v] of Object.entries(defines)) {
  const m = /^(\w+)_RGB$/.exec(k);
  if (m && /^0x[0-9a-fA-F]{6}$/.test(v)) hexOf[k] = "#" + v.slice(2).toLowerCase();
}
function ledColor(tok) {
  for (let i = 0; i < 6; i++) {
    const d = defines[tok];
    if (!d) return null;
    const ug = /^&ug(?:_\w+)?\s+(\w+_RGB)/.exec(d);
    if (ug) return hexOf[ug[1]] === "#000000" ? null : hexOf[ug[1]];
    tok = d;
  }
  return null;
}
const leds = {};
const ugStart = src.indexOf("underglow-layer");
if (ugStart >= 0) {
  const ugBody = src.slice(ugStart, src.indexOf("\n  };", ugStart));
  for (const m of ugBody.matchAll(/(\w+)\s*\{\s*bindings\s*=\s*<([\s\S]*?)>\s*;\s*layer-id\s*=\s*<(\w+)>/g)) {
    const toks = m[2].trim().split(/\s+/);
    const id = layerRef(m[3]);
    if (id && toks.length === 80) leds[id] = toks.map(ledColor);
  }
}

// ---- combos ----
const combos = [];
const cstart = src.search(/combos\s*\{\s*compatible\s*=\s*"zmk,combos"/);
if (cstart >= 0) {
  for (const m of src.slice(cstart).matchAll(/combo_(\w+)\s*\{([\s\S]*?)\};/g)) {
    const body = m[2];
    const prop = (p) => new RegExp(`${p}\\s*=\\s*<([\\s\\S]*?)>\\s*;`).exec(body)?.[1]?.trim();
    const pos = prop("key-positions")?.split(/\s+/).map((t) => +deref(t));
    const binding = prop("bindings")?.replace(/\s+/g, " ");
    if (!pos || !binding) continue;
    const ls = prop("layers")?.split(/\s+/).map(layerRef).filter(Boolean) ?? layerIds;
    let d = describe(binding);
    if (/^&mod_tab_chord/.test(binding)) d = { kind: "key", tap: `${key(binding.split(" ")[1])}+Tab switcher` };
    combos.push({ name: human(m[1]), positions: pos, binding, ...d, layers: ls });
  }
}

const out = {
  source: "config/glove80.keymap",
  generated: new Date().toISOString().slice(0, 10),
  positions: posName,
  layers: layers.map((l) => ({
    id: l.id, name: l.name, internal: l.internal, // no display-name = helper layer
    keys: l.bindings.map((b) => ({ raw: b, ...describe(b) })),
    leds: leds[l.id] ?? null,
  })),
  combos,
};
writeFileSync(outPath, `// Generated by tools/layermap/build.mjs from ${out.source}. Do not edit.\nwindow.KEYMAP = ${JSON.stringify(out)};\n`);
console.log(`${layers.length} layers, ${combos.length} combos, ${Object.keys(leds).length} LED maps -> ${outPath}`);
