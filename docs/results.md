# GPU Benchmark — Interpreting Results

## Per-Test Statistics

Each test produces the following metrics:

| Metric | Description |
|--------|-------------|
| **Score** | Performance in test-specific units (higher = better) |
| **Avg (ms)** | Average frame time in milliseconds |
| **Min / Max (ms)** | Best and worst frame times |
| **P1 / P99 (ms)** | 1st and 99th percentile frame times |
| **Median (ms)** | 50th percentile frame time |
| **CV%** | Coefficient of variation (stddev / avg * 100) |
| **Frames** | Total frames measured |

### CV% — Stability Indicator

CV% shows how consistent the measurements are:

| CV% | Meaning | Action |
|-----|---------|--------|
| < 5% | Excellent stability | Result is reliable |
| 5-15% | Acceptable | Minor variance, result is usable |
| 15-50% | High variance | Background processes or frequency scaling may be affecting results |
| > 50% | Unstable | Marked `[UNSTABLE]` — test may be too lightweight for this GPU |

**Common causes of high CV:**
- Test is too easy for the GPU (frame time in microseconds, timer noise dominates)
- CPU background load (OS scheduler interference)
- GPU frequency scaling (dynamic clock not stabilizing)
- Thermal throttling during the test

**Fix:** Use a heavier preset, close background applications, or increase render resolution.

### P1 and P99

- **P1** — 99% of frames were slower than this (best-case performance)
- **P99** — 99% of frames were faster than this (worst-case performance)
- **P99/Median ratio** — if > 2.0, indicates significant frame time spikes

The P1/P99 range is more useful than Min/Max because it excludes extreme outliers (single-frame spikes from OS interrupts, etc.).

## Composite Score

The composite score aggregates all test results into category scores using geometric mean:

| Category | Tests included |
|----------|---------------|
| **Fill** | Fillrate, Overdraw, Texturing |
| **Geometry** | Geometry, Vertex |
| **Compute** | ShaderALU, ShaderFMA |
| **Overhead** | DrawCall, DrawCallRaw, StateChange, TexUpload |

**Overall** = geometric mean of the 4 category scores.

The composite score is useful for comparing GPUs, but the individual category scores reveal more about where performance differs.

**Note:** Composite uses scores from all completed tests regardless of CV%. Unstable results still contribute to the composite (the `[UNSTABLE]` marker is only a visual warning).

## Bottleneck Analysis

When all 4 categories have data, the benchmark identifies the weakest category relative to the average:

| Weakness ratio | Meaning |
|----------------|---------|
| < 50% of average | **Significantly weaker** — clear bottleneck |
| 50-80% of average | **Weakest category** — notable imbalance |
| > 80% of average | **Balanced** — no strong bottleneck |

### Additional diagnostics

- **DrawCall vs DrawCallRaw**: If DrawCallRaw >> DrawCall, uniform updates are a significant CPU-side cost
- **ShaderALU vs ShaderFMA**: If ShaderFMA >> ShaderALU (3x+), the GPU has few SFU units relative to FMA units (normal for modern NVIDIA/AMD)

### Typical bottleneck patterns

| Bottleneck | Typical cause |
|------------|---------------|
| Fill weak | Low memory bandwidth, few ROPs (integrated GPUs) |
| Geometry weak | Old GPU architecture, poor vertex cache |
| Compute weak | Few shader cores (low-end GPUs) |
| Overhead weak | High driver overhead, weak CPU (normal for high-end GPUs where GPU is much faster than driver) |

## GPU Tier

At startup, the benchmark runs a quick probe (40 frames fillrate + 40 frames geometry at 256x256) and classifies the GPU:

| Tier | Probe score (geometric mean) | Suggested preset |
|------|------------------------------|------------------|
| Legacy | < 50 | Light |
| Low | 50 - 500 | Medium |
| Mid | 500 - 5000 | Heavy |
| High | > 5000 | Ultra |

If probe data is unavailable, the tier is estimated from VRAM size, max texture size, and GL version.

## Comparing Results

For valid comparisons between different GPUs or configurations:

1. **Use the same preset** — different presets have different workloads
2. **Use the same render resolution** — `--render-res WxH`
3. **Use the same timing mode** — `--timing sync` or `--timing gpu`
4. **Check CV%** — only compare results where both have CV < 15%
5. **Use JSON export** for full precision: `--output json --output-file results.json`

### What to compare

- **Same GPU, different drivers**: Composite score shows overall driver regression/improvement
- **Different GPUs, same driver**: Category scores show architectural differences
- **GL2 vs GL3 renderer**: Tests if VAO/instancing provide measurable benefit
- **Different render resolutions**: Fill/Texturing/Shader tests scale with resolution, Overhead tests don't

## Export Formats

### Text (default)

Human-readable table format. Printed to stdout or to `--output-file`.

### CSV

Machine-readable, one row per test. Includes all per-test metrics plus system info columns. Suitable for spreadsheet analysis or automated comparison scripts.

### JSON

Full structured output including:
- System info (CPU, GPU, GL version, OS, VRAM)
- GPU capabilities (VAO, instancing, FBO, timer queries, GPU tier)
- Test config (resolution, warmup/measure frames, vsync)
- Composite scores and bottleneck analysis
- Per-test results with all metrics
