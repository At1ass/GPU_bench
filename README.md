# GPU Benchmark

Cross-platform OpenGL benchmark for comparing GPU performance across different hardware and driver configurations. Targets wide compatibility: from old integrated GPUs (GeForce 6000+, Radeon HD 2000+) to modern discrete cards, on systems from Debian 8 / Windows XP to current releases.

## Features

- **30 GPU tests** covering fill rate, geometry, texturing, compute, draw call overhead, and more
- **5 preset levels** (Light / Medium / Heavy / Ultra / Extreme) for reproducible results
- **Four renderers**: GL2 (OpenGL 2.0), GL3 (3.0+), GL4 (4.3+), GLES (2.0/3.0) with capability-based test filtering
- **Demo mode** — 3DMark-style visual benchmark with "Sanctuary" scene (Stanford Bunny), 4 quality tiers, and composite scoring
- **Engine layer** with PassContext, RenderState, StateCache for reduced GL overhead and redundant call elimination
- **GL debug output** (KHR_debug) with severity-based logging and RenderDoc debug groups
- **Data-driven scenes** loaded from `.scene` files (no recompilation needed)
- **Multi-level logging**: `--debug` for diagnostics, `--debug=verbose` for per-pass trace
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

3DMark-style visual benchmark with the "Sanctuary" scene and Catmull-Rom camera orbit. The demo is a separate binary (`gpu_demo`).

| Tier | GL Required | Shading | Post-FX |
|------|-------------|---------|---------|
| Basic | GL 2.1 | Blinn-Phong, no shadows, shell fur | None |
| Enhanced | GL 3.0+ | + shadow map (1024), SSAO, instanced grass | Bloom, Reinhard tonemap |
| Quality | GL 3.3+ | + PCF shadows (2048), 3 point lights, normal maps, 48 fur shells | + DoF |
| Ultra | GL 4.3+ | Cook-Torrance PBR, PCSS (4096), compute particles, GTAO, tessellation | Volumetric fog, HDR + ACES tonemap, SSR, DoF |

Scene: ~25 objects (Stanford Bunny on pedestal, columns, arch, rocks, grass, trees, puddles) loaded from `data/scenes/sanctuary.scene`. Scoring: geometric mean of normalized FPS across tiers x 10000.

```bash
./gpu_demo                                       # Auto-detect tiers
./gpu_demo --demo-tier 3                         # Specific tier
./gpu_demo --demo-duration 20                    # 20 sec per tier
./gpu_demo --headless --output json              # Headless demo with JSON export
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

# Demo mode (separate binary)
./gpu_demo

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
  --debug                                      Enable debug logging
  --gpu <index>                                Select GPU by index
  --list-gpus                                  List available GPUs and exit
  --help, -h                                   Show help
```

The demo mode is a separate binary with its own options:

```
gpu_demo [options]

Options:
  --demo-tier <1-4>                            Run specific demo tier only
  --demo-duration <seconds>                    Duration per tier (default: 15)
  --renderer <gl2|gl3|gl4|gles|auto>           Renderer (default: auto)
  --headless                                   No GUI, run demo, print results
  --output <text|csv|json>                     Output format (default: text)
  --output-file <path>                         Write results to file
  --debug                                      Enable debug logging
  --debug=verbose                              Verbose trace logging (GL debug groups, per-pass)
  --gpu <index>                                Select GPU by index
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

## Debug Mode

`--debug` enables GL debug output (KHR_debug), shader validation warnings, and frame statistics overlay.
`--debug=verbose` (or `--debug verbose`) adds per-pass pipeline trace logging and GL notification messages.

The frame statistics panel shows real-time:
- Draw calls, state changes (applied/skipped by StateCache)
- Texture binds, RT switches, shader switches
- Compute dispatches, memory barriers
- Objects drawn/culled (frustum culling)

Both `gpu_benchmark` and `gpu_demo` accept the `--debug` flag.

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
  data/
    scenes/
      sanctuary.scene                 # Data-driven scene description
    shaders/                          # External GLSL files (with #pragma include)
    models/bunny.obj                  # Stanford Bunny (scripts/fetch_bunny.sh)
  src/
    core/
      app_config.h                    # Shared application config (--debug, --verbose, etc.)
      poll_callback.h                 # Lightweight polling interface
    engine/
      pass_context.h / .cpp           # Per-frame renderer wrapper
      render_state.h                  # Declarative GL state description
      state_cache.h / .cpp            # Redundant GL call elimination
      fullscreen_pass.h / .cpp        # Fullscreen quad pass template
      geometry_pass.h / .cpp          # Object iteration pass template
      compute_pass.h / .cpp           # Compute dispatch pass template
      draw_list.h / .cpp              # Sort-based draw ordering
      texture_slots.h                 # Fixed texture unit assignments
      frame_stats.h                   # Per-frame counters
    bench/
      main.cpp                        # Benchmark CLI entry point
      bench.h / .cpp                  # Statistics, composite score, bottleneck, GPU tier
      bench_runner.h / .cpp           # Test execution loop
      bench_ui.h / .cpp               # UI layer (UIView/UIState/UIAction)
      stress_runner.h / .cpp          # Stress test mode
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
      gl_debug.h / .cpp               # GL_KHR_debug wrapper, debug groups
      gl_extensions.h / .cpp          # Modern extension checking
      gl_profile.h                    # GL profile detection
      gpu_timer.h / .cpp              # GPU timer queries
      render_context.h / .cpp         # Window + GL context (abstract)
      gl_render_context.h / .cpp      # SDL2 + GL context (concrete)
      scoped_handle.h                 # RAII wrapper for GPU resources
    geometry/
      math_types.h                    # Vec2, Vec3, Vec4, Mat4
      mesh.h                          # Vertex data types, type-safe handles
      mesh_gen.h / .cpp               # Procedural mesh generation
      obj_loader.h / .cpp             # Wavefront OBJ parser
    demo/
      main.cpp                        # Demo CLI entry point (gpu_demo binary)
      demo_runner.h / .cpp            # Tier loop, FPS measurement, scoring
      demo_scene.h / .cpp             # Scene rendering, multi-pass pipeline
      demo_camera.h / .cpp            # Catmull-Rom camera orbit
      scene_loader.h / .cpp           # Data-driven scene loading (.scene files)
      shader_cache.h / .cpp           # Shader permutation cache
      shader_loader.h / .cpp          # External shader loading + #pragma include
      shader_program.h                # RAII shader wrapper
      demo_ui.h / .cpp                # ImGui overlay (FPS, progress, results)
      demo_export.h / .cpp            # Demo results export (text/CSV/JSON)
      demo_resources.h / .cpp         # GPU resource management
      demo_tier_config.cpp            # Tier progressive configuration
      material.h / materials.cpp      # Material definitions
      pipeline_builder.h / .cpp       # Render pipeline construction
      pass_factory.h / .cpp           # Render pass creation
      render_pass.h                   # Render pass interface
    platform/
      hwinfo.h / .cpp                 # CPU, OS detection
      gpu_select.h / .cpp             # GPU enumeration and selection
      data_path.h / .cpp              # Runtime data directory resolution
      logger.h / .cpp                 # Logging (multi-level: info, debug, trace)
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
