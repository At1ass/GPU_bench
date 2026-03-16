# GPU Benchmark

Cross-platform OpenGL benchmark for comparing GPU performance across different hardware and driver configurations. Targets wide compatibility: from old integrated GPUs (GeForce 6000+, Radeon HD 2000+) to modern discrete cards, on systems from Debian 8 / Windows XP to current releases.

## Features

- **30 GPU tests** covering fill rate, geometry, texturing, compute, draw call overhead, and more
- **5 preset levels** (Light / Medium / Heavy / Ultra / Extreme) for reproducible results
- **Four renderers**: GL2 (OpenGL 2.0), GL3 (3.0+), GL4 (4.3+), GLES (2.0/3.0) with capability-based test filtering
- **Demo mode** — 3DMark-style visual benchmark with procedural city scene, 4 quality tiers, and composite scoring
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

### GL2 Universal (12 tests — any OpenGL 2.0+ GPU)

| Test | Unit | What it measures |
|------|------|------------------|
| Fillrate | MPix/s | Raw pixel output rate (opaque fullscreen quads, no blending) |
| Geometry | Mtri/s | Triangle throughput (dense cube grids) |
| Texturing | MTex/s | Texel fetch throughput (large textures, multi-layer) |
| Scene | FPS | Combined rendering: terrain, spheres, cubes, lighting |
| DrawCall | calls/s | Draw call overhead with per-draw uniform updates |
| DrawCallRaw | calls/s | Raw driver submission (no uniforms) |
| Overdraw | MPix/s | Alpha blend pixel throughput (blended fullscreen quads) |
| TexUpload | MB/s | Texture upload throughput (CPU -> GPU) |
| StateChange | ops/s | Shader/texture/blend state switching cost |
| Vertex | Mverts/s | Raw vertex processing throughput |
| ShaderALU | GFLOP/s | Fragment shader transcendentals (sin/cos/pow/sqrt) |
| ShaderFMA | GFLOP/s | Fragment shader pure FMA throughput (acc = acc * a + b) |

### GL3+ Tests (7 tests — OpenGL 3.0+)

| Test | Unit | What it measures |
|------|------|------------------|
| InstancedDraw | Mtri/s | Instanced rendering via glDrawElementsInstanced |
| FBOFillrate | MPix/s | FBO render-to-texture fillrate |
| MRTFill | MPix/s | Multiple render target fill throughput |
| TexArraySample | MTex/s | Texture array sampling rate |
| GeomShader | Mtri/s | Geometry shader amplification (GL 3.2+) |
| UBOSwitch | ops/s | UBO binding switch overhead |
| TransformFeedback | Mverts/s | Transform feedback vertex throughput |

### GL4+ Tests (11 tests — OpenGL 4.0+)

| Test | Unit | What it measures |
|------|------|------------------|
| ComputeFMA | GFLOP/s | Compute shader FMA throughput via SSBO (GL 4.3+) |
| ComputeBW | GB/s | Compute shader memory bandwidth (GL 4.3+) |
| ComputeSMem | GB/s | Compute shared memory bandwidth (GL 4.3+) |
| SSBOAtomics | GOps/s | SSBO atomic operation throughput (GL 4.3+) |
| ComputePrefix | GB/s | Parallel prefix sum / scan (GL 4.3+) |
| IndirectDraw | calls/s | Multi-draw indirect throughput (GL 4.3+) |
| TessellationTP | Mtri/s | Tessellation shader throughput (GL 4.0+) |
| TextureGather | MTex/s | textureGather vs texelFetch (GL 4.0+) |
| ImageLoadStore | GB/s | Image load/store vs SSBO (GL 4.2+) |
| PersistentMap | GB/s | Persistent buffer mapping throughput (GL 4.4+) |
| BindlessTex | ops/s | Bindless texture throughput (ARB extension) |

Tests requiring unsupported features are automatically disabled. For detailed test descriptions, scoring formulas, and parameters, see [docs/tests.md](docs/tests.md).

## Demo Mode

3DMark-style visual benchmark with a procedural "abandoned outpost" scene and Catmull-Rom camera flythrough.

