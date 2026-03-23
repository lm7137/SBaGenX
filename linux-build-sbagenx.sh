#!/bin/bash

# SBaGenX Linux build script
# Builds 32-bit and 64-bit binaries with FLAC, MP3, OGG and ALSA support on x86_64
# Builds only ARM64 binary on aarch64 platforms

# Source common library
. ./lib.sh

section_header "Building SBaGenX for Linux with FLAC, MP3, OGG and ALSA support..."

# Check for required tools
check_required_tools gcc ar

# Check distribution directory
create_dir_if_not_exists "dist"

# Detect host architecture
HOST_ARCH=$(uname -m)
info "Detected host architecture: $HOST_ARCH"

# Define paths for libraries
LIB_PATH_32="libs/linux-x86-libmad.a"
LIB_PATH_64="libs/linux-x86_64-libmad.a"
LIB_PATH_ARM64="libs/linux-arm64-libmad.a"
OGG_LIB_PATH_32="libs/linux-x86-libogg.a"
OGG_LIB_PATH_64="libs/linux-x86_64-libogg.a"
OGG_LIB_PATH_ARM64="libs/linux-arm64-libogg.a"
TREMOR_LIB_PATH_32="libs/linux-x86-libvorbisidec.a"
TREMOR_LIB_PATH_64="libs/linux-x86_64-libvorbisidec.a"
TREMOR_LIB_PATH_ARM64="libs/linux-arm64-libvorbisidec.a"

# Get the version number from the VERSION file
VERSION=$(cat VERSION)
SO_MAJOR=$(printf '%s' "$VERSION" | sed -n 's/^\([0-9][0-9]*\).*/\1/p')
PLOT_SCRIPT_SRC="scripts/sbagenx_plot.py"

# Skip 32-bit build on ARM64
SKIP_32BIT=0
if [ "$HOST_ARCH" = "aarch64" ]; then
    SKIP_32BIT=1
    warning "32-bit compilation is not supported on ARM64, skipping..."
fi

shared_link_libs_are_safe() {
    local arch_flag="$1"
    shift
    local libs=("$@")
    local tmp_c tmp_o tmp_so tmp_log

    tmp_c="$(mktemp /tmp/sbx-shared-probe-XXXXXX.c)"
    tmp_o="${tmp_c%.c}.o"
    tmp_so="${tmp_c%.c}.so"
    tmp_log="${tmp_c%.c}.log"

    printf 'int sbx_shared_probe(void) { return 0; }\n' > "$tmp_c"
    if ! gcc $arch_flag -fPIC -c "$tmp_c" -o "$tmp_o" >/dev/null 2>"$tmp_log"; then
        rm -f "$tmp_c" "$tmp_o" "$tmp_so" "$tmp_log"
        return 1
    fi
    if ! gcc -shared $arch_flag -o "$tmp_so" "$tmp_o" \
        -Wl,--whole-archive "${libs[@]}" -Wl,--no-whole-archive \
        -lm -ldl \
        >/dev/null 2>"$tmp_log"; then
        rm -f "$tmp_c" "$tmp_o" "$tmp_so" "$tmp_log"
        return 1
    fi
    if grep -Eiq 'recompile with -fPIC|DT_TEXTREL|text reloc' "$tmp_log"; then
        rm -f "$tmp_c" "$tmp_o" "$tmp_so" "$tmp_log"
        return 1
    fi

    rm -f "$tmp_c" "$tmp_o" "$tmp_so" "$tmp_log"
    return 0
}

# Build 32-bit version
if [ $SKIP_32BIT = 0 ]; then
    section_header "Building 32-bit version..."

    # Set up compilation flags for 32-bit
    CFLAGS_32="-DT_LINUX -DFLAC_DECODE -m32 -Wall -O3 -I."
    LIBS_32="-lm -lpthread -lasound -ldl"

    # Check for MP3 support (32-bit)
    if [ -f "$LIB_PATH_32" ]; then
        info "Including MP3 support for 32-bit using: $LIB_PATH_32"
        CFLAGS_32="$CFLAGS_32 -DMP3_DECODE"
        LIBS_32="$LIBS_32 $LIB_PATH_32"
    else
        warning "MP3 library not found at $LIB_PATH_32"
        warning "MP3 support will not be included in 32-bit build"
        warning "Run ./linux-build-libs.sh to build the required libraries"
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
        warning "Run ./linux-build-libs.sh to build the required libraries"
    fi

    # Compile 32-bit version
    info "Compiling 32-bit version with flags: $CFLAGS_32"
    info "Libraries: $LIBS_32"

    # Try to compile with 32-bit support
    gcc $CFLAGS_32 sbagenx.c sbagenxlib.c -o dist/sbagenx-linux32 $LIBS_32

    if [ $? -eq 0 ]; then
        success "32-bit compilation successful! Binary created: sbagenx-linux32"
    else
        error "32-bit compilation failed! You may need to install 32-bit development libraries."
    fi
