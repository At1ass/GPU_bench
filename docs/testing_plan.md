# Testing Plan

## Current State

Project has 27,000 lines of C++ code and 251 lines of tests (0.9% coverage). Existing 18 tests cover only `computeStats` and `compositeScores` — statistical functions. Everything else (math, parsing, resource lifecycle, render pipeline) is untested. New developers cannot verify changes without running on real GPU.

Goal: build testing from most critical to least, so each stage delivers immediate value.

---

## Priority 1: Pure Logic Tests (no GPU, no new infrastructure)

These use existing doctest framework. Zero new dependencies. Add test files to `tests/` directory and `CMakeLists.txt gpubench_tests` target. **Highest ROI** — catches regressions in math, parsing, data structures that silently corrupt results.

### 1A. Math (geometry/math_types.h) — ~120 lines of tests

**File:** `tests/test_math.cpp`

Vec3:
- `length()`: unit vectors, zero vector, large values
- `normalized()`: unit vectors stay unit, zero vector → zero (not NaN), near-zero threshold
- `cross()`: orthogonal basis (i×j=k), parallel vectors → zero, anti-commutative (a×b = -b×a)
- `dot()`: orthogonal → 0, parallel → |a|·|b|, self-dot → length²

Mat4:
- `perspective()`: known FOV/aspect → compare with reference values, znear ≈ zfar edge
- `lookAt()`: camera at origin looking +Z → known matrix, verify orthonormal basis
- `translate/scale/rotateY`: identity composition, inverse operations
- `operator*()`: A·I = A, (A·B)·C = A·(B·C) associativity, non-commutative A·B ≠ B·A
- `transformPoint()`: translate moves point, scale scales, rotate rotates
- `transformDirection()`: translate does NOT affect direction, scale does affect

### 1B. OBJ Parser (geometry/obj_loader.cpp) — ~100 lines of tests

**File:** `tests/test_obj_loader.cpp`

Write test OBJ strings to temp files, parse, verify:
- Triangle: `v`×3 + `f 1 2 3` → 3 verts, 3 indices
- Quad: `f 1 2 3 4` → 4 verts, 6 indices (fan triangulation)
- Format variants: `f v/t/n`, `f v//n`, `f v/t`, `f v`
- Negative indices: `f -1 -2 -3` with 3 vertices → same as `f 3 2 1`
- Normals provided → no recomputation; normals absent → recomputed
- Empty file → empty MeshData
- Comments and blank lines skipped
- `normalize()`: mesh centered at origin, fits in [-1,1]

### 1C. Mesh Generation (geometry/mesh_gen.cpp) — ~80 lines

**File:** `tests/test_mesh_gen.cpp`

For each generator verify structural invariants (not visual):
- `quad()`: 4 verts, 6 indices, all UVs in [0,1]
- `cube()`: 24 verts (4 per face × 6), 36 indices, all normals unit-length
- `sphere(16, 8)`: vertex count = (16+1)×(8+1), all normals ≈ unit length
- `recomputeNormals()`: all output normals unit-length, consistent winding
- `boundingRadius()`: sphere radius ≥ max vertex distance from origin
- `optimizeVertexCache()`: index count unchanged, vertex count unchanged, same triangles (set comparison)

### 1D. Preset I/O (bench/preset_io.cpp) — ~60 lines

**File:** `tests/test_preset_io.cpp`

- Roundtrip: save → load → compare all fields
- Malformed input: missing `=`, empty value, garbage text → no crash, defaults preserved
- Boundary clamping: `layers=-5` → clamped to 1, `layers=999999` → clamped to 10000
- Unknown section/key: silently skipped

### 1E. DrawList Sort Key (engine/draw_list.cpp) — ~40 lines

**File:** `tests/test_draw_list.cpp`

- Sort key encoding: shader=1, material=2, depth=0.5 → expected 32-bit value
- Sort priority: shader 2 > shader 1 regardless of material/depth
- Depth quantization: 0.0 → 0x0000, 1.0 → 0xFFFF, 0.5 → ~0x7FFF
- Negative depth clamped to 0, depth > 1 clamped to 1

### 1F. Camera Spline (demo/demo_camera.cpp) — ~40 lines

**File:** `tests/test_camera.cpp`

- `catmullRom()`: t=0 → p1, t=1 → p2 (passes through control points)
- `getPosition(0)` → first keypoint, `getPosition(1)` → last keypoint
- Monotonic t → continuous path (no jumps between segments)

### 1G. Shader Feature Flags (demo/shader_cache.cpp) — ~50 lines

**File:** `tests/test_shader_features.cpp`

- `featuresForTier()`: each tier has exactly one version flag set (mutual exclusion)
- Basic always has SF_VIGNETTE
- Ultra has SF_PBR, SF_SSS, SF_WATER (not present in lower tiers)
- GLES path caps at Enhanced
- GLES 2.0 → SF_GLES_100, GLES 3.0 → SF_GLES_300