| Tier | GL Required | Shading | Post-FX |
|------|-------------|---------|---------|
| Basic | GL 2.1 | Blinn-Phong + fog | None |
| Enhanced | GL 3.0+ | + shadow maps | Bloom, Reinhard tonemap, vignette |
| Quality | GL 3.3+ | + PCF shadows, rim light | + chromatic aberration |
| Ultra | GL 4.3+ | Cook-Torrance PBR | + god rays, ACES tonemap, film grain |

Scene: 53 procedural objects (12 buildings, 15 trees, 12 rocks, 6 lamps, 4 fences, 2 arches, terrain, water). Scoring: geometric mean of normalized FPS across tiers x 10000.

```bash
./gpu_benchmark --demo                          # Auto-detect tiers
./gpu_benchmark --demo --demo-tier 3            # Specific tier
./gpu_benchmark --demo --demo-duration 20       # 20 sec per tier
./gpu_benchmark --headless --demo --output json  # Headless demo with JSON export
```

## Presets

Fixed parameter sets ensure reproducible and comparable results across systems.

| Preset | Target hardware | Warmup | Measure | Duration |
|--------|----------------|--------|---------|----------|
| Light | Integrated GPUs, old cards | 60 frames | 300 frames | 3 sec |
| Medium | Mid-range (default) | 120 frames | 600 frames | 5 sec |
| Heavy | High-end discrete GPUs | 120 frames | 600 frames | 5 sec |
| Ultra | Top-tier, high-VRAM cards | 180 frames | 900 frames | 8 sec |
| Extreme | Flagship GPUs | 240 frames | 1200 frames | 10 sec |

The benchmark auto-classifies your GPU into a tier (legacy/low/mid/high/ultra) using a quick probe at startup and suggests the appropriate preset. If the selected preset exceeds hardware capabilities (e.g. texture size too large), validation will show an error.

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

# Demo mode
./gpu_benchmark --demo

# List available GPUs
./gpu_benchmark --list-gpus
```

## Usage

```
gpu_benchmark [options]

Options:
  --preset <light|medium|heavy|ultra|extreme>  Preset (default: medium)
  --renderer <gl2|gl3|gl4|gles|auto>           Renderer (default: auto)
  --config <path>                              Load config INI file
  --headless                                   No GUI, run tests, print results
  --test <name,...>                             Run specific tests (comma-separated, or "all")
  --output <text|csv|json>                     Output format (default: text)
  --output-file <path>                         Write results to file
  --width <n>                                  Window width (default: 800)
  --height <n>                                 Window height (default: 600)
  --render-res <WxH>                           Render resolution (e.g. 1920x1080, default: native)
  --timing <sync|gpu>                          Timing mode: sync (CPU+glFinish) or gpu (GL_TIME_ELAPSED)
  --stress <seconds>                           Stress test mode (headless only)
  --demo                                       Visual demo benchmark (3DMark-style)
  --demo-tier <1-4>                            Run specific demo tier only
  --demo-duration <seconds>                    Duration per tier (default: 15)
  --gpu <index>                                Select GPU by index
  --list-gpus                                  List available GPUs and exit
  --help, -h                                   Show help
