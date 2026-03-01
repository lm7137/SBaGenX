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

"$BIN" -D -p drop t5,7,3 00ds+^ >"$tmpdir/drop.txt" 2>&1
"$BIN" -D -p sigmoid t5,7,3 00ds+^ >"$tmpdir/sigmoid.txt" 2>&1

grep -q "over 12 minutes" "$tmpdir/drop.txt" || {
  echo "FAIL: drop summary did not preserve explicit 5+7 minute runtime" >&2
  exit 1
}
grep -q "^300.000000 " "$tmpdir/drop.txt" || {
  echo "FAIL: drop keyframes missing 300-second drop endpoint" >&2
  exit 1
}
grep -q "^720.000000 " "$tmpdir/drop.txt" || {
  echo "FAIL: drop keyframes missing 720-second hold endpoint" >&2
  exit 1
}
grep -q "^900.000000 " "$tmpdir/drop.txt" || {
  echo "FAIL: drop keyframes missing 900-second wake endpoint" >&2
  exit 1
}

grep -q "over 12 minutes" "$tmpdir/sigmoid.txt" || {
  echo "FAIL: sigmoid summary did not preserve explicit 5+7 minute runtime" >&2
  exit 1
}
grep -q "^300.000000 " "$tmpdir/sigmoid.txt" || {
  echo "FAIL: sigmoid keyframes missing 300-second drop endpoint" >&2
  exit 1
}
grep -q "^720.000000 " "$tmpdir/sigmoid.txt" || {
  echo "FAIL: sigmoid keyframes missing 720-second hold endpoint" >&2
  exit 1
}
grep -q "^900.000000 " "$tmpdir/sigmoid.txt" || {
  echo "FAIL: sigmoid keyframes missing 900-second wake endpoint" >&2
  exit 1
}

timeout 5 "$BIN" -q 600 -p slide t0.1 200+10/1 >"$tmpdir/slide.txt" 2>&1 || {
  echo "FAIL: built-in slide session did not terminate automatically" >&2
  exit 1
}

grep -q "enabling -E automatically" "$tmpdir/slide.txt" || {
  echo "FAIL: built-in slide session did not advertise automatic end enable" >&2
  exit 1
}
grep -q "Sequence duration" "$tmpdir/slide.txt" || {
  echo "FAIL: built-in slide session did not report finite duration" >&2
  exit 1
}

echo "PASS: built-in program runtimes honor explicit timing and auto-stop by default"
