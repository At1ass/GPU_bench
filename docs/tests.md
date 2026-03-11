# GPU Benchmark — Tests

## Overview

The benchmark includes 14 tests organized into 4 categories for composite scoring:

| Category | Tests | What it measures |
|----------|-------|------------------|
| **Fill** | Fillrate, Overdraw, Texturing | Pixel output, blending, texel fetch |
| **Geometry** | Geometry, Vertex, InstancedDraw (GL3+) | Triangle throughput, vertex processing, instancing |
| **Compute** | ShaderALU, ShaderFMA, ComputeFMA (GL4.3+) | Fragment shader math (transcendentals + FMA), compute shaders |
| **Overhead** | DrawCall, DrawCallRaw, StateChange, TexUpload | CPU/driver overhead |

The **Scene** test is a combined workload and does not belong to any single category.

---

## Fill Category

### Fillrate

**Unit:** Mpix/s (megapixels per second)

Renders multiple overlapping opaque fullscreen quads without blending. Measures raw pixel output rate — the speed at which the GPU can write pixels to the framebuffer.

**Parameters:**
- `layers` — number of fullscreen quads per frame (Light: 100, Medium: 500, Heavy: 1500, Ultra: 4000)

**Scoring formula:**
```
score = (viewport_w * viewport_h * layers) / avg_time_sec / 1e6
```

**What affects it:** ROP count, memory bandwidth, framebuffer format.

### Overdraw

**Unit:** Mpix/s (megapixels per second)

Similar to Fillrate but with alpha blending enabled (alpha = 0.3). Measures blended pixel throughput — how fast the GPU handles read-modify-write operations on the framebuffer.

**Parameters:**
- `layers` — number of blended fullscreen quads (Light: 50, Medium: 200, Heavy: 500, Ultra: 1000)
- `alpha` — blend alpha value (default: 0.3)

**Scoring formula:** Same as Fillrate.

**Comparing with Fillrate:** If Overdraw score is significantly lower than Fillrate, the GPU's blending hardware (ROPs) is the bottleneck. On modern GPUs, the difference is usually small.

### Texturing

**Unit:** Mtex/s (megatexels per second)

Renders textured fullscreen quads with large textures. Measures texel fetch throughput — the speed of texture sampling from GPU memory.

**Parameters:**
- `tex_size` — texture dimensions (Light: 512, Medium: 1024, Heavy: 2048, Ultra: 4096)
- `layers` — number of textured quads per frame (Light: 100, Medium: 300, Heavy: 500, Ultra: 800)

**Scoring formula:**
```
score = (viewport_w * viewport_h * layers) / avg_time_sec / 1e6
```

**What affects it:** Texture cache size, memory bandwidth, TMU (texture mapping unit) count.

---

## Geometry Category

### Geometry

**Unit:** Mtris/s (megatriangles per second)

Renders dense cube grids (NxNxN cubes, each 12 triangles). Measures triangle setup and rasterization throughput.

**Parameters:**
- `grid_size` — cubes per axis, total cubes = N^3 (Light: 10, Medium: 25, Heavy: 35, Ultra: 50)

**Scoring formula:**
```
triangles_per_frame = grid_size^3 * 12
score = triangles_per_frame / avg_time_sec / 1e6
```

**What affects it:** Primitive assembly, triangle setup rate, vertex cache efficiency.

### Vertex

**Unit:** Mverts/s (megavertices per second)

Renders a large vertex buffer with minimal fragment work (tiny triangles). Measures raw vertex processing speed — vertex shader execution and attribute fetch.

**Parameters:**
- `vertex_count` — number of vertices (Light: 100K, Medium: 500K, Heavy: 2M, Ultra: 8M)

**Scoring formula:**
```
score = vertex_count / avg_time_sec / 1e6
```

**What affects it:** Vertex shader units, vertex buffer bandwidth, attribute fetch speed.

### InstancedDraw

**Unit**: Mtri/s (megatriangles per second)

Draws many instances of a small sphere mesh using GL3+ hardware instancing (`glDrawElementsInstanced`). Each instance is positioned via `gl_InstanceID` in a grid pattern using a GLSL 1.40 vertex shader.

