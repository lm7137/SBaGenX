#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
./studio/apply-theia-overlay.sh
cd studio/theia-ide
export NODE_OPTIONS="${NODE_OPTIONS:---max-old-space-size=4096}"
yarn build:dev
