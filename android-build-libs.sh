#!/bin/bash

. ./lib.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ORIGINAL_DIR="$PWD"

BUILD_ROOT="$SCRIPT_DIR/build/android-codecs"
DOWNLOAD_DIR="$BUILD_ROOT/downloads"
WORK_ROOT="$BUILD_ROOT/work"
LOG_DIR="$BUILD_ROOT/logs"
OUTPUT_DIR="$SCRIPT_DIR/libs"

LIBOGG_VERSION="1.3.5"
LIBVORBISIDEC_VERSION="1.0.2+svn16259"
LIBMAD_VERSION="0.15.1b"
LIBVORBISIDEC_TARBALL="libvorbisidec_$LIBVORBISIDEC_VERSION.orig.tar.gz"

ANDROID_API="${ANDROID_API:-24}"
ANDROID_ABIS="${ANDROID_ABIS:-arm64-v8a x86_64}"

HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"

case "$HOST_OS" in
    Linux)
        HOST_TAG="linux-x86_64"
        ;;
    Darwin)
        HOST_TAG="darwin-x86_64"
        ;;
    *)
        error "Unsupported host OS '$HOST_OS'. Use Linux or macOS with the Android NDK."
        exit 1
        ;;
esac

resolve_ndk_root() {
    if [ -n "$ANDROID_NDK_ROOT" ] && [ -d "$ANDROID_NDK_ROOT" ]; then
        echo "$ANDROID_NDK_ROOT"
        return 0
    fi

    if [ -n "$ANDROID_NDK_HOME" ] && [ -d "$ANDROID_NDK_HOME" ]; then
        echo "$ANDROID_NDK_HOME"
        return 0
    fi

    if [ -n "$ANDROID_NDK" ] && [ -d "$ANDROID_NDK" ]; then
        echo "$ANDROID_NDK"
        return 0
    fi

    if [ -d "$HOME/Android/Sdk/ndk/27.1.12297006" ]; then
        echo "$HOME/Android/Sdk/ndk/27.1.12297006"
        return 0
    fi

    local latest
    latest="$(find "$HOME/Android/Sdk/ndk" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort -V | tail -n1)"
    if [ -n "$latest" ] && [ -d "$latest" ]; then
        echo "$latest"
        return 0
    fi

    return 1
}

ANDROID_NDK_ROOT="$(resolve_ndk_root)"
if [ -z "$ANDROID_NDK_ROOT" ]; then
    error "Could not find the Android NDK. Set ANDROID_NDK_ROOT or install the NDK under \$HOME/Android/Sdk/ndk."
    exit 1
fi

TOOLCHAIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG"
if [ ! -d "$TOOLCHAIN" ]; then
    error "Could not find the NDK LLVM toolchain at '$TOOLCHAIN'."
    exit 1
fi

check_required_tools curl make automake autoconf libtool tar sed

section_header "Preparing Android codec build directories..."
create_dir_if_not_exists "$BUILD_ROOT"
create_dir_if_not_exists "$DOWNLOAD_DIR"
create_dir_if_not_exists "$WORK_ROOT"
create_dir_if_not_exists "$LOG_DIR"
create_dir_if_not_exists "$OUTPUT_DIR"

download_sources() {
    cd "$DOWNLOAD_DIR"

    section_header "Downloading libogg..."
    if [ ! -f "libogg-$LIBOGG_VERSION.tar.gz" ] || ! tar -tzf "libogg-$LIBOGG_VERSION.tar.gz" >/dev/null 2>&1; then
        rm -f "libogg-$LIBOGG_VERSION.tar.gz"
        download_verified_targz \
            "libogg-$LIBOGG_VERSION.tar.gz" \
            "https://downloads.xiph.org/releases/ogg/libogg-$LIBOGG_VERSION.tar.gz"
        check_error "Failed to download libogg"
    fi

    section_header "Downloading libvorbisidec (Tremor)..."
    if [ ! -f "$LIBVORBISIDEC_TARBALL" ] || ! tar -tzf "$LIBVORBISIDEC_TARBALL" >/dev/null 2>&1; then
        rm -f "$LIBVORBISIDEC_TARBALL"
        download_verified_targz \
            "$LIBVORBISIDEC_TARBALL" \
            "https://launchpadlibrarian.net/35151187/libvorbisidec_$LIBVORBISIDEC_VERSION.orig.tar.gz" \
            "https://launchpad.net/ubuntu/+archive/primary/+files/libvorbisidec_$LIBVORBISIDEC_VERSION.orig.tar.gz"
        check_error "Failed to download libvorbisidec"
    fi

    section_header "Downloading libmad..."
    if [ ! -f "libmad-$LIBMAD_VERSION.tar.gz" ] || ! tar -tzf "libmad-$LIBMAD_VERSION.tar.gz" >/dev/null 2>&1; then
        rm -f "libmad-$LIBMAD_VERSION.tar.gz"
        download_verified_targz \
            "libmad-$LIBMAD_VERSION.tar.gz" \
            "https://downloads.sourceforge.net/project/mad/libmad/$LIBMAD_VERSION/libmad-$LIBMAD_VERSION.tar.gz"
        check_error "Failed to download libmad"
    fi
}

