#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAMP_FILE="$ROOT_DIR/studio/.build-studio.stamp"
THEIA_DIR="$ROOT_DIR/studio/theia-ide"

if [[ ! -f "$THEIA_DIR/package.json" ]]; then
  git -C "$ROOT_DIR" submodule update --init --recursive studio/theia-ide
fi

cd "$ROOT_DIR"
./studio/apply-theia-overlay.sh
cd "$THEIA_DIR"
export NODE_OPTIONS="${NODE_OPTIONS:---max-old-space-size=4096}"
yarn build:dev
mkdir -p "$(dirname "$STAMP_FILE")"
touch "$STAMP_FILE"
