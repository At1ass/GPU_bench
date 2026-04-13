# Adding a New Render Pass

This guide walks you through adding a new render pass to the demo pipeline.
A render pass is a self-contained rendering step — it reads input textures/buffers,
runs a shader, and writes to a render target or buffer.

## Architecture Overview

```
RenderPassBase (interface)
├── FullscreenPass    — screen-space effects (SSAO, bloom, DoF, composite)
├── GeometryPass      — scene object rendering (opaque, shadow, water, grass)
└── ComputePassBase   — GPU compute (particles, GTAO, auto-exposure)
```

Each template handles the boilerplate (RT binding, state, draw calls).
You implement 2-3 methods with your rendering logic.

## Quick Reference: What to Implement

| Template | Required Methods | Optional Overrides |
|----------|-----------------|-------------------|
| **FullscreenPass** | `setup()`, `inputs()` | `uniforms()` |
| **GeometryPass** | `setup()`, `sceneSetup()` | `objectFilter()`, `perObject()`, `objectList()`, `useSortedDraw()` |
| **ComputePassBase** | `setup()`, `bind()`, `workgroups()` | `barrierFlags()` |

All passes must also implement:
- `name()` — unique string identifier
- `init(const TierResourceView& res)` — one-time initialization (shaders, RT refs)
- `resourceDecls()` / `resourceDeclCount()` — resource dependencies
- `minTier()` — minimum demo tier (Basic/Enhanced/Quality/Ultra)

---

## Step-by-Step: Fullscreen Pass

Example: a vignette effect that darkens screen edges.

### 1. Create the header

`src/demo/passes/vignette_pass.h`:

```cpp
#pragma once
#include "engine/fullscreen_pass.h"

class VignettePass : public FullscreenPass {
public:
    const char* name() const override { return "vignette"; }
    void init(const TierResourceView& res);

protected:
    void setup(const TierResourceView& res) override;
    void inputs(PassContext& ctx, const TierResourceView& res,
                const FrameData& fd) override;
    void uniforms(UniformBlock& ub, const FrameData& fd,
                  const DemoTierConfig& cfg) override;

public:
    // Resource dependencies: reads scene color, writes to same RT (ping-pong)
    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneColor, ResourceAccess::Read },
            { ResourceId::BloomResult, ResourceAccess::Write },
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }

    // Available from Tier 1 (Basic)
    DemoTier minTier() const override { return DemoTier::Basic; }
};
```

### 2. Create the implementation

`src/demo/passes/vignette_pass.cpp`:

```cpp
#include "demo/passes/vignette_pass.h"
#include "demo/tier/tier_resource_view.h"
#include "engine/uniform_id.h"
#include "engine/texture_slots.h"

void VignettePass::init(const TierResourceView& res) {
    // Get shader and quad from shared resources
    ub().init(res.core.vignette_shader);
    setShader(res.core.vignette_shader);
    setQuad(res.bloom.fullscreen_quad);
    // Let pipeline manage the output RT (most common pattern)
    setPipelineManagedRT();
}

void VignettePass::setup(const TierResourceView& res) {
    // Called each frame before execute. Cache per-frame resources if needed.
}

void VignettePass::inputs(PassContext& ctx, const TierResourceView& res,
                          const FrameData& fd) {
    // Bind input textures
    ctx.bindRTTexture(TexSlot::Primary, res.core.scene_rt);
    ub().set(U::SceneTex, TexSlot::Primary);
}

void VignettePass::uniforms(UniformBlock& ub, const FrameData& fd,
                            const DemoTierConfig& cfg) {
    // Set effect parameters
    ub.set(U::ScreenSize, static_cast<float>(fd.viewport_w),
                          static_cast<float>(fd.viewport_h));
    ub.set(U::Time, fd.time);
    // Custom uniforms (add to uniform_registry.def if needed)
}
```

### 3. Write the GLSL shader