refresh_config_helpers() {
    local tree="$1"
    find "$tree" \( -name config.sub -o -name config.guess \) -type f | while read -r helper; do
        case "$(basename "$helper")" in
            config.sub)
                cp "$SCRIPT_DIR/libs/config.sub" "$helper"
                ;;
            config.guess)
                cp "$SCRIPT_DIR/libs/config.guess" "$helper"
                chmod +x "$helper"
                ;;
        esac
    done
}

patch_libmad_configure() {
    local configure_path="$1/configure"
    section_header "Patching libmad configure script..."
    sed -i 's/-fforce-mem//g' "$configure_path"
    sed -i 's/-fthread-jumps//g' "$configure_path"
    sed -i 's/-fcse-follow-jumps//g' "$configure_path"
    sed -i 's/-fcse-skip-blocks//g' "$configure_path"
    sed -i 's/-fregmove//g' "$configure_path"
    sed -i 's/-fexpensive-optimizations//g' "$configure_path"
    sed -i 's/-fschedule-insns2//g' "$configure_path"
    check_error "Failed to patch libmad configure script"
}

extract_sources_for_abi() {
    local abi="$1"
    local abi_work="$WORK_ROOT/$abi"
    rm -rf "$abi_work"
    mkdir -p "$abi_work"

    tar -xzf "$DOWNLOAD_DIR/libogg-$LIBOGG_VERSION.tar.gz" -C "$abi_work"
    check_error "Failed to extract libogg for $abi"

    tar -xzf "$DOWNLOAD_DIR/$LIBVORBISIDEC_TARBALL" -C "$abi_work"
    check_error "Failed to extract libvorbisidec for $abi"

    tar -xzf "$DOWNLOAD_DIR/libmad-$LIBMAD_VERSION.tar.gz" -C "$abi_work"
    check_error "Failed to extract libmad for $abi"

    local tremor_dir
    tremor_dir="$(find "$abi_work" -mindepth 1 -maxdepth 1 -type d -name 'libvorbisidec*' | head -n1)"
    if [ -z "$tremor_dir" ]; then
        error "Could not locate extracted libvorbisidec sources for $abi."
        exit 1
    fi
    if [ "$(basename "$tremor_dir")" != "libvorbisidec_$LIBVORBISIDEC_VERSION" ]; then
        mv "$tremor_dir" "$abi_work/libvorbisidec_$LIBVORBISIDEC_VERSION"
    fi

    refresh_config_helpers "$abi_work/libogg-$LIBOGG_VERSION"
    refresh_config_helpers "$abi_work/libvorbisidec_$LIBVORBISIDEC_VERSION"
    refresh_config_helpers "$abi_work/libmad-$LIBMAD_VERSION"
    patch_libmad_configure "$abi_work/libmad-$LIBMAD_VERSION"
}

set_android_toolchain() {
    local abi="$1"

    case "$abi" in
        arm64-v8a)
            TARGET="aarch64-linux-android"
            ;;
        x86_64)
            TARGET="x86_64-linux-android"
            ;;
        *)
            error "Unsupported ABI '$abi'. Supported values: arm64-v8a, x86_64"
            exit 1
            ;;
    esac

    export AR="$TOOLCHAIN/bin/llvm-ar"
    export AS="$TOOLCHAIN/bin/clang --target=$TARGET$ANDROID_API"
    export CC="$TOOLCHAIN/bin/clang --target=$TARGET$ANDROID_API"
    export CXX="$TOOLCHAIN/bin/clang++ --target=$TARGET$ANDROID_API"
    export LD="$TOOLCHAIN/bin/ld"
    export NM="$TOOLCHAIN/bin/llvm-nm"
    export RANLIB="$TOOLCHAIN/bin/llvm-ranlib"
    export STRIP="$TOOLCHAIN/bin/llvm-strip"

    export CFLAGS="-fPIC -O2 -DLITTLE_ENDIAN=1234 -DBIG_ENDIAN=4321 -DBYTE_ORDER=LITTLE_ENDIAN"
    export CPPFLAGS="-fPIC -DLITTLE_ENDIAN=1234 -DBIG_ENDIAN=4321 -DBYTE_ORDER=LITTLE_ENDIAN"
    export CXXFLAGS="-fPIC -O2 -DLITTLE_ENDIAN=1234 -DBIG_ENDIAN=4321 -DBYTE_ORDER=LITTLE_ENDIAN"
    export LDFLAGS=""
}

