# Engine API Guide

Self-contained rendering engine: `src/engine/` + `src/renderer/` + `src/platform/` + `src/geometry/`.
Zero application-specific dependencies. Build your own demo, benchmark, or visualizer.

## Quick Start

Minimal app: window + renderer + one pass.

```cpp
#include "core/app_config.h"
#include "renderer/context/render_context.h"
#include "renderer/renderer_factory.h"
#include "engine/pass_context.h"
#include "engine/render_pass.h"
#include "engine/render_state.h"
#include "engine/frame_data.h"
#include "engine/scene_data.h"
#include "geometry/mesh_gen.h"

// 1. Window + GL context
AppConfig cfg;
cfg.width = 800; cfg.height = 600;
cfg.backend = RendererBackend::Auto;

auto ctx = std::unique_ptr<RenderContext>(createRenderContext(cfg));
ctx->init(cfg);

// 2. Renderer
auto renderer = std::unique_ptr<Renderer>(createRenderer(cfg.backend));
renderer->init(cfg.width, cfg.height);

// 3. Mesh
MeshHandle cube = renderer->createMesh(MeshGen::cube());

// 4. Render loop
while (running) {
    PassContext pass_ctx(renderer.get());
    pass_ctx.beginFrame();
    renderer->setViewport(0, 0, w, h);
    renderer->clear(0.1f, 0.1f, 0.1f, 1.0f);
    pass_ctx.applyState(RenderState::opaque());
    pass_ctx.drawMesh(cube);
    ctx->swapBuffers();
}

// 5. Cleanup
renderer->destroyMesh(cube);
renderer->shutdown();
ctx->shutdown();
```

See `examples/spinning_cube.cpp` for the full working version.

---

## Architecture

```
┌─────────────────────────────────────────────────┐
│  Your Application                               │
│  ├─ Passes (extend RenderPassBase)              │
│  ├─ PipelinePolicy (RT routing, sync)           │
│  └─ Resources (meshes, textures, shaders)       │
├─────────────────────────────────────────────────┤
│  Engine (src/engine/)                           │
│  ├─ RenderPassBase      abstract pass interface │
│  ├─ FullscreenPass      fullscreen effect base  │
│  ├─ GeometryPass        3D object draw base     │
│  ├─ ComputePassBase     compute dispatch base   │
│  ├─ RenderPipeline      sorted execution plan   │
│  ├─ PipelinePolicy      RT routing interface    │
│  ├─ buildPipeline()     topological sort        │
│  ├─ PassContext          per-frame render bridge │
│  ├─ ShaderCache          uber shader compiler   │
│  ├─ UniformBlock         type-safe uniforms     │
│  └─ RenderState          GL state presets       │
├─────────────────────────────────────────────────┤
│  Renderer (src/renderer/)                       │
│  ├─ Renderer             abstract GPU interface │
│  ├─ GL2/GL3/GL4/GLES     backend impls          │
│  ├─ RenderContext         window + GL context    │
│  ├─ ScopedHandle<T>      RAII for GPU handles   │
│  └─ GL3/GL4/Compute      optional feature APIs  │
├─────────────────────────────────────────────────┤
│  Geometry (src/geometry/)                       │
│  ├─ Vec3, Mat4            math types            │
│  ├─ MeshGen              procedural generators  │
│  └─ ObjLoader            Wavefront OBJ parser   │
└─────────────────────────────────────────────────┘
```

---

## Render Passes

A pass is a self-contained rendering step. Inherit `RenderPassBase`:

```cpp
class MyPass : public RenderPassBase {
public:
    const char* name() const override { return "my_pass"; }

    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override {
        ctx.applyState(RenderState::opaque());
        // ... bind shader, set uniforms, draw meshes
    }
};
```

### Pass Templates

For common patterns, use the engine-provided base classes:

| Base Class | Use Case | You Implement |
|------------|----------|---------------|
| `RenderPassBase` | Full control | `execute()` |
| `FullscreenPass` | Post-process effects | `onExecute()` (texture bind + uniforms) |
| `GeometryPass` | 3D object rendering | `onSceneSetup()` (camera + lights) |
| `ComputePassBase` | GPU compute dispatch | `onBind()` + `onWorkgroups()` |

