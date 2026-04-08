# GPU Benchmark — Building

## Requirements

- **CMake** 3.0+ (3.1+ recommended)
- **C++11** compiler (GCC 4.8+, Clang 3.3+, MSVC 2015+, MinGW-w64)
- **SDL2** development libraries
- **OpenGL** development libraries

## Quick Build (scripts)

### Linux / macOS / FreeBSD / cross-compile

```bash
./scripts/build.sh all        # Build all targets (native + mingw64 + mingw32)
./scripts/build.sh native     # Native build (auto-detects Linux/macOS/FreeBSD)
./scripts/build.sh portable   # Portable static build (SDL2 from source, no runtime deps)
./scripts/build.sh sanitize   # Debug build with ASan + UBSan (address/undefined sanitizers)
./scripts/build.sh mingw64    # Cross-compile Windows 64-bit
./scripts/build.sh mingw32    # Cross-compile Windows 32-bit (Win XP)
./scripts/build.sh clean      # Remove all build directories
```

`linux`, `macos`, `freebsd` are accepted as aliases for `native`.

### Clang build

```bash
CC=clang CXX=clang++ ./scripts/build.sh native
```

### Sanitizer build

```bash
./scripts/build.sh sanitize                    # Dedicated ASan+UBSan target
SANITIZE=1 ./scripts/build.sh native           # Environment variable
```

Builds into `build_sanitize/` with Debug + `-fsanitize=address,undefined`. Also available as CMake option: `-DENABLE_SANITIZERS=ON`.

### Windows (native)

```cmd
scripts\build.bat mingw       # Build with MinGW (MSYS2 / standalone)
scripts\build.bat msvc        # Build with Visual Studio
scripts\build.bat clean       # Remove build directory
```

Set `BUILD_TYPE=Debug` before the command for a debug build.

### Arch Linux

```bash
makepkg -si                   # Build and install from PKGBUILD
```

Binary installs to `/usr/bin/gpu_benchmark`. Runtime dependency: `sdl2`.

---

## Manual Build: Linux

### Install dependencies

**Arch Linux:**
```bash
sudo pacman -S cmake sdl2 mesa
```

**Debian / Ubuntu:**
```bash
sudo apt install cmake libsdl2-dev libgl-dev build-essential
```

On **Debian 8** (oldoldstable) with CMake 3.0.2 — the build system is designed to work:
```bash
sudo apt-get install cmake libsdl2-dev libgl1-mesa-dev g++
```

**Fedora / RHEL:**
```bash
sudo dnf install cmake SDL2-devel mesa-libGL-devel gcc-c++
```

### Build

```bash
git clone --recursive <repo-url>
cd GPU_bechmark
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

If you cloned without `--recursive`, init the ImGui submodule:
```bash
git submodule update --init
```

---

## Manual Build: Windows (MSVC)

1. Install [CMake](https://cmake.org/download/), [Visual Studio](https://visualstudio.microsoft.com/) (with C++ workload), and [SDL2 development libraries](https://github.com/libsdl-org/SDL/releases) (VC version).

2. Set `SDL2_DIR` to the directory containing SDL2's CMake config, or place SDL2 in a standard location.

3. Open a **Developer Command Prompt** (or run `vcvarsall.bat x64`):

```cmd
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Copy `SDL2.dll` next to the resulting `gpu_benchmark.exe`.

**Targeting Windows XP with MSVC:** Use Visual Studio 2017 with the `v141_xp` toolset:
```cmd
cmake .. -T v141_xp -DCMAKE_BUILD_TYPE=Release
```

---

## Manual Build: Windows (MinGW / MSYS2)

### Install (MSYS2)

Open the **MSYS2 MinGW 64-bit** shell:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-SDL2
```

For 32-bit (Windows XP target), use the **MSYS2 MinGW 32-bit** shell:
```bash
pacman -S mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-SDL2
```

### Build

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc)
```

### Standalone MinGW (without MSYS2)

If using standalone MinGW-w64 (e.g. from [winlibs.com](https://winlibs.com/)):

1. Add MinGW `bin/` to `PATH`
2. Download SDL2 development libraries (MinGW version) from [SDL releases](https://github.com/libsdl-org/SDL/releases)
3. Set `SDL2_DIR` or `CMAKE_PREFIX_PATH` to the SDL2 cmake directory

```cmd
set PATH=C:\mingw64\bin;%PATH%
set SDL2_DIR=C:\SDL2\cmake
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j%NUMBER_OF_PROCESSORS%
```

---

## Cross-Compile from Linux (MinGW-w64)

Two toolchain files are provided:
- `cmake/mingw-w64-x86_64.cmake` — 64-bit (Windows Vista+)
- `cmake/mingw-w64-i686.cmake` — 32-bit (Windows XP+)

### Install MinGW toolchain

**Arch Linux:**
```bash
sudo pacman -S mingw-w64-gcc
paru -S mingw-w64-sdl2    # AUR package
```

**Debian / Ubuntu:**
```bash
sudo apt install mingw-w64 g++-mingw-w64-x86-64
# SDL2 for MinGW: download from https://github.com/libsdl-org/SDL/releases (MinGW version)
# and extract to /usr/x86_64-w64-mingw32/
```

### Build (64-bit)

```bash
mkdir build-mingw64 && cd build-mingw64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Build (32-bit, for Windows XP)

```bash
mkdir build-mingw32 && cd build-mingw32
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-i686.cmake -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Distribution

Ship three files: `gpu_benchmark.exe` + `SDL2.dll` + `libwinpthread-1.dll`. libgcc and libstdc++ are linked statically.

```bash
# Copy DLLs from MinGW sysroot:
cp /usr/x86_64-w64-mingw32/bin/SDL2.dll .
cp /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll .
```

### Test under Wine

```bash
wine ./gpu_benchmark.exe --list-gpus
wine ./gpu_benchmark.exe --headless --preset light --test all --output json
```

**Note:** On Wayland, Wine's OpenGL may have issues with the GUI. Use X11:
```bash
GDK_BACKEND=x11 wine ./gpu_benchmark.exe
```
Headless mode works fine on Wayland without this workaround.
