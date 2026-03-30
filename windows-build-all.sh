#!/bin/bash

set -euo pipefail

. ./lib.sh

section_header "Building unified SBaGenX Windows installer (CLI + GUI)..."

section_header "Building CLI/runtime..."
bash ./windows-build-sbagenx.sh

section_header "Building GUI..."
bash ./windows-build-gui.sh

section_header "Creating unified installer..."
bash ./windows-create-installer.sh

section_header "Unified Windows build completed."
