# GPU Benchmark

Cross-platform OpenGL benchmark for comparing GPU performance across different hardware and driver configurations. Targets wide compatibility: from old integrated GPUs (GeForce 6000+, Radeon HD 2000+) to modern discrete cards, on systems from Debian 8 / Windows XP to current releases.

## Features

- **12 GPU tests** covering fill rate, geometry, texturing, compute, draw call overhead, and more
- **4 preset levels** (Light / Medium / Heavy / Ultra) for reproducible results
- **Two renderers**: OpenGL 2.0 (maximum compatibility) and OpenGL 3.0+ (VAO, instancing)
- **GPU timer queries** (GL_TIME_ELAPSED) for precise GPU-side timing on GL 3.3+
- **Composite scoring** with geometric mean by category and bottleneck detection
- **Auto GPU tier classification** via quick probe at startup
- **Render sanity checks** to detect broken shaders or FBO failures
- **Stress test mode** with self-calibrating combined shader and thermal throttling detection
- **Custom render resolution** via FBO (e.g. 4K on a 1080p monitor)
- **Multi-GPU support** with GPU selection by index (`--gpu`, `--list-gpus`)
- **Headless mode** for automated benchmarking without GUI
- **Export results** in Text, CSV, or JSON format
- **Custom presets** via INI config files
- **Hardware detection**: CPU, GPU, VRAM, OS version, GL driver info
- **Arch Linux PKGBUILD** included

## Tests

| Test | Unit | What it measures |
|------|------|------------------|
| Fillrate | Mpix/s | Raw pixel output rate (opaque fullscreen quads, no blending) |
| Geometry | Mtris/s | Triangle throughput (dense cube grids) |
| Texturing | Mtex/s | Texel fetch throughput (large textures, multi-layer) |
| Scene | FPS | Combined rendering: terrain, spheres, cubes, lighting |
| DrawCall | Kcalls/s | Draw call overhead with per-draw uniform updates |
| Overdraw | Mpix/s | Alpha blend pixel throughput (blended fullscreen quads) |
| TexUpload | MB/s | Texture upload throughput (CPU -> GPU) |
| StateChange | Kcalls/s | Shader/texture/blend state switching cost |
| Vertex | Mverts/s | Raw vertex processing throughput |
| ShaderALU | Gops | Fragment shader transcendentals (sin/cos/pow/sqrt) |
| ShaderFMA | GFLOP/s | Fragment shader pure FMA throughput (acc = acc * a + b) |
| DrawCallRaw | Kcalls/s | Draw call overhead without uniform updates |

For detailed test descriptions, scoring formulas, and parameters, see [docs/tests.md](docs/tests.md).

## Presets

Fixed parameter sets ensure reproducible and comparable results across systems.

| Preset | Target hardware | Warmup | Measure | Duration |
|--------|----------------|--------|---------|----------|
| Light | Integrated GPUs, old cards | 60 frames | 300 frames | 3 sec |
| Medium | Mid-range (default) | 120 frames | 600 frames | 5 sec |
| Heavy | High-end discrete GPUs | 120 frames | 600 frames | 5 sec |
| Ultra | Top-tier, high-VRAM cards | 180 frames | 900 frames | 8 sec |

The benchmark auto-classifies your GPU into a tier (legacy/low/mid/high) using a quick probe at startup and suggests the appropriate preset. If the selected preset exceeds hardware capabilities (e.g. texture size too large), validation will show an error.

## Quick Start

```bash
# GUI mode (interactive)
./gpu_benchmark

# Run all tests headless, default preset
./gpu_benchmark --headless --test all

# Ultra preset, 4K render resolution, JSON output
./gpu_benchmark --headless --preset ultra --render-res 3840x2160 --output json --output-file results.json

# Stress test for 5 minutes
./gpu_benchmark --stress 300 --preset ultra

# List available GPUs
./gpu_benchmark --list-gpus
```

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
  --width <n>                           Window width (default: 800)
  --height <n>                          Window height (default: 600)
  --render-res <WxH>                    Render resolution (e.g. 1920x1080, default: native)
  --timing <sync|gpu>                   Timing mode: sync (CPU+glFinish) or gpu (GL_TIME_ELAPSED)
  --stress <seconds>                    Stress test mode (headless only)
  --gpu <index>                         Select GPU by index
  --list-gpus                           List available GPUs and exit
  --help                                Show help
