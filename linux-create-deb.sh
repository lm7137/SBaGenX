#!/bin/bash

# SBaGenX Linux Debian package script
# Creates a .deb package from a previously-built Linux binary

. ./lib.sh

APP_NAME="sbagenx"
PKG_NAME="sbagenx"
HOMEPAGE_URL="https://github.com/lm7137/SBaGenX"
MAINTAINER_DEFAULT="SBaGenX Maintainers <maintainers@sbagenx.invalid>"

BINARY_PATH="${SBAGENX_DEB_BINARY:-}"
DEB_ARCH_OVERRIDE="${SBAGENX_DEB_ARCH:-}"
DEB_REVISION="${SBAGENX_DEB_REVISION:-1}"
OUTPUT_DEB="${SBAGENX_DEB_OUTPUT:-}"
INCLUDE_GUI="${SBAGENX_DEB_WITH_GUI:-1}"
GUI_BINARY_PATH="${SBAGENX_DEB_GUI_BINARY:-gui/src-tauri/target/release/sbagenx-gui}"
GUI_DESKTOP_NAME="sbagenx-gui.desktop"

usage() {
    cat <<USAGE
Usage: $0 [options]

Create a Debian package for SBaGenX from an existing Linux binary.

Options:
  -b, --binary PATH      Path to Linux binary to package
                          (default: auto-detect in dist/)
  -a, --arch ARCH        Debian architecture override (amd64|i386|arm64)
  -r, --revision N       Debian package revision (default: 1)
  -o, --output PATH      Output .deb path (default: dist/sbagenx_<ver>-<rev>_<arch>.deb)
  -h, --help             Show this help

Environment overrides:
  SBAGENX_DEB_BINARY, SBAGENX_DEB_ARCH, SBAGENX_DEB_REVISION, SBAGENX_DEB_OUTPUT,
  SBAGENX_DEB_MAINTAINER
USAGE
}

normalize_deb_version() {
    local version="$1"

    # Keep Debian-safe characters only.
    version=$(echo "$version" | sed 's/[^0-9A-Za-z.+:~_-]/-/g')

    # Convert common prerelease markers for proper version ordering.
    version="${version//-dev./~dev.}"
    version="${version//-alpha./~alpha.}"
    version="${version//-beta./~beta.}"
    version="${version//-rc./~rc.}"

    # Avoid ambiguity with Debian revision separator.
    version="${version//-/~}"

    echo "$version"
}

map_uname_arch_to_deb() {
    case "$1" in
        x86_64|amd64)
            echo "amd64"
            ;;
        i386|i486|i586|i686)
            echo "i386"
            ;;
        aarch64|arm64)
            echo "arm64"
            ;;
        *)
            echo ""
            ;;
    esac
}

detect_deb_arch_from_binary() {
    local binary="$1"
    local file_desc=""

    if ! command -v file >/dev/null 2>&1; then
        echo ""
        return 0
    fi

    file_desc=$(file -b "$binary" 2>/dev/null)

    if echo "$file_desc" | grep -qi "x86-64"; then
        echo "amd64"
    elif echo "$file_desc" | grep -Eqi "80386|i386"; then
        echo "i386"
    elif echo "$file_desc" | grep -Eqi "aarch64|ARM aarch64"; then
        echo "arm64"
    else
        echo ""
    fi
}

map_deb_arch_to_multiarch_libdir() {
    case "$1" in
        amd64)
            echo "x86_64-linux-gnu"
            ;;
        i386)
            echo "i386-linux-gnu"
            ;;
        arm64)
            echo "aarch64-linux-gnu"
            ;;
        *)
            echo ""
            ;;
    esac
}

linux_runtime_arch_hint() {
    case "$1" in
        amd64)
            echo "x86-64|x86_64-linux-gnu"
            ;;
        i386)
            echo "i386-linux-gnu|libc6"
            ;;
        arm64)
            echo "aarch64|aarch64-linux-gnu|arm64"
            ;;
        *)
            echo ""
            ;;
    esac
}

