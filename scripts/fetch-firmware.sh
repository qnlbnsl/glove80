#!/usr/bin/env bash
# Download a CI build and lay it out as firmware/current/ with one plain name per device.
#   scripts/fetch-firmware.sh            latest successful run on the current branch
#   scripts/fetch-firmware.sh <run-id>   a specific run
# The previous firmware/current/ moves to firmware/archive/<date>-<run>/.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

branch=$(git branch --show-current)
run=${1:-$(gh run list --branch "$branch" --status success -L 1 --json databaseId --jq '.[0].databaseId')}
[ -n "$run" ] || { echo "no successful run on $branch" >&2; exit 1; }
sha=$(gh run view "$run" --json headSha --jq '.headSha[0:7]')

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
gh run download "$run" -n firmware -D "$tmp"

out=firmware/current
if [ -d "$out" ]; then
    old=$(cat "$out/.run" 2>/dev/null || echo unknown)
    mkdir -p firmware/archive
    mv "$out" "firmware/archive/$(date +%Y%m%d-%H%M)-run-$old"
fi
mkdir -p "$out/reset" "$out/debug"

# CI artifact name -> local name
map=(
    "glove80_dongle prospector_adapter-xiao_ble__zmk-zmk.uf2:dongle.uf2"
    "glove80_lh-zmk.uf2:left-half.uf2"
    "glove80_rh-zmk.uf2:right-half.uf2"
    "tps65_pad_lh.uf2:left-pad.uf2"
    "tps65_pad_rh.uf2:right-pad.uf2"
    "settings_reset-glove80_lh-zmk.uf2:reset/left-half-settings-reset.uf2"
    "settings_reset-glove80_rh-zmk.uf2:reset/right-half-settings-reset.uf2"
    "settings_reset-xiao_ble__zmk-zmk.uf2:reset/xiao-settings-reset.uf2"
)
for m in "${map[@]}"; do
    src=${m%%:*}; dst=${m##*:}
    if [ -f "$tmp/$src" ]; then mv "$tmp/$src" "$out/$dst"; else echo "missing in run: $src" >&2; fi
done
mv "$tmp"/logging-*.uf2 "$out/debug/" 2>/dev/null || true
for f in "$tmp"/*.uf2; do [ -e "$f" ] && { echo "unmapped: $(basename "$f") -> debug/"; mv "$f" "$out/debug/"; }; done

echo "$run" > "$out/.run"
cat > "$out/README.md" <<R
# Firmware to flash

From CI run $run, branch \`$branch\`, commit $sha ($(date +%F)).

| Device | Double-tap reset, drive shows as | Copy this file |
|---|---|---|
| Dongle (Prospector screen) | DONGLEBOOT | \`dongle.uf2\` |
| Left keyboard half | GLV80LHBOOT | \`left-half.uf2\` |
| Right keyboard half | GLV80RHBOOT | \`right-half.uf2\` |
| Left trackpad | TPADLHBOOT | \`left-pad.uf2\` |
| Right trackpad | TPADRHBOOT | \`right-pad.uf2\` |

- \`reset/\`: clears pairing on one device (flash it, then the normal file again). The dongle and both
  pads use \`xiao-settings-reset.uf2\`. Resetting pairing on one side means re-pairing it with the dongle.
- \`debug/\`: logging builds, for debugging only. They print over USB and can slow the dongle down.
R
echo "firmware/current/ <- run $run ($sha)"; ls -R "$out"
