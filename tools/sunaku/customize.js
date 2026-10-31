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
  // System's unique keys, moved onto Magic's empty right-hand slots.
  Magic: {
    28: "&kp K_APP", 29: "&kp PSCRN", 30: "&kp PAUSE_BREAK", 31: "&kp SLCK", 32: "&kp KP_NUM", 33: "&kp CAPS",
    40: "&kp _LOCK", 41: "&kp _SLEEP", 42: "&kp _POWER", 43: "&linux_magic_sysrq",
  },
};

// "&kp LC(F19)" -> Layout Editor JSON object
function toJson(s) {
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
fs.writeFileSync("device.dtsi", dev);
console.log("layers:", km.layer_names.join(", "));
