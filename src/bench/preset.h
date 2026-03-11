#pragma once
#include "renderer/renderer.h"
#include <string>

struct FillrateParams    { int layers; };
struct GeometryParams    { int grid_size; };
struct TexturingParams   { int tex_size; int layers; };
struct SceneParams       { int terrain_res; int terrain_tex; int obj_tex;
                           int sphere_segs; int sphere_rings; int cube_grid; };
struct DrawCallParams    { int mesh_count; int draws_per_frame; };
struct OverdrawParams    { int layers; float alpha; };
struct TexUploadParams   { int tex_size; int uploads_per_frame; };
struct StateChangeParams { int switches; int shader_count; int tex_count; };
struct VertexParams      { int vertex_count; };
struct ShaderALUParams   { int iterations; };
struct ShaderFMAParams   { int iterations; };
struct InstancedDrawParams { int instance_count; };
struct ComputeFMAParams  { int iterations; int work_groups; };

struct BenchPreset {
    const char* name;
    int warmup_frames;
    int measure_frames;
    double min_duration_sec;

    FillrateParams    fillrate;
    GeometryParams    geometry;
    TexturingParams   texturing;
    SceneParams       scene;
    DrawCallParams    drawcall;
    OverdrawParams    overdraw;
    TexUploadParams   texupload;
    StateChangeParams statechange;
    VertexParams      vertex;
    ShaderALUParams   shader_alu;
    ShaderFMAParams      shader_fma;
    InstancedDrawParams  instanced_draw;
    ComputeFMAParams     compute_fma;
};

enum class PresetIndex {
    Light  = 0,
    Medium = 1,
    Heavy  = 2,
    Ultra  = 3,
    Count  = 4
};

// Get one of the 4 fixed presets
const BenchPreset& getPreset(int index);

// Get default "Custom" preset (copy of Medium)
BenchPreset getCustomPreset();

struct PresetValidation {
    bool ok;
    std::string reason;
};

// Validate preset against hardware caps. Returns error if incompatible.
PresetValidation validatePreset(const BenchPreset& p, const RenderCaps& caps);

// Suggest a preset index based on detected VRAM
int suggestPresetIndex(const RenderCaps& caps);
