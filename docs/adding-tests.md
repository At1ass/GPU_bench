# GPU Benchmark — Adding Custom Tests

## Architecture

All tests inherit from the `BenchTest` base class defined in `src/bench.h`:

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

The benchmark harness calls these methods in order:
1. `setup()` — create GPU resources (meshes, textures, shaders)
2. `render()` — called N times during warmup and measurement
3. `cleanup()` — destroy GPU resources
4. `computeScore()` — compute a score from collected frame times

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

At startup the harness calls `getAvailableCaps()` to probe the current GL
context. Tests whose `required_caps` are not satisfied are **automatically
disabled** in the UI and skipped in CLI/headless runs — no manual checking
needed in your test code.

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

Create `src/tests/test_mytest.cpp`:

```cpp
#include "tests.h"
#include "mesh_gen.h"

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
    // Create other resources...
}

void MyTestClass::render(Renderer* r) {
    r->setDepthTest(false);
    r->setBlending(false);
    r->useShader(Renderer::SHADER_2D_COLOR);

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
    if (times.empty()) return 0;
    double total_ms = 0;
    for (size_t i = 0; i < times.size(); i++) total_ms += times[i];
    double avg_ms = total_ms / times.size();
    if (avg_ms <= 0.0) return 0;

    double ops_per_frame = (double)vw * vh * params_.complexity;
    return ops_per_frame / (avg_ms / 1000.0) / 1e6;
}
```

### 4. Declare the class

In `src/tests/tests.h`:

```cpp
class MyTestClass : public BenchTest {
public:
    MyTestClass(const MyTestParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    MyTestParams params_;
    int vw_, vh_;
    MeshHandle quad_;
};
```

### 5. Register the test

Add **one line** to `src/tests/test_registry.def`:

```cpp
X(MyTest, MyTestClass, "MyTest", "mytest", Compute, "Description of what this test measures", "Mops/s", mytest, Cap_None)
```

That's it — the `TestId` enum value, `g_tests[]` entry, CLI name, and UI label
are all generated automatically from this single line. No need to edit
`app.h` or `app.cpp`.

If your test requires specific GL features, set the capability flags
accordingly. For example, a test that needs instanced rendering:

```cpp
X(MyTest, MyTestClass, "MyTest", "mytest", Geometry, "Instanced test", "Mtri/s", mytest, Cap_Instancing)
```

Or a compute shader test (GL 4.3+):

```cpp
X(MyTest, MyTestClass, "MyTest", "mytest", Compute, "Compute test", "GFLOP/s", mytest, Cap_Compute)
```

### 6. Add to build

In `CMakeLists.txt`, add to `SOURCES`:

```cmake
set(SOURCES
    # ... existing sources ...
    src/tests/test_mytest.cpp
)
```

### 7. Optional: Add to composite scoring

If your test belongs to a category, add it in `src/bench.cpp` `computeCompositeScores()`:

```cpp
// Example: add to Compute category
{
    double vals[3] = {0, 0, 0};  // was [2]
    const BenchResult* r;
    if ((r = findResult(results, "ShaderALU")) && r->score > 0) vals[0] = r->score;
    if ((r = findResult(results, "ShaderFMA")) && r->score > 0) vals[1] = r->score;
    if ((r = findResult(results, "MyTest"))    && r->score > 0) vals[2] = r->score;
    cs.compute = geomean(vals, 3);
}
```

### 8. Optional: Add to config save/load

In `src/config.cpp`:

**saveConfig():**
```cpp
fprintf(f, "\n[mytest]\n");
fprintf(f, "complexity=%d\n", preset.mytest.complexity);
```

**parseLine():**
```cpp
} else if (section == "mytest") {
    if (key == "complexity") p.mytest.complexity = atoi(val.c_str());
}
```

## Renderer API

Tests interact with the GPU through the `Renderer` abstract interface. Key methods:

| Method | Description |
|--------|-------------|
| `createMesh(MeshData)` | Upload vertex/index data, returns handle |
| `destroyMesh(handle)` | Free GPU mesh resources |
| `drawMesh(handle)` | Draw a mesh |
| `drawMeshInstanced(handle, count)` | Draw mesh with hardware instancing (GL3+) |
| `createTexture(w, h, ch, pixels)` | Create texture from pixel data |
| `destroyTexture(handle)` | Free GPU texture |
| `bindTexture(handle)` | Bind texture for rendering |
| `useShader(ShaderType)` | Use built-in shader (2D_COLOR, 2D_TEXTURED, 3D, etc.) |
| `createCustomShader(vs, fs)` | Compile custom GLSL shader |
| `setColor(r, g, b, a)` | Set current draw color |
| `setModel(Mat4)` | Set model transform matrix |
| `setUniformMat4(loc, Mat4)` | Set a mat4 uniform by location |
| `setDepthTest(bool)` | Enable/disable depth test |
| `setBlending(bool)` | Enable/disable alpha blending |
| `setViewport(x, y, w, h)` | Set GL viewport |
| `clear(r, g, b, a)` | Clear framebuffer |
| `resetState()` | Reset GL state to defaults |
| `createComputeShader(source)` | Compile compute shader (GL4.3+ only) |
| `dispatchCompute(x, y, z)` | Launch compute work groups (GL4.3+ only) |
| `createSSBO(binding)` | Create shader storage buffer object (GL4.3+ only) |
| `destroySSBO()` | Destroy SSBO |
| `bindSSBO()` | Bind SSBO for use |

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
| Preset parameter structs | `src/bench/preset.h` |
| Preset values | `src/bench/preset.cpp` |
| Build file | `CMakeLists.txt` |

## Guidelines

- **Always clean up** all GPU resources in `cleanup()` — meshes, textures, shaders
- **Use `INVALID_MESH` / `INVALID_SHADER` / `INVALID_TEXTURE`** as null handles
- **Keep render() deterministic** — same parameters should produce same workload
- **Target GLSL 1.20** for maximum compatibility (GL 2.0+)
- **Don't call glFinish()** in render() — the harness handles synchronization
- **Score should scale linearly** with the workload parameter for meaningful comparisons
- **Set capability flags** in `test_registry.def` if your test needs GL3+/GL4+ features
