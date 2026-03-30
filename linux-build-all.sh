#!/bin/bash

set -euo pipefail

. ./lib.sh

section_header "Building unified SBaGenX Linux package (CLI + GUI)..."

check_required_tools bash cargo npm

section_header "Building CLI/runtime..."
bash ./linux-build-sbagenx.sh

section_header "Building GUI..."
(
  cd gui
  npm run build
  npm run prepare:runtime
  cargo build --release --manifest-path src-tauri/Cargo.toml
)

section_header "Packaging unified Debian installer..."
bash ./linux-create-deb.sh

section_header "Unified Linux build completed."
