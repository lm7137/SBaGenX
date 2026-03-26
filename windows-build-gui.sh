#!/bin/bash

# SBaGenX GUI Windows build script
# Builds the Tauri GUI on a Windows host and stages the installer artifacts.

. ./lib.sh

section_header "Building SBaGenX GUI for Windows..."

HOST_UNAME="$(uname -s)"
case "$HOST_UNAME" in
    MINGW*|MSYS*|CYGWIN*)
        ;;
    *)
        error "windows-build-gui.sh must be run on a Windows host (Git Bash/MSYS2/Cygwin)."
        info "Use this script on Windows after checking out branch gui-v3.0."
        exit 1
        ;;
esac

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"
GUI_DIR="$REPO_ROOT/gui"
DIST_DIR="$REPO_ROOT/dist"
GUI_DIST_DIR="$DIST_DIR/gui"
GUI_BUNDLE_DIR="$GUI_DIR/src-tauri/target/release/bundle"

create_dir_if_not_exists "$DIST_DIR"
create_dir_if_not_exists "$GUI_DIST_DIR"

load_node_env() {
    if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
        return 0
    fi

    if [ -z "$NVM_DIR" ] && [ -d "$HOME/.nvm" ]; then
        export NVM_DIR="$HOME/.nvm"
    fi
    if [ -n "$NVM_DIR" ] && [ -s "$NVM_DIR/nvm.sh" ]; then
        . "$NVM_DIR/nvm.sh"
        if command -v nvm >/dev/null 2>&1; then
            nvm use --lts >/dev/null 2>&1 || true
        fi
    fi
}

load_rust_env() {
    if command -v cargo >/dev/null 2>&1 && command -v rustc >/dev/null 2>&1; then
        return 0
    fi

    if [ -f "$HOME/.cargo/env" ]; then
        . "$HOME/.cargo/env"
    fi
}

load_node_env
load_rust_env

check_required_tools node npm cargo rustc

info "Node: $(node -v)"
info "npm: $(npm -v)"
info "rustc: $(rustc --version)"
info "cargo: $(cargo --version)"

section_header "Ensuring GUI npm dependencies are installed..."
if [ ! -d "$GUI_DIR/node_modules" ]; then
    info "Installing GUI npm dependencies..."
    (cd "$GUI_DIR" && npm install)
    check_error "Failed to install GUI npm dependencies"
fi

section_header "Building Windows sbagenxlib runtime prerequisites..."
if ! (cd "$REPO_ROOT" && SBAGENX_REQUIRE_PY_RUNTIME=0 ./windows-build-sbagenx.sh); then
    error "Failed to build Windows sbagenxlib prerequisites for the GUI."
    exit 1
fi

NODE_ARCH="$(node -p "process.arch")"
case "$NODE_ARCH" in
    x64|arm64)
        WIN_SUFFIX="win64"
        ;;
    ia32|x86)
        WIN_SUFFIX="win32"
        ;;
    *)
        error "Unsupported Node architecture for Windows GUI build: $NODE_ARCH"
        exit 1
        ;;
esac
info "Packaging native GUI for Windows architecture: ${NODE_ARCH} (${WIN_SUFFIX})"

REQUIRED_GUI_RUNTIMES=(
    "sbagenxlib-${WIN_SUFFIX}.dll"
    "libsndfile-${WIN_SUFFIX}.dll"
    "libmp3lame-${WIN_SUFFIX}.dll"
    "libogg-0-${WIN_SUFFIX}.dll"
    "libvorbis-0-${WIN_SUFFIX}.dll"
    "libvorbisenc-2-${WIN_SUFFIX}.dll"
    "libFLAC-${WIN_SUFFIX}.dll"
    "libmpg123-0-${WIN_SUFFIX}.dll"
    "libopus-0-${WIN_SUFFIX}.dll"
    "libwinpthread-1-${WIN_SUFFIX}.dll"
)

section_header "Checking staged GUI runtime DLL prerequisites..."
for runtime in "${REQUIRED_GUI_RUNTIMES[@]}"; do
    if [ ! -f "$DIST_DIR/$runtime" ]; then
        error "Required GUI runtime dependency is missing: dist/$runtime"
        exit 1
    fi
done
success "Required GUI runtime DLLs are present."

section_header "Cleaning previous GUI bundle outputs..."
rm -rf "$GUI_BUNDLE_DIR"

section_header "Typechecking GUI..."
(cd "$GUI_DIR" && npm run check)
check_error "GUI typecheck failed"

section_header "Building Windows GUI bundle..."
(cd "$GUI_DIR" && npm run tauri:build)
check_error "Windows GUI bundle build failed"

section_header "Collecting Windows GUI artifacts..."
mapfile -t NSIS_ARTIFACTS < <(find "$GUI_BUNDLE_DIR" -type f -path "*/nsis/*.exe" | sort)
mapfile -t MSI_ARTIFACTS < <(find "$GUI_BUNDLE_DIR" -type f -path "*/msi/*.msi" | sort)

GUI_PORTABLE_EXE="$GUI_DIR/src-tauri/target/release/sbagenx-gui.exe"

if [ "${#NSIS_ARTIFACTS[@]}" -eq 0 ] && [ "${#MSI_ARTIFACTS[@]}" -eq 0 ]; then
    error "No Windows GUI installer artifacts were produced under $GUI_BUNDLE_DIR"
    exit 1
fi

rm -f "$GUI_DIST_DIR"/sbagenx-gui-windows-setup.exe
rm -f "$GUI_DIST_DIR"/sbagenx-gui-windows.msi
rm -f "$GUI_DIST_DIR"/sbagenx-gui-"${WIN_SUFFIX}".exe

if [ "${#NSIS_ARTIFACTS[@]}" -gt 0 ]; then
    cp "${NSIS_ARTIFACTS[0]}" "$GUI_DIST_DIR/sbagenx-gui-windows-setup.exe"
    check_error "Failed to stage NSIS GUI installer"
    success "Staged NSIS installer: dist/gui/sbagenx-gui-windows-setup.exe"
fi

if [ "${#MSI_ARTIFACTS[@]}" -gt 0 ]; then
    cp "${MSI_ARTIFACTS[0]}" "$GUI_DIST_DIR/sbagenx-gui-windows.msi"
    check_error "Failed to stage MSI GUI installer"
    success "Staged MSI installer: dist/gui/sbagenx-gui-windows.msi"
fi

if [ -f "$GUI_PORTABLE_EXE" ]; then
    cp "$GUI_PORTABLE_EXE" "$GUI_DIST_DIR/sbagenx-gui-${WIN_SUFFIX}.exe"
    check_error "Failed to stage portable GUI executable"
    success "Staged portable GUI executable: dist/gui/sbagenx-gui-${WIN_SUFFIX}.exe"
fi

section_header "Windows GUI build completed."
info "Artifacts:"
if [ -f "$GUI_DIST_DIR/sbagenx-gui-windows-setup.exe" ]; then
    info "  dist/gui/sbagenx-gui-windows-setup.exe"
fi
if [ -f "$GUI_DIST_DIR/sbagenx-gui-windows.msi" ]; then
    info "  dist/gui/sbagenx-gui-windows.msi"
fi
if [ -f "$GUI_DIST_DIR/sbagenx-gui-${WIN_SUFFIX}.exe" ]; then
    info "  dist/gui/sbagenx-gui-${WIN_SUFFIX}.exe"
fi