`data/shaders/uber/vignette.vert` (standard fullscreen vertex shader):
```glsl
in vec3 a_position;
in vec2 a_uv;
out vec2 v_uv;

void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_position, 1.0);
}
```

`data/shaders/uber/vignette.frag`:
```glsl
in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_scene_tex;
uniform vec2 u_screen_size;

void main() {
    vec3 color = texture(u_scene_tex, v_uv).rgb;

    // Vignette: darken edges based on distance from center
    vec2 center = v_uv - 0.5;
    float dist = length(center);
    float vignette = smoothstep(0.7, 0.3, dist);
    color *= vignette;

    frag_color = vec4(color, 1.0);
}
```

### 4. Register the shader in DemoResources

Passes receive their shaders through `TierResourceView` — a read-only snapshot
of all compiled shaders and GPU resources for the current tier. You need to:

**a) Add a field to TierResourceView** (`src/demo/tier/tier_resource_view.h`):

Add a pointer in the appropriate sub-struct (`Core` for always-present,
`Shadow` for shadow-related, etc.):

```cpp
struct Core {
    // ... existing fields ...
    ShaderProgram* vignette_shader = nullptr;   // ← add
};
```

**b) Add a shader field to DemoResources** (`src/demo/scene/demo_resources.h`):

Shaders are stored in nested structs: `core_` for always-present, `shadow_` for
shadow mapping, `bloom_` for bloom, `t4_` for Ultra-only, etc. Add a field:

```cpp
struct CoreRes {
    // ... existing fields ...
    ShaderProgram* vignette_cache = nullptr;   // ← add
} core_;
```

**c) Compile the shader** (`src/demo/scene/demo_resources.cpp`):

There are two patterns depending on shader type:

**Vertex + Fragment shaders** — use `ShaderCache` in `compileTierShaders()`:

```cpp
bool DemoResources::compileTierShaders(Renderer* r, int tier) {
    ShaderFeatureSet feat = featuresForTier(static_cast<DemoTier>(tier), ...);
    // ...
    if (tier >= 2) {
        core_.vignette_cache = shader_cache_.get("vignette", feat, feat);
    }
}
```

`shader_cache_.get("vignette", feat, feat)` loads `data/shaders/uber/vignette.vert`
+ `.frag`, prepends tier-appropriate `#version` / `#define` preamble, processes
`#pragma include`, compiles and caches.

**Compute shaders** — load manually via `ShaderLoader` + `ComputeFeatures`:

```cpp
// In compileTierShaders() tier 4 section, or createT4Resources():
ComputeFeatures* cf = r->features<ComputeFeatures>();
if (cf && cf->hasCompute()) {
    std::string cs = ShaderLoader::load("gl4/my_effect.comp");
    ShaderHandle sh = cf->createComputeShader(cs.c_str());
    if (sh != INVALID_SHADER) {
        t4_.my_compute_shader.adopt(r, sh);
    }
}
```

Compute shaders live in `data/shaders/gl4/` (not `uber/`) and use `ShaderProgram::adopt()`
instead of the cache — they are always tier 4 only.

> **Note:** Post-process passes (SSAO, bloom) compile shaders in dedicated
> `createSSAOResources()` / `createBloomResources()` methods called from `prepare()`,
> not directly in `compileTierShaders()`. Follow the existing pattern for your pass type.

**d) Wire into TierResourceView** (in `DemoResources::viewForTier()`):

The wiring pattern depends on complexity:

**Simple (single shader, null if tier too low):**
```cpp
// TorchPass pattern — shader is null on tiers below minimum
view.core.vignette_shader = core_.vignette_cache;
```

**With fallback chain (per-tier variants):**
```cpp
// Island/Fur pattern — try tier-specific, fall back to tier 1
if (core_.my_cache[idx])
    view.core.my_shader = core_.my_cache[idx];
else if (core_.my_cache[0])
    view.core.my_shader = core_.my_cache[0];
```

