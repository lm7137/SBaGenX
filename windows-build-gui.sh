#!/bin/bash

# SBaGenX GUI Windows build script
# Builds the Tauri GUI on a Windows host and stages the installer artifacts.

. ./lib.sh

section_header "Building SBaGenX GUI for Windows..."

AUTO_INSTALL_DEPS="${SBAGENX_AUTO_INSTALL_DEPS:-1}"

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
GUI_RELEASE_DIR="$GUI_DIR/src-tauri/target/release"
GUI_FORCE_RUNTIME_REBUILD="${SBAGENX_GUI_FORCE_RUNTIME_REBUILD:-0}"
TAURI_CLI_JS="$GUI_DIR/node_modules/@tauri-apps/cli/tauri.js"
TEMP_TAURI_WINDOWS_CONFIG="$GUI_DIR/src-tauri/tauri.windows.conf.json"
VITE_CLI_JS="$GUI_DIR/node_modules/vite/bin/vite.js"
PREPARE_RUNTIME_JS="$GUI_DIR/scripts/prepare-runtime.mjs"
SVELTE_CHECK_JS="$GUI_DIR/node_modules/svelte-check/bin/svelte-check"
TSC_JS="$GUI_DIR/node_modules/typescript/bin/tsc"
VERSION_FILE="$REPO_ROOT/VERSION"

LIBMAD_WIN32="$REPO_ROOT/libs/windows-win32-libmad.a"
LIBMAD_WIN64="$REPO_ROOT/libs/windows-win64-libmad.a"
OGG_WIN32="$REPO_ROOT/libs/windows-win32-libogg.a"
OGG_WIN64="$REPO_ROOT/libs/windows-win64-libogg.a"
TREMOR_WIN32="$REPO_ROOT/libs/windows-win32-libvorbisidec.a"
TREMOR_WIN64="$REPO_ROOT/libs/windows-win64-libvorbisidec.a"
SNDFILE_STATIC_WIN32="$REPO_ROOT/libs/windows-win32-libsndfile.a"
SNDFILE_STATIC_WIN64="$REPO_ROOT/libs/windows-win64-libsndfile.a"
LAME_STATIC_WIN32="$REPO_ROOT/libs/windows-win32-libmp3lame.a"
LAME_STATIC_WIN64="$REPO_ROOT/libs/windows-win64-libmp3lame.a"

create_dir_if_not_exists "$DIST_DIR"
create_dir_if_not_exists "$GUI_DIST_DIR"

cleanup() {
    rm -f "$TEMP_TAURI_WINDOWS_CONFIG"
}

trap cleanup EXIT

MSYS_REPO=""
NODE_BIN=""
NPM_BIN=""

resolve_msys_repo() {
    case "${MSYSTEM:-}" in
        UCRT64) echo "ucrt64" ;;
        MINGW64) echo "mingw64" ;;
        MINGW32) echo "mingw32" ;;
        CLANG64) echo "clang64" ;;
        CLANG32) echo "clang32" ;;
        CLANGARM64) echo "clangarm64" ;;
        *)
            echo ""
            ;;
    esac
}

