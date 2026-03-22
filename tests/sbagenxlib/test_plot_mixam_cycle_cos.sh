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
TMP_SAMPLES="/tmp/sbx_mixam_cycle_cos_$$.dat"
TMP_DESC="/tmp/sbx_mixam_cycle_cos_$$.meta"
trap 'rm -f "$TMP_PNG" "$TMP_SAMPLES" "$TMP_DESC"' EXIT

cat >"$TMP_SAMPLES" <<'EOF_SAMPLES'
0.00 1.00 1.00
0.10 0.90 0.945
0.20 0.65 0.8075
0.30 0.35 0.6425
0.40 0.10 0.505
0.50 0.00 0.450
0.60 0.10 0.505
0.70 0.35 0.6425
0.80 0.65 0.8075
0.90 0.90 0.945
1.00 1.00 1.00
EOF_SAMPLES

cat >"$TMP_DESC" <<'EOF_DESC'
title=MIXAM SINGLE-CYCLE CONTINUOUS-AM PLOT
x_label=CYCLE
top_y_label=ENVELOPE
bottom_y_label=GAIN
x_min=0
x_max=1
top_y_min=0
top_y_max=1
bottom_y_min=0
bottom_y_max=1
line1=H:m=cos s=0.0000 f=0.450
line2=mixam cycle gain = f + (1-f)*envelope
x_ticks=0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1
top_y_ticks=1,0.9,0.8,0.7,0.6,0.5,0.4,0.3,0.2,0.1,0
bottom_y_ticks=1,0.9,0.8,0.7,0.6,0.5,0.4,0.3,0.2,0.1,0
EOF_DESC

python3 "$PLOT_SCRIPT" mixam-cycle \
  --out "$TMP_PNG" \
  --sample-file "$TMP_SAMPLES" \
  --desc-file "$TMP_DESC"

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
