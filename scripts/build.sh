#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"

usage() {
    echo "Usage: $0 <target>"
    echo ""
    echo "Targets:"
    echo "  all             Build all targets (native + mingw64 + mingw32)"
    echo "  native          Native build (auto-detects Linux/macOS/FreeBSD)"
    echo "  linux           Alias for native"
    echo "  macos           Alias for native"
    echo "  freebsd         Alias for native"
    echo "  mingw64         Cross-compile for Windows 64-bit (MinGW-w64)"
    echo "  mingw32         Cross-compile for Windows 32-bit (MinGW-w64, Win XP)"
    echo "  portable        Portable static build (SDL2 built-in, no runtime deps)"
    echo "  clean           Remove all build directories"
    echo ""
    echo "Environment:"
    echo "  BUILD_TYPE      CMake build type (default: Release)"
    echo "  JOBS            Parallel jobs (default: nproc)"
    exit 1
}

JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

# Detect build tool: use gmake on FreeBSD if available, make elsewhere
if command -v gmake >/dev/null 2>&1 && [ "$(uname -s)" = "FreeBSD" ]; then
    MAKE_CMD=gmake
else
    MAKE_CMD=make
fi

build_native() {
    local os_name
    os_name="$(uname -s)"
    local build_dir="$PROJECT_DIR/build_native"
    echo "=== Building for ${os_name} (${BUILD_TYPE}) ==="
    mkdir -p "$build_dir"
    cd "$build_dir"
    cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=1
    $MAKE_CMD -j"$JOBS"
    echo ""
    echo "Done: $build_dir/gpu_benchmark"
}

build_mingw64() {
    local build_dir="$PROJECT_DIR/build_mingw64"
    local toolchain="$PROJECT_DIR/cmake/mingw-w64-x86_64.cmake"

    if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
        echo "Error: x86_64-w64-mingw32-g++ not found. Install mingw-w64-gcc."
        exit 1
    fi

    echo "=== Cross-compiling for Windows x86_64 (${BUILD_TYPE}) ==="
    mkdir -p "$build_dir"
    cd "$build_dir"
    cmake "$PROJECT_DIR" -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    $MAKE_CMD -j"$JOBS"

    local sysroot="/usr/x86_64-w64-mingw32"
    for dll in SDL2.dll libwinpthread-1.dll; do
        if [ -f "$sysroot/bin/$dll" ]; then
            cp "$sysroot/bin/$dll" "$build_dir/"
            echo "Copied $dll"
        else
            echo "Warning: $sysroot/bin/$dll not found"
        fi
    done

    echo ""
    echo "Done: $build_dir/gpu_benchmark.exe"
}

build_mingw32() {
    local build_dir="$PROJECT_DIR/build_mingw32"
    local toolchain="$PROJECT_DIR/cmake/mingw-w64-i686.cmake"

    if ! command -v i686-w64-mingw32-g++ >/dev/null 2>&1; then
        echo "Error: i686-w64-mingw32-g++ not found. Install mingw-w64-gcc (i686)."
        exit 1
    fi

    echo "=== Cross-compiling for Windows i686 / Win XP (${BUILD_TYPE}) ==="
    mkdir -p "$build_dir"
    cd "$build_dir"
    cmake "$PROJECT_DIR" -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    $MAKE_CMD -j"$JOBS"

    local sysroot="/usr/i686-w64-mingw32"
    for dll in SDL2.dll libwinpthread-1.dll; do
        if [ -f "$sysroot/bin/$dll" ]; then
            cp "$sysroot/bin/$dll" "$build_dir/"
            echo "Copied $dll"
        else
            echo "Warning: $sysroot/bin/$dll not found"
        fi
    done

    echo ""
    echo "Done: $build_dir/gpu_benchmark.exe"
}

build_portable() {
    local build_dir="$PROJECT_DIR/build_portable"
    echo "=== Portable static build (${BUILD_TYPE}) ==="
    echo "SDL2 will be downloaded and built from source."
    mkdir -p "$build_dir"
    cd "$build_dir"
    cmake "$PROJECT_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DPORTABLE_BUILD=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=1
    $MAKE_CMD -j"$JOBS"

    echo ""
    echo "Checking dynamic dependencies:"
    ldd "$build_dir/gpu_benchmark" 2>/dev/null | grep -v 'linux-vdso\|ld-linux' || true
    echo ""
    echo "Done: $build_dir/gpu_benchmark"
}

clean() {
    echo "Cleaning build directories..."
    rm -rf "$PROJECT_DIR"/build_native "$PROJECT_DIR"/build_linux
    rm -rf "$PROJECT_DIR"/build_mingw64 "$PROJECT_DIR"/build_mingw32
    rm -rf "$PROJECT_DIR"/build_portable
    echo "Done."
}

build_all() {
    build_native
    build_mingw64
    build_mingw32
    echo ""
    echo "=== All targets built ==="
    echo "  Native:  $PROJECT_DIR/build_native/gpu_benchmark"
    echo "  Win64:   $PROJECT_DIR/build_mingw64/gpu_benchmark.exe"
    echo "  Win32:   $PROJECT_DIR/build_mingw32/gpu_benchmark.exe"
}

case "${1:-}" in
    all)                        build_all ;;
    native|linux|macos|freebsd) build_native ;;
    portable)                   build_portable ;;
    mingw64)                    build_mingw64 ;;
    mingw32)                    build_mingw32 ;;
    clean)                      clean ;;
    *)                          usage ;;
esac
