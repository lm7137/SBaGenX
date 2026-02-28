#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION="$(cat "$ROOT_DIR/VERSION")"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

require_file() {
  [ -f "$1" ] || fail "missing file: $1"
}

require_file "$ROOT_DIR/dist/include/sbagenxlib.h"
require_file "$ROOT_DIR/dist/libsbagenx.so.$VERSION"
require_file "$ROOT_DIR/examples/sbagenxlib/sbx_frontend_smoke.c"

TMP_BIN="/tmp/sbx_frontend_smoke_manual_$$"
TMP_OUT="/tmp/sbx_frontend_smoke_manual_$$.out"
trap 'rm -f "$TMP_BIN" "$TMP_OUT"' EXIT

gcc -O2 -I"$ROOT_DIR/dist/include" "$ROOT_DIR/examples/sbagenxlib/sbx_frontend_smoke.c" \
  -L"$ROOT_DIR/dist" -lsbagenx -lm -o "$TMP_BIN"

LD_LIBRARY_PATH="$ROOT_DIR/dist${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  "$TMP_BIN" >"$TMP_OUT"

grep -q "^api=" "$TMP_OUT" || fail "example output missing api line"
grep -q "fx=1" "$TMP_OUT" || fail "example output missing sampled mix effect count"

echo "PASS: direct shared-library consumer smoke test"
