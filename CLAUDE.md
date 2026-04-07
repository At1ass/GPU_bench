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

# Download Stanford Bunny model (required for demo mode)
./scripts/fetch_bunny.sh

# Quick rebuild after changes
cd build_native && make -j$(nproc)

# Format code
clang-format -i src/**/*.cpp src/**/*.h
```

There are no automated tests or linting beyond compiler warnings (`-Wall -Wextra`).

## Language & Style

- **C++11**, CMake 3.0+, SDL2, OpenGL
- `.clang-format`: 4-space indent, 130-char column limit, `BreakBeforeBraces: Attach`, `PointerAlignment: Left`
- `SortIncludes: Never` — include order is intentional, do not reorder
- Short forms allowed: single-line functions, if/else, loops, case labels
- Use `Log::dbg()` for debug/diagnostic output, not `Log::info()` with boolean guards

## Architecture

### Renderer Abstraction

Abstract `Renderer` base with four backends: `GL2Renderer`, `GL3Renderer`, `GL4Renderer`, `GLESRenderer`. Optional feature interfaces (`GL3Features`, `GL4Features`, `ComputeFeatures`) are queried via LLVM-style virtual dispatch:

```cpp
auto gl3 = renderer->features<GL3Features>();  // returns nullptr if unavailable
if (gl3) gl3->drawMeshInstanced(mesh, count);
```

Each feature interface has a compile-time `FeatureTag<T>::id` mapped to `queryFeature(int)` in the renderer. `renderer_traits<R>` and `test_traits<T>` provide compile-time checks for renderer/test compatibility.

### Engine Layer

Convenience layer between Renderer and Demo passes (`src/engine/`):

- `PassContext` — per-frame singleton wrapping renderer interaction. Created in `DemoScene::renderFrame()`, passed to all pass `execute()` calls. Caches GL3/GL4/Compute feature interfaces. Provides RT management, state application, texture binding, compute dispatch with `BarrierFlags`.

- `RenderState` — declarative GL state as POD struct. Applied atomically via `PassContext::applyState()`. Presets: `opaque()`, `transparent()`, `additive()`, `fullscreen()`, `shadow()`, `depth_only()`.

- `StateCache` — GL state shadow cache in GL2Renderer. Eliminates redundant glEnable/glDisable calls (~50% reduction).

- Pass templates (`FullscreenPass`, `GeometryPass`, `ComputePassBase`) — abstract base classes encapsulating per-pass boilerplate. Author implements `setup`/`inputs`/`uniforms` (fullscreen), `setup`/`sceneSetup`/`objectFilter`/`perObject` (geometry), or `setup`/`bind`/`workgroups` (compute).

- `DrawList` — opt-in sort-based draw call ordering for `GeometryPass`. 32-bit sort key: `shader(8)|material(8)|depth(16)`.

- `TextureSlots` — fixed texture unit enum (`Primary=0` through `Fog=9`).

- `GLDebug` — GL_KHR_debug wrapper with severity-based routing to logger. Debug groups around each pipeline pass for RenderDoc/Nsight. Enable via `--debug`.

- `GLExtensions` — modern `glGetStringi` extension enumeration (GL 3.0+), fallback to legacy for GL 2.x.

Adding a new pass: inherit from template, implement 2-3 methods, ~20-30 lines C++ + shader.
Adding a new scene object: `SceneObject` + `MaterialDef` in `buildScene()`, no GL knowledge needed.

### Test Registry (X-macro)

All 30 tests are defined in `src/tests/test_registry.def` — one line per test:
```
X(TestId, ClassName, "DisplayName", "cli_name", Category, "Desc", "Unit", preset_field, required_caps)
```
This generates the `TestId` enum, metadata table, and factory functions. **Adding a new test = one line in `.def` + implementing the class.**

Tests inherit from `BenchTest` (GL2) or typed subclasses (`GL3BenchTest`, `GL4BenchTest`, `ComputeBenchTest`) which auto-acquire the feature interface and delegate to typed `setupGL3/renderGL3/cleanupGL3` methods.

Capability flags (`Cap_FBO`, `Cap_Compute`, `Cap_Tessellation`, etc.) must match the test's base class — enforced at compile time via `validate_test<>`.

### UI Pattern

Model-View with three structs: `UIView` (read-only snapshot), `UIState` (mutable widgets), `UIAction` (event enum returned by `BenchUI::render()`). App dispatches the action.

### Demo Mode

4-tier visual benchmark (Basic→Ultra) with progressive rendering: Blinn-Phong → shadow/SSAO/bloom → PCF/DoF → PBR/compute/tessellation. Scene uses a single OBJ model (Stanford Bunny) with procedural enrichment (53 objects). Camera follows Catmull-Rom spline orbit.

Shaders load from `data/shaders/` via `ShaderLoader` (supports `#pragma include`), with inline GLSL fallback in `demo_shaders.cpp`.

### Resource Handles

Type-safe `Handle<Tag>` wrappers (`MeshHandle`, `TextureHandle`, `ShaderHandle`, `BufferHandle`, `RenderTargetHandle`) prevent cross-type confusion. GPU resources created in `setup()`, destroyed in `cleanup()`.

### Data Path Resolution

`DataPath::find()` searches: `./data/` → `<exe_dir>/data/` → `<exe_dir>/../share/gpu_benchmark/data/`.

## Key Conventions

- Single-threaded — all GPU work on main thread
- CLI argument parsing uses X-macro + lambda pattern in `main.cpp` (`kArgs[]`)
- Preset params are all POD structs, validated against hardware constraints
- `BenchCallbacks` interface lets App handle events/UI during benchmark execution
- Scoring uses geometric mean by category (Fill, Geometry, Compute, Overhead) with bottleneck detection
- The user communicates in Russian; respond in Russian