else
    warning "Skipping 32-bit build..."
fi

# Build 64-bit version
section_header "Building 64-bit version..."

# Set up compilation flags for 64-bit
if [ "$HOST_ARCH" = "aarch64" ]; then
    # On ARM64, don't use -m64 flag as it's not supported
    CFLAGS_64="-DT_LINUX -DFLAC_DECODE -Wall -O3 -I."
    info "Running on ARM64, using native gcc for 64-bit compilation"
else
    CFLAGS_64="-DT_LINUX -DFLAC_DECODE -m64 -Wall -O3 -I."
fi
LIBS_64="-lm -lpthread -lasound -ldl"

# Check for MP3 support for 64-bit or ARM64
if [ "$HOST_ARCH" = "aarch64" ]; then
    if [ -f "$LIB_PATH_ARM64" ]; then
        info "Including MP3 support for ARM64 using: $LIB_PATH_ARM64"
        CFLAGS_64="$CFLAGS_64 -DMP3_DECODE"
        LIBS_64="$LIBS_64 $LIB_PATH_ARM64"
    else
        warning "MP3 library not found at $LIB_PATH_ARM64"
        warning "MP3 support will not be included in ARM64 build"
        warning "Run ./linux-build-libs.sh to build the required libraries"
    fi
else
    if [ -f "$LIB_PATH_64" ]; then
        info "Including MP3 support for 64-bit using: $LIB_PATH_64"
        CFLAGS_64="$CFLAGS_64 -DMP3_DECODE"
        LIBS_64="$LIBS_64 $LIB_PATH_64"
    else
        warning "MP3 library not found at $LIB_PATH_64"
        warning "MP3 support will not be included in 64-bit build"
        warning "Run ./linux-build-libs.sh to build the required libraries"
    fi
fi

# Check for OGG support for 64-bit or ARM64
if [ "$HOST_ARCH" = "aarch64" ]; then
    if [ -f "$OGG_LIB_PATH_ARM64" ] && [ -f "$TREMOR_LIB_PATH_ARM64" ]; then
        info "Including OGG support for ARM64 using: $OGG_LIB_PATH_ARM64 and $TREMOR_LIB_PATH_ARM64"
        CFLAGS_64="$CFLAGS_64 -DOGG_DECODE"
        # Order is important: first tremor, then ogg
        LIBS_64="$LIBS_64 $TREMOR_LIB_PATH_ARM64 $OGG_LIB_PATH_ARM64"
    else
        warning "OGG libraries not found at $OGG_LIB_PATH_ARM64 or $TREMOR_LIB_PATH_ARM64"
        warning "OGG support will not be included in ARM64 build"
        warning "Run ./linux-build-libs.sh to build the required libraries"
    fi
else
    if [ -f "$OGG_LIB_PATH_64" ] && [ -f "$TREMOR_LIB_PATH_64" ]; then
        info "Including OGG support for 64-bit using: $OGG_LIB_PATH_64 and $TREMOR_LIB_PATH_64"
        CFLAGS_64="$CFLAGS_64 -DOGG_DECODE"
        # Order is important: first tremor, then ogg
        LIBS_64="$LIBS_64 $TREMOR_LIB_PATH_64 $OGG_LIB_PATH_64"
    else
        warning "OGG libraries not found at $OGG_LIB_PATH_64 or $TREMOR_LIB_PATH_64"
        warning "OGG support will not be included in 64-bit build"
        warning "Run ./linux-build-libs.sh to build the required libraries"
    fi
fi

# Compile 64-bit version
info "Compiling 64-bit version with flags: $CFLAGS_64"
info "Libraries: $LIBS_64"

# Replace VERSION with the actual version number
sed "s/__VERSION__/\"$VERSION\"/" sbagenx.c > sbagenx.tmp.c

