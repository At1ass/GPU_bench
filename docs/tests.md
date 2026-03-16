# GPU Benchmark — Tests

## Overview

The benchmark includes 30 tests organized into 4 categories for composite scoring:

| Category | Tests | What it measures |
|----------|-------|------------------|
| **Fill** | Fillrate, Overdraw, Texturing, FBOFillrate, MRTFill, TexArraySample, TextureGather | Pixel output, blending, texel fetch, render-to-texture |
| **Geometry** | Geometry, Vertex, InstancedDraw, GeomShader, TransformFeedback, TessellationTP | Triangle throughput, vertex processing, instancing, amplification |
| **Compute** | ShaderALU, ShaderFMA, ComputeFMA, ComputeBW, ComputeSMem, SSBOAtomics, ComputePrefix, ImageLoadStore | Shader math, memory bandwidth, atomics, shared memory |
| **Overhead** | DrawCall, DrawCallRaw, StateChange, TexUpload, UBOSwitch, IndirectDraw, PersistentMap, BindlessTex | CPU/driver overhead, buffer management |

The **Scene** test is a combined workload and does not belong to any single category.

---

## Fill Category

### Fillrate

**Unit:** MPix/s (megapixels per second)

Renders multiple overlapping opaque fullscreen quads without blending. Measures raw pixel output rate — the speed at which the GPU can write pixels to the framebuffer.

**Parameters:**
- `layers` — number of fullscreen quads per frame (Light: 100, Medium: 500, Heavy: 1500, Ultra: 4000)

**Scoring formula:**
```
score = (viewport_w * viewport_h * layers) / avg_time_sec / 1e6
```

**What affects it:** ROP count, memory bandwidth, framebuffer format.

### Overdraw

**Unit:** MPix/s (megapixels per second)

Similar to Fillrate but with alpha blending enabled (alpha = 0.3). Measures blended pixel throughput — how fast the GPU handles read-modify-write operations on the framebuffer.

**Parameters:**
- `layers` — number of blended fullscreen quads (Light: 50, Medium: 200, Heavy: 500, Ultra: 1000)
- `alpha` — blend alpha value (default: 0.3)

**Scoring formula:** Same as Fillrate.

**Comparing with Fillrate:** If Overdraw score is significantly lower than Fillrate, the GPU's blending hardware (ROPs) is the bottleneck. On modern GPUs, the difference is usually small.

### Texturing

**Unit:** MTex/s (megatexels per second)

Renders textured fullscreen quads with large textures. Measures texel fetch throughput — the speed of texture sampling from GPU memory.

**Parameters:**
- `tex_size` — texture dimensions (Light: 512, Medium: 1024, Heavy: 2048, Ultra: 4096)
- `layers` — number of textured quads per frame (Light: 100, Medium: 300, Heavy: 500, Ultra: 800)

**Scoring formula:**
```
score = (viewport_w * viewport_h * layers) / avg_time_sec / 1e6
```

**What affects it:** Texture cache size, memory bandwidth, TMU (texture mapping unit) count.

### FBOFillrate (GL3+)

**Unit:** MPix/s (megapixels per second)

Renders opaque quads into a framebuffer object (render-to-texture) instead of the default framebuffer. Measures off-screen rendering throughput and FBO overhead.

**Parameters:**
- `rt_size` — render target dimensions (Light: 512, Medium: 1024, Heavy: 2048, Ultra: 4096)
- `layers` — number of fullscreen quads (Light: 100, Medium: 300, Heavy: 500, Ultra: 800)

**Scoring formula:**
```
score = (rt_size^2 * layers) / avg_time_sec / 1e6
```

**Comparing with Fillrate:** The difference shows FBO overhead. On modern drivers it's negligible; on older hardware render target switching can be expensive.

**Requires:** Cap_GL3 | Cap_FBO.

### MRTFill (GL3+)

**Unit:** MPix/s (megapixels per second)

