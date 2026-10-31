// Step 2: build a Layout-Editor-style glove80.keymap from Sunaku's keymap.zmk (original, from git)
// plus the customized keymap.json / regenerated keymap.dtsi / device.dtsi.
// Usage: node assemble.js <orig keymap.json> <orig keymap.zmk> <new keymap.json> <new keymap.dtsi> <new device.dtsi> <out>
const fs = require("fs");
const [origJsonP, zmkP, newJsonP, dtsiP, devP, outP] = process.argv.slice(2);
const orig = JSON.parse(fs.readFileSync(origJsonP, "utf8"));
const km = JSON.parse(fs.readFileSync(newJsonP, "utf8"));
let zmk = fs.readFileSync(zmkP, "utf8");
const dtsi = fs.readFileSync(dtsiP, "utf8");
const dev = fs.readFileSync(devP, "utf8");

const replaceOnce = (s, from, to, what) => {
  const i = s.indexOf(from);
  if (i < 0 || s.indexOf(from, i + 1) >= 0) throw new Error(`${what}: expected exactly one match`);
  return s.slice(0, i) + to + s.slice(i + from.length);
};

// 1. Embedded custom code.
zmk = replaceOnce(zmk, orig.custom_defined_behaviors, dtsi.replace(/\n$/, ""), "behaviors");
zmk = replaceOnce(zmk, orig.custom_devicetree, dev.replace(/\n$/, ""), "devicetree");

// 2. Layer #defines.
const defRe = /(\/\* Automatically generated layer name defines \*\/\n)((?:#define LAYER_\w+ \d+\n)+)/;
if (!defRe.test(zmk)) throw new Error("layer define block not found");
zmk = zmk.replace(defRe, (_, head) => head + km.layer_names.map((n, i) => `#define LAYER_${n} ${i}\n`).join(""));

// 3. Keymap layers: rebuild from rendered originals, keyed by name.
const kstart = zmk.indexOf("/* Automatically generated keymap */");
if (kstart < 0) throw new Error("keymap section not found");
let head = zmk.slice(0, kstart), body = zmk.slice(kstart);
const nodeRe = /\n( *)layer_(\w+) \{\n\s*bindings = <\n([\s\S]*?)\n\s*>;\n\s*\};/g;
const rendered = {};
for (const m of body.matchAll(nodeRe)) {
  rendered[m[2]] = m[3].split(/(?=&)/).map((s) => s.replace(/\s+/g, " ").trim()).filter(Boolean);
}
const oldIndex = Object.fromEntries(orig.layer_names.map((n, i) => [n, i]));
const newIndex = Object.fromEntries(km.layer_names.map((n, i) => [n, i]));
const ROWS = [10, 12, 12, 12, 18, 16];
// Names shown on the Prospector display (default: layer name, "_" -> " ").
const DISPLAY_NAMES = { QWERTY: "Base" };

// Render a JSON binding the simple way, for positions we overrode.
const p = (x) => x.value + (x.params && x.params.length ? "(" + x.params.map(p).join(",") + ")" : "");
const renderJson = (b) => b.value === "Custom" ? b.params[0].value
  : [b.value, ...(b.params || []).map(p)].join(" ");

// HRM layers use "&mo LAY_<key>" for base thumb keys; a key we turned into a plain key has no LAY_.
const definedLay = new Set([...dtsi.matchAll(/#define (LAY_\w+)/g)].map((m) => m[1]));

function remapToken(t) { // numeric layer refs, e.g. "&tog 28"
  const lay = /^&mo (LAY_(\w+))$/.exec(t);
  if (lay && !definedLay.has(lay[1])) return `&kp KEY_${lay[2]}`;
  const m = /^&(tog|mo|to|sl) (\d+)$/.exec(t);
  if (!m) return t;
  const name = orig.layer_names[+m[2]];
  return name in newIndex ? `&${m[1]} ${newIndex[name]}` : "&none";
}

const origJsonLayers = Object.fromEntries(orig.layer_names.map((n, i) => [n, orig.layers[i]]));
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
let firstIndent = null;
body = body.replace(nodeRe, (all, indent, name) => {
  if (!(name in newIndex)) return ""; // dropped layer
  firstIndent ??= indent;
  const toks = rendered[name].map((t, i) => {
    const nb = km.layers[newIndex[name]][i], ob = origJsonLayers[name][i];
    // Changed in JSON (our override, or a dropped-layer remap) -> re-render; else keep LE rendering.
    const layerOf = (b, names) => typeof b?.params?.[0]?.value === "number" ? names[b.params[0].value] : undefined;
    if (!same(nb, ob) && nb.value === ob.value && layerOf(ob, orig.layer_names) !== undefined
        && layerOf(ob, orig.layer_names) === layerOf(nb, km.layer_names)) return t; // same layer, renumbered
    if (!same(nb, ob)) return nb.value === "&none" ? "&none" : renderJson(nb);
    return remapToken(t);
  });
  if (toks.length !== 80) throw new Error(`${name}: ${toks.length} bindings`);
  let k = 0;
  const rows = ROWS.map((n) => indent + "    " + toks.slice(k, (k += n)).join("  "));
  const display = (DISPLAY_NAMES[name] ?? name.replace(/_/g, " "));
  return `\n${indent}layer_${name} {\n${indent}    display-name = "${display}";\n${indent}    bindings = <\n${rows.join("\n")}\n${indent}    >;\n${indent}};`;
});
fs.writeFileSync(outP, head + body);
console.log("wrote", outP);