if [ "$HOST_ARCH" = "aarch64" ]; then
    gcc $CFLAGS_64 sbagenx.tmp.c sbagenxlib.c -o dist/sbagenx-linux-arm64 $LIBS_64
else
    gcc $CFLAGS_64 sbagenx.tmp.c sbagenxlib.c -o dist/sbagenx-linux64 $LIBS_64
fi

if [ $? -eq 0 ]; then
    if [ "$HOST_ARCH" = "aarch64" ]; then
        success "64-bit compilation successful! Created ARM64 binary: sbagenx-linux-arm64"
    else
        success "64-bit compilation successful! Created 64-bit binary: sbagenx-linux64"
    fi
else
    error "64-bit compilation failed!"
fi

# Remove the temporary file
rm -f sbagenx.tmp.c

# Build sbagenxlib static archives (Phase 1 extraction artifacts)
section_header "Building sbagenxlib static libraries..."
create_dir_if_not_exists "build/sbagenxlib"
create_dir_if_not_exists "dist/include"
create_dir_if_not_exists "dist/pkgconfig"

SBX_LIB_CFLAGS_32="$CFLAGS_32 -DSBAGENXLIB_VERSION=\"\\\"$VERSION\\\"\""
SBX_LIB_CFLAGS_64="$CFLAGS_64 -DSBAGENXLIB_VERSION=\"\\\"$VERSION\\\"\""
SBX_SHARED_CFLAGS_32="-DT_LINUX -DFLAC_DECODE -m32 -Wall -O3 -I. -DSBAGENXLIB_VERSION=\"\\\"$VERSION\\\"\""
SBX_SHARED_LIBS_32="-lm -ldl"
SBX_SHARED_CFLAGS_64="-DT_LINUX -DFLAC_DECODE -Wall -O3 -I. -DSBAGENXLIB_VERSION=\"\\\"$VERSION\\\"\""
SBX_SHARED_LIBS_64="-lm -ldl"

if [ "$HOST_ARCH" != "aarch64" ]; then
    SBX_SHARED_CFLAGS_64="-m64 $SBX_SHARED_CFLAGS_64"
fi

if [ $SKIP_32BIT = 0 ]; then
    if [ -f "$LIB_PATH_32" ] && shared_link_libs_are_safe "-m32" "$LIB_PATH_32"; then
        SBX_SHARED_CFLAGS_32="$SBX_SHARED_CFLAGS_32 -DMP3_DECODE"
        SBX_SHARED_LIBS_32="$SBX_SHARED_LIBS_32 $LIB_PATH_32"
    elif [ -f "$LIB_PATH_32" ]; then
        warning "32-bit shared sbagenxlib will be built without MP3 mix-input decode; rebuild decoder libs with ./linux-build-libs.sh to restore shared-library MP3 support"
    fi
    if [ -f "$OGG_LIB_PATH_32" ] && [ -f "$TREMOR_LIB_PATH_32" ]; then
        SBX_SHARED_CFLAGS_32="$SBX_SHARED_CFLAGS_32 -DOGG_DECODE"
        SBX_SHARED_LIBS_32="$SBX_SHARED_LIBS_32 $TREMOR_LIB_PATH_32 $OGG_LIB_PATH_32"
    fi
fi

if [ "$HOST_ARCH" = "aarch64" ]; then
    if [ -f "$LIB_PATH_ARM64" ] && shared_link_libs_are_safe "" "$LIB_PATH_ARM64"; then
        SBX_SHARED_CFLAGS_64="$SBX_SHARED_CFLAGS_64 -DMP3_DECODE"
        SBX_SHARED_LIBS_64="$SBX_SHARED_LIBS_64 $LIB_PATH_ARM64"
    elif [ -f "$LIB_PATH_ARM64" ]; then
        warning "ARM64 shared sbagenxlib will be built without MP3 mix-input decode; rebuild decoder libs with ./linux-build-libs.sh to restore shared-library MP3 support"
    fi
    if [ -f "$OGG_LIB_PATH_ARM64" ] && [ -f "$TREMOR_LIB_PATH_ARM64" ]; then
        SBX_SHARED_CFLAGS_64="$SBX_SHARED_CFLAGS_64 -DOGG_DECODE"
        SBX_SHARED_LIBS_64="$SBX_SHARED_LIBS_64 $TREMOR_LIB_PATH_ARM64 $OGG_LIB_PATH_ARM64"
    fi