### FullscreenPass

Engine handles RT binding, fullscreen state, quad/triangle draw:

```cpp
class SepiaPass : public FullscreenPass {
    void onExecute(PassContext& ctx, FrameData& fd, const SceneData&) override {
        ctx.renderer()->bindRenderTargetTexture(scene_rt_, 0);
        ub().set(U::SceneTex, 0);
        ub().set(U::Time, fd.time);
    }
};
```

Setup in init:
```cpp
ub().init(shader);      // bind UniformBlock to shader
setShader(shader);       // shader for this pass
setOutputScreen(0, 0);   // render to screen (0,0 = viewport from FrameData)
// OR: setOutputRT(rt, w, h)  — render to specific FBO
// OR: setPipelineManagedRT()  — pipeline controls RT binding
```

`FullscreenPass` draws via `ctx.drawFullscreen()` automatically:
- GL 2.1: VBO quad (4 vertices)
- GL 3.0+: fullscreen triangle via `gl_VertexID` (zero vertex fetch)

Override with `setQuad(mesh)` if your shader uses VBO attributes.

---

## Pipeline

### Resource Dependencies

Passes declare what they read/write. Engine topological sort determines execution order:

```cpp
const ResourceDecl* resourceDecls() const override {
    static const ResourceDecl d[] = {
        { ResourceId::SceneColor, ResourceDecl::WRITE }  // this pass writes scene color
    };
    return d;
}
int resourceDeclCount() const override { return 1; }
```

A pass that READs SceneColor will automatically run AFTER the pass that WRITEs it.

Available `ResourceId` values: `ShadowMap`, `SceneColor`, `SceneDepth`, `HDRColor`,
`HDRDepth`, `AOResult`, `BloomResult`, `FogResult`, `SSRResult`, `DoFResult`,
`ExposureData`, `ParticleData`.

### Building a Pipeline

```cpp
std::vector<RenderPassBase*> passes = { &scene_pass, &postprocess_pass };

// Policy controls RT routing
SimplePipelinePolicy policy;

RenderPipeline pipeline;
buildPipeline(pipeline, passes, policy, viewport_w, viewport_h);

// Execute every frame
pipeline.execute(pass_ctx, frame_data, scene_data);
```

Passes can be added in any order — `buildPipeline()` sorts them by dependencies.

### Pipeline Policy

Controls where each pass renders (FBO, screen, specific RT):

```cpp
class MyPolicy : public PipelinePolicy {
public:
    PassRTBinding routePass(const RenderPassBase& pass,
                            int sorted_index, int total) const override {
        PassRTBinding b;
        if (pass.passRole() == PassRole::FinalComposite) {
            b.action = PassRTBinding::Unbind;  // render to screen
        }
        return b;
    }
};
```

`PassRTBinding::Action` values:
- `None` — don't change RT (pass manages its own)
- `BindRT` — bind specific render target
- `Unbind` — switch to default framebuffer
- `BindDest` — bind destination from FrameData (for compositing)

Override `syncHint()` for compute barriers, `pipelinePrologue()` for initial clear.

---

## Shaders

### ShaderCache — Uber Shader Compiler

Compiles GLSL with automatic preamble injection (`#version`, feature `#define`s, compat macros):

```cpp
ShaderCache cache;
cache.init(renderer, feature_defines, define_count);  // feature_defines: app-specific
```