**Post-process with validation (multiple resources):**
```cpp
// SSAO pattern — only wire if ALL required resources exist
ShaderProgram* prog = my_.cache ? my_.cache : nullptr;
if (idx >= 1 && prog && my_.rt) {
    view.my_effect.shader = prog;
    view.my_effect.rt = my_.rt.get();
}
```

Your pass's `init()` should null-check the shader pointer — it will be nullptr
if the tier is too low or shader compilation failed.

> **Naming convention:** Shader files use the pass name without suffix:
> `sky` → `sky.vert` + `sky.frag`, `bloom_extract` → `bloom_extract.vert` + `bloom_extract.frag`.

> **Tip:** For simple post-process passes, you can reuse the standard fullscreen
> vertex shader and only write a custom fragment shader.

### 5. Register in pass registry

`src/demo/pipeline/pass_registry.def` — add one line:

```cpp
PASS(VignettePass)
```

And add the include to `src/demo/passes/all_passes.h`:

```cpp
#include "demo/passes/vignette_pass.h"
```

That's it — the factory auto-creates and `init()`s all passes from the registry.

> **Note:** Passes that need special sub-pass wiring (like SceneToFBOPass) are
> not in the registry — they are handled manually in pass_factory.cpp.

### 6. Add to CMakeLists.txt

In the `DEMO_SOURCES` list, under `# Passes`:

```cmake
src/demo/passes/vignette_pass.cpp
```

### 7. Build and test

```bash
cd build_native && cmake .. && make -j$(nproc)
./gpu_demo  # verify visually
```

---

## Step-by-Step: Geometry Pass

Example: rendering wireframe overlay on scene objects.

### Header

```cpp
#pragma once
#include "engine/geometry_pass.h"

class WireframePass : public GeometryPass {
public:
    const char* name() const override { return "wireframe"; }
    void init(const TierResourceView& res);

protected:
    void setup(const TierResourceView& res) override;
    void sceneSetup(UniformBlock& ub, PassContext& ctx,
                    const FrameData& fd,
                    const TierResourceView& res,
                    const DemoTierConfig& cfg) override;

    // Optional: custom filtering (e.g., only render selected objects)
    bool objectFilter(const SceneObject& obj,
                      const FrameData& fd) override;

public:
    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneColor, ResourceAccess::Write },
            { ResourceId::SceneDepth, ResourceAccess::Read },
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    DemoTier minTier() const override { return DemoTier::Enhanced; }
};
```

### Implementation

```cpp
#include "demo/passes/wireframe_pass.h"
#include "engine/uniform_id.h"

void WireframePass::init(const TierResourceView& res) {
    ub().init(res.core.wireframe_shader);
    setShader(res.core.wireframe_shader);
    setState(RenderState::transparent());  // blend over existing scene
    setPipelineManagedRT();                // pipeline handles RT binding
}

void WireframePass::setup(const TierResourceView& res) {}

void WireframePass::sceneSetup(UniformBlock& ub, PassContext& ctx,
                               const FrameData& fd,
                               const TierResourceView& res,
                               const DemoTierConfig& cfg) {
    // Scene-wide uniforms (set once per frame)
    ub.set(U::Proj, fd.proj);
    ub.set(U::View, fd.view);
    ub.set(U::WireColor, 0.0f, 1.0f, 0.0f);  // green wireframe
}

bool WireframePass::objectFilter(const SceneObject& obj,
                                 const FrameData& fd) {
    // Only render objects within frustum
    return sphereInFrustum(fd.frustum, obj.bounds_center, obj.bounds_radius);
}

// perObject() uses default: sets U::Model, U::MatColor per object
```

---

## Step-by-Step: Compute Pass

Example: computing a histogram of screen luminance.

### Header

