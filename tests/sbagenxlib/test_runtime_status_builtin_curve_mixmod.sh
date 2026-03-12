#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${SBAGENX_TEST_BIN:-$ROOT_DIR/dist/sbagenx-linux64}"
MIX_FILE="$ROOT_DIR/assets/river1.ogg"

if [[ ! -x "$BIN" ]]; then
  echo "Missing binary: $BIN" >&2
  echo "Set SBAGENX_TEST_BIN or run: bash linux-build-sbagenx.sh" >&2
  exit 1
fi

if [[ ! -f "$MIX_FILE" ]]; then
  echo "Missing mix file: $MIX_FILE" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

timeout 8 "$BIN" -q 10 -m "$MIX_FILE" -A -o "$tmpdir/out.flac" \
  -p sigmoid t1,1,0 00as+@/3 mix/97 >"$tmpdir/out.txt" 2>"$tmpdir/err.raw" || true

tr '\r' '\n' <"$tmpdir/err.raw" >"$tmpdir/err.txt"

python3 - <<'PY' "$tmpdir/err.txt"
import re
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
vals = [float(m.group(1)) for m in re.finditer(r"mix/([0-9]+(?:\.[0-9]+)?)", data)]
if len(vals) < 3:
    print("FAIL: expected multiple runtime mix amplitude samples with -A enabled", file=sys.stderr)
    sys.exit(1)
if not any(v < vals[0] for v in vals[1:]):
    print("FAIL: runtime-effective mix amplitude did not decay with -A enabled", file=sys.stderr)
    sys.exit(1)
PY

echo "PASS: built-in curve runtime applies and reports -A mix modulation"
