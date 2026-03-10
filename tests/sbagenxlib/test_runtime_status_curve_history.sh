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

"$BIN" -S -L 0:00:22 -o "$tmpdir/out.wav" -W -p drop 00ls >/dev/null 2>"$tmpdir/err.raw"

python3 - <<'PY' "$tmpdir/err.raw"
import re
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
data = data.replace(b'\x1b[K', b'')

if not re.search(br"00:00:10 [^\r\n]*\n", data):
    print("FAIL: missing 10-second newline status history for curve runtime", file=sys.stderr)
    sys.exit(1)

if not re.search(br"00:00:20 [^\r\n]*\n", data):
    print("FAIL: missing 20-second newline status history for curve runtime", file=sys.stderr)
    sys.exit(1)
PY

echo "PASS: sbagenxlib curve runtime emits 10-second status history"