| Method | Loads from | Preamble | Use case |
|--------|-----------|----------|----------|
| `get(name, feat, feat)` | `uber/{name}.vert` + `.frag` | Yes (#version, defines, macros) | Per-tier uber shaders |
| `getGL4(name)` | `gl4/{name}.vert` + `.frag` | No (shader has own #version) | GL4-specific shaders |
| `getCompute(name, feat)` | `gl4/{name}.comp` | No | Compute shaders |
| `getTess(name, feat)` | `gl4/{name}.vert/.tcs/.tes/.frag` | No | Tessellation (4 stages) |
| `compileInline(key, vs, fs, feat)` | inline strings | Yes | Standalone demos, runtime shaders |

All methods cache results — repeated calls with same name+features return cached program.

```cpp
// Uber (per-tier variants with preamble):
ShaderProgram* island = cache.get("island", features, features);

// GL4 vert+frag (no preamble, shader contains #version 430):
ShaderProgram* pbr = cache.getGL4("island_t4");

// Compute:
ShaderProgram* gtao = cache.getCompute("gtao_t4", features);

// Tessellation (vert + tcs + tes + frag):
ShaderProgram* tess = cache.getTess("tess_t4", features);

// Inline with uber preamble:
ShaderProgram* fx = cache.compileInline("my_fx", vs_src, fs_src, features);
```

### Compat Macros

Shaders use macros instead of raw GLSL syntax. ShaderCache preamble defines them:

| Macro | GLSL 1.20 | GLSL 1.50+ |
|-------|-----------|------------|
| `ATTR_IN` | `attribute` | `in` |
| `VS_OUT` | `varying` | `out` |
| `FS_IN` | `varying` | `in` |
| `FRAG_COLOR` | `gl_FragColor` | `FragColor` (declared) |
| `COMPAT_TEX2D` | `texture2D` | `texture` |
| `GLSL_120` | defined | not defined |

### Fullscreen Vertex Shader (Uber)

Dual-path for VBO quad (GL 2.1) and `gl_VertexID` triangle (GL 3.0+):

```glsl
#ifdef GLSL_120
ATTR_IN vec3 a_pos;
#endif
VS_OUT vec2 v_uv;

void main() {
#ifdef GLSL_120
    v_uv = a_pos.xy * 0.5 + 0.5;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
#else
    vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0;
    v_uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
#endif
}
```

### Feature Defines

Application provides a `FeatureDefine[]` table mapping bitflags to `#define` strings:

```cpp
const FeatureDefine my_features[] = {
    { MY_SHADOWS, "HAS_SHADOWS" },
    { MY_PBR,     "HAS_PBR" },
};
cache.init(renderer, my_features, 2);
```

Shaders use `#ifdef HAS_SHADOWS` to conditionally enable features.

### UniformBlock — Type-Safe Uniforms

```cpp
UniformBlock ub;
ub.init(shader);
ub.use();                         // binds shader
ub.set(U::Proj, proj_matrix);    // Mat4
ub.set(U::Time, 1.5f);           // float
ub.set(U::CamPos, eye);          // Vec3
```

Uniform names registered in `engine/uniform_registry.def`:
```
U(Proj, "u_proj", M4)
U(Time, "u_time", F)
```

Locations resolved lazily on first use, then cached. Zero string ops in hot path.

---

## Renderer

### Creation

```cpp
auto renderer = std::unique_ptr<Renderer>(createRenderer(RendererBackend::Auto));
renderer->init(width, height);
```

Auto-detects highest available GL: GL4 → GL3 → GL2 → GLES.

### Resource Handles

Type-safe opaque handles prevent cross-type confusion:

```cpp
MeshHandle mesh = renderer->createMesh(data);
TextureHandle tex = renderer->createTexture(w, h, channels, pixels);
RenderTargetHandle rt = renderer->createRenderTarget(w, h);
ShaderHandle shader = renderer->createCustomShader(vs, fs);
```

RAII via `ScopedHandle<T>`:
```cpp
ScopedMesh mesh(renderer, renderer->createMesh(data));
// auto-destroyed when scope exits
```

Available: `ScopedMesh`, `ScopedTexture`, `ScopedShader`, `ScopedRenderTarget`, `ScopedBuffer`.

### Optional Features (LLVM-style query)

```cpp
GL3Features* gl3 = renderer->features<GL3Features>();
if (gl3) gl3->drawMeshInstanced(mesh, 100);

GL4Features* gl4 = renderer->features<GL4Features>();
ComputeFeatures* compute = renderer->features<ComputeFeatures>();
```

Returns `nullptr` if feature not available on current hardware.

### RenderState Presets

```cpp
ctx.applyState(RenderState::opaque());      // depth on, cull on, no blend
ctx.applyState(RenderState::transparent()); // depth on, cull off, blend on
ctx.applyState(RenderState::fullscreen());  // no depth/cull/blend
ctx.applyState(RenderState::shadow());      // depth write, no color
```

---

## Geometry

### Procedural Mesh Generation

```cpp
MeshData cube = MeshGen::cube();
MeshData sphere = MeshGen::sphere(32, 16);
MeshData quad = MeshGen::quad();        // fullscreen quad
MeshData terrain = MeshGen::terrain(20.0f, 80);
MeshData cylinder = MeshGen::cylinder(16, 3.0f, 0.3f);
```

### OBJ Loading

```cpp
MeshData bunny = ObjLoader::load("models/bunny.obj");
ObjLoader::normalize(bunny);             // fit into [-1,1]
MeshGen::smoothNormals(bunny);           // recompute smooth normals
MeshGen::optimizeVertexCache(bunny);     // GPU cache optimization
```

### Math

```cpp
Vec3 pos(1, 2, 3);
Vec3 dir = pos.normalized();
float d = Vec3::dot(a, b);
Vec3 c = Vec3::cross(a, b);

Mat4 proj = Mat4::perspective(60.0f, aspect, 0.1f, 50.0f);
Mat4 view = Mat4::lookAt(eye, center, up);
Mat4 model = Mat4::translate(x, y, z) * Mat4::rotateY(angle);
```

---

## Two-Phase Init

GPU classes use `init()` / `shutdown()` instead of constructors/destructors because:
- `init()` returns `bool` (GPU operations can fail; no exceptions in this project)
- GPU resources need `Renderer*` which isn't available at construction time
- Destruction order matters (resources must be freed before Renderer shutdown)

```cpp
ShaderCache cache;
cache.init(renderer);        // may fail → returns bool
// ... use cache ...
cache.destroy();             // explicit, before renderer->shutdown()
```

`ScopedHandle<T>` provides RAII for individual GPU handles where ownership is clear.

---

## CMake Integration

```cmake
# Your app links only engine + core (no demo/, no bench/)
add_executable(my_app src/my_app.cpp)
target_include_directories(my_app PRIVATE src extern)
target_link_libraries(my_app PRIVATE gpubench_engine)
# gpubench_engine already links gpubench_core (renderer, platform, geometry)
```

---

## Examples

| Example | Demonstrates |
|---------|-------------|
| `examples/spinning_cube.cpp` | Minimal: RenderPassBase, PassContext, ShaderProgram, buildPipeline |
| `examples/postprocess_demo.cpp` | Full pipeline: ResourceDecl, FBO, FullscreenPass, ShaderCache::compileInline, PipelinePolicy, topological sort |

---

## Key Files Reference

| Header | Purpose |
|--------|---------|
| `engine/render_pass.h` | `RenderPassBase` — abstract pass interface |
| `engine/fullscreen_pass.h` | `FullscreenPass` — fullscreen effect template |
| `engine/geometry_pass.h` | `GeometryPass` — 3D object draw template |
| `engine/compute_pass.h` | `ComputePassBase` — compute dispatch template |
| `engine/render_pipeline.h` | `RenderPipeline` + `PipelineNode` — execution plan |
| `engine/pipeline_builder.h` | `buildPipeline()` — topological sort + policy emission |
| `engine/pipeline_policy.h` | `PipelinePolicy` — RT routing interface |
| `engine/pass_context.h` | `PassContext` — per-frame render bridge |
| `engine/shader_cache.h` | `ShaderCache` — uber shader compiler |
| `engine/uniform_block.h` | `UniformBlock` — type-safe cached uniforms |
| `engine/render_state.h` | `RenderState` — GL state presets |
| `engine/resource_id.h` | `ResourceId`, `ResourceDecl` — pass dependencies |
| `engine/frame_data.h` | `FrameData` — per-frame camera/time/viewport |
| `engine/scene_data.h` | `SceneData` — scene object lists |
| `renderer/renderer.h` | `Renderer` — abstract GPU interface |
| `renderer/renderer_factory.h` | `createRenderer()` — backend selection |
| `renderer/context/render_context.h` | `RenderContext` — window + GL context |
| `renderer/scoped_handle.h` | `ScopedHandle<T>` — RAII for GPU handles |
| `renderer/features.h` | `GL3Features`, `GL4Features`, `ComputeFeatures` |
| `geometry/math_types.h` | `Vec3`, `Mat4` — math primitives |
| `geometry/mesh_gen.h` | `MeshGen` — procedural mesh generators |