```

For detailed usage examples and configuration, see [docs/usage.md](docs/usage.md).

## Results and Scoring

The benchmark provides per-test statistics:

- **Score** in test-specific units (Mpix/s, Mtris/s, etc.)
- **Timing**: avg, min, max, P1, median, P99
- **CV%** (coefficient of variation) — stability indicator
- **Composite score** — geometric mean by category (Fill, Geometry, Compute, Overhead)
- **Bottleneck analysis** — identifies the weakest GPU subsystem

For interpreting results, see [docs/results.md](docs/results.md).

## Stress Test

The `--stress` flag runs a continuous GPU load test with thermal throttling detection:

```bash
./gpu_benchmark --stress 300 --preset ultra --render-res 3840x2160
```

Uses a self-calibrating combined shader that stresses all GPU units simultaneously (FMA cores, SFU, texture units, ROPs, memory bandwidth). Automatically adjusts pass count per frame to saturate any GPU — from legacy integrated to modern discrete.

Reports every 10 seconds with degradation tracking relative to baseline.

## Building

### Requirements

- **CMake** 3.0+ (3.1+ recommended)
- **C++11** compiler (GCC 4.8+, Clang 3.3+, MSVC 2015+, MinGW-w64)
- **SDL2** development libraries
- **OpenGL** development libraries

### Quick Build

```bash
./scripts/build.sh linux      # Native Linux
./scripts/build.sh mingw64    # Cross-compile Windows 64-bit
./scripts/build.sh mingw32    # Cross-compile Windows 32-bit (Win XP)
./scripts/build.sh all        # All targets
./scripts/build.sh clean      # Remove build directories
```

Windows native:
```cmd
scripts\build.bat mingw       # MinGW (MSYS2 / standalone)
scripts\build.bat msvc        # Visual Studio
```

### Arch Linux

```bash
makepkg -si                   # Build and install from PKGBUILD
```

For detailed build instructions (manual CMake, cross-compilation, MSYS2, etc.), see [docs/building.md](docs/building.md).

## Custom Config

Presets can be saved/loaded as INI files. See [docs/config.md](docs/config.md) for the full format reference.

```bash
./gpu_benchmark --config my_preset.ini --headless --test all
```

## Adding Custom Tests

The benchmark has a modular test architecture. See [docs/adding-tests.md](docs/adding-tests.md) for a guide on implementing new tests.

## Multi-GPU Support

- **Linux**: DRI_PRIME / GLVND environment variables (automatic)
- **Windows**: DXGI enumeration (advisory — requires driver control panel for switching)

## Hardware Detection

| Info | Linux | Windows |
|------|-------|---------|
| CPU | `/proc/cpuinfo` | Registry `CentralProcessor\0` |
| GPU name | `pci.ids` + sysfs | DXGI adapter description |
| VRAM | NVX/ATI GL extensions, GLX_MESA | DXGI `DedicatedVideoMemory` |
| OS version | `uname()` | `RtlGetVersion()` |
| GL version | `glGetString(GL_VERSION)` | `glGetString(GL_VERSION)` |

## Project Structure

```
GPU_bechmark/
  CMakeLists.txt
  PKGBUILD                            # Arch Linux package
  README.md
  docs/                               # Documentation
  scripts/
    build.sh                          # Linux / cross-compile build script
    build.bat                         # Windows native build script
  cmake/
    mingw-w64-x86_64.cmake           # MinGW 64-bit toolchain
    mingw-w64-i686.cmake             # MinGW 32-bit toolchain (Win XP)
  extern/
    imgui/                            # ImGui v1.89.9 (git submodule)
  src/
    main.cpp                          # CLI argument parsing, entry point
    app.h / app.cpp                   # Application loop, ImGui UI, headless/stress mode
    renderer.h                        # Abstract renderer interface
    gl2_renderer.h / .cpp             # OpenGL 2.0 renderer
    gl3_renderer.h / .cpp             # OpenGL 3.0+ renderer (inherits GL2)
    renderer_factory.h / .cpp         # GL version detection, renderer creation
    gl_funcs.h / .cpp                 # GL function loader (X-macro architecture)
    bench.h / .cpp                    # Statistics, composite score, bottleneck, GPU tier
    tests.h                           # All 12 test class declarations
    test_*.cpp                        # Individual test implementations
    gpu_timer.h / .cpp                # GPU timer queries (GL_TIME_ELAPSED)
    preset.h / .cpp                   # Preset definitions, validation
    config.h / .cpp                   # INI config save/load
    results.h / .cpp                  # Text/CSV/JSON export
    hwinfo.h / .cpp                   # CPU, OS detection
    gpu_select.h / .cpp               # GPU enumeration and selection
    mesh.h / mesh_gen.h / .cpp        # Vertex data, procedural mesh generation
    math_types.h                      # Vec2, Vec3, Vec4, Mat4
    timer.h                           # High-resolution timer
    logger.h / .cpp                   # Logging utilities
    compat.h                          # Platform compatibility helpers
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
- **PRIME offloading** (`DRI_PRIME`): buffer sharing between NVIDIA proprietary (display) and Mesa (render) drivers is not supported — the window will be black, but `--headless` works correctly
- `glTexSubImage2D` on very old drivers may be slow (TexUpload test)
- ShaderALU/ShaderFMA tests require GLSL 1.20 support
- Stress test cannot reach full GPU TDP via OpenGL alone (compute shaders require GL 4.3)

## License

TBD
