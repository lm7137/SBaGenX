#!/bin/bash

# Source common library
. ./lib.sh

SETUP_NAME="sbagenx-windows-setup.exe"
ISCC_URL="https://files.jrsoftware.org/is/6/innosetup-6.4.2.exe"
AUTO_INSTALL_DEPS="${SBAGENX_AUTO_INSTALL_DEPS:-1}"
PKG_MANAGER=""
PKG_RUNNER=""
TEMP_DIR=""
XVFB_PID=""
STARTED_XVFB=0
WINEPREFIX_DEFAULT="/tmp/wineprefix"
WINEPREFIX="${WINEPREFIX:-$WINEPREFIX_DEFAULT}"
DISPLAY="${DISPLAY:-:99}"
HOST_UNAME="$(uname -s)"
ISCC_NATIVE=""

cleanup() {
    if [ -n "$TEMP_DIR" ] && [ -d "$TEMP_DIR" ]; then
        rm -rf "$TEMP_DIR"
    fi
    if [ "$STARTED_XVFB" -eq 1 ] && [ -n "$XVFB_PID" ]; then
        kill "$XVFB_PID" >/dev/null 2>&1 || true
    fi
}

is_windows_host() {
    case "$HOST_UNAME" in
        MINGW*|MSYS*|CYGWIN*)
            return 0
            ;;
    esac
    return 1
}

find_native_iscc() {
    if command -v iscc >/dev/null 2>&1; then
        command -v iscc
        return 0
    fi
    if command -v ISCC.exe >/dev/null 2>&1; then
        command -v ISCC.exe
        return 0
    fi

    local candidate
    for candidate in \
        "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" \
        "/c/Program Files/Inno Setup 6/ISCC.exe"; do
        if [ -f "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

install_innosetup_windows() {
    local found

    found="$(find_native_iscc)"
    if [ -n "$found" ]; then
        ISCC_NATIVE="$found"
        return 0
    fi

    if [ "$AUTO_INSTALL_DEPS" = "0" ]; then
        error "Inno Setup compiler (ISCC.exe) not found and auto-install is disabled (SBAGENX_AUTO_INSTALL_DEPS=0)."
        return 1
    fi

    warning "ISCC.exe not found. Attempting automatic install of Inno Setup 6..."

    # First preference: winget (if available) keeps the script simple on Windows.
    if command -v winget >/dev/null 2>&1; then
        info "Trying winget install JRSoftware.InnoSetup..."
        winget install --id JRSoftware.InnoSetup -e --accept-package-agreements --accept-source-agreements --silent --disable-interactivity >/dev/null 2>&1 || true
        found="$(find_native_iscc)"
        if [ -n "$found" ]; then
            success "Installed Inno Setup using winget."
            ISCC_NATIVE="$found"
            return 0
        fi
        warning "winget install did not make ISCC.exe available; trying direct installer fallback."
    fi

    # Fallback: download and run the official installer silently using PowerShell.
    if command -v powershell.exe >/dev/null 2>&1; then
        info "Trying direct download + silent install of Inno Setup 6..."
        if ISCC_URL_PS="$ISCC_URL" powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
            '& { $ErrorActionPreference = "Stop"; $ProgressPreference = "SilentlyContinue"; $tmp = Join-Path $env:TEMP "innosetup-6.4.2.exe"; Invoke-WebRequest -UseBasicParsing -Uri $env:ISCC_URL_PS -OutFile $tmp; Start-Process -FilePath $tmp -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /NOICONS" -Wait; }'; then
            found="$(find_native_iscc)"
            if [ -n "$found" ]; then
                success "Installed Inno Setup using direct installer fallback."
                ISCC_NATIVE="$found"
                return 0
            fi
        fi
    else
        warning "powershell.exe is not available; cannot run direct installer fallback."
    fi

    error "Inno Setup compiler (ISCC.exe) still not found after auto-install attempts."
    info "Install Inno Setup 6 manually from: https://jrsoftware.org/isdl.php"
    return 1
}

detect_package_manager() {
    if command -v apt-get >/dev/null 2>&1; then
        PKG_MANAGER="apt-get"
    elif command -v dnf >/dev/null 2>&1; then
        PKG_MANAGER="dnf"
    elif command -v pacman >/dev/null 2>&1; then
        PKG_MANAGER="pacman"
    elif command -v zypper >/dev/null 2>&1; then
        PKG_MANAGER="zypper"
    else
        PKG_MANAGER=""
    fi
}

set_pkg_runner() {
    if [ "$(id -u)" -eq 0 ]; then
        PKG_RUNNER=""
        return 0
    fi
    if command -v sudo >/dev/null 2>&1; then
        PKG_RUNNER="sudo"
        return 0
    fi
    return 1
}

append_pkg_unique() {
    local pkg="$1"
    local existing
    for existing in "${INSTALL_PKGS[@]}"; do
        if [ "$existing" = "$pkg" ]; then
            return 0
        fi
    done
    INSTALL_PKGS+=("$pkg")
}

pkg_for_cmd() {
    local cmd="$1"
    case "$cmd" in
        pgrep)
            case "$PKG_MANAGER" in
                apt-get|zypper) echo "procps" ;;
                dnf|pacman) echo "procps-ng" ;;
            esac
            ;;
        Xvfb)
            case "$PKG_MANAGER" in
                apt-get) echo "xvfb" ;;
                dnf|zypper) echo "xorg-x11-server-Xvfb" ;;
                pacman) echo "xorg-server-xvfb" ;;
            esac
            ;;
        wine|wineserver|wineboot)
            case "$PKG_MANAGER" in
                apt-get) echo "wine" ;;
                dnf|pacman|zypper) echo "wine" ;;
            esac
            ;;
        curl) echo "curl" ;;
    esac
}

