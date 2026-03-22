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
    w.writeframes(b"\x00\x00\x00\x00" * 44100)
PY

run_native_i() {
  local name="$1"
  shift
  if ! SBAGENX_SEQ_BACKEND=sbagenxlib "$bin" -D -i "$@" >"$tmpdir/$name.txt" 2>&1; then
    echo "FAIL: native immediate parser rejected documented example: $name" >&2
    cat "$tmpdir/$name.txt" >&2
    exit 1
  fi
  grep -q "# sbagenxlib immediate tones" "$tmpdir/$name.txt"
}

run_native_mix_i() {
  local name="$1"
  shift
  if ! SBAGENX_SEQ_BACKEND=sbagenxlib "$bin" -D -m "$tmpdir/mix.wav" -i "$@" >"$tmpdir/$name.txt" 2>&1; then
    echo "FAIL: native immediate parser rejected documented mixed example: $name" >&2
    cat "$tmpdir/$name.txt" >&2
    exit 1
  fi
  grep -q "# sbagenxlib immediate tones" "$tmpdir/$name.txt"
}

run_native_i basic-noise pink/40 100+1.5/20 200-4/36 400+8/2
run_native_i iso 300@10/20
run_native_i wave square:200+10/20
run_native_i multi sine:200+10/15 square:300@8/10
run_native_i spin spin:300+4.2/80
run_native_i mix-tail 200+10/1 mix/50
run_native_mix_i mixspin mix/80 triangle:mixspin:400+6.0/40
run_native_mix_i mixpulse mix/75 square:mixpulse:8/50
run_native_mix_i mixbeat mix/80 mixbeat:6/45

echo "PASS: documented immediate examples stay on the native sbagenxlib path"
