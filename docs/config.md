# GPU Benchmark — Custom Config

## INI File Format

Custom presets are stored as INI files. Load with `--config <path>`.

The config starts from the Medium preset as a base — you only need to specify the values you want to override.

### Full example

```ini
[preset]
name=Custom
warmup_frames=120
measure_frames=600
min_duration_sec=5.0

[fillrate]
layers=500

[geometry]
grid_size=25

[texturing]
tex_size=1024
layers=300

[scene]
terrain_res=256
terrain_tex=1024
obj_tex=1024
sphere_segs=64
sphere_rings=32
cube_grid=10

[drawcall]
mesh_count=500
draws_per_frame=500

[overdraw]
layers=200
alpha=0.30

[texupload]
tex_size=512
uploads_per_frame=20

[statechange]
switches=200
shader_count=8
tex_count=8

[vertex]
vertex_count=500000

[shader_alu]
iterations=200

[shader_fma]
iterations=100

[instanced_draw]
instance_count=5000

[compute_fma]
iterations=100
work_groups=1024
```

## Parameter Reference

### [preset] — General settings

| Key | Type | Description |
|-----|------|-------------|
| `name` | string | Preset display name |
| `warmup_frames` | int | Frames rendered before measurement starts |
| `measure_frames` | int | Minimum frames to measure |
| `min_duration_sec` | float | Minimum measurement duration in seconds |

### [fillrate]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `layers` | int | 500 | Number of fullscreen quads per frame |

### [geometry]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `grid_size` | int | 25 | Cubes per axis (total = N^3) |

**Note:** On GPUs without 32-bit index support, grid_size is limited to 13 (13^3 * 24 = 52728 vertices fits in 16-bit indices).

### [texturing]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `tex_size` | int | 1024 | Texture dimensions (must not exceed GPU max texture size) |
| `layers` | int | 300 | Number of textured quads per frame |

### [scene]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `terrain_res` | int | 256 | Terrain grid vertices per axis |
| `terrain_tex` | int | 1024 | Terrain texture size |
| `obj_tex` | int | 1024 | Object texture size |
| `sphere_segs` | int | 64 | Sphere longitudinal segments |
| `sphere_rings` | int | 32 | Sphere latitudinal rings |
| `cube_grid` | int | 10 | Cubes per axis per grid (6 grids total) |

### [drawcall]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `mesh_count` | int | 500 | Number of mesh objects to create |
| `draws_per_frame` | int | 500 | Draw calls per frame |

**Note:** DrawCallRaw test also uses these parameters.

### [overdraw]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `layers` | int | 200 | Number of blended quads per frame |
| `alpha` | float | 0.30 | Blend alpha value (0.0-1.0) |

### [texupload]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `tex_size` | int | 512 | Upload texture dimensions |
| `uploads_per_frame` | int | 20 | Texture uploads per frame |

### [statechange]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `switches` | int | 200 | State changes per frame |
| `shader_count` | int | 8 | Number of shaders to rotate |
| `tex_count` | int | 8 | Number of textures to rotate |

### [vertex]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `vertex_count` | int | 500000 | Number of vertices to process |

### [shader_alu]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `iterations` | int | 200 | Fragment shader loop iterations (sin/cos/pow/sqrt) |

### [shader_fma]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `iterations` | int | 100 | Fragment shader FMA loop iterations |

### [instanced_draw]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `instance_count` | int | 5000 | Number of instances per draw call |

### [compute_fma]

| Key | Type | Default (Medium) | Description |
|-----|------|-------------------|-------------|
| `iterations` | int | 100 | Compute shader FMA loop iterations |
| `work_groups` | int | 1024 | Number of work groups dispatched |

## Preset Comparison

| Parameter | Light | Medium | Heavy | Ultra |
|-----------|-------|--------|-------|-------|
| warmup_frames | 60 | 120 | 120 | 180 |
| measure_frames | 300 | 600 | 600 | 900 |
| min_duration_sec | 3.0 | 5.0 | 5.0 | 8.0 |
| fillrate.layers | 100 | 500 | 1500 | 4000 |
| geometry.grid_size | 10 | 25 | 35 | 50 |
| texturing.tex_size | 512 | 1024 | 2048 | 4096 |
| texturing.layers | 100 | 300 | 500 | 800 |
| scene.terrain_res | 128 | 256 | 512 | 1024 |
| drawcall.draws | 100 | 500 | 2000 | 5000 |
| overdraw.layers | 50 | 200 | 500 | 1000 |
| texupload.tex_size | 256 | 512 | 1024 | 2048 |
| statechange.switches | 50 | 200 | 500 | 1000 |
| vertex.count | 100K | 500K | 2M | 8M |
| shader_alu.iterations | 50 | 200 | 500 | 1500 |
| shader_fma.iterations | 50 | 100 | 200 | 400 |
| instanced_draw.instance_count | 1000 | 5000 | 20000 | 50000 |
| compute_fma.iterations | 50 | 100 | 200 | 400 |
| compute_fma.work_groups | 256 | 1024 | 4096 | 16384 |

## Saving Configs

In the GUI, click the "Custom Parameters" panel, modify values, and use Export to save. Alternatively, copy any example above and edit the values.

## Validation

When loading a config, the benchmark validates it against GPU capabilities:
- Texture sizes must not exceed `GL_MAX_TEXTURE_SIZE`
- Geometry grid must fit in index buffer limits (16-bit on old GPUs)
- If validation fails, an error message is shown and the test is skipped
