#!/usr/bin/env node
// Extract layers from config/glove80.keymap into trainer/keymap.json.
// Usage: node trainer/tools/parse-keymap.mjs [keymap] [out]
import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const keymapPath = resolve(process.argv[2] ?? `${here}/../../config/glove80.keymap`);
const outPath = resolve(process.argv[3] ?? `${here}/../keymap.json`);

// Glove80 physical rows, in binding order (80 keys).
const ROW_SIZES = [10, 12, 12, 12, 18, 16];

const src = readFileSync(keymapPath, "utf8")
  .replace(/\/\*[\s\S]*?\*\//g, "")
  .replace(/\/\/.*$/gm, "");

const keymapStart = src.search(/keymap\s*\{\s*compatible\s*=\s*"zmk,keymap"/);
if (keymapStart < 0) throw new Error("no zmk,keymap node found");
const body = src.slice(keymapStart);

const layers = [];
const layerRe = /layer_(\w+)\s*\{([\s\S]*?)bindings\s*=\s*<([\s\S]*?)>\s*;/g;
for (const m of body.matchAll(layerRe)) {
  const [, id, head, raw] = m;
  const display = /display-name\s*=\s*"([^"]*)"/.exec(head)?.[1] ?? id;
  const bindings = raw
    .split(/(?=&)/)
    .map((s) => s.replace(/\s+/g, " ").trim())
    .filter(Boolean);
  if (bindings.length !== 80) {
    console.warn(`layer ${id}: ${bindings.length} bindings (expected 80), skipped`);
    continue;
  }
  const rows = [];
  let i = 0;
  for (const n of ROW_SIZES) { rows.push(bindings.slice(i, i + n)); i += n; }
  layers.push({ id, name: display, index: layers.length, rows, keys: bindings.map(describe) });
}

// Turn a raw binding into something the game can show and check.
function describe(b) {
  const [beh, ...rest] = b.split(" ");
  const arg = rest.join(" ");
  switch (beh) {
    case "&trans": return { kind: "trans" };
    case "&none": return { kind: "none" };
    case "&kp": return { kind: "key", key: arg };
    case "&mo": case "&to": case "&tog": case "&sl":
      return { kind: "layer", action: beh.slice(1), layer: arg.replace(/^LAYER_/, "") };
    case "&thumb": case "&space": {
      const [layer, tap] = rest;
      return { kind: "layer-tap", layer: layer.replace(/^LAYER_/, ""), tap };
    }
    default: {
      const hrm = /^\((\w+),/.exec(arg);
      if (hrm) return { kind: "mod-tap", behavior: beh.slice(1), tap: hrm[1] };
      return { kind: "other", raw: b };
    }
  }
}

writeFileSync(outPath, JSON.stringify({ source: "config/glove80.keymap", rowSizes: ROW_SIZES, layers }, null, 1));
console.log(`${layers.length} layers -> ${outPath}`);
for (const l of layers) {
  const used = l.keys.filter((k) => k.kind !== "trans" && k.kind !== "none").length;
  console.log(`  ${l.index.toString().padStart(2)} ${l.id.padEnd(12)} ${used} bound`);
}