Renders to multiple color attachments simultaneously (2-4 render targets). Measures MRT fill throughput — critical for deferred rendering G-buffer performance.

**Parameters:**
- `rt_size` — render target dimensions (Light: 512, Medium: 1024, Heavy: 2048, Ultra: 4096)
- `num_targets` — number of color attachments (Light: 2, Medium: 3, Heavy: 4, Ultra: 4)
- `layers` — quads per frame (Light: 50, Medium: 100, Heavy: 200, Ultra: 400)

**Scoring formula:**
```
score = (rt_size^2 * num_targets * layers) / avg_time_sec / 1e6
```

**What affects it:** ROP bandwidth with parallel writes, color cache efficiency, deferred rendering pipeline performance.

**Requires:** Cap_GL3 | Cap_FBO.

### TexArraySample (GL3+)

**Unit:** MTex/s (megatexels per second)

Samples from a 2D texture array (`sampler2DArray`) cycling through layers. Measures texture array fetch throughput and layer indexing impact on cache locality.

**Parameters:**
- `tex_size` — texture dimensions per layer (Light: 256, Medium: 512, Heavy: 1024, Ultra: 2048)
- `array_layers` — number of layers in the array (Light: 8, Medium: 16, Heavy: 32, Ultra: 64)
- `sample_layers` — sampling passes per frame (Light: 50, Medium: 100, Heavy: 200, Ultra: 400)

**Scoring formula:**
```
score = (viewport_w * viewport_h * sample_layers) / avg_time_sec / 1e6
```

**What affects it:** Texture cache efficiency with layer indexing, TMU throughput, memory bandwidth.

**Requires:** Cap_GL3.

### TextureGather (GL4.0+)

**Unit:** MTex/s (megatexels per second)

Uses `textureGather()` to fetch 4 texels in a single operation (2x2 block). Measures specialized hardware gather unit performance.

**Parameters:**
- `tex_size` — texture dimensions (Light: 256, Medium: 512, Heavy: 1024, Ultra: 2048)
- `iterations` — gather passes per frame (Light: 50, Medium: 100, Heavy: 200, Ultra: 400)

**Scoring formula:**
```
score = (viewport_w * viewport_h * iterations * 4) / avg_time_sec / 1e6
```

**What affects it:** Texture gather unit efficiency, cache performance for 2x2 pixel blocks.

**Requires:** Cap_GL4.

---

## Geometry Category

### Geometry

**Unit:** Mtri/s (megatriangles per second)

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

### InstancedDraw (GL3+)

**Unit:** Mtri/s (megatriangles per second)

Draws many instances of a small sphere mesh using GL3+ hardware instancing (`glDrawElementsInstanced`). Each instance is positioned via `gl_InstanceID` in a grid pattern.

**Parameters:**
- `instance_count` — number of instances per frame (Light: 1000, Medium: 5000, Heavy: 20000, Ultra: 50000)

**Scoring formula:**
```
score = (triangles_per_instance * instance_count) / avg_time_sec / 1e6
```

**What affects it:** Instancing hardware efficiency, vertex shader throughput, per-instance attribute fetch.

**Requires:** Cap_Instancing.

### GeomShader (GL3.2+)

**Unit:** Mtri/s (megatriangles per second)

Feeds point primitives through a geometry shader that amplifies each point into multiple triangles (triangle strip output). Measures geometry shader pipeline capacity.

**Parameters:**
- `input_points` — number of input points (Light: 10K, Medium: 50K, Heavy: 200K, Ultra: 500K)
- `tris_per_point` — triangles emitted per input point (Light: 4, Medium: 6, Heavy: 8, Ultra: 12)

**Scoring formula:**
```
score = (input_points * tris_per_point) / avg_time_sec / 1e6
```

**What affects it:** Geometry shader execution throughput, vertex amplification rate, stream-out bandwidth.

**Requires:** Cap_GeometryShader.