```

For detailed usage examples and configuration, see [docs/usage.md](docs/usage.md).

## Results and Scoring

The benchmark provides per-test statistics:

- **Score** in test-specific units (MPix/s, Mtri/s, etc.)
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
./scripts/build.sh native     # Linux / macOS / FreeBSD
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

| Info | Linux / macOS / FreeBSD | Windows |
|------|-------------------------|---------|
| CPU | `/proc/cpuinfo`, `sysctl` | Registry `CentralProcessor\0` |
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
    build.sh                          # Linux / macOS / FreeBSD / cross-compile
    build.bat                         # Windows native build script
  cmake/
    mingw-w64-x86_64.cmake           # MinGW 64-bit toolchain
    mingw-w64-i686.cmake             # MinGW 32-bit toolchain (Win XP)
  extern/
    imgui/                            # ImGui v1.89.9 (git submodule)
  src/
    main.cpp                          # CLI parsing, entry point
    app.h / app.cpp                   # Application coordinator
    ui/
      bench_ui.h / .cpp               # UI layer (UIView/UIState/UIAction)
    bench/
      bench.h / .cpp                  # Statistics, composite score, bottleneck, GPU tier
      bench_runner.h / .cpp           # Test execution loop
      stress_runner.h / .cpp          # Stress test mode
      poll_callback.h                 # Lightweight polling interface
      preset.h / .cpp                 # Preset definitions, validation
      preset_io.h / .cpp              # INI config save/load
      results.h / .cpp                # Text/CSV/JSON export, OutputFormat
    tests/
      tests.h                         # Base test classes + helpers
      test_registry.h / .cpp / .def   # X-macro test registry with capability flags
      test_*.cpp                      # 30 test implementations
    renderer/
      renderer.h                      # Abstract renderer interface
      features.h                      # GL3Features, GL4Features, ComputeFeatures
      renderer_backend.h              # RendererBackend enum
      renderer_factory.h / .cpp       # Auto-detection and creation
      gl2_renderer.h / .cpp           # OpenGL 2.0 renderer (base)
      gl3_renderer.h / .cpp           # OpenGL 3.0+ (VAO, instancing, MRT, UBO, GS, TF)
      gl4_renderer.h / .cpp           # OpenGL 4.3+ (compute, SSBO, indirect, tessellation)
      gles_renderer.h / .cpp          # OpenGL ES 2.0/3.0
      gl_funcs.h / .cpp               # GL function loading (X-macro)
      gl_loader.h / .cpp              # imgl3w loader
      gpu_timer.h / .cpp              # GPU timer queries
      render_context.h / .cpp         # Window + GL context (abstract)
      gl_render_context.h / .cpp      # SDL2 + GL context (concrete)
      scoped_handle.h                 # RAII wrapper for GPU resources
    geometry/
      math_types.h                    # Vec2, Vec3, Vec4, Mat4
      mesh.h                          # Vertex data types, type-safe handles
      mesh_gen.h / .cpp               # Procedural mesh generation
    demo/
      demo_runner.h / .cpp            # Tier loop, FPS measurement, scoring
      demo_scene.h / .cpp             # Procedural city scene, multi-pass rendering
      demo_camera.h / .cpp            # Catmull-Rom camera path
      demo_shaders.h / .cpp           # GLSL programs (T1-T4 variants)
      demo_ui.h / .cpp                # ImGui overlay (FPS, progress, results)
      demo_export.h / .cpp            # Demo results export (text/CSV/JSON)
    platform/
      hwinfo.h / .cpp                 # CPU, OS detection
      gpu_select.h / .cpp             # GPU enumeration and selection
      logger.h / .cpp                 # Logging
      timer.h                         # High-resolution timer
      compat.h                        # Platform compatibility, RAII wrappers
```

## Compatibility

### Minimum GPU requirements

- OpenGL 2.0 support (GeForce 6000+, Radeon HD 2000+, Intel GMA 950+)
- For GL3 renderer: OpenGL 3.0+ (GeForce 8000+, Radeon HD 2000+, Intel HD 2000+)
- For GL4 renderer: OpenGL 4.3+ (GeForce 400+, Radeon HD 5000+, Intel Haswell+)

### Tested platforms

- **Linux**: Arch Linux (kernel 6.x)
- **macOS**: Apple M1 (ARM, via Metal-backed GL)
- **FreeBSD**: Supported (Mesa)
- **Windows**: Windows 11, Wine 11.x (cross-compiled)
- **Drivers**: NVIDIA proprietary, Mesa RadeonSI

### Known limitations

- Windows GPU selection is advisory (requires driver control panel for actual switching)
- Wine: DXGI enumeration works, but may show only the host GPU
- **PRIME offloading** (`DRI_PRIME`): buffer sharing between NVIDIA proprietary (display) and Mesa (render) drivers is not supported — the window will be black, but `--headless` works correctly
- `glTexSubImage2D` on very old drivers may be slow (TexUpload test)
- ShaderALU/ShaderFMA tests require GLSL 1.20 support
- macOS: GL3 GPU timer queries may be unreliable via Metal translation layer

## License

TBD
