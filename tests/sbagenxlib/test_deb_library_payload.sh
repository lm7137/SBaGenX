#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../.."

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

DEB="${1:-dist/sbagenx_3.0.0~beta.3-1_amd64.deb}"
[ -f "$DEB" ] || fail "missing deb: $DEB"

LISTING="$(dpkg-deb -c "$DEB")"

echo "$LISTING" | grep -q './usr/bin/sbagenx$' || fail "missing packaged binary"
echo "$LISTING" | grep -q './usr/include/sbagenxlib.h$' || fail "missing packaged sbagenxlib.h"
echo "$LISTING" | grep -q './usr/include/sbagenlib.h$' || fail "missing packaged sbagenlib.h"
echo "$LISTING" | grep -q './usr/lib/x86_64-linux-gnu/libsbagenx.a$' || fail "missing packaged static archive"
echo "$LISTING" | grep -q './usr/lib/x86_64-linux-gnu/libsbagenx.so.3.0.0-beta.3$' || fail "missing packaged shared library"
echo "$LISTING" | grep -q './usr/lib/x86_64-linux-gnu/libsbagenx.so -> libsbagenx.so.3$' || fail "missing packaged so symlink"
echo "$LISTING" | grep -q './usr/lib/x86_64-linux-gnu/libsbagenx.so.3 -> libsbagenx.so.3.0.0-beta.3$' || fail "missing packaged soname symlink"
echo "$LISTING" | grep -q './usr/lib/x86_64-linux-gnu/pkgconfig/sbagenxlib.pc$' || fail "missing packaged pkg-config file"

echo "PASS: deb includes sbagenxlib runtime/development payload"