**Parameters**: `instance_count` — number of instances per frame.

**Scoring**: `(triangles_per_instance × instance_count) / avg_frame_time_sec / 1e6`

**Preset values**:
| Preset | instance_count |
|--------|---------------|
| Light  | 1000          |
| Medium | 5000          |
| Heavy  | 20000         |
| Ultra  | 50000         |

**Requires**: GL3+ with instancing support (Cap_Instancing). Disabled on GL2 renderer.

---

## Compute Category

### ShaderALU

**Unit:** Gops (giga-operations per second)

Fullscreen quad with a heavy fragment shader using transcendental functions (sin, cos, pow, sqrt) in a loop. Measures Special Function Unit (SFU) throughput.

**Parameters:**
- `iterations` — loop iterations in the fragment shader (Light: 50, Medium: 200, Heavy: 500, Ultra: 1500)

**Scoring formula:**
```
ops_per_pixel = 8 * iterations  (4 transcendental + 4 arithmetic per iteration)
score = (viewport_w * viewport_h * ops_per_pixel) / avg_time_sec / 1e9
```

**Note:** Transcendental functions (sin/cos/pow) execute on SFU units, which are much fewer than FMA units on modern GPUs. This test specifically measures SFU throughput.

### ShaderFMA

**Unit:** GFLOP/s (giga floating-point operations per second)

Fullscreen quad with a pure FMA (fused multiply-add) shader. Uses two interleaved accumulator chains to prevent compiler optimization: `acc0 = acc0 * acc1 + acc0`.

**Parameters:**
- `iterations` — loop iterations (Light: 50, Medium: 100, Heavy: 200, Ultra: 400)

**Scoring formula:**
```
flops_per_pixel = 24 * iterations  (3x vec4 mul + 3x vec4 add per iteration)
score = (viewport_w * viewport_h * flops_per_pixel) / avg_time_sec / 1e9
```

**Comparing ShaderALU vs ShaderFMA:** If ShaderFMA score >> ShaderALU score (3x or more), the GPU has significantly more FMA units than SFU units. This is typical for NVIDIA Turing+ and AMD RDNA+ architectures.

### ComputeFMA

**Unit:** GFLOP/s (giga floating-point operations per second)

Pure compute shader FMA benchmark. Uses a GLSL 4.30 compute shader with two interleaved vec4 FMA accumulators (`acc = acc * a + b`) writing results to a Shader Storage Buffer Object (SSBO). Local workgroup size is 64.

**Parameters:**
- `iterations` — FMA loop iterations per invocation
- `work_groups` — number of workgroups dispatched

**Scoring formula:**
```
score = (work_groups × 64 × iterations × 24) / avg_frame_time_sec / 1e9
```

Where 24 = FLOPs per iteration (2 vec4 accumulators × 4 components × 3 ops per FMA iteration = 24 FLOP).

**Preset values:**
| Preset | iterations | work_groups |
|--------|-----------|-------------|
| Light  | 50        | 256         |
| Medium | 100       | 1024        |
| Heavy  | 200       | 4096        |
| Ultra  | 400       | 16384       |

**Requires:** GL4.3+ with compute shader support (Cap_Compute). Disabled on GL2/GL3 renderers.

---

## Overhead Category

### DrawCall

**Unit:** Kcalls/s (thousands of draw calls per second)

Submits many draw calls per frame, each with a uniform matrix update (`setModel()`). Measures the combined cost of draw call submission + uniform state update.

**Parameters:**
- `mesh_count` — number of mesh objects (Light: 100, Medium: 500, Heavy: 2000, Ultra: 5000)
- `draws_per_frame` — draw calls per frame (Light: 100, Medium: 500, Heavy: 2000, Ultra: 5000)

**Scoring formula:**
```
score = draws_per_frame / avg_time_sec / 1e3
```

**What affects it:** Driver overhead, CPU single-thread performance, GL validation cost.

### DrawCallRaw

**Unit:** Kcalls/s (thousands of draw calls per second)