### 1H. Path Utilities (platform/data_path.cpp, platform/logger.h) — ~50 lines

**File:** `tests/test_platform.cpp`

- `hasPathTraversal("../etc/passwd")` → true
- `hasPathTraversal("models/bunny.obj")` → false
- `hasPathTraversal("a/../../b")` → true
- `hasPathTraversal(nullptr)` → true
- `extractSubsystem("src/renderer/gl2.cpp")` → "renderer"
- `extractSubsystem("src/demo/scene.cpp")` → "demo"
- `extractSubsystem("main.cpp")` → "app" (default)
- `extractFilename("/home/user/src/foo.cpp")` → "foo.cpp"

### 1I. Score Computation Formulas (tests/*.cpp) — ~80 lines

**File:** `tests/test_score_formulas.cpp`

For each BenchTest, verify `computeScore()` with known inputs:
- FillrateTest: 200 layers, 512×512, avg 1.0ms → expected MPix/s
- GeometryTest: grid 25 (187500 tris), avg 1.0ms → expected Mtri/s
- TexUploadTest: 256×256, 3ch, 4 uploads, avg 1.0ms → expected MB/s
- ComputeFMATest: 64 workgroups, 100 iters, avg 1.0ms → expected GFLOP/s
- All 12 GL2 tests + representative GL3/GL4 tests

**Total Priority 1: ~620 lines, 8 test files, ~80 TEST_CASEs**

---

## Priority 2: NullRenderer + Resource Lifecycle Tests

### 2A. NullRenderer Implementation — ~200 lines

**File:** `src/renderer/null_renderer.h`, `src/renderer/null_renderer.cpp`

Implements all ~40 pure virtual methods from Renderer:
- Resource methods: allocate Handle IDs from counter, track alive counts
- Draw/state methods: no-op, increment counters
- Info methods: return fixed strings ("NullRenderer", "Null Vendor", etc.)
- Timer queries: return false (no GPU timing)
- Render targets: supported (counter-based), no actual FBO
- Caps: configurable via constructor (default: GL 4.6 equivalent, all features)

Counters exposed:
```cpp
struct NullStats {
    int meshes_created, meshes_destroyed;
    int textures_created, textures_destroyed;
    int shaders_created, shaders_destroyed;
    int render_targets_created, render_targets_destroyed;
    int draw_calls, state_changes, clears;
    int alive_meshes() const { return meshes_created - meshes_destroyed; }
    // ...
};
```

Optional: NullGL3Features, NullGL4Features, NullComputeFeatures — for testing feature-dependent code paths.

### 2B. BenchTest Lifecycle Tests — ~150 lines

**File:** `tests/test_bench_lifecycle.cpp`

For each of 30 tests via test_registry:
```cpp
TEST_CASE("All BenchTests: no resource leaks") {
    NullRenderer r;
    r.init(512, 512);
    for (int i = 0; i < NUM_TESTS; i++) {
        auto test = g_tests[i].factory(default_preset);
        test->setup(&r, 512, 512);
        test->render(&r);
        test->cleanup(&r);
        CHECK(r.stats().alive_meshes() == 0);
        CHECK(r.stats().alive_textures() == 0);
        CHECK(r.stats().alive_shaders() == 0);
    }
}
```

Individual lifecycle edge cases:
- Double cleanup: `setup → cleanup → cleanup` → no crash
- Render without setup: `render` → no crash (graceful no-op via handle checks)
- Setup with tiny viewport (1×1): no crash

### 2C. ScopedHandle RAII Tests — ~60 lines

**File:** `tests/test_scoped_handle.cpp`

