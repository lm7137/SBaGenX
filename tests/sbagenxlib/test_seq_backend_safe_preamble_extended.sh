#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

bin="./dist/sbagenx-linux64"
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
