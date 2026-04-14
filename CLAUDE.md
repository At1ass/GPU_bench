# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.
You are a professional Senior Game Engine Developer from ID Software, Activision and Crytech

## Build Commands

```bash
# Native build (Linux/macOS/FreeBSD) — output: build_native/gpu_benchmark
./scripts/build.sh native

# Cross-compile Windows
./scripts/build.sh mingw64    # 64-bit
./scripts/build.sh mingw32    # 32-bit (Win XP compat)

# Portable static build (SDL2 from source)
./scripts/build.sh portable

# Android APK
./scripts/build.sh android

# Download Stanford Bunny model (required for demo mode)
./scripts/fetch_bunny.sh

# Quick rebuild after changes
cd build_native && make -j$(nproc)

# Run unit tests (87 cases in 16 suites)
./build_native/gpubench_tests
./build_native/gpubench_tests -ts="Vec3"    # Run specific suite

# Format code
clang-format -i src/**/*.cpp src/**/*.h
```

## Language & Style

- **C++11**, CMake 3.0+, SDL2, OpenGL
- `.clang-format`: 4-space indent, 130-char column limit, `BreakBeforeBraces: Attach`, `PointerAlignment: Left`
- `SortIncludes: Never` — include order is intentional, do not reorder
- Short forms allowed: single-line functions, if/else, loops, case labels
- Use `Log::dbg()` for debug/diagnostic output, not `Log::info()` with boolean guards

## Project Structure

```
src/
  core/                          # AppConfig, PollCallback
  renderer/                      # Abstract Renderer interface, features, factory
    backend/                     # GL2/GL3/GL4/GLES renderer implementations
      gl/                        # GL loader, funcs, extensions, debug, timer
    context/                     # SDL window + GL/GLES context
  engine/                        # Pass framework (FullscreenPass, GeometryPass, ComputePassBase)
                                 # ShaderCache, ShaderLoader, MeshPool (engine-generic)
                                 # DrawList, StateCache, RenderState, UniformBlock, TextureSlots
                                 # frustum.h (frustum culling, bounds), shader_program.h
  bench/                         # BenchRunner, StressRunner, presets, results, UI
  tests/                         # 30 benchmark test implementations + X-macro registry
  demo/                          # Demo entry point, runner, UI, export
    scene/                       # DemoScene, DemoCamera, DemoResources, SceneLoader, materials
    tier/                        # ShaderBank, shader_registry.def, demo feature flags, tier config
    pipeline/                    # PipelineBuilder, PassFactory, pass_registry.def
    passes/                      # 24 render pass implementations
  geometry/                      # Vec3, Mat4, MeshData, MeshGen, ObjLoader
  platform/                      # Logger, Timer, DataPath, HWInfo, GpuSelect, compat
    posix/                       # Linux/FreeBSD: getExeDir, HWInfo, gpu_select
    win32/                       # Windows: getExeDir, HWInfo, gpu_select (DXGI)
    android/                     # Android: getExeDir, HWInfo (system properties)
    darwin/                      # macOS: getExeDir
  launcher/                      # Desktop launcher GUI (separate binary)
android/                         # Gradle project (2 Activities: Benchmark + Demo)
tests/                           # Unit tests (doctest 2.4, 87 cases, 16 suites)
data/shaders/                    # External GLSL files (with #pragma include)
data/models/                     # Stanford Bunny OBJ
data/scenes/                     # Data-driven scene files (.scene)
```

## Architecture

### Renderer Abstraction

Abstract `Renderer` base with four backends in `renderer/backend/`: `GL2Renderer`, `GL3Renderer`, `GL4Renderer`, `GLESRenderer`. Optional feature interfaces (`GL3Features`, `GL4Features`, `ComputeFeatures`) queried via LLVM-style virtual dispatch:

```cpp
auto gl3 = renderer->features<GL3Features>();  // returns nullptr if unavailable
if (gl3) gl3->drawMeshInstanced(mesh, count);
```

