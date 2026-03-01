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

"$BIN" -D -p drop 00a.5s >"$tmpdir/drop_frac.txt" 2>&1
"$BIN" -D -p sigmoid 00-bs >"$tmpdir/sig_neg.txt" 2>&1
"$BIN" -D -p drop 00l.5s >"$tmpdir/drop_tail.txt" 2>&1

grep -q "from 10Hz to 4.05Hz" "$tmpdir/drop_frac.txt" || {
  echo "FAIL: drop did not honor positive fractional target a.5 -> 4.05Hz" >&2
  exit 1
}

grep -q "from 10Hz to 5.1Hz" "$tmpdir/sig_neg.txt" || {
  echo "FAIL: sigmoid did not honor mirrored negative target -b -> 5.1Hz" >&2
  exit 1
}

grep -q "from 10Hz to 0.25Hz" "$tmpdir/drop_tail.txt" || {
  echo "FAIL: drop did not honor tail fractional target l.5 -> 0.25Hz" >&2
  exit 1
}

echo "PASS: built-in program target specs honor fractional and mirrored negative letters"
