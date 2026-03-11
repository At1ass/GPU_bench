# GPU Benchmark

Cross-platform OpenGL benchmark for comparing GPU performance across different hardware and driver configurations. Targets wide compatibility: from old integrated GPUs (GeForce 6000+, Radeon HD 2000+) to modern discrete cards, on systems from Debian 8 / Windows XP to current releases.

## Features

- **10 GPU tests** covering different aspects of graphics performance
- **4 preset levels** (Light / Medium / Heavy / Ultra) for reproducible results
- **Two renderers**: OpenGL 2.0 (maximum compatibility) and OpenGL 3.0+ (VAO, instancing)
- **Multi-GPU support** with GPU selection by index (`--gpu`, `--list-gpus`)
- **Headless mode** for automated benchmarking without GUI
- **Export results** in Text, CSV, or JSON format
- **Custom presets** via INI config files
- **Hardware detection**: CPU, GPU, VRAM, OS version, GL driver info

## Tests

| Test | Unit | What it measures |
|------|------|------------------|
| Fillrate | Mpix/s | Raw pixel fill rate (alpha-blended fullscreen quads) |
| Geometry | Mtris/s | Triangle throughput (dense cube grids) |
| Texturing | Mtex/s | Texel fetch throughput (large textures, multi-layer) |
| Scene | FPS | Combined rendering: terrain, spheres, cubes, lighting |
| DrawCall | Kcalls/s | Driver/CPU overhead of draw call submission |
| Overdraw | Mpix/s | Pure alpha blend bandwidth |
| TexUpload | MB/s | Texture upload throughput (CPU -> GPU) |
| StateChange | Kcalls/s | Shader/texture/blend state switching cost |
| Vertex | Mverts/s | Raw vertex processing throughput (no fragment work) |
| ShaderALU | Gops | Fragment shader ALU (sin/cos/pow/sqrt loop) |

## Presets

Fixed parameter sets ensure reproducible and comparable results across systems.

| Preset | Target hardware | Warmup | Measure | Duration |
|--------|----------------|--------|---------|----------|
| Light | Integrated GPUs, old cards | 60 frames | 300 frames | 3 sec |
| Medium | Mid-range (default) | 120 frames | 600 frames | 5 sec |
| Heavy | High-end discrete GPUs | 120 frames | 600 frames | 5 sec |
| Ultra | Top-tier, high-VRAM cards | 180 frames | 900 frames | 8 sec |

The benchmark auto-suggests a preset based on detected VRAM, but never auto-scales parameters. If the selected preset exceeds hardware capabilities (e.g. texture size too large), validation will show an error.

## Usage

```
gpu_benchmark [options]

Options:
  --preset <light|medium|heavy|ultra>   Preset (default: medium)
  --renderer <gl2|gl3|auto>             Renderer (default: auto)
  --config <path>                       Load config INI file
  --headless                            No GUI, run tests, print results
  --test <name,...>                      Run specific tests (comma-separated, or "all")
  --output <text|csv|json>              Output format (default: text)
  --output-file <path>                  Write results to file
  --width <n>                           Viewport width (default: 800)
  --height <n>                          Viewport height (default: 600)
  --gpu <index>                         Select GPU by index
  --list-gpus                           List available GPUs and exit
  --help                                Show help
```

### Examples

List available GPUs:
```bash
./gpu_benchmark --list-gpus
```

Run all tests with the Medium preset, output to terminal:
```bash
./gpu_benchmark --headless --preset medium --test all
```

Run specific tests, save as JSON:
```bash
./gpu_benchmark --headless --preset heavy --test fillrate,geometry,scene --output json --output-file results.json
```

Run with a specific GPU and renderer:
```bash
./gpu_benchmark --gpu 1 --renderer gl2 --headless --preset light --test all
```

Interactive GUI mode (default):
```bash
./gpu_benchmark
```

## Custom Config

Presets can be saved/loaded as INI files:

