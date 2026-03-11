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

## Step-by-Step Guide

### 1. Define parameters

Add a parameter struct in `src/preset.h`:

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

In `src/preset.cpp`, add values for all 4 presets:

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

Create `src/test_mytest.cpp`:

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

In `src/tests.h`:

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

**`src/app.h`** — increment `NUM_TESTS` and add the test name:

```cpp
static const int NUM_TESTS = 13; // was 12
```

**`src/app.cpp`** — add to `test_names_[]`:

```cpp
const char* App::test_names_[NUM_TESTS] = {
    // ... existing tests ...
    "MyTest"
};
```

Add to `runSelectedTests()`:

```cpp
if (test_enabled_[12] && running_) { MyTestClass t(p.mytest); runTest(&t); }
```

### 6. Add to build

In `CMakeLists.txt`, add to `SOURCES`:

```cmake
set(SOURCES
    # ... existing sources ...
    src/test_mytest.cpp
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
| `createTexture(w, h, ch, pixels)` | Create texture from pixel data |
| `destroyTexture(handle)` | Free GPU texture |
| `bindTexture(handle)` | Bind texture for rendering |
| `useShader(ShaderType)` | Use built-in shader (2D_COLOR, 2D_TEXTURED, 3D, etc.) |
| `createCustomShader(vs, fs)` | Compile custom GLSL shader |
| `setColor(r, g, b, a)` | Set current draw color |
| `setModel(Mat4)` | Set model transform matrix |
| `setDepthTest(bool)` | Enable/disable depth test |
| `setBlending(bool)` | Enable/disable alpha blending |
| `setViewport(x, y, w, h)` | Set GL viewport |
| `clear(r, g, b, a)` | Clear framebuffer |
| `resetState()` | Reset GL state to defaults |

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

## Guidelines

- **Always clean up** all GPU resources in `cleanup()` — meshes, textures, shaders
- **Use `INVALID_MESH` / `INVALID_SHADER` / `INVALID_TEXTURE`** as null handles
- **Keep render() deterministic** — same parameters should produce same workload
- **Target GLSL 1.20** for maximum compatibility (GL 2.0+)
- **Don't call glFinish()** in render() — the harness handles synchronization
- **Score should scale linearly** with the workload parameter for meaningful comparisons
