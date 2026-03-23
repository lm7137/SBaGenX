#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

bin="$repo_root/dist/sbagenx-linux64"
if [ ! -x "$bin" ]; then
  echo "SKIP: $bin not built" >&2
  exit 0
fi

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

python3 - <<'PY' "$tmpdir/mix.wav"
import sys, wave
path = sys.argv[1]
with wave.open(path, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(44100)
    w.writeframes(b"\x00\x00\x00\x00" * 44100 * 5)
PY

seq="$tmpdir/extended_safe.sbg"
cat > "$seq" <<EOF2
-SE
-Q
-Wo $tmpdir/out.wav
-m $tmpdir/mix.wav
-A d=0.2:e=0.4:k=5:E=0.6
-I s=0:d=1:a=0.5:r=0.5:e=3
-H d=0.2:a=0.1:r=0.3:e=2:f=0.25
-b 24
-L 0:00:05
-N
-V 75
-w triangle
-c 200=2
-d default

base: mix/100 200@4/20 mixam:beat
NOW base
+00:00:05 -
EOF2

if ! SBAGENX_SEQ_BACKEND=sbagenxlib "$bin" -D "$seq" >"$tmpdir/debug.txt" 2>&1; then
  echo "FAIL: extended safe preamble did not stay on native path" >&2
  cat "$tmpdir/debug.txt" >&2
  exit 1
fi

grep -q "# sbagenxlib keyframes" "$tmpdir/debug.txt"
grep -q "triangle:200@4/20 linear" "$tmpdir/debug.txt"
grep -q "#   mixfx\\[0\\]\\[0\\] 0.000000 mixam:beat:s=0:d=0.2:a=0.1:r=0.3:e=2:f=0.25 linear" "$tmpdir/debug.txt"

if ! SBAGENX_SEQ_BACKEND=sbagenxlib "$bin" "$seq" >"$tmpdir/render.log" 2>&1; then
  echo "FAIL: extended safe preamble render failed" >&2
  cat "$tmpdir/render.log" >&2
  exit 1
fi

if [ -s "$tmpdir/render.log" ]; then
  echo "FAIL: safe preamble -Q should suppress normal host warnings" >&2
  cat "$tmpdir/render.log" >&2
  exit 1
fi

python3 - <<'PY' "$tmpdir/out.wav"
import sys, wave
path = sys.argv[1]
with wave.open(path, "rb") as w:
    dur = w.getnframes() / w.getframerate()
    if dur < 4.7 or dur > 5.05:
        raise SystemExit(f"unexpected duration {dur}")
    if w.getsampwidth() != 3:
        raise SystemExit(f"unexpected sample width {w.getsampwidth()}")
PY

echo "PASS: extended safe preamble options stay native and affect runtime/output"

seq_debug="$tmpdir/extended_safe_debug.sbg"
cat > "$seq_debug" <<EOF3
-SE
-D
-I s=0:d=1:e=3

base: 200@4/20
NOW base
EOF3

if ! SBAGENX_SEQ_BACKEND=sbagenxlib "$bin" "$seq_debug" >"$tmpdir/debug_preamble.txt" 2>&1; then
  echo "FAIL: safe preamble -D did not stay on native path" >&2
  cat "$tmpdir/debug_preamble.txt" >&2
  exit 1
fi

grep -q "# sbagenxlib keyframes" "$tmpdir/debug_preamble.txt"
grep -q "sine:200@4/20 linear" "$tmpdir/debug_preamble.txt"

echo "PASS: safe preamble -D stays native"

seq_plot="$tmpdir/plot_safe.sbg"
cat > "$seq_plot" <<EOF4
-SE
-P

base: 200@4/20
NOW base
EOF4

(
  cd "$tmpdir"
  if ! SBAGENX_SEQ_BACKEND=sbagenxlib "$bin" "$seq_plot" >"$tmpdir/plot_preamble.txt" 2>&1; then
    echo "FAIL: safe preamble -P did not stay on native path" >&2
    cat "$tmpdir/plot_preamble.txt" >&2
    exit 1
  fi
)

grep -q "Isochronic cycle graph saved to:" "$tmpdir/plot_preamble.txt"
ls "$tmpdir"/iso_cycle_*.png >/dev/null 2>&1

echo "PASS: safe preamble -P stays native"

seq_graph="$tmpdir/graph_safe.sbg"
cat > "$seq_graph" <<EOF5
-SE
-G

base: 200@4/20
NOW base
EOF5

set +e
SBAGENX_SEQ_BACKEND=sbagenxlib "$bin" "$seq_graph" >"$tmpdir/graph_preamble.txt" 2>&1
rc=$?
set -e

if [ "$rc" -eq 0 ]; then
  echo "FAIL: safe preamble -G should reject sequence-mode use explicitly" >&2
  cat "$tmpdir/graph_preamble.txt" >&2
  exit 1
fi

grep -q -- "-G is only supported with -p drop/-p sigmoid/-p curve" "$tmpdir/graph_preamble.txt"

echo "PASS: safe preamble -G now errors explicitly instead of falling back"
