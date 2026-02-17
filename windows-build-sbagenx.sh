#!/bin/bash

# SBaGenX Windows build script
# Builds 32-bit and 64-bit Windows binaries with FLAC, MP3 and OGG support using MinGW

# Source common library
. ./lib.sh

section_header "Building SBaGenX for Windows (32-bit and 64-bit) with FLAC, MP3 and OGG support..."

# Check for MinGW cross-compilers
if ! command -v i686-w64-mingw32-gcc &> /dev/null || ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    error "MinGW cross-compilers not found. Please install them."
    info "On Debian/Ubuntu: sudo apt-get install mingw-w64"
    info "On Fedora: sudo dnf install mingw64-gcc mingw32-gcc"
    info "On Arch: sudo pacman -S mingw-w64-gcc"
    exit 1
fi

# Create libs directory if it doesn't exist
create_dir_if_not_exists "libs"

# Check distribution directory
create_dir_if_not_exists "dist"

# Get version from VERSION file
VERSION=$(cat VERSION)

# Extract numeric version and build number for RC file
NUMERIC_VERSION=$(echo $VERSION | sed 's/-.*$//')
BUILD_DATE=$(echo $VERSION | sed -n 's/.*-dev\.\([0-9]\{8\}\)\..*$/\1/p')
BUILD_NUMBER="0"

if [ ! -z "$BUILD_DATE" ]; then
    # Use last 4 digits of date as build number (avoid overflow)
    BUILD_NUMBER=$(echo $BUILD_DATE | tail -c 5)  # Gets "0606"
fi

# Create version for RC file (format: major,minor,patch,build)
VERSION_RC=$(echo $NUMERIC_VERSION | sed 's/\./,/g'),$BUILD_NUMBER

# Create resource file with version information
cat > /tmp/sbagen.rc << EOF
#include <windows.h>

// Include icon
1 ICON "assets/sbagenx.ico"

VS_VERSION_INFO VERSIONINFO
FILEVERSION     $VERSION_RC
PRODUCTVERSION  $VERSION_RC
FILEFLAGSMASK   VS_FFI_FILEFLAGSMASK
FILEFLAGS       0
FILEOS          VOS__WINDOWS32
FILETYPE        VFT_APP
FILESUBTYPE     0
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904E4"
        BEGIN
            VALUE "CompanyName",      "SBaGenX"
            VALUE "FileDescription",  "SBaGenX Brainwave Generator"
            VALUE "FileVersion",      "$VERSION"
            VALUE "InternalName",     "sbagenx"
            VALUE "LegalCopyright",   "GPLv2"
            VALUE "OriginalFilename", "sbagenx.exe"
            VALUE "ProductName",      "SBaGenX"
            VALUE "ProductVersion",   "$VERSION"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1252
    END
END
EOF

# Compile resource file for both architectures
i686-w64-mingw32-windres /tmp/sbagen.rc -O coff -o /tmp/sbagen32.res
x86_64-w64-mingw32-windres /tmp/sbagen.rc -O coff -o /tmp/sbagen64.res

# Define paths for libraries
LIB_PATH_32="libs/windows-win32-libmad.a"
LIB_PATH_64="libs/windows-win64-libmad.a"
OGG_LIB_PATH_32="libs/windows-win32-libogg.a"
OGG_LIB_PATH_64="libs/windows-win64-libogg.a"
TREMOR_LIB_PATH_32="libs/windows-win32-libvorbisidec.a"
TREMOR_LIB_PATH_64="libs/windows-win64-libvorbisidec.a"
SNDFILE_STATIC_LIB_PATH_32="libs/windows-win32-libsndfile.a"
SNDFILE_STATIC_LIB_PATH_64="libs/windows-win64-libsndfile.a"
LAME_STATIC_LIB_PATH_32="libs/windows-win32-libmp3lame.a"
LAME_STATIC_LIB_PATH_64="libs/windows-win64-libmp3lame.a"
RUNTIME_DLL_DIR_32="libs/windows-win32-runtime"
RUNTIME_DLL_DIR_64="libs/windows-win64-runtime"

