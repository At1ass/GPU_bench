# GPU Benchmark — Usage Guide

## GUI Mode

Launch without arguments for interactive mode:

```bash
./gpu_benchmark
```

The GUI provides:
- Preset selection (Light / Medium / Heavy / Ultra / Extreme)
- Render resolution selection (native or custom via FBO)
- Individual test checkboxes
- Custom parameter editing
- Real-time results table with CV% color coding
- Composite score and bottleneck analysis
- Export button (Text / CSV / JSON)
- Live preview scene when idle

## Headless Mode

```bash
./gpu_benchmark --headless [options]
```

Runs benchmarks without GUI, outputs results to stdout (or `--output-file`). Useful for automated testing, CI pipelines, and remote systems.

On Linux, works without a display server (X11/Wayland) if SDL 2.0.22+ is available (uses SDL offscreen video driver). On Windows, a display connection is always required.

## Demo Mode

3DMark-style visual benchmark with the "Sanctuary" scene (Stanford Bunny on pedestal, columns, arch, rocks, grass, trees, puddles) and Catmull-Rom camera orbit through 4 quality tiers (Basic / Enhanced / Quality / Ultra).

The demo is built as a separate binary (`gpu_demo`):

```bash
# Auto-detect supported tiers, run all
./gpu_demo

# Specific tier only
./gpu_demo --tier 3

# Custom duration (seconds per tier)
./gpu_demo --duration 20

# Headless demo with JSON export
./gpu_demo --headless --output json --output-file demo.json

# Demo with debug overlay (frame stats, GL debug output)
./gpu_demo --debug

# Verbose trace logging (per-pass pipeline trace, GL debug groups)
./gpu_demo --debug=verbose
```

The demo runs each tier sequentially, measuring FPS throughout. After all tiers complete, a results screen is shown (interactive mode) or results are exported (headless mode). The demo score is a geometric mean of normalized FPS across tiers, scaled by 10000.

The scene is loaded from `data/scenes/sanctuary.scene` — a data-driven format that allows editing object placement and materials without recompilation.

## CLI Examples

### Basic usage

```bash
# All tests, Medium preset
./gpu_benchmark --headless --test all

# Specific tests
./gpu_benchmark --headless --test fillrate,geometry,scene

# Ultra preset, JSON output
./gpu_benchmark --headless --preset ultra --test all --output json
```

### Custom render resolution

```bash
# 4K resolution via FBO (even on a 1080p monitor)
./gpu_benchmark --headless --preset ultra --render-res 3840x2160 --test all

# 720p (lighter than native on a 4K monitor)
./gpu_benchmark --headless --render-res 1280x720 --test all
```

When render resolution differs from the window size, the benchmark renders to an FBO (framebuffer object) and blits to the screen. Requires FBO support (GL 2.1 with GL_EXT_framebuffer_object or GL 3.0+).

### GPU timer queries

```bash
# Use GPU-side timing instead of CPU timer + glFinish
./gpu_benchmark --headless --timing gpu --test all
```

GPU timing uses `GL_TIME_ELAPSED` queries (requires GL 3.3+ or GL_ARB_timer_query). Falls back to sync timing if unavailable.

### Multi-GPU selection

```bash
# List available GPUs
./gpu_benchmark --list-gpus

# Select GPU by index
./gpu_benchmark --gpu 1 --headless --test all
```

### Renderer selection

```bash
# Force OpenGL 2.0 renderer
./gpu_benchmark --renderer gl2 --headless --test all

# Force OpenGL 3.0+ renderer
./gpu_benchmark --renderer gl3 --headless --test all

# Force OpenGL 4.3+ renderer (compute shader support)
./gpu_benchmark --renderer gl4 --headless --test all

# Force OpenGL ES 2.0/3.0 renderer (for embedded/mobile-class GPUs)
./gpu_benchmark --renderer gles --headless --test all

# Auto-detect (default)
./gpu_benchmark --renderer auto --headless --test all
```

### Output to file

```bash
# JSON to file
./gpu_benchmark --headless --test all --output json --output-file results.json

# CSV for spreadsheet analysis
./gpu_benchmark --headless --test all --output csv --output-file results.csv
```

### Stress test

```bash
# 5-minute stress test
./gpu_benchmark --stress 300 --preset ultra

# Stress at 4K
./gpu_benchmark --stress 300 --preset ultra --render-res 3840x2160
```

The stress test:
1. Calibrates pass count to achieve ~40ms per frame on any GPU
2. Runs a combined shader (FMA + SFU + texture + blending) continuously
3. Reports FPS and degradation every 10 seconds
4. Detects thermal throttling (>5% drop from baseline)

### Custom config

```bash
# Load custom preset from INI file
./gpu_benchmark --config my_preset.ini --headless --test all
```

## Window Size

The `--width` and `--height` options set the window size (default: 800x600). This is separate from render resolution:

```bash
# Large window, native render resolution
./gpu_benchmark --width 1920 --height 1080

# Small window, 4K render resolution (via FBO)
./gpu_benchmark --width 800 --height 600 --render-res 3840x2160
```

## Test Names for --test

Use comma-separated CLI names:

**GL2 universal (any GPU):**
```
fillrate, geometry, texturing, scene, drawcall, drawcall_raw,
overdraw, texupload, statechange, vertex, shader_alu, shader_fma
```

**GL3+ (OpenGL 3.0+):**
```
instanced_draw, fbo_fillrate, mrt_fill, texarray_sample,
geom_shader, ubo_switch, transform_feedback
```

**GL4+ (OpenGL 4.0+):**
```
compute_fma, compute_bw, compute_smem, ssbo_atomics, compute_prefix,
indirect_draw, tess_tp, tex_gather, image_ls, persistent_map, bindless_tex
```

Or `all` to run everything:

```bash
./gpu_benchmark --headless --test all
```

**Note:** Tests requiring unsupported GL features are automatically skipped in headless mode and greyed out in GUI. For example, `compute_fma` requires GL 4.3+ and will be skipped on older GPUs.