- Scope exit destroys resource
- Move transfers ownership (source doesn't destroy)
- `reset()` destroys immediately
- `release()` detaches without destroying
- `assign()` destroys previous before accepting new
- Default-constructed ScopedHandle: no-op on destroy

### 2D. Handle Type Safety Tests — ~30 lines

**File:** `tests/test_handles.cpp`

- `Handle<MeshTag>` and `Handle<TextureTag>` are different types (compile-time, but verify at runtime via sizeof/alignment)
- `explicit operator bool()`: zero → false, non-zero → true
- `explicit operator unsigned int()`: round-trip `Handle(42).id == 42`
- Equality/inequality operators

### 2E. StateCache Tests — ~60 lines

**File:** `tests/test_state_cache.cpp`

- Initial state: all UNKNOWN
- `setDepthTest(true)` → returns true (changed), second call → false (no change)
- `reset()` → all back to UNKNOWN, next set → returns true
- `useProgram(5)` → true, `useProgram(5)` → false, `useProgram(6)` → true
- `bindTexture(unit, id)`: out-of-range unit → always returns true
- `bindFramebuffer()`: same FBO → false

**Total Priority 2: ~500 lines, NullRenderer + 5 test files, ~60 TEST_CASEs**

---

## Priority 3: Integration & Regression Tests

### 3A. BenchRunner Flow Test — ~80 lines

**File:** `tests/test_bench_runner.cpp`

Using NullRenderer:
- `runAll()` completes without crash, produces 12+ results (GL2 tests)
- Each result has `valid == true`, `score > 0`, `frames > 0`
- `computeAnalysis()` produces non-zero composite scores
- Progress callback fires, returns monotonically increasing values

### 3B. Demo Scene Construction Test — ~60 lines

**File:** `tests/test_demo_scene.cpp`

Using NullRenderer:
- `DemoResources::prepare()` succeeds (shaders are no-op, but mesh gen works)
- Scene builds with >0 objects
- Tier config returns valid config for each tier 1-4
- `maxSupportedTier()` returns 4 for NullRenderer with full caps

### 3C. Shader Feature Coverage Test — ~40 lines

**File:** `tests/test_shader_tier_coverage.cpp`

- Every tier 1-4 × {desktop, core_profile, gles2, gles3} → valid feature set
- No two tiers produce identical feature sets (progressive enhancement)
- Every feature set has exactly one version flag set
- Ultra ⊃ Quality ⊃ Enhanced ⊃ Basic (monotonic feature addition for desktop)

### 3D. Export Format Tests — ~50 lines

**File:** `tests/test_export.cpp`

Using pre-built BenchResult vector:
- Text export: contains test names, scores, units
- CSV export: valid CSV (header + N data rows, comma-separated)
- JSON export: valid JSON (parseable, contains expected fields)

**Total Priority 3: ~230 lines, 4 test files, ~30 TEST_CASEs**

---

## Implementation Order

| Step | What | Files | Tests | Deps |
|------|------|-------|-------|------|
| 1 | Math tests | test_math.cpp | ~20 | none |
| 2 | OBJ + MeshGen tests | test_obj_loader.cpp, test_mesh_gen.cpp | ~20 | none |
| 3 | Preset I/O + Platform | test_preset_io.cpp, test_platform.cpp | ~15 | none |
| 4 | DrawList + Camera + ShaderFeatures | test_draw_list.cpp, test_camera.cpp, test_shader_features.cpp | ~15 | none |
| 5 | Score formulas | test_score_formulas.cpp | ~15 | none |
| 6 | NullRenderer | null_renderer.h/cpp | 0 | new code |
| 7 | Lifecycle + RAII + Handle + StateCache | test_bench_lifecycle.cpp, test_scoped_handle.cpp, test_handles.cpp, test_state_cache.cpp | ~40 | NullRenderer |
| 8 | BenchRunner + DemoScene + Export | test_bench_runner.cpp, test_demo_scene.cpp, test_export.cpp | ~20 | NullRenderer |
| 9 | Shader tier coverage | test_shader_tier_coverage.cpp | ~10 | NullRenderer (optional) |

Steps 1-5 can all be implemented immediately — no new infrastructure needed.
Steps 6-9 require NullRenderer first.

## Build Integration

Add to `CMakeLists.txt` gpubench_tests target:
```cmake
set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_bench_stats.cpp
    tests/test_bench_scoring.cpp
    # Priority 1
    tests/test_math.cpp
    tests/test_obj_loader.cpp
    tests/test_mesh_gen.cpp
    tests/test_preset_io.cpp
    tests/test_draw_list.cpp
    tests/test_camera.cpp
    tests/test_shader_features.cpp
    tests/test_platform.cpp
    tests/test_score_formulas.cpp
    # Priority 2 (after NullRenderer)
    tests/test_bench_lifecycle.cpp
    tests/test_scoped_handle.cpp
    tests/test_handles.cpp
    tests/test_state_cache.cpp
    # Priority 3
    tests/test_bench_runner.cpp
    tests/test_demo_scene.cpp
    tests/test_shader_tier_coverage.cpp
    tests/test_export.cpp
)
```

NullRenderer goes into gpubench_core library (alongside GL2/GL3/GL4/GLES renderers).

## Verification

After each step:
```bash
cd build_native && make -j$(nproc) && ./gpubench_tests
```
All tests must pass. Zero tolerance for flaky tests — all pure logic, deterministic.

## Expected Outcome

| Metric | Before | After Priority 1 | After All |
|--------|--------|-------------------|-----------|
| Test files | 3 | 11 | 20 |
| TEST_CASEs | 18 | ~100 | ~170 |
| Lines of tests | 251 | ~870 | ~1600 |
| Coverage | scoring only | math, parsing, data structures, formulas | + resource lifecycle, runner flow, scene construction |
| Can test without GPU | scoring only | all pure logic | everything except shader compilation and visual output |
