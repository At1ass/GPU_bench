# GPU Benchmark — Adding Custom Tests

## Architecture

All tests inherit from `BenchTest` or one of the typed base classes defined
in `src/bench/bench.h`:

```cpp
class BenchTest {
public:
    virtual ~BenchTest() {}
    virtual const char* name() const = 0;
    virtual const char* scoreUnit() const = 0;
    virtual const char* description() const = 0;

    virtual void setup(Renderer* r, int viewport_w, int viewport_h) = 0;
    virtual void render(Renderer* r) = 0;
    virtual void cleanup(Renderer* r) = 0;

    virtual double computeScore(const std::vector<double>& frame_times_ms,
                                int viewport_w, int viewport_h) = 0;
};
```

### Typed Base Classes (GL3/GL4/Compute)

Tests that require GL3+, GL4+, or Compute features should inherit from the
appropriate typed base class instead of `BenchTest`:

| Base Class | Use When | Methods to Override |
|---|---|---|
| `BenchTest` | GL2 universal tests | `setup/render/cleanup(Renderer*)` |
| `GL3BenchTest` | GL3+ features (instancing, MRT, UBO, TF, GS, tex arrays) | `setupGL3/renderGL3/cleanupGL3(Renderer&, GL3Features&)` |
| `ComputeBenchTest` | Compute shaders + SSBO | `setupCompute/renderCompute/cleanupCompute(Renderer&, ComputeFeatures&)` |
| `GL4BenchTest` | GL4+ features (indirect draw) | `setupGL4/renderGL4/cleanupGL4(Renderer&, GL4Features&)` |

The typed base classes:
- Automatically acquire the feature interface via `r->features<GL3Features>()` / `r->features<ComputeFeatures>()` / `r->features<GL4Features>()`
- Assert at runtime if the feature is unavailable (catches cap mismatch bugs)
- Mark `setup/render/cleanup` as `final` — you override the typed variants instead
- Provide compile-time validation via `static_assert` in `test_registry.cpp`

The benchmark harness calls these methods in order:
1. `setup()` — create GPU resources (meshes, textures, shaders)
2. `render()` — called N times during warmup and measurement
3. `cleanup()` — destroy GPU resources
4. `computeScore()` — compute a score from collected frame times

### Feature Interfaces

GL3/GL4/Compute methods live in separate feature interfaces
(`src/renderer/features.h`), not in the base `Renderer`:

| Interface | Accessed Via | Methods |
|---|---|---|
| `GL3Features` | `r->features<GL3Features>()` | `drawMeshInstanced`, MRT, texture arrays, geometry shaders, UBO, transform feedback |
| `ComputeFeatures` | `r->features<ComputeFeatures>()` | `createComputeShader`, `dispatchCompute`, SSBO |
| `GL4Features` | `r->features<GL4Features>()` | `createIndirectBuffer`, `multiDrawMeshIndirect` |

Base `Renderer` methods (draw, textures, shaders, FBO, timer) are available
to all tests via the `Renderer&` reference.

### Test Registry (X-macro)

Tests are registered via the X-macro file `src/tests/test_registry.def` — the
**single source of truth** for all tests. Adding a new test means adding one
line to this file. Everything else (enum IDs, the `g_tests[]` metadata table,
UI labels, CLI names) is generated automatically.

Each line has the format:

```
X(TestId, ClassName, "DisplayName", "cli_name", Category, "Description", "Unit", preset_field, required_caps)
```

The generated infrastructure lives in two headers:

- **`src/tests/test_registry.h`** — `TestId` enum, `TestInfo` struct,
  `TestCap` capability flags, `g_tests[]` array, `NUM_TESTS` constant.
- **`src/tests/tests.h`** — test class declarations and helper includes.

### Capability Flags

The 9th field in `test_registry.def` is a bitmask of `TestCap` flags that
declare which GL features the test requires. Available flags
(from `src/tests/test_registry.h`):

| Flag | Bit | Meaning |
|------|-----|---------|
| `Cap_None` | 0 | No special requirements (GL 2.0) |
| `Cap_FBO` | 1 << 0 | Framebuffer objects |
| `Cap_VAO` | 1 << 1 | Vertex Array Objects |
| `Cap_Instancing` | 1 << 2 | Instanced rendering (GL 3.3+ / ARB) |
| `Cap_Compute` | 1 << 3 | Compute shaders (GL 4.3+) |
| `Cap_TimerQuery` | 1 << 4 | GL_TIME_ELAPSED queries |
| `Cap_GL3` | 1 << 5 | Requires OpenGL 3.x context |
| `Cap_GL4` | 1 << 6 | Requires OpenGL 4.x context |
| `Cap_GeometryShader` | 1 << 7 | Geometry shaders (GL 3.2+) |
| `Cap_Tessellation` | 1 << 8 | Tessellation shaders (GL 4.0+) |
| `Cap_ImageLoadStore` | 1 << 9 | Image load/store (GL 4.2+) |
| `Cap_BufferStorage` | 1 << 10 | Persistent buffer mapping (GL 4.4+) |
| `Cap_BindlessTexture` | 1 << 11 | Bindless textures (ARB extension) |

