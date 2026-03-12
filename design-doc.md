# Benchmark Test Registry & Architecture Notes (C++11)

This document summarizes the design discussion for improving the benchmark project's architecture. It focuses on creating a robust compile‑time test registry, improving extensibility, and removing duplicated logic using X‑macros and data‑driven structures compatible with C++11.

---

# Goals

1. Single source of truth for all tests
2. Avoid duplicated lists across UI, CLI, factory, and runner
3. Compile‑time registry with zero runtime overhead
4. Data‑driven scoring and aggregation
5. Fast CLI lookup
6. Compatible with C++11

---

# Central Registry Using X‑Macros

Create a file containing the canonical test list.

File: `test_registry.def`

```
X(Fillrate,     TestFillrate,     "fillrate",     Pixel,   "Pixel fill throughput",     "MPix/s")
X(Geometry,     TestGeometry,     "geometry",     Geometry,"Triangle throughput",      "Mtri/s")
X(Texturing,    TestTexturing,    "texturing",    Texture, "Texture sampling rate",    "MTex/s")
X(Scene,        TestScene,        "scene",        Mixed,   "Scene rendering workload", "FPS")
X(DrawCall,     TestDrawCall,     "drawcall",     CPU,     "Driver drawcall overhead", "calls/s")
X(Overdraw,     TestOverdraw,     "overdraw",     Pixel,   "ROP overdraw stress",      "MPix/s")
X(TexUpload,    TestTexUpload,    "texupload",    Memory,  "Texture upload bandwidth", "MB/s")
X(StateChange,  TestStateChange,  "statechange",  Driver,  "Pipeline state overhead",  "ops/s")
X(Vertex,       TestVertex,       "vertex",       Geometry,"Vertex processing rate",   "Mverts/s")
X(ShaderALU,    TestShaderALU,    "shader_alu",   Compute, "Shader ALU throughput",    "GFLOP/s")
X(ShaderFMA,    TestShaderFMA,    "shader_fma",   Compute, "Shader FMA throughput",    "GFLOP/s")
X(DrawCallRaw,  TestDrawCallRaw,  "drawcall_raw", CPU,     "Raw driver submission",    "calls/s")
```

Fields:

```
(id, class, cli_name, category, description, unit)
```

---

# Test ID Enumeration

```
enum class TestId
{
#define X(id, cls, name, cat, desc, unit) id,
#include "test_registry.def"
#undef X

    Count
};
```

---

# Category Enumeration

```
enum class TestCategory
{
    Pixel,
    Geometry,
    Texture,
    Compute,
    Memory,
    CPU,
    Driver,
    Mixed,

    Count
};
```

---

# Metadata Structure

```
struct TestInfo
{
    TestId id;
    const char* name;
    TestCategory category;
    const char* description;
    const char* unit;

    std::unique_ptr<BenchTest> (*factory)(const BenchPreset&);
};
```

---

# Factory Helper

```
template<typename T>
std::unique_ptr<BenchTest> createTestFactory(const BenchPreset& preset)
{
    return std::unique_ptr<BenchTest>(new T(preset));
}
```

---

# Compile‑Time Test Table

```
static const TestInfo g_tests[] =
{
#define X(id, cls, name, cat, desc, unit) \
{ TestId::id, name, TestCategory::cat, desc, unit, &createTestFactory<cls> },

#include "test_registry.def"

#undef X
};
```

```
constexpr size_t kTestCount = sizeof(g_tests) / sizeof(g_tests[0]);
```

---

# Creating Tests

```
std::unique_ptr<BenchTest> createTest(TestId id, const BenchPreset& preset)
{
    for(size_t i = 0; i < kTestCount; i++)
    {
        if(g_tests[i].id == id)
            return g_tests[i].factory(preset);
    }

    return nullptr;
}
```

---

# UI Listing

```
for(size_t i = 0; i < kTestCount; i++)
{
    ImGui::Checkbox(g_tests[i].name, &enabled[i]);

    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", g_tests[i].description);
}
```

---

# Data‑Driven Category Aggregation

```
struct CategoryStat
{
    double sum = 0.0;
    int count = 0;

    void add(double v)
    {
        sum += v;
        count++;
    }

    double avg() const
    {
        return count ? sum / count : 0.0;
    }
};
```

```
std::array<CategoryStat, (size_t)TestCategory::Count> catStats;

for(const auto& r : results)
{
    TestCategory cat = g_tests[(size_t)r.id].category;
    catStats[(size_t)cat].add(r.score);
}
```

---

# Composite Score (Geometric Mean)

