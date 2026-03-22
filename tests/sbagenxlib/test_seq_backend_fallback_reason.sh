#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

bin="${repo_root}/dist/sbagenx-linux64"
if [[ ! -x "$bin" ]]; then
  echo "FAIL: binary not found: $bin" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

out="$tmpdir/out.txt"
seq="$tmpdir/unsupported-safe-preamble.sbg"

cat >"$seq" <<'EOF'
-c 100=0.8
tone: 200+4/20
00:00 tone
EOF

set +e
"$bin" -D "$seq" >"$out" 2>&1
rc=$?
set -e

if [[ $rc -ne 0 ]]; then
  echo "FAIL: expected unsupported safe preamble fixture to continue under legacy fallback" >&2
  cat "$out" >&2
  exit 1
fi

if ! grep -q "Falling back to legacy sequence parser/runtime: safe preamble option -c is not supported by the sbagenxlib bridge" "$out"; then
  echo "FAIL: expected explicit sbagenxlib fallback reason for unsupported safe preamble option" >&2
  cat "$out" >&2
  exit 1
fi

if ! grep -q "\*\*\* Sequence duration: 00:00:00 (hh:mm:ss) \*\*\*" "$out"; then
  echo "FAIL: expected legacy dump to continue after reported fallback" >&2
  cat "$out" >&2
  exit 1
fi

echo "PASS: sequence fallback reports an explicit sbagenxlib rejection reason"
