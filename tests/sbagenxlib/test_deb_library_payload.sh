#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../.."

RAW_VERSION="$(cat VERSION)"
DEB_VERSION="$(printf '%s' "$RAW_VERSION" | sed 's/[^0-9A-Za-z.+:~_-]/-/g' | sed 's/-dev\\./~dev./g; s/-alpha\\./~alpha./g; s/-beta\\./~beta./g; s/-rc\\./~rc./g; s/-/~/g')"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

DEB="${1:-dist/sbagenx_${DEB_VERSION}-1_amd64.deb}"
[ -f "$DEB" ] || fail "missing deb: $DEB"

LISTING="$(dpkg-deb -c "$DEB")"

grep -q './usr/bin/sbagenx$' <<<"$LISTING" || fail "missing packaged binary"
grep -q './usr/include/sbagenxlib.h$' <<<"$LISTING" || fail "missing packaged sbagenxlib.h"
grep -q './usr/include/sbagenlib.h$' <<<"$LISTING" || fail "missing packaged sbagenlib.h"
grep -q './usr/lib/x86_64-linux-gnu/libsbagenx.a$' <<<"$LISTING" || fail "missing packaged static archive"
grep -q "./usr/lib/x86_64-linux-gnu/libsbagenx.so.${RAW_VERSION}\$" <<<"$LISTING" || fail "missing packaged shared library"
grep -q './usr/lib/x86_64-linux-gnu/libsbagenx.so -> libsbagenx.so.3$' <<<"$LISTING" || fail "missing packaged so symlink"
grep -q "./usr/lib/x86_64-linux-gnu/libsbagenx.so.3 -> libsbagenx.so.${RAW_VERSION}\$" <<<"$LISTING" || fail "missing packaged soname symlink"
grep -q './usr/lib/x86_64-linux-gnu/pkgconfig/sbagenxlib.pc$' <<<"$LISTING" || fail "missing packaged pkg-config file"
grep -q './usr/lib/sbagenx/libsndfile.so.1$' <<<"$LISTING" || fail "missing packaged libsndfile runtime"
grep -q './usr/lib/sbagenx/libmp3lame.so.0$' <<<"$LISTING" || fail "missing packaged libmp3lame runtime"
grep -q './usr/lib/sbagenx/libFLAC.so.12$' <<<"$LISTING" || fail "missing packaged libFLAC runtime"
grep -q './usr/lib/sbagenx/libmpg123.so.0$' <<<"$LISTING" || fail "missing packaged libmpg123 runtime"
grep -q './usr/lib/sbagenx/libogg.so.0$' <<<"$LISTING" || fail "missing packaged libogg runtime"
grep -q './usr/lib/sbagenx/libvorbis.so.0$' <<<"$LISTING" || fail "missing packaged libvorbis runtime"
grep -q './usr/lib/sbagenx/libvorbisenc.so.2$' <<<"$LISTING" || fail "missing packaged libvorbisenc runtime"
grep -q './usr/lib/sbagenx/libopus.so.0$' <<<"$LISTING" || fail "missing packaged libopus runtime"

echo "PASS: deb includes sbagenxlib runtime/development payload and bundled codec runtimes"