```cpp
#pragma once
#include "engine/compute_pass.h"

class LuminanceHistogramPass : public ComputePassBase {
public:
    const char* name() const override { return "luminance_histogram"; }
    void init(const TierResourceView& res);

protected:
    void setup(const TierResourceView& res) override;
    void bind(PassContext& ctx, UniformBlock& ub,
              const TierResourceView& res,
              const FrameData& fd,
              const DemoTierConfig& cfg) override;
    void workgroups(const FrameData& fd, const DemoTierConfig& cfg,
                    int& gx, int& gy, int& gz) override;
    unsigned int barrierFlags() const override { return Barrier_SSBO; }

public:
    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRColor, ResourceAccess::Read },
            { ResourceId::ExposureData, ResourceAccess::Write },
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
};
```

### Implementation

```cpp
void LuminanceHistogramPass::init(const TierResourceView& res) {
    ub().init(res.t4.histogram_shader);
    setShader(res.t4.histogram_shader);
}

void LuminanceHistogramPass::setup(const TierResourceView& res) {}

void LuminanceHistogramPass::bind(PassContext& ctx, UniformBlock& ub,
                                  const TierResourceView& res,
                                  const FrameData& fd,
                                  const DemoTierConfig& cfg) {
    ctx.bindRTTexture(TexSlot::Primary, res.t4.hdr.scene_rt);
    ctx.bindSSBO(res.t4.exposure_ssbo, 0);
    ub.set(U::ScreenSize, static_cast<float>(fd.viewport_w),
                          static_cast<float>(fd.viewport_h));
}

void LuminanceHistogramPass::workgroups(const FrameData& fd,
                                        const DemoTierConfig& cfg,
                                        int& gx, int& gy, int& gz) {
    gx = (fd.viewport_w + 15) / 16;
    gy = (fd.viewport_h + 15) / 16;
    gz = 1;
}
```

---

## Checklist

When adding a new pass, verify:

- [ ] Header in `src/demo/passes/` with class declaration
- [ ] Implementation in `src/demo/passes/` with `init()` + template methods
- [ ] GLSL shader(s) in `data/shaders/uber/`
- [ ] Shader field added to `TierResourceView` struct
- [ ] Shader compiled via `shader_cache_.get()` in `DemoResources::prepare()`
- [ ] Shader wired into `TierResourceView` in `DemoResources::viewForTier()`
- [ ] One line in `src/demo/pipeline/pass_registry.def`: `PASS(MyPassName)`
- [ ] Include added to `src/demo/passes/all_passes.h`
- [ ] Added to `DEMO_SOURCES` in `CMakeLists.txt`
- [ ] `resourceDecls()` correctly declares Read/Write dependencies
- [ ] `minTier()` matches required GL features
- [ ] `name()` is unique across all passes
- [ ] Builds and runs: `make -j$(nproc) && ./gpu_demo`

## Key Files Reference

| File | Purpose |
|------|---------|
| `src/engine/render_pass.h` | Base interface — all passes inherit from this |
| `src/engine/fullscreen_pass.h` | Template for screen-space effects |
| `src/engine/geometry_pass.h` | Template for scene object rendering |
| `src/engine/compute_pass.h` | Template for GPU compute shaders |
| `src/engine/pass_context.h` | Renderer bridge — RT, textures, state, draw |
| `src/engine/render_state.h` | State presets: `opaque()`, `transparent()`, etc. |
| `src/engine/texture_slots.h` | Fixed texture unit assignments |
| `src/engine/uniform_id.h` | Uniform name registry |
| `src/engine/uniform_block.h` | Type-safe uniform setter with caching |
| `src/engine/resource_id.h` | ResourceId enum + ResourceDecl for dependencies |
| `src/demo/pipeline/pass_registry.def` | X-macro registry — one line per pass |
| `src/demo/passes/all_passes.h` | Aggregated includes for all pass headers |
| `src/demo/pipeline/pass_factory.cpp` | Pass creation (registry expansion + special wiring) |