load_node_env() {
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

find_native_windows_node() {
    local candidate
    for candidate in \
        "/c/Program Files/nodejs/node.exe" \
        "/c/Program Files (x86)/nodejs/node.exe" \
        "/c/Users/${USERNAME}/AppData/Local/Programs/nodejs/node.exe"; do
        if [ -f "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

find_native_windows_npm() {
    local candidate
    for candidate in \
        "/c/Program Files/nodejs/npm.cmd" \
        "/c/Program Files (x86)/nodejs/npm.cmd" \
        "/c/Users/${USERNAME}/AppData/Local/Programs/nodejs/npm.cmd"; do
        if [ -f "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

resolve_tauri_binding_package() {
    local node_arch="$1"
    case "$node_arch" in
        x64) echo "@tauri-apps/cli-win32-x64-msvc" ;;
        ia32|x86) echo "@tauri-apps/cli-win32-ia32-msvc" ;;
        arm64) echo "@tauri-apps/cli-win32-arm64-msvc" ;;
        *)
            echo ""
            ;;
    esac
}

select_node_toolchain() {
    local node_path=""
    local npm_path=""

    node_path="$(find_native_windows_node || true)"
    npm_path="$(find_native_windows_npm || true)"
    if [ -n "$node_path" ] && [ -n "$npm_path" ]; then
        NODE_BIN="$node_path"
        NPM_BIN="$npm_path"
        return 0
    fi

    if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
        NODE_BIN="$(command -v node)"
        NPM_BIN="$(command -v npm)"
        return 0
    fi

    return 1
}

load_rust_env() {
    if command -v cargo >/dev/null 2>&1 && command -v rustc >/dev/null 2>&1; then
        return 0
    fi

    if [ -f "$HOME/.cargo/env" ]; then
        . "$HOME/.cargo/env"
    fi
}

tauri_cli_binding_usable() {
    (cd "$GUI_DIR" && "$NODE_BIN" -e "require('@tauri-apps/cli')") >/dev/null 2>&1
}

run_tauri_cli() {
    if [ ! -f "$TAURI_CLI_JS" ]; then
        error "Tauri CLI entrypoint not found at $TAURI_CLI_JS"
        return 1
    fi
    (cd "$GUI_DIR" && "$NODE_BIN" "$TAURI_CLI_JS" "$@")
}

run_vite_build() {
    if [ ! -f "$VITE_CLI_JS" ]; then
        error "Vite CLI entrypoint not found at $VITE_CLI_JS"
        return 1
    fi
    (cd "$GUI_DIR" && "$NODE_BIN" "$VITE_CLI_JS" build)
}

run_prepare_runtime() {
    if [ ! -f "$PREPARE_RUNTIME_JS" ]; then
        error "Runtime staging script not found at $PREPARE_RUNTIME_JS"
        return 1
    fi
    (cd "$GUI_DIR" && "$NODE_BIN" "$PREPARE_RUNTIME_JS")
}

run_gui_typecheck() {
    if [ ! -f "$SVELTE_CHECK_JS" ] || [ ! -f "$TSC_JS" ]; then
        error "GUI typecheck entrypoints are missing from node_modules"
        return 1
    fi
    (cd "$GUI_DIR" && "$NODE_BIN" "$SVELTE_CHECK_JS" --tsconfig ./tsconfig.app.json)
    if [ $? -ne 0 ]; then
        return 1
    fi
    (cd "$GUI_DIR" && "$NODE_BIN" "$TSC_JS" -p tsconfig.node.json)
}

write_temp_windows_tauri_config() {
    cat > "$TEMP_TAURI_WINDOWS_CONFIG" <<'EOF'
{
  "build": {
    "beforeBuildCommand": ""
  }
}
EOF
}

resolve_tauri_cli_version() {
    if [ ! -f "$GUI_DIR/package-lock.json" ]; then
        return 1
    fi

    "$NODE_BIN" -e "const fs=require('fs'); const j=JSON.parse(fs.readFileSync(process.argv[1],'utf8')); const pkgs=j.packages||{}; const pkg=pkgs['node_modules/@tauri-apps/cli']; if(!pkg||!pkg.version){process.exit(1)} process.stdout.write(pkg.version);" "$GUI_DIR/package-lock.json"
}

resolve_mingw_objdump() {
    local target="$1"
    local objdump_path=""
    objdump_path=$(command -v "${target}-objdump" 2>/dev/null || true)
    if [ -n "$objdump_path" ]; then
        echo "$objdump_path"
        return 0
    fi
    objdump_path=$(command -v objdump 2>/dev/null || true)
    if [ -n "$objdump_path" ]; then
        echo "$objdump_path"
        return 0
    fi
    return 1
}

resolve_mingw_nm() {
    local target="$1"
    local nm_path=""
    nm_path=$(command -v "${target}-nm" 2>/dev/null || true)
    if [ -n "$nm_path" ]; then
        echo "$nm_path"
        return 0
    fi
    nm_path=$(command -v nm 2>/dev/null || true)
    if [ -n "$nm_path" ]; then
        echo "$nm_path"
        return 0
    fi
    return 1
}

resolve_current_arch_target() {
    case "$WIN_SUFFIX" in
        win64) echo "x86_64-w64-mingw32" ;;
        win32) echo "i686-w64-mingw32" ;;
        *) echo "" ;;
    esac
}