run_package_install() {
    case "$PKG_MANAGER" in
        apt-get)
            # Keep this non-interactive for CI and headless environments.
            export DEBIAN_FRONTEND=noninteractive
            if printf '%s\n' "${INSTALL_PKGS[@]}" | grep -qx "wine"; then
                if command -v dpkg >/dev/null 2>&1 && ! dpkg --print-foreign-architectures | grep -qx "i386"; then
                    info "Enabling i386 multiarch (required for 32-bit Wine prefix)..."
                    ${PKG_RUNNER:+$PKG_RUNNER }dpkg --add-architecture i386
                fi
                append_pkg_unique "wine32"
                append_pkg_unique "wine64"
            fi
            info "Updating apt package metadata..."
            ${PKG_RUNNER:+$PKG_RUNNER }apt-get update
            info "Installing packages: ${INSTALL_PKGS[*]}"
            ${PKG_RUNNER:+$PKG_RUNNER }apt-get install -y "${INSTALL_PKGS[@]}"
            ;;
        dnf)
            info "Installing packages: ${INSTALL_PKGS[*]}"
            ${PKG_RUNNER:+$PKG_RUNNER }dnf install -y "${INSTALL_PKGS[@]}"
            ;;
        pacman)
            info "Installing packages: ${INSTALL_PKGS[*]}"
            ${PKG_RUNNER:+$PKG_RUNNER }pacman -Sy --noconfirm "${INSTALL_PKGS[@]}"
            ;;
        zypper)
            info "Installing packages: ${INSTALL_PKGS[*]}"
            ${PKG_RUNNER:+$PKG_RUNNER }zypper --non-interactive install "${INSTALL_PKGS[@]}"
            ;;
        *)
            return 1
            ;;
    esac

    return 0
}

ensure_required_commands() {
    local required=("$@")
    local cmd
    local pkg
    local missing=()

    for cmd in "${required[@]}"; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing+=("$cmd")
        fi
    done

    if [ "${#missing[@]}" -eq 0 ]; then
        return 0
    fi

    warning "Missing required tools: ${missing[*]}"

    if [ "$AUTO_INSTALL_DEPS" = "0" ]; then
        error "Automatic dependency install is disabled (SBAGENX_AUTO_INSTALL_DEPS=0)."
        return 1
    fi

    detect_package_manager
    if [ -z "$PKG_MANAGER" ]; then
        error "Could not detect a supported package manager (apt-get/dnf/pacman/zypper)."
        return 1
    fi
    if ! set_pkg_runner; then
        error "Need root privileges or sudo to auto-install dependencies."
        return 1
    fi

    INSTALL_PKGS=()
    for cmd in "${missing[@]}"; do
        pkg=$(pkg_for_cmd "$cmd")
        if [ -z "$pkg" ]; then
            error "No package mapping available for missing tool '$cmd' on $PKG_MANAGER."
            return 1
        fi
        append_pkg_unique "$pkg"
    done

    section_header "Installing missing dependencies..."
    if ! run_package_install; then
        error "Failed to install required dependencies automatically."
        return 1
    fi

    for cmd in "${missing[@]}"; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            error "Tool '$cmd' is still missing after installation."
            return 1
        fi
    done

    success "Dependencies installed successfully."
    return 0
}

trap cleanup EXIT

