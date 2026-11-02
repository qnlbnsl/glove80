// Step 1: customize Sunaku's keymap.json + device.dtsi + keymap.dtsi.erb in-place (run inside the sunaku worktree).
const fs = require("fs");

// Alternate alpha layouts (QWERTY only), Factory (key test), System (merged into Magic below).
const DROP = ["Enthium", "Dvorak", "Colemak", "Factory", "System"];
// Position overrides per layer, as rendered ZMK bindings.
const OVERRIDES = {
  QWERTY: {
    0: "&kp UP", 1: "&kp DOWN", 2: "&kp F3", 3: "&kp F4", 4: "&kp F5", 5: "&kp F6",
    6: "&kp F7", 7: "&kp F8", 8: "&kp LEFT", 9: "&kp RIGHT",
    10: "&kp EQUAL", 21: "&kp MINUS", 22: "&kp TAB", 33: "&kp BSLH", 65: "&kp GRAVE",
    54: "&kp LC(F19)", // LH T3: Wispr Flow push-to-talk (was PG_UP)
  },
  // Vim arrows on H J K L; Copy (displaced from H) moves to ";".
  Cursor: { 40: "&kp LEFT", 41: "&kp DOWN", 42: "&kp UP", 43: "&kp RIGHT", 44: "&kp _COPY" },
  // Media on the RH thumbs as in the pre-Sunaku-update keymap: T3 play/pause, T2 prev, T1 next, T6 mute.
  Function: { 55: "&kp C_PP", 56: "&kp C_PREV", 57: "&kp C_NEXT", 72: "&kp C_MUTE" },
  // System's unique keys, moved onto Magic's empty right-hand slots.
  Magic: {
    28: "&kp K_APP", 29: "&kp PSCRN", 30: "&kp PAUSE_BREAK", 31: "&kp SLCK", 32: "&kp KP_NUM", 33: "&kp CAPS",
    40: "&kp _LOCK", 41: "&kp _SLEEP", 42: "&kp _POWER", 43: "&linux_magic_sysrq",
  },
};

// "&kp LC(F19)" -> Layout Editor JSON object
function toJson(s) {
  if (s === "&trans" || s === "&none") return { value: s };
  const m = /^&kp\s+(.+)$/.exec(s);
  if (!m) return { value: "Custom", params: [{ value: s, params: [] }] };
  const parse = (t) => {
    const k = /^(\w+)\((.+)\)$/.exec(t);
    return k ? { value: k[1], params: [parse(k[2])] } : { value: t, params: [] };
  };
  return { value: "&kp", params: [parse(m[1].trim())] };
}

const km = JSON.parse(fs.readFileSync("keymap.json", "utf8"));
const dropIdx = DROP.map((n) => km.layer_names.indexOf(n));
if (dropIdx.some((i) => i < 0)) throw new Error("layer to drop not found");
const remap = (old) => { // old layer index -> new index, or null if dropped
  if (dropIdx.includes(old)) return null;
  return old - dropIdx.filter((d) => d < old).length;
};
const LAYER_BEHAVIORS = new Set(["&tog", "&mo", "&to", "&sl", "&lt"]);
km.layers = km.layers.filter((_, i) => !dropIdx.includes(i)).map((layer) =>
  layer.map((b) => {
    const p = b.params && b.params[0];
    if (LAYER_BEHAVIORS.has(b.value) && p && typeof p.value === "number") {
      const n = remap(p.value);
      return n === null ? { value: "&none" } : { ...b, params: [{ ...p, value: n }, ...b.params.slice(1)] };
    }
    // Custom bindings naming a dropped layer: "&thumb LAYER_X KEY" keeps its tap, anything else goes.
    const custom = b.value === "Custom" && p.value;
    const dead = custom && DROP.find((d) => new RegExp(`\\bLAYER_${d}\\b`).test(custom));
    if (dead) {
      const tap = new RegExp(`^&\\w+ LAYER_${dead} (\\w+)$`).exec(custom.trim());
      return tap ? toJson(`&kp ${tap[1]}`) : { value: "&none" };
    }
    return b;
  })
);
km.layer_names = km.layer_names.filter((n) => !DROP.includes(n));
for (const [layer, pos] of Object.entries(OVERRIDES)) {
  const li = km.layer_names.indexOf(layer);
  for (const [i, s] of Object.entries(pos)) km.layers[li][i] = toJson(s);
}