resolve_current_arch_gcc() {
    local target
    target="$(resolve_current_arch_target)"
    if [ -z "$target" ]; then
        return 1
    fi
    command -v "${target}-gcc" 2>/dev/null || true
}

build_current_arch_sbagenxlib_shared() {
    local target=""
    local gcc_bin=""
    local version=""
    local cflags="-DT_MINGW -DFLAC_DECODE -Wall -O3 -I. -Ilibs"
    local libs="-lwinmm"
    local output=""
    local implib=""

    target="$(resolve_current_arch_target)"
    gcc_bin="$(resolve_current_arch_gcc)"
    if [ -z "$target" ] || [ -z "$gcc_bin" ]; then
        return 1
    fi

    if [ ! -f "$VERSION_FILE" ]; then
        error "Missing VERSION file at $VERSION_FILE"
        return 1
    fi
    version="$(cat "$VERSION_FILE")"

    case "$WIN_SUFFIX" in
        win64)
            if [ -f "$LIBMAD_WIN64" ]; then
                cflags="$cflags -DMP3_DECODE"
                libs="$libs $LIBMAD_WIN64"
            fi
            if [ -f "$OGG_WIN64" ] && [ -f "$TREMOR_WIN64" ]; then
                cflags="$cflags -DOGG_DECODE"
                libs="$libs $TREMOR_WIN64 $OGG_WIN64"
            fi
            if [ -f "$SNDFILE_STATIC_WIN64" ] && [ -f "$LAME_STATIC_WIN64" ]; then
                cflags="$cflags -DSTATIC_OUTPUT_ENCODERS"
                libs="$libs $SNDFILE_STATIC_WIN64 $LAME_STATIC_WIN64 ${SBAGENX_STATIC_ENCODER_DEPS_WIN64}"
            fi
            output="$DIST_DIR/sbagenxlib-win64.dll"
            implib="$DIST_DIR/libsbagenx-win64.dll.a"
            ;;
        win32)
            if [ -f "$LIBMAD_WIN32" ]; then
                cflags="$cflags -DMP3_DECODE"
                libs="$libs $LIBMAD_WIN32"
            fi
            if [ -f "$OGG_WIN32" ] && [ -f "$TREMOR_WIN32" ]; then
                cflags="$cflags -DOGG_DECODE"
                libs="$libs $TREMOR_WIN32 $OGG_WIN32"
            fi
            if [ -f "$SNDFILE_STATIC_WIN32" ] && [ -f "$LAME_STATIC_WIN32" ]; then
                cflags="$cflags -DSTATIC_OUTPUT_ENCODERS"
                libs="$libs $SNDFILE_STATIC_WIN32 $LAME_STATIC_WIN32 ${SBAGENX_STATIC_ENCODER_DEPS_WIN32}"
            fi
            output="$DIST_DIR/sbagenxlib-win32.dll"
            implib="$DIST_DIR/libsbagenx-win32.dll.a"
            ;;
        *)
            return 1
            ;;
    esac

    create_dir_if_not_exists "$DIST_DIR"
    info "Rebuilding current-arch sbagenxlib shared library for ${WIN_SUFFIX}..."
    (cd "$REPO_ROOT" && "$gcc_bin" ${cflags} -DSBAGENXLIB_VERSION="\"${version}\"" -shared sbagenxlib.c \
        -Wl,--out-implib,"$implib" \
        -Wl,--export-all-symbols -Wl,--enable-auto-import \
        -o "$output" ${libs})
}

