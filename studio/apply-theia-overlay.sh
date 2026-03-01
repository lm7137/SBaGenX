#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THEIA_DIR="$ROOT_DIR/studio/theia-ide"
OVERLAY_EXT_DIR="$ROOT_DIR/studio/overlay/theia-extensions/sbagenx-studio"
TARGET_EXT_DIR="$THEIA_DIR/theia-extensions/sbagenx-studio"
ELECTRON_PKG="$THEIA_DIR/applications/electron/package.json"

if [[ ! -d "$THEIA_DIR" ]]; then
  echo "Missing Theia submodule at $THEIA_DIR" >&2
  exit 1
fi

if [[ ! -d "$OVERLAY_EXT_DIR" ]]; then
  echo "Missing Studio overlay at $OVERLAY_EXT_DIR" >&2
  exit 1
fi

rm -rf "$TARGET_EXT_DIR"
mkdir -p "$(dirname "$TARGET_EXT_DIR")"
rsync -a --delete "$OVERLAY_EXT_DIR"/ "$TARGET_EXT_DIR"/

node - "$ELECTRON_PKG" <<'NODE'
const fs = require('fs');
const path = process.argv[2];
const pkg = JSON.parse(fs.readFileSync(path, 'utf8'));
pkg.description = 'SBaGenX Studio desktop IDE';
pkg.productName = 'SBaGenX Studio';
pkg.theia = pkg.theia || {};
pkg.theia.frontend = pkg.theia.frontend || {};
pkg.theia.frontend.config = pkg.theia.frontend.config || {};
pkg.theia.frontend.config.applicationName = 'SBaGenX Studio';
pkg.theia.backend = pkg.theia.backend || {};
pkg.theia.backend.config = pkg.theia.backend.config || {};
pkg.theia.backend.config.configurationFolder = '.sbagenx-studio';
pkg.dependencies = pkg.dependencies || {};
pkg.dependencies['theia-ide-sbagenx-studio-ext'] = pkg.version || '1.69.0';
fs.writeFileSync(path, JSON.stringify(pkg, null, 2) + '\n');
NODE

echo "Applied SBaGenX Studio overlay to $THEIA_DIR"