resolve_linux_runtime_soname_path() {
    local soname="$1"
    local deb_arch="$2"
    local hint
    local line
    local path=""

    hint="$(linux_runtime_arch_hint "$deb_arch")"

    if command -v ldconfig >/dev/null 2>&1; then
        while IFS= read -r line; do
            case "$line" in
                *"$soname"*"=>"*)
                    if [ -n "$hint" ] && echo "$line" | grep -Eqi "$hint"; then
                        path="$(echo "$line" | sed 's/.*=>[[:space:]]*//')"
                        break
                    fi
                    if [ -z "$path" ]; then
                        path="$(echo "$line" | sed 's/.*=>[[:space:]]*//')"
                    fi
                    ;;
            esac
        done < <(ldconfig -p 2>/dev/null)
    fi

    if [ -z "$path" ]; then
        for candidate in \
            "/lib/${MULTIARCH_LIBDIR}/${soname}" \
            "/usr/lib/${MULTIARCH_LIBDIR}/${soname}" \
            "/lib64/${soname}" \
            "/usr/lib64/${soname}" \
            "/lib/${soname}" \
            "/usr/lib/${soname}"; do
            if [ -e "$candidate" ]; then
                path="$candidate"
                break
            fi
        done
    fi

    if [ -n "$path" ] && [ -e "$path" ]; then
        readlink -f "$path"
        return 0
    fi
    return 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        -b|--binary)
            if [ -z "$2" ]; then
                error "Missing value for $1"
                exit 1
            fi
            BINARY_PATH="$2"
            shift 2
            ;;
        -a|--arch)
            if [ -z "$2" ]; then
                error "Missing value for $1"
                exit 1
            fi
            DEB_ARCH_OVERRIDE="$2"
            shift 2
            ;;
        -r|--revision)
            if [ -z "$2" ]; then
                error "Missing value for $1"
                exit 1
            fi
            DEB_REVISION="$2"
            shift 2
            ;;
        -o|--output)
            if [ -z "$2" ]; then
                error "Missing value for $1"
                exit 1
            fi
            OUTPUT_DEB="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            error "Unknown argument: $1"
            usage
            exit 1
            ;;
    esac
 done

section_header "Creating Debian package for SBaGenX..."

check_required_tools dpkg-deb

if [ ! -f VERSION ]; then
    error "VERSION file not found"
    exit 1
fi

RAW_VERSION=$(cat VERSION)
DEB_VERSION=$(normalize_deb_version "$RAW_VERSION")

if [ -z "$DEB_VERSION" ]; then
    error "Could not normalize package version from VERSION='$RAW_VERSION'"
    exit 1
fi

if [ -z "$BINARY_PATH" ]; then
    case "$(uname -m)" in
        x86_64)
            [ -f "dist/sbagenx-linux64" ] && BINARY_PATH="dist/sbagenx-linux64"
            ;;
        aarch64)
            [ -f "dist/sbagenx-linux-arm64" ] && BINARY_PATH="dist/sbagenx-linux-arm64"
            ;;
        i386|i486|i586|i686)
            [ -f "dist/sbagenx-linux32" ] && BINARY_PATH="dist/sbagenx-linux32"
            ;;
    esac
fi

if [ -z "$BINARY_PATH" ]; then
    for candidate in dist/sbagenx-linux64 dist/sbagenx-linux-arm64 dist/sbagenx-linux32; do
        if [ -f "$candidate" ]; then
            BINARY_PATH="$candidate"
            break
        fi
    done
fi

if [ -z "$BINARY_PATH" ] || [ ! -f "$BINARY_PATH" ]; then
    error "No Linux binary found to package. Run ./linux-build-sbagenx.sh first or pass --binary."
    exit 1
fi

if [ "$INCLUDE_GUI" = "1" ] && [ ! -f "$GUI_BINARY_PATH" ]; then
    error "No Linux GUI binary found to package at $GUI_BINARY_PATH"
    info "Build the GUI first or set SBAGENX_DEB_WITH_GUI=0 for a CLI-only package."
    exit 1
fi

