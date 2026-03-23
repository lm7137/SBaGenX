#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${SBAGENX_TEST_BIN:-$ROOT_DIR/dist/sbagenx-linux64}"

if [[ ! -x "$BIN" ]]; then
  echo "Missing binary: $BIN" >&2
  echo "Set SBAGENX_TEST_BIN or run: bash linux-build-sbagenx.sh" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

"$BIN" -Q -W -o "$tmpdir/plain.wav" -L 00:00:01 -i 80+4/10 >/dev/null 2>&1
"$BIN" -Q -W -o "$tmpdir/adjusted.wav" -L 00:00:01 -c 80=2 -i 80+4/10 >/dev/null 2>&1

python3 - <<'PY' "$tmpdir/plain.wav" "$tmpdir/adjusted.wav"
import array, math, sys, wave

def peak(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            raise SystemExit(f"unexpected sample width for {path}: {w.getsampwidth()}")
        frames = w.readframes(w.getnframes())
    pcm = array.array("h")
    pcm.frombytes(frames)
    return max(abs(v) for v in pcm)

plain = peak(sys.argv[1])
adjusted = peak(sys.argv[2])
if plain <= 0:
    raise SystemExit("plain render peak is zero")
ratio = adjusted / plain
if ratio < 1.8 or ratio > 2.2:
    raise SystemExit(f"unexpected -c amplitude ratio {ratio}")
PY

echo "PASS: native runtime applies -c amplitude adjustment during render"