```
double composite = 1.0;
int used = 0;

for(auto& s : catScore)
{
    if(s > 0)
    {
        composite *= s;
        used++;
    }
}

if(used > 0)
    composite = pow(composite, 1.0 / used);
```

---

# Bottleneck Detection

```
size_t bottleneck = 0;
double minScore = std::numeric_limits<double>::max();

for(size_t i = 0; i < catScore.size(); i++)
{
    if(catScore[i] > 0 && catScore[i] < minScore)
    {
        minScore = catScore[i];
        bottleneck = i;
    }
}
```

---

# Compile‑Time String Hashing For CLI

Used to avoid expensive string comparisons.

FNV‑1a constexpr hash:

```
constexpr uint32_t fnv1a(const char* s, uint32_t h = 2166136261u)
{
    return *s ? fnv1a(s + 1, (h ^ uint32_t(*s)) * 16777619u) : h;
}
```

Runtime version:

```
uint32_t fnv1a_runtime(const std::string& s)
{
    uint32_t h = 2166136261u;

    for(char c : s)
        h = (h ^ uint32_t(c)) * 16777619u;

    return h;
}
```

Macro:

```
#define HASH(str) (fnv1a(str))
```

CLI lookup:

```
TestId findTestByName(const std::string& name)
{
    switch(fnv1a_runtime(name))
    {
#define X(id, cls, cli, cat, desc, unit) \
    case HASH(cli): \
        if(name == cli) return TestId::id; \
        break;

#include "test_registry.def"

#undef X

    default:
        return TestId::Count;
    }
}
```

---

# Advantages Of This Architecture

Single source of truth

Zero duplication across:

* UI
* CLI
* Factory
* Registry

Compile‑time metadata

Fast CLI lookup

Data‑driven scoring

Minimal runtime overhead

Easy to extend

Add a new test by inserting a single line in `test_registry.def`.

---

# Future Improvements

Possible extensions:

1. Category weights for composite scoring
2. Capability flags (requires compute, FBO, etc.)
3. Automatic help generation
4. Automatic JSON export schema
5. Test filtering by capability

---

# Architectural Result

Final structure:

Test Registry (.def)
↓
Compile‑time metadata table
↓
Factory / UI / CLI / Help
↓
Benchmark Runner
↓
Data‑driven aggregation
↓
Composite score + bottleneck detection

---

# Benchmark Runner Refactor

Large monolithic application controllers often accumulate too many responsibilities. A cleaner architecture splits responsibilities into smaller components.

Suggested split:

```
App
 ├─ BenchmarkRunner
 ├─ StressRunner
 ├─ UiPresenter
 └─ ResultExporter
```

Responsibilities:

BenchmarkRunner

* executes selected tests
* controls warmup/measurement loops
* handles timing modes
* collects raw results

StressRunner

* long-running stress workloads
* thermal / throttling detection
* adaptive workload calibration

UiPresenter

* renders UI
* displays results
* handles user interaction

ResultExporter

* JSON
* CSV
* text reports

This separation significantly reduces the size and complexity of the main application file.

---

# Capability Flags For Tests

Some tests require specific GPU features.

Example flags:

```
enum TestCaps
{
    Cap_None        = 0,
    Cap_FBO         = 1 << 0,
    Cap_Compute     = 1 << 1,
    Cap_Texture3D   = 1 << 2,
    Cap_TimerQuery  = 1 << 3
};
```

Extend registry entries:

```
X(Fillrate, TestFillrate, "fillrate", Pixel, "Pixel fill throughput", "MPix/s", Cap_None)
```

Runner checks capabilities before execution:

```
if((testCaps & gpuCaps) != testCaps)
    skip_test();
```

This allows automatic compatibility filtering across:

* GL2
* GL3
* GL4
* GLES

---

# Automatic Test Filtering

Tests can be skipped dynamically when hardware does not support them.

Example logic:

```
for(size_t i = 0; i < kTestCount; i++)
{
    if((g_tests[i].caps & gpuCaps) != g_tests[i].caps)
        continue;

    runTest(g_tests[i]);
}
```

Benefits:

* no manual compatibility logic
* cleaner runner
* portable benchmarks

---

# Score Normalization

Different tests produce results with different units.

Examples:

* MPix/s
* Mtri/s
* GFLOP/s
* calls/s

To combine them fairly we normalize scores relative to baseline references.

Example structure:

```
struct ScoreNormalization
{
    double reference;
};
```

Example:

```
normalized = raw_score / reference_score;
```

Composite scores should be computed from normalized values.

---

# Category Weights

Some benchmark suites prefer weighting certain workloads.

Example configuration:

```
struct CategoryInfo
{
    const char* name;
    double weight;
};
```