if [ -z "$DEB_ARCH_OVERRIDE" ]; then
    DEB_ARCH=$(detect_deb_arch_from_binary "$BINARY_PATH")
else
    DEB_ARCH="$DEB_ARCH_OVERRIDE"
fi

if [ -z "$DEB_ARCH" ]; then
    DEB_ARCH=$(map_uname_arch_to_deb "$(uname -m)")
fi

case "$DEB_ARCH" in
    amd64|i386|arm64)
        ;;
    *)
        error "Unsupported or unknown Debian architecture '$DEB_ARCH'. Use --arch amd64|i386|arm64."
        exit 1
        ;;
esac

if ! echo "$DEB_REVISION" | grep -Eq '^[0-9]+$'; then
    error "Invalid package revision '$DEB_REVISION' (expected integer)."
    exit 1
fi

PKG_FULL_VERSION="${DEB_VERSION}-${DEB_REVISION}"
MAINTAINER="${SBAGENX_DEB_MAINTAINER:-$MAINTAINER_DEFAULT}"
MULTIARCH_LIBDIR="$(map_deb_arch_to_multiarch_libdir "$DEB_ARCH")"

if [ -z "$OUTPUT_DEB" ]; then
    OUTPUT_DEB="dist/${PKG_NAME}_${PKG_FULL_VERSION}_${DEB_ARCH}.deb"
fi

BUILD_ROOT="build/deb"
STAGE_DIR="${BUILD_ROOT}/stage"

info "Using binary: $BINARY_PATH"
info "Debian architecture: $DEB_ARCH"
info "Multiarch library dir: /usr/lib/${MULTIARCH_LIBDIR}"
info "Package version: $PKG_FULL_VERSION"
info "Output package: $OUTPUT_DEB"

rm -rf "$STAGE_DIR"

create_dir_if_not_exists "$STAGE_DIR/DEBIAN"
create_dir_if_not_exists "$STAGE_DIR/usr/bin"
create_dir_if_not_exists "$STAGE_DIR/usr/include"
create_dir_if_not_exists "$STAGE_DIR/usr/lib/${MULTIARCH_LIBDIR}"
create_dir_if_not_exists "$STAGE_DIR/usr/lib/${MULTIARCH_LIBDIR}/pkgconfig"
create_dir_if_not_exists "$STAGE_DIR/usr/lib/${PKG_NAME}"
create_dir_if_not_exists "$STAGE_DIR/usr/share/${PKG_NAME}/docs"
create_dir_if_not_exists "$STAGE_DIR/usr/share/${PKG_NAME}/examples"
create_dir_if_not_exists "$STAGE_DIR/usr/share/${PKG_NAME}/scripts"
create_dir_if_not_exists "$STAGE_DIR/usr/share/${PKG_NAME}/assets"
create_dir_if_not_exists "$STAGE_DIR/usr/share/doc/${PKG_NAME}"
if [ "$INCLUDE_GUI" = "1" ]; then
    create_dir_if_not_exists "$STAGE_DIR/usr/share/applications"
    create_dir_if_not_exists "$STAGE_DIR/usr/share/icons/hicolor/32x32/apps"
    create_dir_if_not_exists "$STAGE_DIR/usr/share/icons/hicolor/128x128/apps"
    create_dir_if_not_exists "$STAGE_DIR/usr/share/icons/hicolor/256x256/apps"
fi

# Program binary
cp "$BINARY_PATH" "$STAGE_DIR/usr/bin/${APP_NAME}"
if [ "$INCLUDE_GUI" = "1" ]; then
    cp "$GUI_BINARY_PATH" "$STAGE_DIR/usr/bin/sbagenx-gui"
fi

# Library development/runtime artifacts
if [ -f "dist/include/sbagenxlib.h" ]; then
    cp "dist/include/sbagenxlib.h" "$STAGE_DIR/usr/include/"
fi
if [ -f "dist/include/sbagenlib.h" ]; then
    cp "dist/include/sbagenlib.h" "$STAGE_DIR/usr/include/"
fi