At startup the harness calls `getAvailableCaps()` to probe the current GL
context. Tests whose `required_caps` are not satisfied are **automatically
disabled** in the UI and skipped in CLI/headless runs — no manual checking
needed in your test code.

**Compile-time validation:** `test_registry.cpp` uses `static_assert` to
verify that the base class matches the caps. For example, a test with
`Cap_GL3` must inherit `GL3BenchTest`, and a test inheriting
`ComputeBenchTest` must have `Cap_Compute`.

## Step-by-Step Guide

### 1. Define parameters

Add a parameter struct in `src/bench/preset.h`:

```cpp
struct MyTestParams { int complexity; };
```

Add the field to `BenchPreset`:

```cpp
struct BenchPreset {
    // ... existing fields ...
    MyTestParams mytest;
};
```

### 2. Set preset values

In `src/bench/preset.cpp`, add values for all 4 presets:

```cpp
static const BenchPreset PRESETS[PRESET_COUNT] = {
    // Light
    {
        // ... existing params ...
        { 10 },    // mytest complexity
    },
    // Medium
    {
        // ...
        { 50 },
    },
    // Heavy
    {
        // ...
        { 200 },
    },
    // Ultra
    {
        // ...
        { 500 },
    },
};
```

### 3. Implement the test

#### GL2 test (inherits BenchTest)

Create `src/tests/test_mytest.cpp`:

```cpp
#include "tests/tests.h"
#include "geometry/mesh_gen.h"

MyTestClass::MyTestClass(const MyTestParams& params)
    : params_(params), vw_(0), vh_(0), quad_(INVALID_MESH) {}

const char* MyTestClass::name() const { return "MyTest"; }
const char* MyTestClass::scoreUnit() const { return "Mops/s"; }
const char* MyTestClass::description() const {
    return "Description of what this test measures.";
}

void MyTestClass::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    quad_ = r->createMesh(MeshGen::quad());
}

void MyTestClass::render(Renderer* r) {
    r->setDepthTest(false);
    r->useShader(Renderer::ShaderType::Color2D);
    for (int i = 0; i < params_.complexity; i++) {
        r->setColor(/* ... */);
        r->drawMesh(quad_);
    }
}

void MyTestClass::cleanup(Renderer* r) {
    r->destroyMesh(quad_);
    quad_ = INVALID_MESH;
}

double MyTestClass::computeScore(const std::vector<double>& times, int vw, int vh) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    double ops_per_frame = (double)vw * vh * params_.complexity;
    return ops_per_frame / (avg_ms / 1000.0) / 1e6;
}
```

#### GL3 test (inherits GL3BenchTest)

```cpp
#include "tests/tests.h"
#include "renderer/features.h"
#include "geometry/mesh_gen.h"

MyGL3Test::MyGL3Test(const MyGL3Params& params) : params_(params) {}
const char* MyGL3Test::name() const { return "MyGL3Test"; }
// ... scoreUnit(), description() ...

void MyGL3Test::setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) {
    mesh_ = r.createMesh(MeshGen::sphere(8, 6));   // base Renderer
    ubo_ = gl3.createUBO(sizeof(float) * 4);        // GL3 feature
}

void MyGL3Test::renderGL3(Renderer& r, GL3Features& gl3) {
    r.useCustomShader(shader_);                      // base Renderer
    gl3.bindUBO(ubo_, 0);                            // GL3 feature
    r.drawMesh(mesh_);                               // base Renderer
}

void MyGL3Test::cleanupGL3(Renderer& r, GL3Features& gl3) {
    gl3.destroyUBO(ubo_);                            // GL3 feature
    r.destroyMesh(mesh_);                            // base Renderer
}

double MyGL3Test::computeScore(const std::vector<double>& t, int vw, int vh) {
    // ... same as BenchTest ...
}
```

Register with GL3 cap:
```cpp
X(MyGL3, MyGL3Test, "MyGL3Test", "mygl3", Overhead, "GL3 test", "ops/s", mygl3, Cap_GL3)
```

#### Compute test (inherits ComputeBenchTest)