else
    if [ -f "$LIB_PATH_64" ] && shared_link_libs_are_safe "-m64" "$LIB_PATH_64"; then
        SBX_SHARED_CFLAGS_64="$SBX_SHARED_CFLAGS_64 -DMP3_DECODE"
        SBX_SHARED_LIBS_64="$SBX_SHARED_LIBS_64 $LIB_PATH_64"
    elif [ -f "$LIB_PATH_64" ]; then
        warning "64-bit shared sbagenxlib will be built without MP3 mix-input decode; rebuild decoder libs with ./linux-build-libs.sh to restore shared-library MP3 support"
    fi
    if [ -f "$OGG_LIB_PATH_64" ] && [ -f "$TREMOR_LIB_PATH_64" ]; then
        SBX_SHARED_CFLAGS_64="$SBX_SHARED_CFLAGS_64 -DOGG_DECODE"
        SBX_SHARED_LIBS_64="$SBX_SHARED_LIBS_64 $TREMOR_LIB_PATH_64 $OGG_LIB_PATH_64"
    fi
fi

if [ $SKIP_32BIT = 0 ]; then
    gcc $SBX_LIB_CFLAGS_32 -c sbagenxlib.c -o build/sbagenxlib/sbagenxlib-linux32.o
    if [ $? -eq 0 ]; then
        ar rcs dist/libsbagenx-linux32.a build/sbagenxlib/sbagenxlib-linux32.o
        if [ $? -eq 0 ]; then
            success "Created sbagenxlib archive: dist/libsbagenx-linux32.a"
        else
            warning "Failed to archive dist/libsbagenx-linux32.a"
        fi
    else
        warning "Failed to compile sbagenxlib for 32-bit"
    fi
fi

section_header "Building sbagenxlib shared libraries..."
if [ -z "$SO_MAJOR" ]; then
    warning "Could not derive shared library major version from VERSION='$VERSION'"
    SO_MAJOR="0"
fi

if [ $SKIP_32BIT = 0 ]; then
    gcc $SBX_SHARED_CFLAGS_32 -fPIC -c sbagenxlib.c -o build/sbagenxlib/sbagenxlib-linux32.pic.o
    if [ $? -eq 0 ]; then
        gcc -shared -m32 -Wl,-soname,libsbagenx-linux32.so.$SO_MAJOR \
            -o dist/libsbagenx-linux32.so.$VERSION \
            build/sbagenxlib/sbagenxlib-linux32.pic.o $SBX_SHARED_LIBS_32
        if [ $? -eq 0 ]; then
            ln -sfn "libsbagenx-linux32.so.$VERSION" dist/libsbagenx-linux32.so.$SO_MAJOR
            ln -sfn "libsbagenx-linux32.so.$SO_MAJOR" dist/libsbagenx-linux32.so
            success "Created sbagenxlib shared library: dist/libsbagenx-linux32.so.$VERSION"
        else
            warning "Failed to link dist/libsbagenx-linux32.so.$VERSION"
        fi
    else
        warning "Failed to compile PIC sbagenxlib for 32-bit"
    fi
fi

if [ "$HOST_ARCH" = "aarch64" ]; then
    gcc $SBX_LIB_CFLAGS_64 -c sbagenxlib.c -o build/sbagenxlib/sbagenxlib-linux-arm64.o
    if [ $? -eq 0 ]; then
        ar rcs dist/libsbagenx-linux-arm64.a build/sbagenxlib/sbagenxlib-linux-arm64.o
        if [ $? -eq 0 ]; then
            success "Created sbagenxlib archive: dist/libsbagenx-linux-arm64.a"
        else
            warning "Failed to archive dist/libsbagenx-linux-arm64.a"
        fi
    else
        warning "Failed to compile sbagenxlib for ARM64"
    fi

    gcc $SBX_SHARED_CFLAGS_64 -fPIC -c sbagenxlib.c -o build/sbagenxlib/sbagenxlib-linux-arm64.pic.o
    if [ $? -eq 0 ]; then
        gcc -shared -Wl,-soname,libsbagenx.so.$SO_MAJOR \
            -o dist/libsbagenx.so.$VERSION \
            build/sbagenxlib/sbagenxlib-linux-arm64.pic.o $SBX_SHARED_LIBS_64
        if [ $? -eq 0 ]; then
            ln -sfn "libsbagenx.so.$VERSION" dist/libsbagenx.so.$SO_MAJOR
            ln -sfn "libsbagenx.so.$SO_MAJOR" dist/libsbagenx.so
            success "Created sbagenxlib shared library: dist/libsbagenx.so.$VERSION"
        else
            warning "Failed to link dist/libsbagenx.so.$VERSION"
        fi
    else
        warning "Failed to compile PIC sbagenxlib for ARM64"
    fi
