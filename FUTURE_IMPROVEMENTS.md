# Future Improvements

## 1. Test Coverage Expansion

### New GL2 Tests (low effort)

| Test | What it measures | Rationale |
|------|-----------------|-----------|
| **BilinearVsNearest** | Texture filtering cost | Shows overhead of bilinear/trilinear filtering vs nearest |
| **DepthComplexity** | Early-Z rejection | Important for forward rendering — how efficiently GPU culls occluded pixels |
| **SmallTriangles** | Rasterizer throughput on small triangles | On old GPUs, small triangles (< 8 pixels) render catastrophically slow |
| **VBOUpload** | Dynamic vertex data upload | Complements TexUpload — measures CPU->GPU for VBO (glBufferSubData) |
| **AlphaTest** | Cost of discard/alpha test vs alpha blend | On old GPUs alpha test can be faster or slower than blend |

### New GL3/GL4 Tests

| Test | What it measures | GL | Rationale |
|------|-----------------|:--:|-----------|
| **TessellationThroughput** | Tessellation shader performance | 4.0 | Only pipeline stage without a test |
| **TextureGather** | textureGather() vs 4x texelFetch | 4.0 | Critical for SSAO, shadow mapping |
| **ImageLoadStore** | Image read/write vs SSBO | 4.2 | Separate memory path on GPU |
| **ComputePrefix** | Parallel prefix sum (scan) | 4.3 | Real compute pattern, not synthetic |
| **PersistentMapping** | GL_MAP_PERSISTENT_BIT streaming | 4.4 | Modern GPU-driven rendering path |
| **BindlessTexture** | ARB_bindless_texture throughput | 4.4+ | Eliminates bind overhead entirely |

Adding a test = 1 line in `.def` + one `.cpp` file (50-100 lines). Infrastructure is ready.

---

## 2. Measurement Improvements

### Statistics and Validation
- **Outlier trimming**: Currently all frame times go into calculation. Trim mean (discard top/bottom 5%) would give more stable results
- **Minimum N**: Automatically increase measure_frames until confidence interval narrows to +/-5%
- **Repeated runs**: "Best of 3 runs" mode to reduce background process influence
- **GPU clock detection**: If sysfs is available (`/sys/class/drm/card*/gt_cur_freq_mhz`), log GPU frequency during test to detect boost/throttle

### Score Normalization
- Currently Composite Score is an absolute number without scale. Introduce a **reference baseline** (e.g., GeForce GT 710 = 1000 points) for cross-run/cross-machine comparability
- Score per clock / score per watt — if frequency/power info is available

---

## 3. Quality Infrastructure

### CI/CD Pipeline
```
GitHub Actions -> build (linux/mingw64/mingw32) -> smoke test (headless) -> release
```
- Automatic build on 3 platforms on every push
- Headless smoke test: run `--headless --test fillrate --preset light` — verify binary works
- Release workflow: tag -> build -> zip -> GitHub Release

### Unit Tests
- Testable without GPU: `computePercentile()`, `computeStats()`, `validatePreset()`, `computeCompositeScore()`, `detectBottleneck()`, INI parser
- Don't require GL context, can run in CI without GPU

### Static Analysis
- `clang-tidy` (modernize-*, bugprone-*, performance-*)
- `cppcheck` for leak and UB detection
- Integration via CMake preset or CI step

---

## 4. UX and Usability

### UI Improvements
- **Charts**: Sparkline/bar chart for each test (frame time distribution) — ImGui supports `ImGui::PlotHistogram()`
- **A/B comparison**: Load previous JSON result and show delta (green/red) next to current results
- **GPU profile**: On first run save results to `~/.gpu_benchmark/`, on repeat — show regression
- **Methodology tooltip**: On hover over test show shader, score formula, what exactly is the bottleneck

### CLI Improvements
- `--list-tests` — print all tests with description and requirements
- `--compare <file1.json> <file2.json>` — diff two results in terminal
- `--repeat N` — N runs with best/average selection
- `--filter category=compute` — run tests by category
- Tab-completion script for bash/zsh

---

## 5. Cross-Platform

### macOS
- Current status: GL fallback via Apple's deprecated OpenGL. Works, but GL 4.1 max
- Options: keep as-is (Apple deprecated GL but it still works) or MoltenVK (Vulkan->Metal translation)