```cpp
void MyCompute::setupCompute(Renderer& r, ComputeFeatures& comp, int, int) {
    shader_ = comp.createComputeShader(source);      // ComputeFeatures
    ssbo_ = comp.createSSBO(size);                    // ComputeFeatures
    u_loc_ = r.getCustomUniformLoc(shader_, "u_n");  // base Renderer
}

void MyCompute::renderCompute(Renderer& r, ComputeFeatures& comp) {
    r.useCustomShader(shader_);                       // base Renderer
    comp.bindSSBO(ssbo_, 0);                          // ComputeFeatures
    comp.dispatchCompute(groups, 1, 1);               // ComputeFeatures
    comp.computeMemoryBarrier();                      // ComputeFeatures
}
```

Register with compute cap:
```cpp
X(MyComp, MyCompute, "MyCompute", "mycomp", Compute, "Compute test", "GFLOP/s", mycomp, Cap_Compute)
```

### 4. Declare the class

In `src/tests/tests.h`:

```cpp
// GL2 test:
class MyTestClass : public BenchTest {
    // ... setup/render/cleanup override ...
};

// GL3 test:
class MyGL3Test : public GL3BenchTest {
    // ... setupGL3/renderGL3/cleanupGL3 override ...
    // computeScore still overrides BenchTest
};

// Compute test:
class MyCompute : public ComputeBenchTest {
    // ... setupCompute/renderCompute/cleanupCompute override ...
};
```

### 5. Register the test

Add **one line** to `src/tests/test_registry.def`:

```cpp
X(MyTest, MyTestClass, "MyTest", "mytest", Compute, "Description of what this test measures", "Mops/s", mytest, Cap_None)
```

That's it — the `TestId` enum value, `g_tests[]` entry, CLI name, and UI label
are all generated automatically from this single line. No need to edit
`main.cpp` or any coordinator file.

If your test requires specific GL features, set the capability flags
accordingly. For example, a test that needs instanced rendering:

```cpp
X(MyTest, MyTestClass, "MyTest", "mytest", Geometry, "Instanced test", "Mtri/s", mytest, Cap_Instancing)
```

Or a compute shader test (GL 4.3+):

```cpp
X(MyTest, MyTestClass, "MyTest", "mytest", Compute, "Compute test", "GFLOP/s", mytest, Cap_Compute)
```

**Important:** The base class must match the caps. Compile-time `static_assert`
will catch mismatches (e.g. `Cap_GL3` with `BenchTest`, or `Cap_None` with
`GL3BenchTest`).

### 6. Add to build

In `CMakeLists.txt`, add to `SOURCES`:

```cmake
set(SOURCES
    # ... existing sources ...
    src/tests/test_mytest.cpp
)
```

### 7. Optional: Add to composite scoring

Category scoring is automatic — it's driven by the `TestCategory` field in
`test_registry.def`. Just pick the right category.

### 8. Optional: Add to config save/load

In `src/bench/preset_io.cpp`:

**saveConfig():**
```cpp
fprintf(f.get(), "\n[mytest]\n");
fprintf(f.get(), "complexity=%d\n", preset.mytest.complexity);
```

**parseLine():**
```cpp
} else if (section == "mytest") {
    if (key == "complexity") p.mytest.complexity = atoi(val.c_str());
}
```

## Renderer API

Tests interact with the GPU through the `Renderer` abstract interface (base)
and feature interfaces (GL3/GL4/Compute).

### Base Renderer (available to all tests)

| Method | Description |
|--------|-------------|
| `createMesh(MeshData)` | Upload vertex/index data, returns handle |
| `destroyMesh(handle)` | Free GPU mesh resources |
| `drawMesh(handle)` | Draw a mesh |
| `createTexture(w, h, ch, pixels)` | Create texture from pixel data |
| `destroyTexture(handle)` | Free GPU texture |
| `bindTexture(handle)` | Bind texture for rendering |
| `useShader(ShaderType)` | Use built-in shader (Color2D, Textured2D, Scene3D) |
| `createCustomShader(vs, fs)` | Compile custom GLSL shader |
| `useCustomShader(handle)` | Use a custom shader |
| `getCustomUniformLoc(handle, name)` | Get uniform location |
| `setColor(r, g, b, a)` | Set current draw color |
| `setModel(Mat4)` | Set model transform matrix |
| `setUniformMat4(loc, Mat4)` | Set a mat4 uniform by location |
| `setDepthTest(bool)` | Enable/disable depth test |
| `setBlending(bool)` | Enable/disable alpha blending |
| `setViewport(x, y, w, h)` | Set GL viewport |
| `clear(r, g, b, a)` | Clear framebuffer |
| `resetState()` | Reset GL state to defaults |
| `createRenderTarget(w, h)` | Create FBO render target |
| `bindRenderTarget(handle)` | Bind FBO (0 = default) |

