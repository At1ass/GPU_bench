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
    TextureHandle scene_depth_tex;  // depth from scene FBO

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
        , ssao_rt(), ssao_blur_rt(), ssao_noise_tex(), scene_depth_tex() {}
};