### Wayland
- SDL2 supports Wayland, but gpu_select.cpp relies on DRI (/sys/class/drm/). Verify compatibility
- EGL context creation instead of GLX for native Wayland

### FreeBSD
- Minimal changes: `/proc/cpuinfo` -> `sysctl hw.model`, rest via GL

### Static SDL2 Linking
- For maximum portability — single binary without dependencies
- `cmake -DSDL2_STATIC=ON` + bundled SDL2

---

## 6. Results and Analytics

### Local History
- SQLite database in `~/.gpu_benchmark/history.db`
- Each run = record (timestamp, GPU, driver version, preset, scores)
- CLI: `--history` — show performance trend
- UI: score-over-time chart (driver degradation detection)

### Regression Detection
- `--baseline <file.json>` — compare against reference
- Automatic verdict: "Fillrate degraded 15% vs baseline" / "Compute improved 8%"
- Exit code != 0 on regression > threshold — for CI integration

### Online Database (optional, long-term)
- Anonymous result submission (opt-in)
- Web dashboard with percentiles by GPU model
- "Your GPU scores in Xth percentile among GTX 1060 submissions"

---

## 7. Architectural Refactoring

### Split App Class
```
App (coordinator, ~200 lines)
+-- SessionManager (preset selection, validation, GPU tier)
+-- PreviewScene (preview rendering, 3D scene)
+-- ExportManager (format selection, file writing)
```

Currently App is ~1,500 lines. Splitting simplifies testing and maintenance.

### MRT Texture Leak Fix
- `GLFBO` struct stores only one color texture. With MRT, 2-4 are created but 1-3 are leaked
- Fix: `std::vector<GLuint> color_textures` in GLFBO

### unique_ptr in BenchRunner
```cpp
// Current:
BenchTest* test = g_tests[i].factory(preset);
runTest(test, ...);
delete test;  // if runTest() throws — leak

// Should be:
auto test = std::unique_ptr<BenchTest>(g_tests[i].factory(preset));
runTest(test.get(), ...);
```

### GPU Timer Sentinel
- On timeout return `-1.0` instead of `0.0`, log warning

---

## 8. Packaging and Distribution

- **AppImage** (Linux) — single file, runs on any distro
- **AUR package** (Arch Linux) — `gpu-benchmark-git`
- **Flatpak** — sandboxed, but needs GPU access (org.freedesktop.Platform.GL)
- **Windows installer** — NSIS or WiX, with Start Menu icon
- **Portable ZIP** — already exists (`bench.zip`), formalize

---

## Priority Matrix

| Direction | Effort | Impact | Recommendation |
|-----------|:------:|:------:|:--------------:|
| New GL2 tests | Low | Medium | Quick win |
| MRT leak + unique_ptr | Low | High | Do now |
| CI/CD pipeline | Low | High | Do now |
| Unit tests (stats) | Low | Medium | Do now |
| CLI: --list-tests, --compare | Low | Medium | Quick win |
| Frame time charts in UI | Medium | Medium | Next release |
| A/B result comparison | Medium | High | Next release |
| New GL4 tests | Medium | Medium | As needed |
| Regression detection | Medium | High | Next release |
| Split App class | Medium | Medium | Next refactoring |
| AppImage/AUR | Medium | Medium | For distribution |
| SQLite history | High | Medium | Long-term |
| Vulkan backend | Very High | High | Separate project |
| Online database | Very High | Medium | Long-term |

---

## Recommended Roadmap

### v0.6 (immediate)
1. Fix MRT texture leak + unique_ptr in BenchRunner
2. CI pipeline (GitHub Actions: build linux/mingw64/mingw32)
3. `--list-tests` in CLI
4. Unit tests for stats/preset/scoring (no GPU required)

### v0.7 (next release)
1. 2-3 new GL2 tests (SmallTriangles, DepthComplexity, BilinearVsNearest)
2. Frame time histogram in UI
3. `--compare` and `--baseline` in CLI
4. AppImage for Linux

### v1.0 (stable release)
1. Full set of ~30 tests
2. A/B comparison in UI
3. Local result history
4. Regression detection with CI exit codes
5. User documentation
