#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
GUI_DIR="$ROOT_DIR/gui"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

if ! command -v npm >/dev/null 2>&1; then
  echo "SKIP: npm not installed"
  exit 0
fi

bash "$ROOT_DIR/linux-build-sbagenx.sh" >/dev/null
(
  cd "$GUI_DIR"
  npm run prepare:runtime >/dev/null
)

cargo run --manifest-path "$ROOT_DIR/tests/gui/runtime_bundle_smoke/Cargo.toml" \
  >/tmp/sbagenx_gui_runtime_bundle_smoke_$$.out \
  || {
    cat /tmp/sbagenx_gui_runtime_bundle_smoke_$$.out >&2 || true
    fail "runtime bundle cargo smoke test failed"
  }

rm -f /tmp/sbagenx_gui_runtime_bundle_smoke_$$.out

echo "PASS: GUI runtime bundle smoke test"
