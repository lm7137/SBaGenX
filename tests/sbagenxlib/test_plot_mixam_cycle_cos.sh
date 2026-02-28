#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PLOT_SCRIPT="$ROOT_DIR/scripts/sbagenx_plot.py"

if [[ ! -f "$PLOT_SCRIPT" ]]; then
  echo "FAIL: expected plot script at $PLOT_SCRIPT" >&2
  exit 1
fi

if ! python3 - <<'PY' >/dev/null 2>&1
import cairo
PY
then
  echo "SKIP: python3 cairo module not available"
  exit 0
fi

TMP_PNG="/tmp/sbx_mixam_cycle_cos_$$.png"
trap 'rm -f "$TMP_PNG"' EXIT

python3 "$PLOT_SCRIPT" mixam-cycle \
  --out "$TMP_PNG" \
  --h-m cos --h-s 0 --h-d 0.5 --h-a 0.5 --h-r 0.5 --h-e 3 --h-f 0.45

TMP_PNG="$TMP_PNG" python3 - <<'PY'
import os
import sys
import cairo

png = os.environ["TMP_PNG"]
surf = cairo.ImageSurface.create_from_png(png)
w = surf.get_width()
h = surf.get_height()
stride = surf.get_stride()
data = surf.get_data()

ml, mr, mt, mb, gap = 130, 40, 40, 160, 70
panel_h = int((h - mt - mb - gap) / 2.0)
top_y0, top_y1 = mt, mt + panel_h - 1
bot_y0, bot_y1 = mt + panel_h + gap, mt + panel_h + gap + panel_h - 1
x0, x1 = ml, w - mr - 1

def detect_span(y0, y1, color):
    ys = []
    for y in range(y0, y1 + 1):
        row = y * stride
        found = False
        for x in range(x0, x1 + 1):
            i = row + x * 4
            b = data[i]
            g = data[i + 1]
            r = data[i + 2]
            if color == "red":
                hit = (r > 180 and r > g + 35 and r > b + 35)
            else:
                hit = (b > 150 and b > g + 15 and b > r + 35)
            if hit:
                found = True
                break
        if found:
            ys.append(y)
    if not ys:
        return 0
    return max(ys) - min(ys)

red_span = detect_span(top_y0, top_y1, "red")
blue_span = detect_span(bot_y0, bot_y1, "blue")

min_red_span = int(panel_h * 0.60)
min_blue_span = int(panel_h * 0.40)

if red_span < min_red_span:
    print(f"FAIL: red envelope span too small ({red_span} < {min_red_span})", file=sys.stderr)
    sys.exit(1)
if blue_span < min_blue_span:
    print(f"FAIL: blue gain span too small ({blue_span} < {min_blue_span})", file=sys.stderr)
    sys.exit(1)
PY

echo "PASS: mixam-cycle cosine plot regression test"