### GL3Features (`r->features<GL3Features>()`)

| Method | Description |
|--------|-------------|
| `drawMeshInstanced(h, count)` | Draw with hardware instancing |
| `createMRTRenderTarget(w, h, n)` | FBO with multiple color attachments |
| `createTextureArray(w, h, layers, ch, px)` | 2D texture array |
| `bindTextureArray(handle)` | Bind as GL_TEXTURE_2D_ARRAY |
| `createShaderVGF(vs, gs, fs)` | Vertex+Geometry+Fragment shader |
| `createUBO/updateUBO/bindUBO/destroyUBO` | Uniform buffer objects |
| `setRasterizerDiscard(bool)` | Enable/disable rasterizer |
| `createTransformFeedbackBuffer/Shader` | Transform feedback |
| `beginTransformFeedback/endTransformFeedback` | TF capture |

### ComputeFeatures (`r->features<ComputeFeatures>()`)

| Method | Description |
|--------|-------------|
| `createComputeShader(source)` | Compile compute shader (GL4.3+) |
| `dispatchCompute(x, y, z)` | Launch compute work groups |
| `computeMemoryBarrier()` | Memory barrier for SSBO |
| `createSSBO/destroySSBO/bindSSBO` | Shader storage buffer objects |

### GL4Features (`r->features<GL4Features>()`)

| Method | Description |
|--------|-------------|
| `createIndirectBuffer(size, data)` | Create indirect draw buffer |
| `destroyIndirectBuffer(handle)` | Free indirect buffer |
| `multiDrawMeshIndirect(mesh, buf, count, stride)` | Multi-draw indirect |
| `setPatchVertices(n)` | Set tessellation patch size |
| `createTessShader(vs, tcs, tes, fs)` | Compile tessellation shader pipeline |
| `bindImageTexture(unit, tex, access, format)` | Bind texture for image load/store |
| `imageMemoryBarrier()` | Memory barrier for image operations |
| `createPersistentBuffer(size, flags)` | Create buffer with persistent mapping |
| `mapPersistentBuffer(handle)` | Map buffer for persistent CPU access |
| `getBindlessHandle(tex)` | Get bindless texture handle (ARB) |
| `makeTextureResident(handle)` / `makeTextureNonResident(handle)` | Manage residency |

### Mesh generation helpers

`MeshGen` provides procedural geometry:

```cpp
MeshGen::quad()              // Fullscreen quad (-1..1)
MeshGen::cubeGrid(n)         // NxNxN grid of cubes
MeshGen::sphere(segs, rings) // UV sphere
MeshGen::terrain(res, size)  // Heightmap terrain
```

### Custom shaders

Use GLSL 1.20 (`#version 120`) for GL 2.0 compatibility:

```cpp
ShaderHandle sh = r->createCustomShader(vs_source, fs_source);
int loc = r->getCustomUniformLoc(sh, "u_param");
r->useCustomShader(sh);
r->setUniform1f(loc, value);
r->drawMesh(quad);
r->destroyCustomShader(sh);
```

## File Reference

| What | Where |
|------|-------|
| Test implementation | `src/tests/test_mytest.cpp` |
| Test class declaration | `src/tests/tests.h` |
| Test registration (X-macro) | `src/tests/test_registry.def` |
| Registry types & enums | `src/tests/test_registry.h` |
| Feature interfaces | `src/renderer/features.h` |
| Base renderer interface | `src/renderer/renderer.h` |
| Preset parameter structs | `src/bench/preset.h` |
| Preset values | `src/bench/preset.cpp` |
| Config save/load | `src/bench/preset_io.cpp` |
| Build file | `CMakeLists.txt` |

## Guidelines

- **Always clean up** all GPU resources in `cleanup()` — meshes, textures, shaders
- **Use `INVALID_MESH` / `INVALID_SHADER` / `INVALID_TEXTURE`** as null handles
- **Keep render() deterministic** — same parameters should produce same workload
- **Target GLSL 1.20** for maximum compatibility (GL 2.0+)
- **Don't call glFinish()** in render() — the harness handles synchronization
- **Score should scale linearly** with the workload parameter for meaningful comparisons
- **Set capability flags** in `test_registry.def` if your test needs GL3+/GL4+ features
- **Match base class to caps** — `GL3BenchTest` for GL3 caps, `ComputeBenchTest` for compute, etc.