```ini
[preset]
name=Custom
warmup_frames=120
measure_frames=600
min_duration_sec=5.0

[fillrate]
layers=500

[geometry]
grid_size=25

[texturing]
tex_size=1024
layers=300

[scene]
terrain_res=256
terrain_tex=1024
obj_tex=1024
sphere_segs=64
sphere_rings=32
cube_grid=10

[drawcall]
mesh_count=500
draws_per_frame=500

[overdraw]
layers=200
alpha=0.30

[texupload]
tex_size=512
uploads_per_frame=20

[statechange]
switches=200
shader_count=8
tex_count=8

[vertex]
vertex_count=500000

[shader_alu]
iterations=200
```

Load with: `./gpu_benchmark --config my_preset.ini --headless --test all`

## Building

### Requirements

- **CMake** 3.0+ (3.1+ recommended)
- **C++11** compiler (GCC 4.8+, Clang 3.3+, MSVC 2015+, MinGW-w64)
- **SDL2** development libraries
- **OpenGL** development libraries

### Linux

#### Arch Linux

```bash
sudo pacman -S cmake sdl2 mesa
```

#### Debian / Ubuntu

```bash
sudo apt install cmake libsdl2-dev libgl-dev build-essential
```

On **Debian 8** (oldoldstable) with CMake 3.0.2 — the build system is designed to work:
```bash
sudo apt-get install cmake libsdl2-dev libgl1-mesa-dev g++
```

#### Fedora / RHEL

```bash
sudo dnf install cmake SDL2-devel mesa-libGL-devel gcc-c++
```

#### Build

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

### Windows (native, MSVC)