Identical to DrawCall but without per-draw uniform updates. The model matrix is set once before the loop. Measures pure draw call submission overhead.

**Comparing DrawCall vs DrawCallRaw:** The difference shows the cost of uniform updates. If DrawCallRaw is >1.5x faster, uniform updates are a significant bottleneck. On most modern GPUs/drivers, the difference is small (<10%).

### StateChange

**Unit:** Kcalls/s (thousands of state changes per second)

Alternates between different shaders, textures, and blend states between draw calls. Measures the cost of GPU pipeline state switching.

**Parameters:**
- `switches` — number of state switches per frame (Light: 50, Medium: 200, Heavy: 500, Ultra: 1000)
- `shader_count` — number of shaders to rotate through (Light: 4, Medium: 8, Heavy: 12, Ultra: 16)
- `tex_count` — number of textures to rotate through (Light: 4, Medium: 8, Heavy: 12, Ultra: 16)

**Scoring formula:**
```
score = switches / avg_time_sec / 1e3
```

**What affects it:** Driver state tracking, shader program linking, texture binding overhead.

### TexUpload

**Unit:** MB/s (megabytes per second)

Uploads textures from CPU memory to GPU memory using `glTexSubImage2D`. Measures the CPU-to-GPU data transfer path.

**Parameters:**
- `tex_size` — texture dimensions (Light: 256, Medium: 512, Heavy: 1024, Ultra: 2048)
- `uploads_per_frame` — uploads per frame (Light: 10, Medium: 20, Heavy: 30, Ultra: 50)

**Scoring formula:**
```
bytes_per_upload = tex_size * tex_size * 3
score = (bytes_per_upload * uploads_per_frame) / avg_time_sec / 1e6
```

**What affects it:** PCIe bandwidth, DMA engine, driver staging buffer implementation.

---

## Scene Test

### Scene

**Unit:** FPS (frames per second)

Combined rendering workload with multiple object types:
- Heightmap terrain with checkerboard texture
- 80+ textured spheres orbiting in concentric rings
- 6 cube grids with solid-color cubes
- 10 semi-transparent overlay spheres (blended)
- Directional lighting (Phong in GL3, Gouraud in GL2)

**Parameters:**
- `terrain_res` — terrain grid resolution (Light: 128, Ultra: 1024)
- `terrain_tex` / `obj_tex` — texture sizes
- `sphere_segs` / `sphere_rings` — sphere mesh detail
- `cube_grid` — cubes per axis per grid

This test is a combined workload that exercises all GPU subsystems. It does not participate in composite scoring because its bottleneck varies by hardware.

---

## Measurement Methodology

### Frame Timing

Each test frame is measured individually:
1. `glFinish()` before starting the frame timer (drain GPU pipeline)
2. Start CPU timer (nanosecond precision via `CLOCK_MONOTONIC` / `QueryPerformanceCounter`)
3. Execute `test->render()`
4. `glFinish()` after rendering (ensure GPU completes)
5. Record elapsed time

Optional GPU timer (`--timing gpu`): wraps the render call with `GL_TIME_ELAPSED` queries for GPU-side timing without CPU overhead.

### Warmup and Measurement

- **Warmup** frames are rendered but not measured (allows GPU to stabilize frequency, fill caches)
- **Measurement** continues until both conditions are met: minimum frame count AND minimum duration
- This ensures enough samples even on fast GPUs and enough time for slow ones

### Sanity Check

After the first warmup frame, the benchmark reads back a 4x4 pixel region from the center of the framebuffer. If all pixels are black (zero), the test is marked with `[SANITY FAIL]` — this catches broken shaders, FBO errors, or unsupported GL features.

### Statistical Output

For each test:
- **Avg** — arithmetic mean of all frame times
- **Min / Max** — best and worst frame times
- **P1 / P99** — 1st and 99th percentile (outlier-robust bounds)
- **Median** — 50th percentile
- **CV%** — coefficient of variation (stddev / avg * 100). Below 5% is excellent, above 50% marks the result as `[UNSTABLE]`