STATIC_ARCHIVE=""
case "$DEB_ARCH" in
    amd64)
        [ -f "dist/libsbagenx-linux64.a" ] && STATIC_ARCHIVE="dist/libsbagenx-linux64.a"
        ;;
    i386)
        [ -f "dist/libsbagenx-linux32.a" ] && STATIC_ARCHIVE="dist/libsbagenx-linux32.a"
        ;;
    arm64)
        [ -f "dist/libsbagenx-linux-arm64.a" ] && STATIC_ARCHIVE="dist/libsbagenx-linux-arm64.a"
        ;;
esac

if [ -n "$STATIC_ARCHIVE" ]; then
    cp "$STATIC_ARCHIVE" "$STAGE_DIR/usr/lib/${MULTIARCH_LIBDIR}/libsbagenx.a"
fi

SO_MAJOR="${RAW_VERSION%%.*}"
if [ -L "dist/libsbagenx.so" ] || [ -f "dist/libsbagenx.so" ]; then
    cp -a "dist/libsbagenx.so" "$STAGE_DIR/usr/lib/${MULTIARCH_LIBDIR}/"
fi
if [ -L "dist/libsbagenx.so.${SO_MAJOR}" ] || [ -f "dist/libsbagenx.so.${SO_MAJOR}" ]; then
    cp -a "dist/libsbagenx.so.${SO_MAJOR}" "$STAGE_DIR/usr/lib/${MULTIARCH_LIBDIR}/"
fi
if [ -f "dist/libsbagenx.so.${RAW_VERSION}" ]; then
    cp "dist/libsbagenx.so.${RAW_VERSION}" "$STAGE_DIR/usr/lib/${MULTIARCH_LIBDIR}/"
fi

if [ -f "dist/pkgconfig/sbagenxlib.pc" ]; then
    sed "s|^libdir=.*|libdir=\${exec_prefix}/lib/${MULTIARCH_LIBDIR}|" \
        "dist/pkgconfig/sbagenxlib.pc" \
        > "$STAGE_DIR/usr/lib/${MULTIARCH_LIBDIR}/pkgconfig/sbagenxlib.pc"
fi

# Private bundled runtime codec libraries for export/mix decoding.
for runtime_soname in \
    libsndfile.so.1 \
    libmp3lame.so.0 \
    libFLAC.so.12 \
    libmpg123.so.0 \
    libogg.so.0 \
    libvorbis.so.0 \
    libvorbisenc.so.2 \
    libopus.so.0; do
    runtime_src="$(resolve_linux_runtime_soname_path "$runtime_soname" "$DEB_ARCH" || true)"
    if [ -z "$runtime_src" ] || [ ! -f "$runtime_src" ]; then
        error "Required Linux runtime codec library not found for packaging: $runtime_soname"
        exit 1
    fi
    install -m 644 "$runtime_src" "$STAGE_DIR/usr/lib/${PKG_NAME}/${runtime_soname}"
done

# Project assets/resources
if [ -d docs ]; then
    cp -a docs/. "$STAGE_DIR/usr/share/${PKG_NAME}/docs/"
fi
if [ -d examples ]; then
    cp -a examples/. "$STAGE_DIR/usr/share/${PKG_NAME}/examples/"
fi
if [ -d scripts ]; then
    cp -a scripts/. "$STAGE_DIR/usr/share/${PKG_NAME}/scripts/"
fi
if [ -d assets ]; then
    cp -a assets/. "$STAGE_DIR/usr/share/${PKG_NAME}/assets/"
fi

# Top-level docs
for doc_file in README.md USAGE.md RESEARCH.md COPYING.txt NOTICE.txt ChangeLog.txt VERSION; do
    if [ -f "$doc_file" ]; then
        cp "$doc_file" "$STAGE_DIR/usr/share/doc/${PKG_NAME}/"
    fi
done

if [ -d licenses ]; then
    cp -a licenses "$STAGE_DIR/usr/share/doc/${PKG_NAME}/"
fi

if [ "$INCLUDE_GUI" = "1" ]; then
    cat > "$STAGE_DIR/usr/share/applications/${GUI_DESKTOP_NAME}" <<'EOF'