### TransformFeedback (GL3+)

**Unit:** Mverts/s (megavertices per second)

Processes vertices through a vertex shader with rasterization disabled, capturing transformed outputs to a feedback buffer (24 bytes/vertex: 2x vec3). Runs 10 passes per frame.

**Parameters:**
- `vertex_count` — number of vertices (Light: 100K, Medium: 500K, Heavy: 2M, Ultra: 8M)

**Scoring formula:**
```
score = (vertex_count * 10) / avg_time_sec / 1e6
```

**What affects it:** Vertex processing throughput, TF write bandwidth, GPU streaming write performance.

**Requires:** Cap_GL3.

### TessellationTP (GL4.0+)

**Unit:** Mtri/s (megatriangles per second)

Subdivides triangle patches on the GPU using tessellation control and evaluation shaders. Measures fixed-function tessellation unit throughput. Runs 10 passes per frame.

**Parameters:**
- `patch_subdivisions` — tessellation level (Light: 4, Medium: 8, Heavy: 16, Ultra: 32)
- `patches` — number of input patches (Light: 1000, Medium: 5000, Heavy: 20000, Ultra: 50000)

**Scoring formula:**
```
score = (patches * tess_level^2 * 10) / avg_time_sec / 1e6
```

**What affects it:** Tessellation unit performance, patch processing rate, vertex generation throughput.

**Requires:** Cap_Tessellation.

---

## Compute Category

### ShaderALU

**Unit:** GFLOP/s (giga floating-point operations per second)

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

### ComputeFMA (GL4.3+)

**Unit:** GFLOP/s (giga floating-point operations per second)

Pure compute shader FMA benchmark. Uses a GLSL 4.30 compute shader with two interleaved vec4 FMA accumulators (`acc = acc * a + b`) writing results to an SSBO. Local workgroup size is 64.

**Parameters:**
- `iterations` — FMA loop iterations per invocation (Light: 50, Medium: 100, Heavy: 200, Ultra: 400)
- `work_groups` — number of workgroups dispatched (Light: 256, Medium: 1024, Heavy: 4096, Ultra: 16384)

**Scoring formula:**
```
score = (work_groups * 64 * iterations * 24) / avg_time_sec / 1e9
```

**Comparing with ShaderFMA:** Measures the same FMA throughput but via compute shaders instead of fragment shaders. Difference reveals compute pipeline vs graphics pipeline efficiency.

**Requires:** Cap_Compute.

### ComputeBW (GL4.3+)

**Unit:** GB/s (gigabytes per second)

Compute shader memory bandwidth test. Each invocation reads one vec4 from an input SSBO and writes one vec4 to an output SSBO with minimal ALU. Local workgroup size is 256.

**Parameters:**
- `buffer_size_mb` — buffer size in megabytes (Light: 4, Medium: 16, Heavy: 64, Ultra: 256)
- `work_groups` — number of workgroups (Light: 256, Medium: 1024, Heavy: 4096, Ultra: 16384)

**Scoring formula:**
```
score = (2 * buffer_size_mb * 1024 * 1024) / avg_time_sec / 1e9
```

Factor of 2 accounts for read + write.

**What affects it:** Global memory bandwidth, memory controller performance, SSBO cache efficiency.

**Requires:** Cap_Compute.

### ComputeSMem (GL4.3+)

**Unit:** GB/s (gigabytes per second)

Measures shared memory (LDS) bandwidth in compute shaders. Workgroups fill shared memory and perform read-modify-write passes in tight loops. Local workgroup size is 256, shared array is 256 floats.

**Parameters:**
- `iterations` — R/W loop iterations (Light: 100, Medium: 500, Heavy: 2000, Ultra: 5000)
- `work_groups` — number of workgroups (Light: 256, Medium: 1024, Heavy: 4096, Ultra: 16384)