else
    gcc $SBX_LIB_CFLAGS_64 -c sbagenxlib.c -o build/sbagenxlib/sbagenxlib-linux64.o
    if [ $? -eq 0 ]; then
        ar rcs dist/libsbagenx-linux64.a build/sbagenxlib/sbagenxlib-linux64.o
        if [ $? -eq 0 ]; then
            success "Created sbagenxlib archive: dist/libsbagenx-linux64.a"
        else
            warning "Failed to archive dist/libsbagenx-linux64.a"
        fi
    else
        warning "Failed to compile sbagenxlib for 64-bit"
    fi

    gcc $SBX_SHARED_CFLAGS_64 -fPIC -c sbagenxlib.c -o build/sbagenxlib/sbagenxlib-linux64.pic.o
    if [ $? -eq 0 ]; then
        gcc -shared -m64 -Wl,-soname,libsbagenx.so.$SO_MAJOR \
            -o dist/libsbagenx.so.$VERSION \
            build/sbagenxlib/sbagenxlib-linux64.pic.o $SBX_SHARED_LIBS_64
        if [ $? -eq 0 ]; then
            ln -sfn "libsbagenx.so.$VERSION" dist/libsbagenx.so.$SO_MAJOR
            ln -sfn "libsbagenx.so.$SO_MAJOR" dist/libsbagenx.so
            success "Created sbagenxlib shared library: dist/libsbagenx.so.$VERSION"
        else
            warning "Failed to link dist/libsbagenx.so.$VERSION"
        fi
    else
        warning "Failed to compile PIC sbagenxlib for 64-bit"
    fi
fi

cp sbagenxlib.h dist/include/sbagenxlib.h
if [ $? -eq 0 ]; then
    success "Bundled public header: dist/include/sbagenxlib.h"
else
    warning "Could not copy sbagenxlib.h into dist/include"
fi
cp sbagenlib.h dist/include/sbagenlib.h
if [ $? -eq 0 ]; then
    success "Bundled compatibility header: dist/include/sbagenlib.h"
else
    warning "Could not copy sbagenlib.h into dist/include"
fi

cat > dist/pkgconfig/sbagenxlib.pc <<EOF
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: sbagenxlib
Description: SBaGenX reusable synthesis/runtime library
Version: ${VERSION}
Libs: -L\${libdir} -lsbagenx -lm
Cflags: -I\${includedir}
EOF
if [ $? -eq 0 ]; then
    success "Bundled pkg-config metadata: dist/pkgconfig/sbagenxlib.pc"
else
    warning "Could not write dist/pkgconfig/sbagenxlib.pc"
fi

cat > dist/pkgconfig/sbagenxlib-uninstalled.pc <<EOF
prefix=\${pcfiledir}/..
exec_prefix=\${prefix}
libdir=\${prefix}
includedir=\${prefix}/include

Name: sbagenxlib-uninstalled
Description: SBaGenX reusable synthesis/runtime library (uninstalled tree)
Version: ${VERSION}
Libs: -L\${libdir} -lsbagenx -lm
Cflags: -I\${includedir}
EOF
if [ $? -eq 0 ]; then
    success "Bundled uninstalled pkg-config metadata: dist/pkgconfig/sbagenxlib-uninstalled.pc"
else
    warning "Could not write dist/pkgconfig/sbagenxlib-uninstalled.pc"
fi

# Bundle Python/Cairo plot backend script for dist binaries
section_header "Bundling plot backend script..."
if [ -f "$PLOT_SCRIPT_SRC" ]; then
    mkdir -p dist/scripts
    cp "$PLOT_SCRIPT_SRC" dist/scripts/sbagenx_plot.py
    success "Bundled plot script: dist/scripts/sbagenx_plot.py"
else
    warning "Plot script not found at $PLOT_SCRIPT_SRC"
    warning "Graph plotting may fall back to built-in renderer only"
fi

section_header "Build process completed!" 
