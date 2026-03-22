#pragma once
#include "renderer/renderer.h"
#include "demo/shader_program.h"
#include "geometry/mesh.h"

struct TierResourceView {
    ShaderProgram* sky_shader;
    ShaderProgram* island_shader;
    ShaderProgram* fur_shader;
    ShaderProgram* particle_shader;

    MeshHandle model_mesh;
    MeshHandle sky_mesh;
    MeshHandle ground_mesh;
    MeshHandle rock_mesh;
    MeshHandle grass_mesh;
    MeshHandle particle_mesh;

    TextureHandle fur_tex;
    TextureHandle fur_mask_tex;  // INVALID_TEXTURE if not loaded

    float model_bounding_radius;

    // T2+ shadow mapping
    ShaderProgram* shadow_shader;
    RenderTargetHandle shadow_rt;
    TextureHandle shadow_depth_tex;
    int shadow_map_size;

    // T2+ bloom post-processing
    ShaderProgram* bloom_extract_shader;
    ShaderProgram* bloom_blur_shader;
    ShaderProgram* bloom_composite_shader;
    MeshHandle fullscreen_quad;
    RenderTargetHandle scene_rt;
    RenderTargetHandle bright_rt;
    RenderTargetHandle blur_rt;
    float bloom_strength;

    // T2+ instanced grass
    ShaderProgram* grass_shader;
    MeshHandle grass_blade_mesh;

    // T2+ SSAO
    ShaderProgram* ssao_shader;
    ShaderProgram* ssao_blur_shader;
    RenderTargetHandle ssao_rt;
    RenderTargetHandle ssao_blur_rt;
    TextureHandle ssao_noise_tex;
    TextureHandle scene_depth_tex;

    // T3+ normal map
    TextureHandle normal_map_tex;

    // T4+ PBR / compute / tessellation / HDR
    ShaderProgram* tess_shader;
    ShaderProgram* compute_particle_shader;
    ShaderProgram* particle_render_shader;
    ShaderProgram* volumetric_fog_shader;
    ShaderProgram* tone_map_shader;
    BufferHandle particle_ssbo;
    int compute_particle_count;
    RenderTargetHandle hdr_scene_rt;
    RenderTargetHandle hdr_bright_rt;
    TextureHandle hdr_depth_tex;
    RenderTargetHandle fog_rt;

    // T4 Ultra: Compute GTAO
    ShaderProgram* gtao_shader;
    ShaderProgram* gtao_blur_shader;
    TextureHandle gtao_tex;
    TextureHandle gtao_blur_tex;

    // T4 Ultra: Compute Bloom
    ShaderProgram* bloom_down_compute;
    ShaderProgram* bloom_up_compute;
    static const int BLOOM_MIP_COUNT = 6;
    TextureHandle bloom_mips[BLOOM_MIP_COUNT];

    // T4 Ultra: Auto-Exposure
    ShaderProgram* histogram_shader;
    ShaderProgram* exposure_shader;
    BufferHandle histogram_ssbo;
    BufferHandle exposure_ssbo;

    // T4 Ultra: SSR
    ShaderProgram* ssr_shader;
    TextureHandle ssr_tex;

    // T4 Ultra: DoF
    ShaderProgram* dof_shader;
    TextureHandle dof_tex;

    // T4 Ultra: Puddles
    static const int PUDDLE_COUNT = 3;
    MeshHandle puddle_meshes[PUDDLE_COUNT];

    TierResourceView()
        : sky_shader(nullptr), island_shader(nullptr), fur_shader(nullptr)
        , particle_shader(nullptr)
        , model_mesh(), sky_mesh(), ground_mesh()
        , rock_mesh(), grass_mesh(), particle_mesh()
        , fur_tex(), fur_mask_tex()
        , model_bounding_radius(0.0f)
        , shadow_shader(nullptr), shadow_rt(), shadow_depth_tex()
        , shadow_map_size(0)
        , bloom_extract_shader(nullptr), bloom_blur_shader(nullptr)
        , bloom_composite_shader(nullptr)
        , fullscreen_quad(), scene_rt(), bright_rt(), blur_rt()
        , bloom_strength(0.0f)
        , grass_shader(nullptr), grass_blade_mesh()
        , ssao_shader(nullptr), ssao_blur_shader(nullptr)
        , ssao_rt(), ssao_blur_rt(), ssao_noise_tex(), scene_depth_tex()
        , normal_map_tex()
        , tess_shader(nullptr), compute_particle_shader(nullptr)
        , particle_render_shader(nullptr), volumetric_fog_shader(nullptr)
        , tone_map_shader(nullptr), particle_ssbo(), compute_particle_count(0)
        , hdr_scene_rt(), hdr_bright_rt(), hdr_depth_tex(), fog_rt()
        , gtao_shader(nullptr), gtao_blur_shader(nullptr)
        , gtao_tex(), gtao_blur_tex()
        , bloom_down_compute(nullptr), bloom_up_compute(nullptr)
        , histogram_shader(nullptr), exposure_shader(nullptr)
        , histogram_ssbo(), exposure_ssbo()
        , ssr_shader(nullptr), ssr_tex()
        , dof_shader(nullptr), dof_tex()
    {
        for (int i = 0; i < PUDDLE_COUNT; i++) puddle_meshes[i] = MeshHandle();
        for (int i = 0; i < BLOOM_MIP_COUNT; i++) bloom_mips[i] = TextureHandle();
    }
};
