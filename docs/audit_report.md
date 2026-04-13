# GPU Benchmark — Full Code Audit Report

**Date:** 2026-04-11
**Scope:** All modules and subsystems (~27,000 LOC C++11)
**Review level:** Chromium / LLVM / Boost / C++ Core Guidelines

---

## Table of Contents

1. [Module Dependency Graph](#1-module-dependency-graph)
2. [Renderer (26 files, ~3800 LOC)](#2-renderer-26-files-3800-loc)
3. [Engine (22 files, ~1200 LOC)](#3-engine-22-files-1200-loc)
4. [Tests (30 tests, ~60 files)](#4-tests-30-tests-60-files)
5. [Demo (82 files, ~5000+ LOC)](#5-demo-82-files-5000-loc)
6. [Bench + Platform + Geometry + UI](#6-bench--platform--geometry--ui)
7. [Testing Plan Analysis](#7-testing-plan-analysis-docstesting_planmd)
8. [Full Issue Table](#8-full-issue-table)
9. [Overall Assessment](#9-overall-assessment)

---

## 1. Module Dependency Graph

### Layer Hierarchy

```
Layer 0 — Foundation (no internal dependencies):
+-- PLATFORM   (logger, hwinfo, gpu_select, timer, compat, data_path)
+-- CORE       (app_config, poll_callback)

Layer 1 — Graphics foundation:
+-- GEOMETRY   (mesh, mesh_gen, math_types, obj_loader)  --> platform
+-- RENDERER   (GL2/GL3/GL4/GLES)                        --> geometry, platform

Layer 2 — Engine abstraction:
+-- ENGINE     (pass_context, fullscreen/geometry/compute_pass,
|               draw_list, state_cache, texture_slots, gl_debug) --> renderer, geometry

Layer 3 — Applications:
+-- TESTS      (30 bench tests, registry)   --> renderer, geometry, bench
+-- BENCH      (bench_runner, stress_runner) --> renderer, tests, platform
+-- DEMO       (demo_runner, 24 render pass) --> renderer, engine, geometry
+-- LAUNCHER   (ImGui launcher)              --> platform (only)
```

### Inter-Module Edges

```
platform <--- geometry <--- renderer <--- engine <--- demo
    ^              ^            ^            |
    |              |            |            +---> demo/shader_program.h (!! circular)
    |              |            |
    |              +------------+--- tests ---> bench
    |                           |
    +--- core ----------------->|
                                |
                       launcher (isolated)
```

### Architectural Issue: Circular Dependency ENGINE <-> DEMO

`uniform_block.h` (engine) includes `demo/shader_program.h`, while demo depends on
engine. `shader_program.h` should be extracted to engine or renderer to break the cycle.

---

## 2. Renderer (26 files, ~3800 LOC)

### Architecture Grade: A

Hierarchy `Renderer -> GL2Renderer -> GL3Renderer -> GL4Renderer` + separate `GLESRenderer` —
clean and extensible. LLVM-style feature query via `features<T>()` + `FeatureTag<T>::id` —
correct pattern for multiple inheritance with pointer adjustment.

Handle system (`Handle<Tag>`) with compile-time type safety and `static_assert` on
absence of implicit conversions:

```cpp
static_assert(!std::is_same<MeshHandle, TextureHandle>::value, "");
static_assert(std::is_trivially_copyable<MeshHandle>::value, "");
```

RAII via `ScopedHandle<H>` with move semantics, deleted copy — correct.

### Critical Issues

#### [CRITICAL] Dangling handle in `blitToScreen()` — gl2_renderer.cpp:~1079

```cpp
if (!blit_quad_ready_) {
    MeshData qd;
    // ...
    blit_quad_ = createMesh(qd);
    blit_quad_ready_ = true;      // <-- Set EVEN if createMesh returned INVALID
}
drawMesh(blit_quad_);             // <-- Uses potentially invalid handle
```

If `createMesh()` returns `INVALID_MESH`, the flag is permanently true and all subsequent
`blitToScreen()` calls silently draw with an invalid mesh.

**Fix:**
```cpp
blit_quad_ = createMesh(qd);
blit_quad_ready_ = (blit_quad_ != INVALID_MESH);
```

### High Issues

#### [HIGH] Complete absence of `glGetError()` in the renderer

All 9 renderer .cpp files have **zero** `glGetError()` calls. Per Khronos recommendations,
errors must be checked at least after resource creation (`glTexImage2D`, `glBufferData`,
`glGenFramebuffers`). The only check present is `glCheckFramebufferStatus()` for FBOs.

This violates GL Best Practices: texture allocation errors (OOM, exceeding
`GL_MAX_TEXTURE_SIZE`) pass silently.

**Recommendation:** Add a `CHECK_GL_ERROR(operation)` macro and call it after all resource
creation calls.

#### [HIGH] Inconsistent error handling across renderer methods

| Method | Behavior on invalid handle |
|--------|---------------------------|
| `destroyRenderTarget()` | Checks `isValid()`, logs |
| `bindRenderTarget()` | Silently does nothing |
| `drawMesh()` | Silently skips |
| `blitToScreen()` | Checks `isValid()`, returns early |

A unified strategy is needed: either `LOG_WRN` + early return everywhere, or silent skip
everywhere.

### Medium Issues

- **No `w/h > caps_.max_texture_size` check** in `createMRTRenderTarget()` (gl3_renderer.cpp:273)
  — `GL_INVALID_VALUE` from `glTexImage2D` passes silently
- **Integer overflow** at `Handle(meshes_.size())` — if `size() > UINT_MAX`, silent
  truncation. Practically impossible but formally UB
- **No thread-safety contract documentation** for `GLLoader`, `GLExtensions`, `GLDebug`
  (static mutable state). Project is single-threaded but this should be explicitly documented
- **`GLDebug::s_msg_counts`** not cleared on repeated `init()` — rate limiting leaks
  across GL context re-creations

### Strengths

- Move-only semantics for all GL resources (`GLMesh`, `GLFBO`, `GLTexture`) — prevents
  double-delete
- Proper GL state pairing: all `glBind*()` have matching unbind `(0)`
- VAO state capture in GL3 — correct bind/unbind order
- Shader compilation error handling with `glGetShaderInfoLog` — correct
- StateCache eliminates ~50% redundant GL calls — good optimization
- Version parsing via `sscanf("%d.%d")` — safe
- sysfs VRAM detection on Linux — correct fallback

---

## 3. Engine (22 files, ~1200 LOC)

### Architecture Grade: A+

Sokol-gfx/bgfx-inspired composable pass framework. Template Method pattern for
`FullscreenPass`, `GeometryPass`, `ComputePassBase`. Zero-cost abstractions, proper RAII.

`PassContext` — per-frame singleton with lazy feature caching. Non-copyable, non-owning
pointers documented. `RenderState` — POD struct with factory presets (`opaque()`,
`transparent()`, `shadow()`).

### Issues

#### [HIGH] `sqrtf()` in hot path — geometry_pass.cpp:33

```cpp
float dist = sqrtf(dx * dx + dy * dy + dz * dz);
float depth = dist / kDemoFar;
```

With 1000 objects x 60fps = 60,000 `sqrtf()`/sec. For DrawList sorting only **order**
matters, and `sqrt` is monotonic. Replacing with squared distance preserves order:

```cpp
float dist_sq = dx * dx + dy * dy + dz * dz;
float depth = std::min(1.0f, dist_sq / (kDemoFar * kDemoFar));
```

#### [MEDIUM] Truncation instead of rounding in DrawList — draw_list.cpp:11

```cpp
uint16_t depth_bits = static_cast<uint16_t>(d * 65535.0f);  // truncation
```

Correct: `static_cast<uint16_t>(d * 65535.0f + 0.5f)`. Practical effect is minimal
(~0.0015% error) but formally incorrect.

#### [MEDIUM] `FrameData::tier_int` instead of enum — frame_data.h:31

Uses `int tier_int` instead of `DemoTier` enum. Feature flags (`has_shadows`,
`has_bloom`, etc.) are duplicated between `FrameData` and `DemoTierConfig`, creating a
desynchronization risk.

#### [LOW] No assertions in `MaterialDef`

`metallic`, `roughness` can be outside [0,1] without warning.

### Strengths

- DrawList sort key (32 bits: shader|material|depth) — pattern from DOOM 2016
- ResourceDecl with Read/Write access — enables automatic dependency resolution
- TextureSlots — fixed slots (0-9), prevent state changes within frame
- GLDebug with rate-limited logging and severity routing — best practices
- GLExtensions with word-boundary matching for legacy path — correct

---

## 4. Tests (30 tests, ~60 files)

### Architecture Grade: A-

X-macro registry (`test_registry.def`) — one line per test, generates enum, metadata,
factory. Compile-time validation via `test_traits<T>` + `renderer_traits<R>`. Typed
subclasses (`GL3BenchTest`, `GL4BenchTest`, `ComputeBenchTest`) with automatic feature
interface acquisition.

### Critical Issues

#### [CRITICAL] BindlessTexTest missing Cap_GL4 in registry

`test_registry.def:36`: test inherits `GL4BenchTest`, uses `GL4Features`, but the
capability mask only has `Cap_BindlessTexture` — no `Cap_GL4`. Can be scheduled on a
GL3-only renderer where the downcast silently returns `nullptr`.

#### [CRITICAL] Viewport state leak — test_fbo_fillrate.cpp and test_mrt_fill.cpp

```cpp
void FBOFillrateTest::renderGL3(...) {
    r.bindRenderTarget(rt_);
    r.setViewport(0, 0, rt_size_, rt_size_);  // <-- Custom viewport
    // ... render ...
    r.bindRenderTarget(RenderTargetHandle(0));
    r.setDepthTest(true);  // <-- Restores depth test but NOT viewport
}
```

The next test in sequence inherits viewport = (0, 0, `rt_size_`, `rt_size_`) instead of
(0, 0, `vw`, `vh`). Affects all tests following FBOFillrate/MRTFill — incorrect results.

**Fix:** Add `r.setViewport(0, 0, vw, vh);` before `bindRenderTarget(0)`.

#### [CRITICAL] BindlessTexTest cleanup handles/textures size mismatch

```cpp
// setup: handles_ and textures_ populated in parallel, but if makeTextureResident() fails,
// handles_.size() != textures_.size()
// cleanup: iterates handles_ and textures_ SEPARATELY — may access nonexistent index
```

**Fix:** Use `std::min(handles_.size(), textures_.size())` or validate sizes match.

### High Issues

#### [HIGH] Division-by-zero in 5+ tests

If a preset parameter = 0 (`layers_`, `switches_`, `params_.iterations`),
`computeScore()` divides by zero -> NaN score.

Affected: `test_shader_alu.cpp`, `test_shader_fma.cpp`, `test_scene.cpp`,
`test_statechange.cpp`, `test_overdraw.cpp`.

#### [HIGH] Uniform location not validated — test_image_load_store.cpp:78-81

```cpp
int loc_size = r.getCustomUniformLoc(shader_, "u_image_size");
r.setUniform1i(loc_size, params_.image_size);  // <-- loc_size can be -1 -> UB
```

#### [HIGH] Silent feature interface failure

If `r->features<GL3Features>()` returns `nullptr`, the test becomes a no-op, but CPU
frame time is still measured. Result: invalid benchmark scores with misleadingly low values.

### Methodology Issues

#### [MEDIUM] `avgFrameMs()` uses arithmetic mean — tests.h:8-13

```cpp
inline double avgFrameMs(const std::vector<double>& times) {
    // Arithmetic mean — vulnerable to outliers
    // One driver hiccup (50ms instead of 5ms) causes 10-20% error
}
```

`bench.cpp:140` already has `percentile(0.5)` for median, but `computeScore()` in all 30
tests uses `avgFrameMs()`.

Recommendations: median + outlier rejection + warmup frame exclusion.

---

## 5. Demo (82 files, ~5000+ LOC)

### Architecture Grade: A

4-tier progressive rendering (Basic -> Ultra), 24 render passes, pipeline builder with
topological sort dependency resolution. RAII via `unique_ptr<RenderPassBase>`,
`ScopedMesh`, `ScopedTexture`. Graceful fallback: missing OBJ -> procedural sphere.

### Issues

#### [MEDIUM] SceneLoader — sscanf without return value check

```cpp
static Vec3 parseVec3(const char* val) {
    Vec3 v;
    sscanf(val, "%f %f %f", &v.x, &v.y, &v.z);  // <-- May return 0, 1, or 2
    return v;  // <-- Partially initialized vector
}
```

No check for `!= 3`, no NaN/Inf validation. Malformed input (`"1e400 foo bar"`) can
create Inf/NaN that poison all matrix math.

**Fix:**
```cpp
static Vec3 parseVec3(const char* val) {
    Vec3 v;
    if (sscanf(val, "%f %f %f", &v.x, &v.y, &v.z) != 3) {
        LOG_WRN("SceneLoader: invalid vec3 '%s'", val);
        return Vec3(0, 0, 0);
    }
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
        LOG_WRN("SceneLoader: NaN/Inf in vec3");
        return Vec3(0, 0, 0);
    }
    return v;
}
```

#### [MEDIUM] `vector::erase(begin())` in hot loop — demo_runner.cpp:142-145

```cpp
if (frame_history.size() >= 120u)
    frame_history.erase(frame_history.begin());  // O(n) shift every frame
frame_history.push_back(prev_ms);
```

Every frame after the first 2 seconds — O(120) memmove. Use `std::deque` or a circular
buffer instead.

#### [LOW] Tier config validation — NaN not caught

`tier_config_validate.cpp` checks ranges `[min, max]`, but NaN passes both comparisons
(`NaN >= min` == false, `NaN <= max` == false) — neither check catches it.

**Fix:** Add `std::isfinite()` checks for all float parameters.

### Strengths

- ShaderLoader with `#pragma include` deduplication (via `std::set<string>`) — prevents
  recursion
- ShaderCache — GLSL permutation caching with feature flags, safe preamble construction
  (no injection)
- Catmull-Rom camera — mathematically correct, edge cases handled (t=0, t=1, segment
  overflow)
- Pipeline builder — topological sort with dependency resolution via `ResourceDecl`
- DataPath — `hasPathTraversal()` correctly rejects `../` components

---

## 6. Bench + Platform + Geometry + UI

### Bench

#### [MEDIUM] Integer overflow in stress_runner.cpp:157

```cpp
int new_passes = (ms > 0.1)
    ? static_cast<int>(lround(passes * TARGET_FRAME_MS / ms))
    : passes * 4;  // <-- Overflow if passes > INT_MAX / 4
```

#### [MEDIUM] CSV export incomplete — results.cpp:73-98

Missing `gpu_ms` and `p99_median_ratio` fields.

#### [MEDIUM] `trimmedTimes()` — bench.cpp:30

`trim * 2 >= n` can overflow at extreme values.

### Platform

Foundation of the project — clean, no serious issues:

- **Logger:** All format strings are compile-time constants, no format string injection.
  2048-byte stack buffers with snprintf — safe.
- **Timer:** `CLOCK_MONOTONIC` on Unix, `QueryPerformanceCounter` on Windows — correct.
  No overflow in double conversion.
- **DataPath:** `hasPathTraversal()` — correct protection against `../` traversal.
  Multi-level fallback search (./data/ -> exe_dir/data/ -> exe_dir/../share/).
- **HWInfo:** Registry access on Windows with error checking, sysfs on Linux with proper
  file handle cleanup.

#### [LOW] `atoi()` without validation — gpu_select.cpp:153

`atoi` does not distinguish error from legitimate 0. Use `strtol` with end pointer check.

#### [LOW] `parseIntArg()` — main.cpp:32

`strtol` -> `int` cast without overflow check (values > INT_MAX silently truncate).

#### [LOW] Resolution parsing without upper bound — main.cpp:143

`"999999x999999"` parses successfully. Add `rw <= 16384 && rh <= 16384`.

### Geometry

Clean. OBJ parser with fixed buffer (`char line[4096]`, `fgets`), fan triangulation for
quads, negative index support, vertex deduplication. `math_types.h` — `normalize()`
protected from division by zero (`l > 1e-8f`).

### UI

UIView/UIState/UIAction — clean Model-View pattern. Bounds check on tier color index.
No issues.

---

## 7. Testing Plan Analysis (docs/testing_plan.md)

### Verdict: Excellent plan with several recommended additions

**Strengths:**
- Correct prioritization: pure logic -> NullRenderer -> integration
- Highest ROI first (Priority 1 requires no infrastructure)
- NullRenderer is the right idea for GPU-free testing
- Clear implementation table with dependencies
- "Zero tolerance for flaky tests" — correct policy

### Recommended Additions

1. **Priority 1, DrawList test (1E):** Add sorted output correctness test — insert 10
   elements with different shader/material/depth, sort, verify
   shader-grouping > material-grouping > depth-order.

2. **Priority 1: Missing `avgFrameMs()` edge case tests:**
   - Empty vector -> 0.0
   - One element -> that element
   - All identical -> that value
   - With outliers -> verify mean does not deviate excessively

3. **Priority 2: NullRenderer should support failure injection:**
   ```cpp
   null_renderer.setFailNextCreate(true);  // createMesh() returns INVALID
   ```
   This enables testing error paths (e.g., the `blitToScreen()` bug).

4. **Priority 2: Lifecycle tests — add viewport state verification.**
   After each test, check that viewport is restored. This catches the FBOFillrate/MRTFill
   bug.

5. **Priority 3: Add SceneLoader tests:**
   - Malformed vec3 ("foo bar baz") -> default, no crash
   - NaN/Inf values -> rejected
   - Missing sections -> partial scene loaded

6. **Not mentioned — negative tests for:**
   - `blitToScreen()` with invalid FBO handle
   - `features<T>()` on renderer without that feature
   - `computeScore()` with zero preset parameter (division by zero)
   - `getCustomUniformLoc()` returns -1

7. **Coverage metric:** Plan promises ~170 TEST_CASEs / ~1600 LOC of tests. For a ~27,000
   LOC project this is ~6% by code volume. Good for critical paths, but tests for
   audit-confirmed bugs should be prioritized — they have **confirmed value**.

---

## 8. Full Issue Table

### CRITICAL (P0) — fix immediately

| # | Issue | File | Type |
|---|-------|------|------|
| 1 | Dangling handle in `blitToScreen()` | gl2_renderer.cpp:~1079 | Memory safety |
| 2 | BindlessTexTest missing Cap_GL4 | test_registry.def:36 | Type safety |
| 3 | Viewport leak in FBOFillrate/MRTFill | test_fbo_fillrate.cpp, test_mrt_fill.cpp | State corruption |
| 4 | BindlessTexTest cleanup size mismatch | test_bindless_tex.cpp:89-103 | Memory safety |

### HIGH (P1) — significant impact

| # | Issue | File | Type |
|---|-------|------|------|
| 5 | No `glGetError()` in entire renderer | renderer/*.cpp | Khronos violation |
| 6 | Division-by-zero in 5+ tests | test_shader_alu.cpp et al. | UB |
| 7 | `sqrtf()` in hot path DrawList | geometry_pass.cpp:33 | Performance |
| 8 | Uniform location not validated | test_image_load_store.cpp:78 | UB |
| 9 | Silent feature failure -> invalid scores | bench.cpp (GL3/GL4/Compute tests) | Correctness |
| 10 | Inconsistent error handling in renderer | gl2_renderer.cpp | API contract |

### MEDIUM (P2) — fix before release

| # | Issue | File | Type | Status |
|---|-------|------|------|--------|
| 11 | SceneLoader: sscanf no check, no NaN guard | scene_loader.cpp | Input validation | |
| 12 | `avgFrameMs()` arithmetic mean vs median | tests.h:8-13 | Methodology | |
| 13 | `vector::erase(begin())` in frame history | demo_runner.cpp:142 | Performance | |
| 14 | MRT: no `w/h > max_texture_size` check | gl3_renderer.cpp:273 | Bounds check | |
| 15 | Depth quantization truncation vs rounding | draw_list.cpp:11 | Correctness | |
| 16 | Integer overflow in stress_runner | stress_runner.cpp:157 | Arithmetic | |
| 17 | CSV export missing fields | results.cpp:73-98 | Completeness | |
| 18 | `FrameData::tier_int` instead of enum | frame_data.h:31 | Type safety | |
| 19 | Tier config validation NaN not caught | tier_config_validate.cpp | Input validation | |
| 20 | ENGINE <-> DEMO circular dependency | uniform_block.h -> shader_program.h | Architecture | **FIXED** |
| 28 | GLESRenderer is GLES 2.0 only | gles_renderer.cpp | Feature gap | |

Issue 20 fixed: shader_program.h moved to engine/, frustum math extracted to engine/frustum.h.

**Issue 28 (NEW): GLESRenderer lacks GLES 3.0 features.** GLESRenderer is written as a
GLES 2.0 renderer with minimal GLES 3.0 additions (VAO, FBO, 32-bit indices). Missing
GLES 3.0 core features that are available on all modern Android devices (2013+):
instancing (`glDrawElementsInstanced`), MRT, transform feedback, UBO,
`glBlitFramebuffer`. This means ~7 bench tests (InstancedDraw, FBOFillrate, MRTFill,
TexArraySample, UBOSwitch, TransformFeedback, GeomShader) are skipped on Android
despite hardware support. Fix: extend GLESRenderer or create GLES3Renderer with
GL3Features interface support for GLES 3.0+ devices.

### LOW (P3) — code quality

| # | Issue | File | Type |
|---|-------|------|------|
| 21 | `atoi` without validation | gpu_select.cpp:153 | Robustness |
| 22 | `strtol` -> `int` overflow | main.cpp:32 | Bounds |
| 23 | Resolution without upper bound | main.cpp:143 | Bounds |
| 24 | GLDebug msg_counts not reset | gl_debug.cpp:14 | Lifecycle |
| 25 | MaterialDef no validation assertions | material.h | Defensive |
| 26 | Cap_TimerQuery defined but unused | test_registry.h:18 | Dead code |
| 27 | `features<T>()` lifetime contract undocumented | renderer.h | API documentation |

---

## 9. Overall Assessment

| Criterion | Grade | Notes |
|-----------|-------|-------|
| **Architecture** | **A** | Clean hierarchy, LLVM-style dispatch, sokol-inspired passes, proper layering |
| **Type Safety** | **A-** | Handle\<Tag\>, FeatureTag, static_assert. Minus for BindlessTex cap mismatch and tier_int |
| **Memory Safety** | **B+** | RAII everywhere, move-only GL resources. Minus for blitToScreen dangling handle and BindlessTex cleanup |
| **OpenGL Practices** | **B** | Excellent state pairing and VAO usage. Minus for absent glGetError and bounds checking |
| **Performance** | **B+** | StateCache, sort key batching, vertex cache opt. Minus for sqrtf and erase(begin()) |
| **C++ Practices** | **A-** | High C++11 standard. Explicit constructors, noexcept moves, deleted copy. Minus for atoi, strtol overflow |
| **Input Validation** | **B-** | DataPath traversal excellent. SceneLoader and CLI args insufficient |
| **Testing** | **B-** | Priority 1 implemented: 87 cases / 605 assertions in 16 suites. Priority 2-3 (NullRenderer, lifecycle) pending |
| **Documentation** | **B** | CLAUDE.md is comprehensive. Thread-safety and API lifetime contracts undocumented |

**Overall: B+ / A-** — production-grade codebase with 4 critical and 6 high issues to fix.
Architectural decisions are on par with professional render engines. After fixing P0/P1 and
implementing the testing plan — ready for release.

### Recommended Fix Order

1. ~~**Testing plan Priority 1**: 9 test files, 87 TEST_CASEs in 16 suites~~ **DONE**
2. ~~**P0 fixes** (4 issues): blitToScreen, BindlessTex caps, viewport restore, cleanup mismatch~~ **DONE**
3. ~~**P1 fixes** (5 issues): glGetError macro, sqrtf opt, uniform loc validation, feature failure handling, error handling~~ **DONE** (P1 #6 division-by-zero was already guarded)
4. ~~**P2 #20**: ENGINE <-> DEMO circular dependency~~ **DONE**
5. **P2 fixes** (9 remaining + new #28 GLES3 gap)
6. **Testing plan Priority 2-3** (~12 hours): NullRenderer + lifecycle + integration tests
7. **P3 fixes** (7 issues, ~2 hours): code quality improvements