STATIC_ENCODERS_32=0
STATIC_ENCODERS_64=0

find_mingw_dll() {
    local target="$1"
    shift
    local gcc_bin="${target}-gcc"
    local sysroot=""
    local gcc_path=""
    local gcc_dir=""
    local mingw_prefix=""
    local dll
    local candidate

    case "$target" in
        i686-w64-mingw32) mingw_prefix="mingw32" ;;
        x86_64-w64-mingw32) mingw_prefix="mingw64" ;;
        *) mingw_prefix="" ;;
    esac

    if command -v "$gcc_bin" >/dev/null 2>&1; then
        gcc_path=$(command -v "$gcc_bin" 2>/dev/null)
        gcc_dir=$(dirname "$gcc_path")
        sysroot=$($gcc_bin -print-sysroot 2>/dev/null)
    fi

    for dll in "$@"; do
        for candidate in \
            "${gcc_dir}/${dll}" \
            "${gcc_dir}/../bin/${dll}" \
            "/${mingw_prefix}/bin/${dll}" \
            "/usr/${target}/bin/${dll}" \
            "/usr/${target}/lib/${dll}" \
            "${sysroot}/bin/${dll}" \
            "${sysroot}/usr/bin/${dll}" \
            "${sysroot}/${target}/bin/${dll}"; do
            if [ -n "$candidate" ] && [ -f "$candidate" ]; then
                echo "$candidate"
                return 0
            fi
        done

        if [ -n "$mingw_prefix" ]; then
            candidate=$(find "/${mingw_prefix}" -maxdepth 7 -type f -name "$dll" 2>/dev/null | head -n 1)
            if [ -n "$candidate" ] && [ -f "$candidate" ]; then
                echo "$candidate"
                return 0
            fi
        fi

        candidate=$(find /usr -maxdepth 7 -type f -name "$dll" -path "*${target}*" 2>/dev/null | head -n 1)
        if [ -n "$candidate" ] && [ -f "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

copy_runtime_dll() {
    local arch_tag="$1"
    local target="$2"
    local output_name="$3"
    shift 3
    local src
    local dll
    local manual_dir=""
    local project_dir=""

    if [ "$arch_tag" = "win32" ]; then
        manual_dir="${SBAGENX_WIN32_DLL_DIR}"
        project_dir="${RUNTIME_DLL_DIR_32}"
    elif [ "$arch_tag" = "win64" ]; then
        manual_dir="${SBAGENX_WIN64_DLL_DIR}"
        project_dir="${RUNTIME_DLL_DIR_64}"
    fi

    if [ -n "$manual_dir" ]; then
        for dll in "$@"; do
            if [ -f "${manual_dir}/${dll}" ]; then
                src="${manual_dir}/${dll}"
                break
            fi
        done
    fi

    if [ -z "$src" ] && [ -n "$project_dir" ]; then
        for dll in "$@"; do
            if [ -f "${project_dir}/${dll}" ]; then
                src="${project_dir}/${dll}"
                break
            fi
        done
    fi

    if [ -z "$src" ]; then
        src=$(find_mingw_dll "$target" "$@")
    fi

    if [ -n "$src" ] && [ -f "$src" ]; then
        cp "$src" "dist/${output_name}-${arch_tag}.dll"
        success "Bundled runtime DLL: dist/${output_name}-${arch_tag}.dll"
    else
        warning "Could not find ${output_name} runtime DLL for ${target}; installer will rely on system-provided runtime"
        warning "Hint: add DLLs under ${project_dir}/, or set SBAGENX_${arch_tag^^}_DLL_DIR to a folder containing one of: $*"
    fi
}

# Build 32-bit version
section_header "Building 32-bit version..."

# Set up compilation flags for 32-bit
CFLAGS_32="-DT_MINGW -DFLAC_DECODE -Wall -O3 -I. -Ilibs"
LIBS_32="-lwinmm"

# Check for MP3 support (32-bit)
if [ -f "$LIB_PATH_32" ]; then
    info "Including MP3 support for 32-bit using: $LIB_PATH_32"
    CFLAGS_32="$CFLAGS_32 -DMP3_DECODE"
    LIBS_32="$LIBS_32 $LIB_PATH_32"
else
    warning "MP3 library not found at $LIB_PATH_32"
    warning "MP3 support will not be included in 32-bit build"
    warning "Run ./windows-build-libs.sh to build the required libraries"
fi

# Check for OGG support (32-bit)
if [ -f "$OGG_LIB_PATH_32" ] && [ -f "$TREMOR_LIB_PATH_32" ]; then
    info "Including OGG support for 32-bit using: $OGG_LIB_PATH_32 and $TREMOR_LIB_PATH_32"
    CFLAGS_32="$CFLAGS_32 -DOGG_DECODE"
    # Order is important: first tremor, then ogg
    LIBS_32="$LIBS_32 $TREMOR_LIB_PATH_32 $OGG_LIB_PATH_32"
else
    warning "OGG libraries not found at $OGG_LIB_PATH_32 or $TREMOR_LIB_PATH_32"
    warning "OGG support will not be included in 32-bit build"
    warning "Run ./windows-build-libs.sh to build the required libraries"
fi

# Optional static output encoder path (single-binary friendly)
if [ -f "$SNDFILE_STATIC_LIB_PATH_32" ] && [ -f "$LAME_STATIC_LIB_PATH_32" ]; then
    info "Including static output encoders for 32-bit using: $SNDFILE_STATIC_LIB_PATH_32 and $LAME_STATIC_LIB_PATH_32"
    CFLAGS_32="$CFLAGS_32 -DSTATIC_OUTPUT_ENCODERS"
    LIBS_32="$LIBS_32 $SNDFILE_STATIC_LIB_PATH_32 $LAME_STATIC_LIB_PATH_32 ${SBAGENX_STATIC_ENCODER_DEPS_WIN32}"
    STATIC_ENCODERS_32=1
else
    warning "Static encoder libs not found for 32-bit at $SNDFILE_STATIC_LIB_PATH_32 or $LAME_STATIC_LIB_PATH_32"
    warning "Falling back to runtime-loaded encoder DLLs for 32-bit output encoding"
fi

# Compile 32-bit version
info "Compiling 32-bit version with flags: $CFLAGS_32"
info "Libraries: $LIBS_32"

# Replace VERSION with the actual version number
sed "s/__VERSION__/\"$VERSION\"/" sbagenx.c > sbagenx.tmp.c

i686-w64-mingw32-gcc $CFLAGS_32 sbagenx.tmp.c /tmp/sbagen32.res -o dist/sbagenx-win32.exe $LIBS_32

if [ $? -eq 0 ]; then
    success "32-bit compilation successful! Created 32-bit binary: dist/sbagenx-win32.exe"
else
    error "32-bit compilation failed!"
fi

# Build 64-bit version
section_header "Building 64-bit version..."

# Set up compilation flags for 64-bit
CFLAGS_64="-DT_MINGW -DFLAC_DECODE -Wall -O3 -I. -Ilibs"
LIBS_64="-lwinmm"

# Check for MP3 support (64-bit)
if [ -f "$LIB_PATH_64" ]; then
    info "Including MP3 support for 64-bit using: $LIB_PATH_64"
    CFLAGS_64="$CFLAGS_64 -DMP3_DECODE"
    LIBS_64="$LIBS_64 $LIB_PATH_64"
else
    warning "MP3 library not found at $LIB_PATH_64"
    warning "MP3 support will not be included in 64-bit build"
    warning "Run ./windows-build-libs.sh to build the required libraries"
fi

# Check for OGG support (64-bit)
if [ -f "$OGG_LIB_PATH_64" ] && [ -f "$TREMOR_LIB_PATH_64" ]; then
    info "Including OGG support for 64-bit using: $OGG_LIB_PATH_64 and $TREMOR_LIB_PATH_64"
    CFLAGS_64="$CFLAGS_64 -DOGG_DECODE"
    # Order is important: first tremor, then ogg
    LIBS_64="$LIBS_64 $TREMOR_LIB_PATH_64 $OGG_LIB_PATH_64"
else
    warning "OGG libraries not found at $OGG_LIB_PATH_64 or $TREMOR_LIB_PATH_64"
    warning "OGG support will not be included in 64-bit build"
    warning "Run ./windows-build-libs.sh to build the required libraries"
fi

# Optional static output encoder path (single-binary friendly)
if [ -f "$SNDFILE_STATIC_LIB_PATH_64" ] && [ -f "$LAME_STATIC_LIB_PATH_64" ]; then
    info "Including static output encoders for 64-bit using: $SNDFILE_STATIC_LIB_PATH_64 and $LAME_STATIC_LIB_PATH_64"
    CFLAGS_64="$CFLAGS_64 -DSTATIC_OUTPUT_ENCODERS"
    LIBS_64="$LIBS_64 $SNDFILE_STATIC_LIB_PATH_64 $LAME_STATIC_LIB_PATH_64 ${SBAGENX_STATIC_ENCODER_DEPS_WIN64}"
    STATIC_ENCODERS_64=1
else
    warning "Static encoder libs not found for 64-bit at $SNDFILE_STATIC_LIB_PATH_64 or $LAME_STATIC_LIB_PATH_64"
    warning "Falling back to runtime-loaded encoder DLLs for 64-bit output encoding"
fi

# Compile 64-bit version
info "Compiling 64-bit version with flags: $CFLAGS_64"
info "Libraries: $LIBS_64"

x86_64-w64-mingw32-gcc $CFLAGS_64 sbagenx.tmp.c /tmp/sbagen64.res -o dist/sbagenx-win64.exe $LIBS_64

if [ $? -eq 0 ]; then
    success "64-bit compilation successful! Created 64-bit binary: dist/sbagenx-win64.exe"
else
    error "64-bit compilation failed!"
fi

section_header "Bundling optional runtime encoder DLLs..."
if [ "$STATIC_ENCODERS_32" -eq 1 ]; then
    info "Skipping win32 runtime DLL bundle (static output encoders linked)"
else
    copy_runtime_dll "win32" "i686-w64-mingw32" "libsndfile" "libsndfile-1.dll" "sndfile.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libmp3lame" "libmp3lame-0.dll" "libmp3lame.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libFLAC" "libFLAC.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libmpg123-0" "libmpg123-0.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libogg-0" "libogg-0.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libopus-0" "libopus-0.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libvorbis-0" "libvorbis-0.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libvorbisenc-2" "libvorbisenc-2.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libgcc_s_dw2-1" "libgcc_s_dw2-1.dll"
    copy_runtime_dll "win32" "i686-w64-mingw32" "libwinpthread-1" "libwinpthread-1.dll"
fi
if [ "$STATIC_ENCODERS_64" -eq 1 ]; then
    info "Skipping win64 runtime DLL bundle (static output encoders linked)"
else
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libsndfile" "libsndfile-1.dll" "sndfile.dll"
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libmp3lame" "libmp3lame-0.dll" "libmp3lame.dll"
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libFLAC" "libFLAC.dll"
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libmpg123-0" "libmpg123-0.dll"
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libogg-0" "libogg-0.dll"
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libopus-0" "libopus-0.dll"
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libvorbis-0" "libvorbis-0.dll"
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libvorbisenc-2" "libvorbisenc-2.dll"
    copy_runtime_dll "win64" "x86_64-w64-mingw32" "libwinpthread-1" "libwinpthread-1.dll"
fi

# Clean up temporary files
rm -f /tmp/sbagen.rc /tmp/sbagen32.res /tmp/sbagen64.res sbagenx.tmp.c

section_header "Build process completed!" 