build_for_abi() {
    local abi="$1"
    local abi_work="$WORK_ROOT/$abi"
    local install_dir="$abi_work/install"
    local build_triple

    build_triple="$(sh "$SCRIPT_DIR/libs/config.guess")"
    set_android_toolchain "$abi"
    extract_sources_for_abi "$abi"
    mkdir -p "$install_dir"

    section_header "Building libogg for $abi..."
    cd "$abi_work/libogg-$LIBOGG_VERSION"
    ./configure \
        --host="$TARGET" \
        --build="$build_triple" \
        --prefix="$install_dir" \
        --enable-static \
        --disable-shared \
        > "$LOG_DIR/libogg-$abi.log" 2>&1
    check_error "libogg configuration for $abi failed" "$LOG_DIR/libogg-$abi.log"
    make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" >> "$LOG_DIR/libogg-$abi.log" 2>&1
    check_error "libogg compilation for $abi failed" "$LOG_DIR/libogg-$abi.log"
    make install >> "$LOG_DIR/libogg-$abi.log" 2>&1
    check_error "libogg installation for $abi failed" "$LOG_DIR/libogg-$abi.log"

    section_header "Building libvorbisidec (Tremor) for $abi..."
    cd "$abi_work/libvorbisidec_$LIBVORBISIDEC_VERSION"
    CPPFLAGS="-I$install_dir/include -fPIC" \
    LDFLAGS="-L$install_dir/lib" \
    ./autogen.sh \
        --host="$TARGET" \
        --build="$build_triple" \
        --prefix="$install_dir" \
        --enable-static \
        --disable-shared \
        --with-ogg="$install_dir" \
        > "$LOG_DIR/libvorbisidec-$abi.log" 2>&1
    check_error "Failed to generate/configure libvorbisidec ($abi)" "$LOG_DIR/libvorbisidec-$abi.log"
    make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" >> "$LOG_DIR/libvorbisidec-$abi.log" 2>&1
    check_error "libvorbisidec compilation for $abi failed" "$LOG_DIR/libvorbisidec-$abi.log"
    make install >> "$LOG_DIR/libvorbisidec-$abi.log" 2>&1
    check_error "libvorbisidec installation for $abi failed" "$LOG_DIR/libvorbisidec-$abi.log"

    section_header "Building libmad for $abi..."
    cd "$abi_work/libmad-$LIBMAD_VERSION"
    ./configure \
        --host="$TARGET" \
        --build="$build_triple" \
        --prefix="$install_dir" \
        --enable-static \
        --disable-shared \
        --disable-debugging \
        > "$LOG_DIR/libmad-$abi.log" 2>&1
    check_error "libmad configuration for $abi failed" "$LOG_DIR/libmad-$abi.log"
    make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" >> "$LOG_DIR/libmad-$abi.log" 2>&1
    check_error "libmad compilation for $abi failed" "$LOG_DIR/libmad-$abi.log"
    make install >> "$LOG_DIR/libmad-$abi.log" 2>&1
    check_error "libmad installation for $abi failed" "$LOG_DIR/libmad-$abi.log"

    cp "$install_dir/lib/libogg.a" "$OUTPUT_DIR/android-$abi-libogg.a"
    cp "$install_dir/lib/libvorbisidec.a" "$OUTPUT_DIR/android-$abi-libvorbisidec.a"
    cp "$install_dir/lib/libmad.a" "$OUTPUT_DIR/android-$abi-libmad.a"
    check_error "Failed to copy Android codec archives for $abi"
}

section_header "Android codec build configuration"
info "Host architecture: $HOST_ARCH"
info "NDK root: $ANDROID_NDK_ROOT"
info "NDK toolchain: $TOOLCHAIN"
info "Android API: $ANDROID_API"
info "Target ABIs: $ANDROID_ABIS"

download_sources

for abi in $ANDROID_ABIS; do
    build_for_abi "$abi"
done

cd "$ORIGINAL_DIR"
success "Android codec libraries built successfully."
success "Artifacts written to $OUTPUT_DIR:"
for abi in $ANDROID_ABIS; do
    info "  android-$abi-libogg.a"
    info "  android-$abi-libvorbisidec.a"
    info "  android-$abi-libmad.a"
done