1. Install [CMake](https://cmake.org/download/), [Visual Studio](https://visualstudio.microsoft.com/) (with C++ workload), and [SDL2 development libraries](https://github.com/libsdl-org/SDL/releases) (VC version).

2. Set `SDL2_DIR` to the directory containing SDL2's CMake config, or place SDL2 in a standard location.

```cmd
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Copy `SDL2.dll` next to the resulting `gpu_benchmark.exe`.

### Windows (cross-compile from Linux with MinGW)

Cross-compilation from Linux using MinGW-w64 produces a Windows .exe that works natively and under Wine.

#### Arch Linux

```bash
sudo pacman -S mingw-w64-gcc
paru -S mingw-w64-sdl2    # AUR package
```

#### Debian / Ubuntu

```bash
sudo apt install mingw-w64 g++-mingw-w64-x86-64
# SDL2 for MinGW: download from https://github.com/libsdl-org/SDL/releases (MinGW version)
# and extract to /usr/x86_64-w64-mingw32/
```

#### Build

```bash
mkdir build-mingw && cd build-mingw
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Copy required DLLs from the MinGW sysroot:
```bash
cp /usr/x86_64-w64-mingw32/bin/SDL2.dll .
cp /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll .
```

Distribution: `gpu_benchmark.exe` + `SDL2.dll` + `libwinpthread-1.dll` (three files). libgcc and libstdc++ are linked statically.

#### Test under Wine

```bash
wine ./gpu_benchmark.exe --list-gpus
wine ./gpu_benchmark.exe --headless --preset light --test all --output json
```

**Note:** On Wayland, Wine's OpenGL does not support the fixed-function pipeline (GUI will show a black screen). Use the X11 backend:
```bash
GDK_BACKEND=x11 wine ./gpu_benchmark.exe
```
Headless mode works fine on Wayland without this workaround.

## Multi-GPU Support

The benchmark can enumerate and select GPUs on multi-GPU systems.

### Linux

GPU selection uses environment variables set before the GL context is created:

- `DRI_PRIME=pci-DDDD_BB_DD_F` for Mesa drivers (Nouveau, RadeonSI, Intel, Zink/NVK)
- `__GLX_VENDOR_LIBRARY_NAME` for GLVND on X11
- `__EGL_VENDOR_LIBRARY_FILENAMES` for GLVND on Wayland

The process re-executes itself with the correct environment variables since GLVND requires them before library loading.

Tested configurations:
- NVIDIA proprietary + AMD integrated (Radeon Graphics) on Wayland
- Works with Nouveau, RadeonSI, Intel, llvmpipe, Zink/NVK drivers

### Windows

GPU enumeration uses DXGI (`IDXGIFactory::EnumAdapters`). Selection is advisory — a warning is shown that GPU switching on Windows typically requires driver control panel settings (NVIDIA Control Panel, AMD Radeon Settings).

## Hardware Detection

| Info | Linux | Windows |
|------|-------|---------|
| CPU | `/proc/cpuinfo` | Registry `CentralProcessor\0` |
| GPU name | `/usr/share/hwdata/pci.ids` + sysfs | DXGI adapter description |
| VRAM | NVX/ATI GL extensions, GLX_MESA_query_renderer, sysfs | DXGI `DedicatedVideoMemory` |
| OS version | `uname()` | `RtlGetVersion()` (accurate on Win 8.1+) |
| GL version | `glGetString(GL_VERSION)` | `glGetString(GL_VERSION)` |

## Project Structure

```
GPU_bechmark/
  CMakeLists.txt
  README.md
  cmake/
    mingw-w64-x86_64.cmake       # MinGW cross-compilation toolchain
  extern/
    imgui/                        # ImGui v1.89.9 (git submodule)
  src/
    main.cpp                      # CLI argument parsing, entry point
    app.h / app.cpp               # Application loop, ImGui UI, headless mode
    renderer.h                    # Abstract renderer interface
    gl2_renderer.h / .cpp         # OpenGL 2.0 renderer
    gl3_renderer.h / .cpp         # OpenGL 3.0+ renderer (inherits GL2)
    renderer_factory.h / .cpp     # GL version detection, renderer creation
    gl_funcs.h / .cpp             # GL function loader (runtime on Windows)
    bench.h / .cpp                # Benchmark harness (timing, statistics)
    tests.h                       # All test class declarations
    test_fillrate.cpp             # Fillrate test
    test_geometry.cpp             # Geometry throughput test
    test_texturing.cpp            # Texturing throughput test
    test_scene.cpp                # Combined scene test
    test_drawcall.cpp             # Draw call overhead test
    test_overdraw.cpp             # Alpha blend bandwidth test
    test_texupload.cpp            # Texture upload test
    test_statechange.cpp          # State change overhead test
    test_vertex.cpp               # Vertex throughput test
    test_shader_alu.cpp           # Shader ALU test
    preset.h / .cpp               # Preset definitions, validation
    config.h / .cpp               # INI config save/load
    results.h / .cpp              # Text/CSV/JSON export
    hwinfo.h / .cpp               # CPU, OS detection
    gpu_select.h / .cpp           # GPU enumeration and selection
    mesh.h                        # Vertex/mesh data structures
    mesh_gen.h / .cpp             # Procedural mesh generation
    math_types.h                  # Vec2, Vec3, Vec4, Mat4
    timer.h                       # High-resolution timer
    compat.h                      # Platform compatibility helpers
```

## Compatibility

### Minimum GPU requirements

- OpenGL 2.0 support (GeForce 6000+, Radeon HD 2000+, Intel GMA 950+)
- For GL3 renderer: OpenGL 3.0+ (GeForce 8000+, Radeon HD 2000+, Intel HD 2000+)

### Tested platforms

- **Linux**: Arch Linux (kernel 6.x), Debian 8+ (kernel 3.16+)
- **Windows**: Windows XP+ (native), Wine 11.x (cross-compiled)
- **Drivers**: NVIDIA proprietary, Nouveau, Mesa RadeonSI, Intel, llvmpipe, Zink/NVK

### Known limitations

- Windows GPU selection is advisory (requires driver control panel for actual switching)
- Wine: DXGI enumeration works, but may show only the host GPU
- `glTexSubImage2D` on very old drivers may be slow (TexUpload test)
- ShaderALU test requires GLSL 1.20 support

## License

TBD