# Check if Windows executables exist
if [ ! -f dist/sbagenx-win32.exe ] || [ ! -f dist/sbagenx-win64.exe ]; then
    error "Windows executables not found. Run ./windows-build-sbagenx.sh first."
    exit 1
fi

# Remove the existing installer if it exists
rm -f "dist/${SETUP_NAME}"

section_header "Creating Windows Installer..."

# Native Windows/MSYS2 path: use Inno Setup directly, no Wine/Xvfb.
if is_windows_host; then
    if ! install_innosetup_windows || [ -z "$ISCC_NATIVE" ]; then
        exit 1
    fi

    info "Using native Inno Setup compiler: $ISCC_NATIVE"
    info "Creating installer..."
    # Prevent MSYS2 from rewriting ISCC switch arguments like /O+ and /Q.
    if ! MSYS2_ARG_CONV_EXCL='*' "$ISCC_NATIVE" /O+ /Q "setup.iss"; then
        error "Failed to create installer with native Inno Setup"
        exit 1
    fi

    if [ ! -f "dist/${SETUP_NAME}" ]; then
        error "Failed to create installer"
        exit 1
    fi

    success "Installer created successfully at dist/${SETUP_NAME}"
    rm -rf build
    section_header "Build process completed!"
    exit 0
fi

# Check and auto-install dependencies if missing
if ! ensure_required_commands pgrep Xvfb wine wineserver wineboot curl; then
    error "Missing dependencies. Install them manually or rerun with SBAGENX_AUTO_INSTALL_DEPS=1."
    exit 1
fi

# Set up wine environment
export WINEARCH=win32
export WINEPREFIX
export WINEDEBUG=-all
export WINEDLLOVERRIDES="winemenubuilder.exe=d"
export DISPLAY

# Increase Wine memory limits
export WINE_LARGE_ADDRESS_AWARE=1
export WINE_HEAP_MAXRESERVE=4096

# Get Xvfb PID
XVFB_PID=$(pgrep -f "Xvfb ${DISPLAY} " | head -n 1)

# Start Xvfb to provide a virtual display if it's not already running
if [ -z "$XVFB_PID" ]; then
    rm -f /tmp/.X${DISPLAY/:/}-lock
    info "Starting Xvfb for headless Wine operation..."
    Xvfb "$DISPLAY" -screen 0 1024x768x16 &
    XVFB_PID=$!
    STARTED_XVFB=1
    sleep 2  # Wait for Xvfb to start
    if ! kill -0 "$XVFB_PID" >/dev/null 2>&1; then
        error "Failed to start Xvfb on display $DISPLAY"
        exit 1
    fi
fi

# Initialize Wine prefix if it doesn't exist
if [ ! -d "$WINEPREFIX" ]; then
    info "Initializing Wine prefix..."
    if ! wineboot -i > /dev/null 2>&1; then
        error "wineboot failed. Ensure 32-bit Wine support is installed (e.g., wine32 on Debian/Ubuntu)."
        exit 1
    fi
    wineserver -w
fi

# Check if Inno Setup is installed in Wine
ISCC="$WINEPREFIX/drive_c/Program Files/Inno Setup 6/ISCC.exe"
if [ ! -f "$ISCC" ]; then
    info "Inno Setup not found. Downloading and installing..."

    # Create temp directory
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR" || exit 1

    # Download Inno Setup
    if ! curl -fsSL -o innosetup.exe "$ISCC_URL"; then
        error "Failed to download Inno Setup"
        exit 1
    fi

    # Install Inno Setup silently
    info "Installing Inno Setup..."
    if ! wine innosetup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /NOICONS; then
        error "Failed to run Inno Setup installer under Wine"
        exit 1
    fi
    wineserver -w

    # Clean up
    cd - > /dev/null || exit 1
    rm -rf "$TEMP_DIR"
    TEMP_DIR=""

    if [ ! -f "$ISCC" ]; then
        error "Failed to install Inno Setup"
        exit 1
    fi
fi

# Create the installer
info "Creating installer..."

# Kill any hanging wine processes
wineserver -k

# Run ISCC with increased memory limits and in silent mode
wine "$ISCC" /O+ /Q setup.iss

# Check if the installer was created successfully
if [ ! -f "dist/${SETUP_NAME}" ]; then
    error "Failed to create installer"

    # Kill any hanging processes
    wineserver -k
    exit 1
fi

success "Installer created successfully at dist/${SETUP_NAME}"

# Final cleanup
wineserver -w
rm -rf "$WINEPREFIX" build

section_header "Build process completed!" 