[Desktop Entry]
Categories=AudioVideo;Audio;
Comment=SBaGenX desktop GUI
Exec=sbagenx-gui
StartupWMClass=sbagenx-gui
Icon=sbagenx-gui
Name=SBaGenX GUI
Terminal=false
Type=Application
EOF
    install -m 644 "gui/src-tauri/icons/32x32.png" \
        "$STAGE_DIR/usr/share/icons/hicolor/32x32/apps/sbagenx-gui.png"
    install -m 644 "gui/src-tauri/icons/128x128.png" \
        "$STAGE_DIR/usr/share/icons/hicolor/128x128/apps/sbagenx-gui.png"
    install -m 644 "gui/src-tauri/icons/128x128@2x.png" \
        "$STAGE_DIR/usr/share/icons/hicolor/256x256/apps/sbagenx-gui.png"
fi

cat > "$STAGE_DIR/usr/share/doc/${PKG_NAME}/copyright" <<COPYRIGHT
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: SBaGenX
Source: ${HOMEPAGE_URL}

Files: *
Copyright:
  SBaGen (original): Jim Peters
  SBaGen+ fork: Ruan Klein
  SBaGenX fork: Lech Madrzyk and contributors
License: GPL-2.0-only
 The project is distributed under the GNU General Public License v2.
 See /usr/share/doc/${PKG_NAME}/COPYING.txt for the full license text.
COPYRIGHT

# Normalize file permissions for package content.
find "$STAGE_DIR" -type d -exec chmod 755 {} +
find "$STAGE_DIR" -type f -exec chmod 644 {} +
chmod 755 "$STAGE_DIR/usr/bin/${APP_NAME}"
if [ "$INCLUDE_GUI" = "1" ]; then
    chmod 755 "$STAGE_DIR/usr/bin/sbagenx-gui"
fi

# Keep helper scripts executable in installed tree.
if [ -d "$STAGE_DIR/usr/share/${PKG_NAME}/scripts" ]; then
    find "$STAGE_DIR/usr/share/${PKG_NAME}/scripts" -type f -exec chmod 755 {} +
fi

INSTALLED_SIZE=$(du -sk "$STAGE_DIR" | awk '{print $1}')

GUI_DEPENDS=""
GUI_DESC=""
if [ "$INCLUDE_GUI" = "1" ]; then
    GUI_DEPENDS=", libwebkit2gtk-4.1-0, libgtk-3-0"
    GUI_DESC=" This package also installs the SBaGenX desktop GUI."
fi

cat > "$STAGE_DIR/DEBIAN/control" <<CONTROL
Package: ${PKG_NAME}
Version: ${PKG_FULL_VERSION}
Section: sound
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: ${MAINTAINER}
Depends: libc6 (>= 2.17), libasound2 (>= 1.0.16)${GUI_DEPENDS}
Recommends: python3, python3-cairo
Installed-Size: ${INSTALLED_SIZE}
Homepage: ${HOMEPAGE_URL}
Description: Sequenced Brainwave Generator command-line synthesizer
 SBaGenX is a command-line tool for generating binaural beats,
 isochronic tones, and scripted brainwave sessions.
 It is a continuation of the SBaGen lineage for modern systems.
 .
 This package also installs the reusable sbagenxlib C library,
 public headers, and pkg-config metadata for frontend/binding development.
${GUI_DESC}
CONTROL

# Optional package metadata integrity list.
(
    cd "$STAGE_DIR" || exit 1
    find usr -type f -print0 | xargs -0 md5sum > DEBIAN/md5sums
)

create_dir_if_not_exists "$(dirname "$OUTPUT_DEB")"

if dpkg-deb --help 2>/dev/null | grep -q -- '--root-owner-group'; then
    dpkg-deb --root-owner-group --build "$STAGE_DIR" "$OUTPUT_DEB"
else
    dpkg-deb --build "$STAGE_DIR" "$OUTPUT_DEB"
fi

check_error "Failed to build Debian package"

success "Debian package created: $OUTPUT_DEB"
info "Install with: sudo apt install ./$(basename "$OUTPUT_DEB")"
