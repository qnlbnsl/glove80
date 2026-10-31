#!/usr/bin/env bash
# Regenerate config/glove80.keymap from Sunaku's glove80-keymaps plus our customizations.
#
#   tools/sunaku/sync.sh [sunaku-ref]     (default: the ref pinned in tools/sunaku/REF)
#
# Our changes live in customize.js (dropped layers, key overrides, config defines, RGB tweaks)
# and assemble.js (display names). Needs: git, node, ruby with the rake + erb gems
# (gem install --user-install rake erb).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
ref=${1:-$(cat "$here/REF")}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
git clone -q https://github.com/sunaku/glove80-keymaps "$work/sunaku"
cd "$work/sunaku"
git checkout -q "$ref"
git show HEAD:keymap.json > "$work/orig.json"
git show HEAD:keymap.zmk > "$work/orig.zmk"

node "$here/customize.js"
PATH="$(ruby -e 'print Gem.user_dir')/bin:$PATH" rake keymap.dtsi
node "$here/assemble.js" "$work/orig.json" "$work/orig.zmk" keymap.json keymap.dtsi device.dtsi \
  "$repo/config/glove80.keymap"

git rev-parse HEAD > "$here/REF"
echo "Built from sunaku/glove80-keymaps $(git log -1 --format='%h %ad %s' --date=short)"
