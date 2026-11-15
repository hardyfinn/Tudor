#!/usr/bin/env bash
# Builds docs/Tudor-Manual.pdf from docs/MANUAL.md. Needs pandoc, python3 and Node with Playwright (Chromium).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
cp "$root/scripts/manual/style.css" "$tmp/style.css"
cp -r "$root/docs/img" "$tmp/img"
pandoc "$root/docs/MANUAL.md" -s --metadata title="Tudor for VCV Rack: manual" -c style.css -o "$tmp/manual.html"
python3 - "$tmp/manual.html" <<'PY'
import re, sys
p = sys.argv[1]; h = open(p).read()
h = re.sub(r'<header id="title-block-header">.*?</header>', '', h, flags=re.S)
open(p, 'w').write(h)
PY
node "$root/scripts/manual/topdf.js" "$tmp/manual.html" "$root/docs/Tudor-Manual.pdf"
rm -rf "$tmp"
echo "wrote docs/Tudor-Manual.pdf"