**Scoring formula:**
```
score = (8 * iterations * work_groups * 256) / avg_time_sec / 1e9
```

Factor of 8 = read (4 bytes) + write (4 bytes) per iteration per invocation.

**What affects it:** On-chip shared memory bandwidth, LDS throughput, workgroup synchronization efficiency.

**Requires:** Cap_Compute.

### SSBOAtomics (GL4.3+)

**Unit:** GOps/s (giga-operations per second)

Measures SSBO atomic operation throughput. Each invocation performs `atomicAdd`, `atomicMin`, `atomicMax` on contended counters (3 atomic ops per iteration). Local workgroup size is 64.

**Parameters:**
- `iterations` — atomic ops per invocation (Light: 50, Medium: 200, Heavy: 500, Ultra: 1500)
- `work_groups` — number of workgroups (Light: 256, Medium: 1024, Heavy: 4096, Ultra: 16384)

**Scoring formula:**
```
score = (work_groups * 64 * iterations * 3) / avg_time_sec / 1e9
```

**What affects it:** Atomic operation throughput, memory contention handling, GPU cache coherency.

**Requires:** Cap_Compute.

### ComputePrefix (GL4.3+)

**Unit:** GB/s (gigabytes per second)

Parallel prefix sum (Hillis-Steele scan algorithm) using shared memory with barriers. Tests combined compute shader + shared memory + synchronization throughput. Local workgroup size is 256.

**Parameters:**
- `element_count` — number of elements to scan (Light: 64K, Medium: 256K, Heavy: 1M, Ultra: 4M)
- `work_groups` — number of workgroups (Light: 256, Medium: 1024, Heavy: 4096, Ultra: 16384)

**Scoring formula:**
```
score = (2 * element_count * 4) / avg_time_sec / 1e9
```

Factor of 2 for read + write, 4 bytes per uint element.

**What affects it:** Shared memory bandwidth, barrier synchronization performance, compute throughput with data dependencies.

**Requires:** Cap_Compute.

### ImageLoadStore (GL4.2+)

**Unit:** GB/s (gigabytes per second)

Compute shader that reads from an `image2D` via `imageLoad()` and writes results to an SSBO. Measures image unit bandwidth compared to pure SSBO access. Local workgroup size is 16x16, format is RGBA32F (16 bytes/texel).

**Parameters:**
- `image_size` — image dimensions (Light: 256, Medium: 512, Heavy: 1024, Ultra: 2048)
- `iterations` — load passes per dispatch (Light: 50, Medium: 100, Heavy: 200, Ultra: 400)

**Scoring formula:**
```
score = (image_size^2 * (iterations * 16 + 16)) / avg_time_sec / 1e9
```

Each iteration reads 16 bytes (imageLoad), final pass writes 16 bytes (SSBO).

**What affects it:** Image unit bandwidth, imageLoad performance vs structured buffer access.

**Requires:** Cap_ImageLoadStore | Cap_Compute.

---

## Overhead Category

### DrawCall

**Unit:** calls/s (draw calls per second)

Submits many draw calls per frame, each with a uniform matrix update (`setModel()`). Measures the combined cost of draw call submission + uniform state update.

**Parameters:**
- `mesh_count` — number of mesh objects (Light: 100, Medium: 500, Heavy: 2000, Ultra: 5000)
- `draws_per_frame` — draw calls per frame (Light: 100, Medium: 500, Heavy: 2000, Ultra: 5000)

**Scoring formula:**
```
score = draws_per_frame / avg_time_sec
```

**What affects it:** Driver overhead, CPU single-thread performance, GL validation cost.

### DrawCallRaw

**Unit:** calls/s (draw calls per second)

Identical to DrawCall but without per-draw uniform updates. The model matrix is set once before the loop. Measures pure draw call submission overhead.

**Comparing DrawCall vs DrawCallRaw:** The difference shows the cost of uniform updates. If DrawCallRaw is >1.5x faster, uniform updates are a significant bottleneck. On most modern GPUs/drivers, the difference is small (<10%).

