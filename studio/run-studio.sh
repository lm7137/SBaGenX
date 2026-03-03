#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAMP_FILE="$ROOT_DIR/studio/.build-studio.stamp"
THEIA_DIR="$ROOT_DIR/studio/theia-ide"
TARGET_EXT_DIR="$THEIA_DIR/theia-extensions/sbagenx-studio"
needs_rebuild=0

is_newer_than_stamp() {
  local path="$1"
  [[ -e "$path" ]] && [[ ! -e "$STAMP_FILE" || "$path" -nt "$STAMP_FILE" ]]
}

if [[ ! -e "$STAMP_FILE" || ! -d "$TARGET_EXT_DIR/lib" ]]; then
  needs_rebuild=1
fi

while IFS= read -r -d '' path; do
  if is_newer_than_stamp "$path"; then
    needs_rebuild=1
    break
  fi
done < <(find "$ROOT_DIR/studio/overlay/theia-extensions/sbagenx-studio" -type f -print0)

if [[ "$needs_rebuild" -eq 0 ]]; then
  while IFS= read -r -d '' path; do
    if is_newer_than_stamp "$path"; then
      needs_rebuild=1
      break
    fi
  done < <(find "$ROOT_DIR/studio" -maxdepth 1 -type f \( -name 'run-studio.sh' -o -name 'build-studio.sh' -o -name 'apply-theia-overlay.sh' \) -print0)
fi

cd "$ROOT_DIR"
if [[ "$needs_rebuild" -eq 1 ]]; then
  ./studio/build-studio.sh
else
  ./studio/apply-theia-overlay.sh
fi
cd "$THEIA_DIR"
unset ELECTRON_RUN_AS_NODE || true
export NODE_OPTIONS="${NODE_OPTIONS:---max-old-space-size=4096}"

ARGS=("$@")
have_workspace=0
have_disable_gpu=0

for arg in "${ARGS[@]}"; do
  case "$arg" in
    --disable-gpu|--disable-gpu=*)
      have_disable_gpu=1
      ;;
    -*)
      ;;
    *)
      have_workspace=1
      ;;
  esac
done

if [ "$have_disable_gpu" -eq 0 ]; then
  ARGS=(--disable-gpu "${ARGS[@]}")
fi

if [ "$have_workspace" -eq 0 ]; then
  ARGS+=("$ROOT_DIR")
fi

yarn electron start -- "${ARGS[@]}"