Each feature interface has a compile-time `FeatureTag<T>::id` mapped to `queryFeature(int)` in the renderer. `renderer_traits<R>` and `test_traits<T>` provide compile-time checks for renderer/test compatibility.

### Engine Layer (`src/engine/`)

Convenience layer between Renderer and Demo passes:

- `PassContext` — per-frame singleton wrapping renderer interaction. Caches GL3/GL4/Compute feature interfaces. Provides RT management, state application, texture binding, compute dispatch with `BarrierFlags`.
- `RenderState` — declarative GL state as POD struct. Presets: `opaque()`, `transparent()`, `additive()`, `fullscreen()`, `shadow()`, `depth_only()`.
- `StateCache` — GL state shadow cache in GL2Renderer. Eliminates ~50% redundant glEnable/glDisable calls.
- Pass templates (`FullscreenPass`, `GeometryPass`, `ComputePassBase`) — author implements 2-3 methods per pass. See `docs/adding_render_pass.md`.
- `DrawList` — sort-based draw call ordering. 32-bit sort key: `shader(8)|material(8)|depth(16)`.
- `TextureSlots` — fixed texture unit enum (`Primary=0` through `Fog=9`).
- `GLDebug` — GL_KHR_debug wrapper. Stubs to no-op on GLES via header inline.
- `GLExtensions` — modern `glGetStringi` extension enumeration (GL 3.0+), fallback to legacy.
- `frustum.h` — frustum extraction, sphere culling, bounds, view inverse.
- `shader_program.h` — RAII shader wrapper with lazy uniform location caching.

### Test Registry (X-macro)

All 30 tests defined in `src/tests/test_registry.def` — one line per test:
```
X(TestId, ClassName, "DisplayName", "cli_name", Category, "Desc", "Unit", preset_field, required_caps)
```

### Pass Registry (X-macro)

All 21 standard render passes defined in `src/demo/pipeline/pass_registry.def`:
```
PASS(ClassName)
```
Special passes (SceneToFBOPass, TessellatedModelPass) with sub-pass wiring handled manually in `pass_factory.cpp`.

### Demo Mode

4-tier visual benchmark (Basic->Ultra). Scene loaded from `data/scenes/sanctuary.scene`. Camera follows Catmull-Rom spline orbit. Shaders declared in `shader_registry.def`, compiled via `ShaderBank` (wraps `ShaderCache` + `ShaderLoader`). Uber shaders from `data/shaders/uber/`, GL4/compute/tess from `data/shaders/gl4/`.

### Resource Handles

Type-safe `Handle<Tag>` wrappers prevent cross-type confusion. RAII via `ScopedHandle<H>`. GPU resources created in `setup()`, destroyed in `cleanup()`.

### Data Path Resolution

`getDataPath()` searches: `./data/` -> `<exe_dir>/data/` -> `<exe_dir>/../share/gpu_benchmark/data/`.
On Android: returns relative path directly (SDL_RWFromFile resolves from APK assets/).
File I/O via `readTextFile()` using SDL_RWops (works on both desktop and Android).

### Android Build

Two Activities in one APK (`BenchmarkActivity`, `DemoActivity`), each loading a separate .so (`libgpu_benchmark.so`, `libgpu_demo.so`). SDL2 built from source via FetchContent. GLES 3.0+ via `CB_GLES_NATIVE` define. GL3/GL4 renderers compile to empty on GLES. GPUTimer and GLDebug have inline stubs in headers.

## Key Conventions

- Single-threaded — all GPU work on main thread
- Per-platform source files in `platform/{posix,win32,android,darwin}/` — CMake selects which to compile
- `CB_GLES_NATIVE` — defined on Android, selects GLES headers, stubs desktop-only features
- `CB_NEED_GL_LOAD` — defined on Windows/macOS/static builds, enables GL function pointer loading
- `CHECK_GL_ERROR("op")` — debug-only glGetError check macro (release = no-op)
- Preset params are all POD structs, validated against hardware constraints
- Scoring uses geometric mean by category with bottleneck detection
- The user communicates in Russian; respond in Russian