dll_exports_symbol() {
    local dll_path="$1"
    local target="$2"
    local symbol="$3"
    local objdump_bin=""
    local nm_bin=""

    if [ ! -f "$dll_path" ]; then
        return 1
    fi

    objdump_bin="$(resolve_mingw_objdump "$target" || true)"
    if [ -n "$objdump_bin" ] && "$objdump_bin" -p "$dll_path" 2>/dev/null | grep -Fq "$symbol"; then
        return 0
    fi

    nm_bin="$(resolve_mingw_nm "$target" || true)"
    if [ -n "$nm_bin" ] && "$nm_bin" -g "$dll_path" 2>/dev/null | grep -Fq "$symbol"; then
        return 0
    fi

    return 1
}

repair_tauri_cli_binding() {
    local tauri_version=""
    local node_arch=""
    local binding_pkg=""

    tauri_version="$(resolve_tauri_cli_version || true)"
    if [ -z "$tauri_version" ]; then
        return 1
    fi

    node_arch="$("$NODE_BIN" -p "process.arch")"
    binding_pkg="$(resolve_tauri_binding_package "$node_arch")"
    if [ -z "$binding_pkg" ]; then
        return 1
    fi

    warning "Tauri CLI native binding is missing; installing ${binding_pkg}@${tauri_version}..."
    if ! (cd "$GUI_DIR" && "$NPM_BIN" install --no-save "${binding_pkg}@${tauri_version}"); then
        return 1
    fi

    tauri_cli_binding_usable
}

refresh_gui_node_modules() {
    warning "Refreshing GUI node_modules with native Windows npm..."
    rm -rf "$GUI_DIR/node_modules"
    (cd "$GUI_DIR" && "$NPM_BIN" install --include=optional)
}

