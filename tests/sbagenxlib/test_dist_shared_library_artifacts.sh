#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../.."

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

require_file() {
  [ -f "$1" ] || fail "missing file: $1"
}

require_link() {
  [ -L "$1" ] || fail "missing symlink: $1"
}

require_file "dist/libsbagenx-linux64.a"
require_file "dist/libsbagenx.so.3.0.0-beta.3"
require_link "dist/libsbagenx.so.3"
require_link "dist/libsbagenx.so"
require_file "dist/include/sbagenxlib.h"
require_file "dist/include/sbagenlib.h"
require_file "dist/pkgconfig/sbagenxlib.pc"

TARGET_MAJOR="$(readlink dist/libsbagenx.so.3)"
TARGET_BASE="$(readlink dist/libsbagenx.so)"
[ "$TARGET_MAJOR" = "libsbagenx.so.3.0.0-beta.3" ] || fail "unexpected soname target: $TARGET_MAJOR"
[ "$TARGET_BASE" = "libsbagenx.so.3" ] || fail "unexpected base symlink target: $TARGET_BASE"

grep -q '^Version: 3.0.0-beta.3$' dist/pkgconfig/sbagenxlib.pc || fail "pkg-config version mismatch"
grep -q '^Libs: -L${libdir} -lsbagenx -lm$' dist/pkgconfig/sbagenxlib.pc || fail "pkg-config libs mismatch"

echo "PASS: shared library dist artifacts present"