### StateChange

**Unit:** ops/s (state changes per second)

Alternates between different shaders, textures, and blend states between draw calls. Measures the cost of GPU pipeline state switching.

**Parameters:**
- `switches` — number of state switches per frame (Light: 50, Medium: 200, Heavy: 500, Ultra: 1000)
- `shader_count` — number of shaders to rotate through (Light: 4, Medium: 8, Heavy: 12, Ultra: 16)
- `tex_count` — number of textures to rotate through (Light: 4, Medium: 8, Heavy: 12, Ultra: 16)

**Scoring formula:**
```
score = switches / avg_time_sec
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

### UBOSwitch (GL3+)

**Unit:** ops/s (operations per second)

Rapidly rebinds different Uniform Buffer Objects between draw calls with color writes disabled (to minimize fill cost). Measures UBO binding switch overhead.

**Parameters:**
- `ubo_count` — number of UBO objects (Light: 8, Medium: 16, Heavy: 32, Ultra: 64)
- `switches_per_frame` — rebinds per frame (Light: 100, Medium: 500, Heavy: 2000, Ultra: 5000)

**Scoring formula:**
```
score = switches_per_frame / avg_time_sec
```

**What affects it:** Driver constant buffer rebind efficiency, command buffer submit overhead, GPU pipeline synchronization.

**Requires:** Cap_GL3.

### IndirectDraw (GL4.3+)

**Unit:** calls/s (draw calls per second)

Uses `glMultiDrawElementsIndirect` to submit draw commands from a GPU buffer. The GPU reads draw parameters directly from memory rather than receiving them from the CPU. Runs 20 passes per frame.

**Parameters:**
- `command_count` — number of indirect draw commands (Light: 100, Medium: 1000, Heavy: 5000, Ultra: 20000)
- `mesh_tris` — triangles per draw (Light: 12, Medium: 12, Heavy: 36, Ultra: 36)

**Scoring formula:**
```
score = (command_count * 20) / avg_time_sec
```

**What affects it:** Indirect draw dispatch overhead, GPU-side command parsing, hardware command processor performance.

**Requires:** Cap_GL4.

### PersistentMap (GL4.4+)

**Unit:** GB/s (gigabytes per second)

Streams data from CPU to GPU via persistently mapped buffers (`MAP_PERSISTENT_BIT | MAP_COHERENT_BIT`) with GPU fence synchronization. Runs 100 passes per frame.

**Parameters:**
- `buffer_size_mb` — buffer size in megabytes (Light: 4, Medium: 16, Heavy: 64, Ultra: 256)
- `frames_in_flight` — number of buffer regions for multi-buffering (Light: 2, Medium: 3, Heavy: 3, Ultra: 3)

**Scoring formula:**
```
score = (buffer_size_mb * 1024 * 1024 * 100) / avg_time_sec / 1e9
```

**What affects it:** CPU-GPU synchronization latency, persistent mapping overhead, PCIe/interconnect bandwidth.

**Requires:** Cap_BufferStorage.

### BindlessTex (ARB extension)

**Unit:** ops/s (operations per second)

Renders with textures accessed via bindless handles (`ARB_bindless_texture`) instead of `glBindTexture`. Each draw uses a different 64x64 RGBA texture accessed through a resident handle.

**Parameters:**
- `tex_count` — number of textures (Light: 16, Medium: 64, Heavy: 256, Ultra: 512)
- `draws_per_frame` — draw calls per frame (Light: 100, Medium: 500, Heavy: 2000, Ultra: 5000)

**Scoring formula:**
```
score = draws_per_frame / avg_time_sec
```

**Comparing with DrawCall:** The difference shows the benefit of eliminating texture binding overhead. On drivers with efficient bindless support, this can be significantly faster.

**Requires:** Cap_BindlessTexture.

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
