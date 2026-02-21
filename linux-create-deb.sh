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

if [ -z "$OUTPUT_DEB" ]; then
    OUTPUT_DEB="dist/${PKG_NAME}_${PKG_FULL_VERSION}_${DEB_ARCH}.deb"
fi

BUILD_ROOT="build/deb"
STAGE_DIR="${BUILD_ROOT}/stage"

info "Using binary: $BINARY_PATH"
info "Debian architecture: $DEB_ARCH"
info "Package version: $PKG_FULL_VERSION"
info "Output package: $OUTPUT_DEB"

rm -rf "$STAGE_DIR"

create_dir_if_not_exists "$STAGE_DIR/DEBIAN"
create_dir_if_not_exists "$STAGE_DIR/usr/bin"
create_dir_if_not_exists "$STAGE_DIR/usr/share/${PKG_NAME}/docs"
create_dir_if_not_exists "$STAGE_DIR/usr/share/${PKG_NAME}/examples"
create_dir_if_not_exists "$STAGE_DIR/usr/share/${PKG_NAME}/scripts"
create_dir_if_not_exists "$STAGE_DIR/usr/share/${PKG_NAME}/assets"
create_dir_if_not_exists "$STAGE_DIR/usr/share/doc/${PKG_NAME}"

# Program binary
cp "$BINARY_PATH" "$STAGE_DIR/usr/bin/${APP_NAME}"

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

# Keep helper scripts executable in installed tree.
if [ -d "$STAGE_DIR/usr/share/${PKG_NAME}/scripts" ]; then
    find "$STAGE_DIR/usr/share/${PKG_NAME}/scripts" -type f -exec chmod 755 {} +
fi

INSTALLED_SIZE=$(du -sk "$STAGE_DIR" | awk '{print $1}')

cat > "$STAGE_DIR/DEBIAN/control" <<CONTROL
Package: ${PKG_NAME}
Version: ${PKG_FULL_VERSION}
Section: sound
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: ${MAINTAINER}
Depends: libc6 (>= 2.17), libasound2 (>= 1.0.16)
Recommends: python3, python3-cairo
Installed-Size: ${INSTALLED_SIZE}
Homepage: ${HOMEPAGE_URL}
Description: Sequenced Brainwave Generator command-line synthesizer
 SBaGenX is a command-line tool for generating binaural beats,
 isochronic tones, and scripted brainwave sessions.
 It is a continuation of the SBaGen lineage for modern systems.
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