Example table:

```
{"Pixel", 1.0}
{"Geometry", 1.0}
{"Texture", 1.0}
{"Compute", 1.0}
{"Memory", 1.0}
{"CPU", 0.7}
{"Driver", 0.5}
{"Mixed", 1.0}
```

Composite scoring then becomes:

```
composite *= pow(score, weight);
```

---

# Result Export Architecture

Result export should be independent from the benchmark runner.

Recommended structure:

```
ResultExporter
 ├─ JSONExporter
 ├─ CSVExporter
 └─ TextExporter
```

Each exporter receives a result structure:

```
struct BenchmarkResult
{
    std::vector<TestResult> tests;
    double compositeScore;
    TestCategory bottleneck;
};
```

---

# JSON Result Schema

Example JSON output structure:

```
{
  "gpu": "GPU Name",
  "driver": "Driver Version",
  "preset": "Heavy",
  "tests": [
    { "name": "fillrate", "score": 1234.5, "unit": "MPix/s" },
    { "name": "geometry", "score": 987.2, "unit": "Mtri/s" }
  ],
  "categories": {
    "Pixel": 1200,
    "Geometry": 980
  },
  "composite": 1050,
  "bottleneck": "Geometry"
}
```

---

# Benchmark Execution Pipeline

Full runtime flow:

```
CLI / UI
     ↓
Test Selection
     ↓
BenchmarkRunner
     ↓
Test Execution
     ↓
Result Collection
     ↓
Category Aggregation
     ↓
Composite Score
     ↓
Bottleneck Detection
     ↓
Result Export
```

This pipeline keeps the system modular and maintainable while preserving high performance and minimal runtime overhead.

---

# GPU Capability Detection Layer

A dedicated capability detection layer allows the benchmark to adapt to the hardware and driver environment.

Example capability structure:

```
struct GPUCaps
{
    bool hasTimerQuery;
    bool hasCompute;
    bool hasTexture3D;
    bool hasFBO;

    int maxTextureSize;
    int maxDrawBuffers;
};
```

Capabilities should be detected once during initialization and stored in a central object.

```
GPUCaps detectCaps();
```

Tests can check requirements before execution.

This avoids runtime failures and improves portability across different GPU generations.

---

# Deterministic Benchmark Timing

Benchmarks should minimize timing noise and driver scheduling artifacts.

Recommended techniques:

* disable vsync
* warmup frames
* explicit GPU synchronization
* stable workload sizes

Typical sequence:

```
warmup phase
measurement phase
cooldown
```

Measurement loop example:

```
for(int i = 0; i < iterations; i++)
{
    renderer.draw();
}
```

Timing can use either:

* CPU timing
* GPU timer queries

GPU timers generally provide more stable results.

---

# Test Reproducibility

To ensure consistent results across runs, the benchmark should maintain deterministic workloads.

Strategies:

* fixed random seeds
* deterministic mesh generation
* deterministic shader parameters

Example:

```
std::mt19937 rng(1337);
```

This prevents run-to-run variability in geometry or texture generation.

---

# Benchmark Calibration Phase

Different GPUs have widely varying performance levels.

A calibration phase helps determine appropriate workload sizes.

Example:

```
calibrate()
{
    measure small workload
    estimate throughput
    scale workload
}
```

Benefits:

* prevents extremely short tests
* prevents excessively long runs
* stabilizes measurement accuracy

---

# Driver Anomaly Detection

Drivers occasionally introduce performance anomalies.

The benchmark can detect suspicious results.

Example checks:

* extremely low throughput
* unstable frame times
* inconsistent GPU timer results

Example logic:

```
if(cv > threshold)
    mark_result_unstable();
```

Where `cv` is coefficient of variation.

This allows flagging unreliable results.

---

# Automatic Regression Detection

When benchmarks are used repeatedly (for example in CI or labs), regression detection becomes useful.

Example workflow:

```
previous_results.json
current_results.json

compare_scores()
```

Regression rule example:

```
if(current_score < previous_score * 0.9)
    report_regression();
```

Possible uses:

* driver testing
* GPU firmware validation
* graphics stack regression tracking

---

# Extended Benchmark Architecture

With all components combined the architecture becomes:

```
Registry (.def)
      ↓
Compile-time metadata
      ↓
Capability Detection
      ↓
Test Filtering
      ↓
Calibration
      ↓
Benchmark Runner
      ↓
Timing + Measurement
      ↓
Category Aggregation
      ↓
Composite Score
      ↓
Bottleneck Analysis
      ↓
Result Export
      ↓
Regression Detection
```

This structure supports both interactive benchmarking and automated performance testing environments.
