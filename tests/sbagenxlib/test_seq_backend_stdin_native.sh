#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

bin="${repo_root}/dist/sbagenx-linux64"
if [[ ! -x "$bin" ]]; then
  echo "FAIL: binary not found: $bin" >&2
  exit 1
fi

out="/tmp/sbx_stdin_native_$$.out"
trap 'rm -f "$out"' EXIT

printf 'tone: 200+4/20\n00:00 tone\n' | \
  SBAGENX_SEQ_BACKEND=sbagenxlib "$bin" -D - >"$out"

grep -q "^# sbagenxlib keyframes" "$out" || {
  echo "FAIL: stdin native sequence did not emit sbagenxlib keyframes" >&2
  cat "$out" >&2
  exit 1
}

grep -q "^0.000000 200+4/20 linear$" "$out" || {
  echo "FAIL: stdin native sequence missing expected keyframe content" >&2
  cat "$out" >&2
  exit 1
}

echo "PASS: sbagenxlib direct sequence loading now accepts stdin"