// Omarchy layer (hold RH T2): every key sends Super + its Base key, so Omarchy's own Super+key
// bindings apply as-is (Q quit, W close, F fullscreen, 1-0 workspaces, -/= resize, ...).
// Exceptions below: HJKL focus, left thumbs as Shift/Ctrl/Alt (Super+Shift+3 = move window to 3),
// left T4-T6 launchers, F-row screen capture.
const OMARCHY = {
  2: "&kp PSCRN", 3: "&kp LA(PSCRN)", 4: "&kp LG(PSCRN)", 5: "&kp LG(LC(PSCRN))", 6: "&kp LG(LC(C))", 7: "&none",
  28: "&kp LG(L)", 29: "&kp LG(J)", 30: "&kp LG(K)", // layout, split, keybindings (displaced by HJKL)
  40: "&kp LG(LEFT)", 41: "&kp LG(DOWN)", 42: "&kp LG(UP)", 43: "&kp LG(RIGHT)",
  46: "&kp LSHFT", 63: "&kp RSHFT",
  52: "&kp LSHFT", 53: "&kp LCTRL", 54: "&kp LALT",          // LH T1-T3
  69: "&kp LG(RET)", 70: "&kp LG(SPACE)", 71: "&kp LG(ESC)", // LH T4-T6: terminal, menu, system menu
  56: "&trans", 64: "&trans", 79: "&trans",                  // the held key; Magic
};
{
  const base = km.layers[km.layer_names.indexOf("QWERTY")];
  const baseKey = (b) => { // plain keycode of a Base binding, if any
    const p = b.params?.[0];
    if (b.value === "&kp" && p && !p.params?.length) return p.value;
    if (b.value === "Custom") return /^&(?:Left|Right)\w+ \((\w+),/.exec(p.value.trim())?.[1];
  };
  const layer = base.map((b, i) => {
    if (OMARCHY[i]) return toJson(OMARCHY[i]);
    const k = baseKey(b);
    return k ? toJson(`&kp LG(${k})`) : { value: "&none" };
  });
  const at = km.layer_names.indexOf("Magic"); // insert just below Magic
  for (const l of km.layers) for (const b of l) {
    const p = b.params?.[0];
    if (LAYER_BEHAVIORS.has(b.value) && p && typeof p.value === "number" && p.value >= at) p.value++;
  }
  km.layers.splice(at, 0, layer);
  km.layer_names.splice(at, 0, "Omarchy");
  base[56] = toJson("&thumb LAYER_Omarchy INSERT"); // RH T2: hold Omarchy (was Lower; Lower stays on LH T2)
}
fs.writeFileSync("keymap.json", JSON.stringify(km, null, 2) + "\n");

// Config defines: Linux default (keep), bilateral enforcement off.
let erb = fs.readFileSync("keymap.dtsi.erb", "utf8");
const bil = "#define ENFORCE_BILATERAL    // cancels";
if (!erb.includes(bil)) throw new Error("ENFORCE_BILATERAL line not found");
erb = erb.replace(bil, "//#define ENFORCE_BILATERAL  // cancels");
fs.writeFileSync("keymap.dtsi.erb", erb);

// Cursor RGB: highlight the HJKL arrows (row 3 left as Sunaku's).
let dev = fs.readFileSync("device.dtsi", "utf8");
const cur = /(      Cursor \{\n        bindings = <\n(?:.*\n){2})(.*\n)(.*\n)/;
const m = cur.exec(dev);
if (!m) throw new Error("Cursor RGB block not found");
dev = dev.replace(cur, m[1] + m[2] +
  "          YLW MAJ CYN GRN YLW GDN                                     WHT WHT WHT WHT GDN YLW\n");

// RGB maps for layers Sunaku's device.dtsi doesn't cover (Magic had System's; Omarchy is ours).
// 80 colours in key-position order (rows of 10, 12, 12, 12, 18, 16).
const RGB_ROWS = [10, 12, 12, 12, 18, 16];
function rgbBlock(name, colors) {
  const c = colors.trim().split(/\s+/);
  if (c.length !== 80) throw new Error(`${name} RGB map: ${c.length} colours`);
  let k = 0;
  const rows = RGB_ROWS.map((n) => "          " + c.slice(k, (k += n)).join(" "));
  return `      #ifdef LAYER_${name}\n      ${name} {\n        bindings = <\n${rows.join("\n")}\n        >;\n` +
    `        layer-id = <LAYER_${name}>;\n      };\n      #endif\n\n`;
}
const OMARCHY_RGB = `
  WHT WHT RED ORN ORN ORN ORN ___ WHT WHT
  YLW CYN CYN CYN CYN CYN CYN CYN CYN CYN CYN YLW
  CYN RED RED ___ ___ PUR PUR PUR PUR PUR PUR ___
  YLW ___ PUR ___ PUR PUR WHT WHT WHT WHT ___ ___
  GRN ___ GDN GDN GDN ___ GRN GRN GRN ___ ___ BLU ___ ___ AZU ___ AZU GRN
  ___ ___ ___ ___ ___ BLU BLU BLU ___ ___ ___ ___ ___ ___ ___ ___`;
const MAGIC_RGB = `
  RED ___ ___ ___ ___ ___ ___ ___ ___ RED
  ___ ___ ___ WHT ___ ___ ___ ___ ___ ___ ___ ___
  ___ MAJ CYN YLW WHT GRN ORN ORN ORN ORN ORN ORN
  RED MAJ CYN YLW WHT GRN RED RED RED RED ___ RED
  RED ___ ___ ___ ___ ___ BLU BLU ___ ___ ___ ___ ___ ___ ___ ___ ___ RED
  ___ WHT ___ ___ ___ BLU BLU CYN ___ ___ ___ ___ ___ ___ WHT ___`;
const anchor = "      #ifdef LAYER_Factory";
if (!dev.includes(anchor)) throw new Error("Factory RGB block (insert anchor) not found");
dev = dev.replace(anchor, rgbBlock("Omarchy", OMARCHY_RGB) + rgbBlock("Magic", MAGIC_RGB) + anchor);
fs.writeFileSync("device.dtsi", dev);
console.log("layers:", km.layer_names.join(", "));
