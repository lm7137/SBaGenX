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

PKG_CONFIG_BIN=""
if command -v pkg-config >/dev/null 2>&1; then
  PKG_CONFIG_BIN="pkg-config"
elif command -v pkgconf >/dev/null 2>&1; then
  PKG_CONFIG_BIN="pkgconf"
else
  echo "SKIP: pkg-config/pkgconf not installed"
  exit 0
fi

require_file "$ROOT_DIR/dist/pkgconfig/sbagenxlib-uninstalled.pc"
require_file "$ROOT_DIR/dist/include/sbagenxlib.h"
require_file "$ROOT_DIR/dist/libsbagenx.so.$VERSION"
require_file "$ROOT_DIR/examples/sbagenxlib/sbx_frontend_smoke.c"

TMP_BIN="/tmp/sbx_frontend_smoke_$$"
TMP_OUT="/tmp/sbx_frontend_smoke_$$.out"
trap 'rm -f "$TMP_BIN" "$TMP_OUT"' EXIT

PKG_CONFIG_PATH="$ROOT_DIR/dist/pkgconfig" \
  gcc -O2 "$ROOT_DIR/examples/sbagenxlib/sbx_frontend_smoke.c" \
  $(PKG_CONFIG_PATH="$ROOT_DIR/dist/pkgconfig" "$PKG_CONFIG_BIN" --cflags --libs sbagenxlib-uninstalled) \
  -o "$TMP_BIN"

LD_LIBRARY_PATH="$ROOT_DIR/dist${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  "$TMP_BIN" >"$TMP_OUT"

grep -q "^api=" "$TMP_OUT" || fail "example output missing api line"
grep -q "fx=1" "$TMP_OUT" || fail "example output missing sampled mix effect count"

echo "PASS: uninstalled pkg-config shared-consumer smoke test"
