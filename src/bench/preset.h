#pragma once
#include "renderer/renderer.h"
#include <cstdint>
#include <string>
#include <type_traits>

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
struct FBOFillrateParams        { int rt_size; int layers; };
struct MRTFillParams            { int rt_size; int num_targets; int layers; };
struct TexArraySampleParams     { int tex_size; int array_layers; int sample_layers; };
struct GeometryShaderParams     { int input_points; int tris_per_point; };
struct UBOSwitchParams          { int ubo_count; int switches_per_frame; };
struct TransformFeedbackParams  { int vertex_count; };
struct ComputeBandwidthParams   { int buffer_size_mb; int work_groups; };
struct IndirectDrawParams       { int command_count; int mesh_tris; };
struct ComputeSharedMemParams   { int iterations; int work_groups; };
struct SSBOAtomicsParams        { int iterations; int work_groups; };
struct TessellationTPParams     { int patch_subdivisions; int patches; };
struct TextureGatherParams      { int tex_size; int iterations; };
struct ImageLoadStoreParams     { int image_size; int iterations; };
struct ComputePrefixParams      { int element_count; int work_groups; };
struct PersistentMapParams      { int buffer_size_mb; int frames_in_flight; };
struct BindlessTexParams        { int tex_count; int draws_per_frame; };

#define ASSERT_POD(T) static_assert(std::is_pod<T>::value, #T " must be POD")
ASSERT_POD(FillrateParams);
ASSERT_POD(GeometryParams);
ASSERT_POD(TexturingParams);
ASSERT_POD(SceneParams);
ASSERT_POD(DrawCallParams);
ASSERT_POD(OverdrawParams);
ASSERT_POD(TexUploadParams);
ASSERT_POD(StateChangeParams);
ASSERT_POD(VertexParams);
ASSERT_POD(ShaderALUParams);
ASSERT_POD(ShaderFMAParams);
ASSERT_POD(InstancedDrawParams);
ASSERT_POD(ComputeFMAParams);
ASSERT_POD(FBOFillrateParams);
ASSERT_POD(MRTFillParams);
ASSERT_POD(TexArraySampleParams);
ASSERT_POD(GeometryShaderParams);
ASSERT_POD(UBOSwitchParams);
ASSERT_POD(TransformFeedbackParams);
ASSERT_POD(ComputeBandwidthParams);
ASSERT_POD(IndirectDrawParams);
ASSERT_POD(ComputeSharedMemParams);
ASSERT_POD(SSBOAtomicsParams);
ASSERT_POD(TessellationTPParams);
ASSERT_POD(TextureGatherParams);
ASSERT_POD(ImageLoadStoreParams);
ASSERT_POD(ComputePrefixParams);
ASSERT_POD(PersistentMapParams);
ASSERT_POD(BindlessTexParams);
#undef ASSERT_POD

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
    FBOFillrateParams       fbo_fillrate;
    MRTFillParams           mrt_fill;
    TexArraySampleParams    texarray_sample;
    GeometryShaderParams    geom_shader;
    UBOSwitchParams         ubo_switch;
    TransformFeedbackParams transform_feedback;
    ComputeBandwidthParams  compute_bw;
    IndirectDrawParams      indirect_draw;
    ComputeSharedMemParams  compute_smem;
    SSBOAtomicsParams       ssbo_atomics;
    TessellationTPParams    tess_tp;
    TextureGatherParams     tex_gather;
    ImageLoadStoreParams    image_ls;
    ComputePrefixParams     compute_prefix;
    PersistentMapParams     persistent_map;
    BindlessTexParams       bindless_tex;
};

enum class PresetIndex {
    Light   = 0,
    Medium  = 1,
    Heavy   = 2,
    Ultra   = 3,
    Extreme = 4,
    Count   = 5
};

// Get one of the 4 fixed presets
const BenchPreset& getPreset(int index);

// Get default "Custom" preset (copy of Medium)
BenchPreset getCustomPreset();

struct PresetValidation {
    bool ok;
    std::string reason;

    PresetValidation() = default;
    PresetValidation(const PresetValidation&) = default;
    PresetValidation& operator=(const PresetValidation&) = default;
    PresetValidation(PresetValidation&&) noexcept = default;
    PresetValidation& operator=(PresetValidation&&) noexcept = default;
};

// Validate preset against hardware caps. Returns error if incompatible.
PresetValidation validatePreset(const BenchPreset& p, const RenderCaps& caps);

// Suggest a preset index based on detected VRAM and capabilities
int suggestPresetIndex(const RenderCaps& caps, uint32_t available_caps);