ensure_native_windows_node() {
    local current_node=""
    local native_node=""
    local native_npm=""

    current_node="$(command -v node 2>/dev/null || true)"
    if [ -n "$current_node" ] && [[ "$current_node" != /usr/* ]] && [[ "$current_node" != /mingw* ]] && [[ "$current_node" != /ucrt64/* ]] && [[ "$current_node" != /clang* ]]; then
        return 0
    fi

    native_node="$(find_native_windows_node || true)"
    native_npm="$(find_native_windows_npm || true)"
    if [ -n "$native_node" ] && [ -n "$native_npm" ]; then
        NODE_BIN="$native_node"
        NPM_BIN="$native_npm"
        return 0
    fi

    if [ "$AUTO_INSTALL_DEPS" = "0" ]; then
        return 1
    fi

    if ! install_with_winget "OpenJS.NodeJS.LTS"; then
        return 1
    fi

    native_node="$(find_native_windows_node || true)"
    native_npm="$(find_native_windows_npm || true)"
    if [ -n "$native_node" ] && [ -n "$native_npm" ]; then
        NODE_BIN="$native_node"
        NPM_BIN="$native_npm"
        return 0
    fi

    return 1
}

find_winget() {
    if command -v winget >/dev/null 2>&1; then
        command -v winget
        return 0
    fi
    if command -v winget.exe >/dev/null 2>&1; then
        command -v winget.exe
        return 0
    fi
    local candidate="/c/Users/${USERNAME}/AppData/Local/Microsoft/WindowsApps/winget.exe"
    if [ -f "$candidate" ]; then
        echo "$candidate"
        return 0
    fi
    return 1
}

install_with_winget() {
    local package_id="$1"
    local winget_bin=""

    winget_bin="$(find_winget || true)"
    if [ -z "$winget_bin" ]; then
        return 1
    fi

    info "Trying winget install ${package_id}..."
    "$winget_bin" install --id "$package_id" -e --accept-package-agreements --accept-source-agreements --silent --disable-interactivity >/dev/null 2>&1 || return 1
    return 0
}

pacman_install() {
    local pkg="$1"

    if ! command -v pacman >/dev/null 2>&1; then
        return 1
    fi

    info "Installing ${pkg} with pacman..."
    pacman -Sy --noconfirm --needed "$pkg" >/dev/null 2>&1 || return 1
    hash -r
    return 0
}

resolve_msys_package_for_command() {
    local cmd="$1"

    case "$cmd" in
        node|npm)
            case "$MSYS_REPO" in
                ucrt64) echo "mingw-w64-ucrt-x86_64-nodejs" ;;
                mingw64) echo "mingw-w64-x86_64-nodejs" ;;
                mingw32) echo "mingw-w64-i686-nodejs" ;;
                *)
                    echo ""
                    ;;
            esac
            ;;
        cargo|rustc)
            case "$MSYS_REPO" in
                ucrt64) echo "mingw-w64-ucrt-x86_64-rust" ;;
                mingw64) echo "mingw-w64-x86_64-rust" ;;
                mingw32) echo "mingw-w64-i686-rust" ;;
                *)
                    echo ""
                    ;;
            esac
            ;;
        x86_64-w64-mingw32-gcc)
            case "$MSYS_REPO" in
                ucrt64) echo "mingw-w64-ucrt-x86_64-gcc" ;;
                mingw64) echo "mingw-w64-x86_64-gcc" ;;
                *)
                    echo ""
                    ;;
            esac
            ;;
        i686-w64-mingw32-gcc)
            echo "mingw-w64-i686-gcc"
            ;;
        *)
            echo ""
            ;;
    esac
}

ensure_command_available() {
    local cmd="$1"
    local pacman_pkg=""

    if command -v "$cmd" >/dev/null 2>&1; then
        return 0
    fi

    if [ "$AUTO_INSTALL_DEPS" = "0" ]; then
        return 1
    fi

    pacman_pkg="$(resolve_msys_package_for_command "$cmd")"
    if [ -n "$pacman_pkg" ] && pacman_install "$pacman_pkg"; then
        if command -v "$cmd" >/dev/null 2>&1; then
            return 0
        fi
    fi

    case "$cmd" in
        node|npm)
            if ensure_native_windows_node; then
                return 0
            fi
            ;;
        cargo|rustc)
            if install_with_winget "Rustlang.Rustup"; then
                load_rust_env
                hash -r
                if command -v "$cmd" >/dev/null 2>&1; then
                    return 0
                fi
            fi
            ;;
    esac

    return 1
}

ensure_required_windows_gui_tools() {
    local require_compilers="${1:-1}"
    local missing=()
    local tool

    for tool in node npm cargo rustc; do
        if ! ensure_command_available "$tool"; then
            missing+=("$tool")
        fi
    done

    if [ "$require_compilers" = "1" ]; then
        for tool in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
            if ! ensure_command_available "$tool"; then
                missing+=("$tool")
            fi
        done
    fi

    if [ "${#missing[@]}" -ne 0 ]; then
        error "Missing required tools: ${missing[*]}"
        if [ "$AUTO_INSTALL_DEPS" = "0" ]; then
            info "Automatic dependency install is disabled (SBAGENX_AUTO_INSTALL_DEPS=0)."
        else
            info "If auto-install did not succeed, install the missing tools manually and rerun the script."
        fi
        return 1
    fi

    return 0
}

ensure_current_arch_windows_compiler() {
    local compiler=""

    case "$WIN_SUFFIX" in
        win64) compiler="x86_64-w64-mingw32-gcc" ;;
        win32) compiler="i686-w64-mingw32-gcc" ;;
        *)
            return 1
            ;;
    esac

    if ! ensure_command_available "$compiler"; then
        error "Missing required tool: $compiler"
        if [ "$AUTO_INSTALL_DEPS" = "0" ]; then
            info "Automatic dependency install is disabled (SBAGENX_AUTO_INSTALL_DEPS=0)."
        else
            info "If auto-install did not succeed, install the missing tool manually and rerun the script."
        fi
        return 1
    fi

    return 0
}

MSYS_REPO="$(resolve_msys_repo)"
load_node_env
load_rust_env

if ! ensure_required_windows_gui_tools 0; then
    exit 1
fi

if ! select_node_toolchain; then
    error "Could not resolve a usable Node/npm toolchain."
    exit 1
fi

if ! ensure_native_windows_node; then
    error "A native Windows Node.js installation is required for Tauri on this MSYS2 shell."
    info "Install Node.js LTS for Windows or rerun with SBAGENX_AUTO_INSTALL_DEPS=1."
    exit 1
fi

info "Node: $("$NODE_BIN" -v)"
info "npm: $("$NPM_BIN" -v)"
info "rustc: $(rustc --version)"
info "cargo: $(cargo --version)"
info "MSYS environment: ${MSYSTEM:-unknown}"

section_header "Ensuring GUI npm dependencies are installed..."
if [ ! -d "$GUI_DIR/node_modules" ]; then
    info "Installing GUI npm dependencies..."
    (cd "$GUI_DIR" && "$NPM_BIN" install)
    check_error "Failed to install GUI npm dependencies"
fi

section_header "Checking Tauri CLI native binding..."
if ! tauri_cli_binding_usable; then
    if ! repair_tauri_cli_binding; then
        if ! refresh_gui_node_modules; then
            error "Failed to refresh GUI npm dependencies with native Windows npm."
            exit 1
        fi
        if ! tauri_cli_binding_usable; then
            if ! repair_tauri_cli_binding; then
                error "Tauri CLI native binding is unavailable for the active Node runtime."
                info "This usually means gui/node_modules was installed under a different host/runtime and must be rebuilt on Windows."
                exit 1
            fi
        fi
    fi
fi

NODE_ARCH="$("$NODE_BIN" -p "process.arch")"
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

GUI_REQUIRED_SBAGENXLIB_EXPORTS=(
    "sbx_validate_sbg_text"
    "sbx_validate_sbgf_text"
    "sbx_free_diagnostics"
)

gui_runtime_ready() {
    local runtime
    for runtime in "${REQUIRED_GUI_RUNTIMES[@]}"; do
        if [ ! -f "$DIST_DIR/$runtime" ]; then
            return 1
        fi
    done
    return 0
}

gui_sbagenxlib_exports_ready() {
    local dll_path="$DIST_DIR/sbagenxlib-${WIN_SUFFIX}.dll"
    local target=""
    local symbol=""

    case "$WIN_SUFFIX" in
        win64) target="x86_64-w64-mingw32" ;;
        win32) target="i686-w64-mingw32" ;;
        *) return 1 ;;
    esac

    for symbol in "${GUI_REQUIRED_SBAGENXLIB_EXPORTS[@]}"; do
        if ! dll_exports_symbol "$dll_path" "$target" "$symbol"; then
            return 1
        fi
    done

    return 0
}

section_header "Checking staged GUI runtime DLL prerequisites..."
if gui_runtime_ready && gui_sbagenxlib_exports_ready && [ "$GUI_FORCE_RUNTIME_REBUILD" != "1" ]; then
    success "Required GUI runtime DLLs and sbagenxlib exports are already present for ${WIN_SUFFIX}; skipping CLI/library rebuild."
else
    if ! gui_runtime_ready; then
        warning "Current-arch GUI runtime DLLs are missing or rebuild was forced."
    elif ! gui_sbagenxlib_exports_ready; then
        warning "Current-arch sbagenxlib DLL is present but missing required GUI exports."
        if build_current_arch_sbagenxlib_shared && gui_sbagenxlib_exports_ready; then
            success "Rebuilt current-arch sbagenxlib DLL with required GUI exports."
        else
            warning "Current-arch sbagenxlib refresh did not complete; falling back to full Windows runtime rebuild."
        fi
    else
        warning "Windows GUI runtime rebuild was forced."
    fi

    if gui_runtime_ready && gui_sbagenxlib_exports_ready && [ "$GUI_FORCE_RUNTIME_REBUILD" != "1" ]; then
        :
    elif ! ensure_current_arch_windows_compiler; then
        error "The GUI runtime rebuild path requires the ${WIN_SUFFIX} MinGW compiler."
        exit 1
    else
        section_header "Building Windows sbagenxlib runtime prerequisites..."
        if ! (cd "$REPO_ROOT" && SBAGENX_REQUIRE_PY_RUNTIME=0 SBAGENX_WINDOWS_ARCHES="$WIN_SUFFIX" ./windows-build-sbagenx.sh); then
            error "Failed to build Windows sbagenxlib prerequisites for the GUI."
            exit 1
        fi

        if ! gui_runtime_ready; then
            error "Required GUI runtime DLLs are still missing after the CLI/library build."
            exit 1
        fi

        if ! gui_sbagenxlib_exports_ready; then
            error "sbagenxlib-${WIN_SUFFIX}.dll still does not export the required GUI symbols after rebuild."
            exit 1
        fi
    fi
fi

section_header "Cleaning previous GUI bundle outputs..."
rm -rf "$GUI_BUNDLE_DIR"

section_header "Typechecking GUI..."
run_gui_typecheck
check_error "GUI typecheck failed"

section_header "Building frontend assets with native Windows npm..."
run_vite_build
check_error "GUI frontend build failed"

section_header "Staging GUI runtime bundle..."
run_prepare_runtime
check_error "GUI runtime staging failed"

section_header "Writing temporary Windows Tauri config override..."
write_temp_windows_tauri_config
check_error "Failed to write temporary Windows Tauri config override"

section_header "Building Windows GUI bundle..."
run_tauri_cli build
check_error "Windows GUI bundle build failed"

section_header "Collecting Windows GUI artifacts..."
mapfile -t NSIS_ARTIFACTS < <(find "$GUI_BUNDLE_DIR" -type f -path "*/nsis/*.exe" | sort)
mapfile -t MSI_ARTIFACTS < <(find "$GUI_BUNDLE_DIR" -type f -path "*/msi/*.msi" | sort)

GUI_PORTABLE_EXE="$GUI_DIR/src-tauri/target/release/sbagenx-gui.exe"
WEBVIEW2_LOADER_DLL="$GUI_RELEASE_DIR/WebView2Loader.dll"

if [ "${#NSIS_ARTIFACTS[@]}" -eq 0 ] && [ "${#MSI_ARTIFACTS[@]}" -eq 0 ]; then
    error "No Windows GUI installer artifacts were produced under $GUI_BUNDLE_DIR"
    exit 1
fi

rm -f "$GUI_DIST_DIR"/sbagenx-gui-windows-setup.exe
rm -f "$GUI_DIST_DIR"/sbagenx-gui-windows.msi
rm -f "$GUI_DIST_DIR"/sbagenx-gui-"${WIN_SUFFIX}".exe
rm -f "$GUI_DIST_DIR"/WebView2Loader.dll

if [ "$WIN_SUFFIX" = "win64" ] || [ "$WIN_SUFFIX" = "win32" ]; then
    if [ ! -f "$WEBVIEW2_LOADER_DLL" ]; then
        error "Missing $WEBVIEW2_LOADER_DLL after GUI build."
        info "Windows GNU builds require WebView2Loader.dll next to the executable."
        exit 1
    fi
fi

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

if [ -f "$WEBVIEW2_LOADER_DLL" ]; then
    cp "$WEBVIEW2_LOADER_DLL" "$GUI_DIST_DIR/WebView2Loader.dll"
    check_error "Failed to stage WebView2Loader.dll"
    success "Staged WebView2 loader: dist/gui/WebView2Loader.dll"
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
if [ -f "$GUI_DIST_DIR/WebView2Loader.dll" ]; then
    info "  dist/gui/WebView2Loader.dll"
fi
