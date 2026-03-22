#pragma once
#include "renderer/renderer.h"
#include "renderer/scoped_handle.h"
#include "demo/shader_program.h"
#include "demo/mesh_pool.h"
#include "demo/tier_resource_view.h"
#include "geometry/mesh.h"

enum class DemoTier;

class DemoResources {
public:
    DemoResources();
    ~DemoResources();

    DemoResources(const DemoResources&) = delete;
    DemoResources& operator=(const DemoResources&) = delete;
    DemoResources(DemoResources&&) = delete;
    DemoResources& operator=(DemoResources&&) = delete;

    bool prepare(Renderer* r, int max_tier, int render_w, int render_h);
    TierResourceView viewForTier(DemoTier tier);
    void destroy();

private:
    Renderer* renderer_;
    bool prepared_;

    // Shared meshes
    MeshPool scene_meshes_;
    MeshHandle model_mesh_;
    MeshHandle ground_mesh_;
    MeshHandle rock_mesh_;
    MeshHandle grass_mesh_;
    MeshHandle particle_mesh_;
    float model_bounding_radius_;
    ScopedMesh sky_mesh_;

    // Shared textures
    ScopedTexture fur_tex_;
    ScopedTexture fur_mask_tex_;

    // Sky shader (GL2.1, shared across all tiers)
    ShaderProgram sky_shader_;

    // Per-tier shaders (index 0..3 = tier 1..4)
    static const int MAX_TIERS = 4;
    ShaderProgram island_shaders_[MAX_TIERS];
    ShaderProgram fur_shaders_[MAX_TIERS];

    // Particle shader (shared, T1+)
    ShaderProgram particle_shader_;

    // T2+ shadow mapping
    ShaderProgram shadow_shader_;
    ScopedRenderTarget shadow_rt_;
    TextureHandle shadow_depth_tex_;
    int shadow_map_size_;

    // T2+ bloom post-processing
    ShaderProgram bloom_extract_shader_;
    ShaderProgram bloom_blur_shader_;
    ShaderProgram bloom_composite_shader_;
    ScopedMesh fullscreen_quad_;
    ScopedRenderTarget scene_rt_;
    ScopedRenderTarget bright_rt_;
    ScopedRenderTarget blur_rt_;
    float bloom_strength_;

    // T2+ instanced grass
    ShaderProgram grass_shader_;
    ShaderProgram grass_shader_t3_;
    MeshHandle grass_blade_mesh_;

    // T2+ SSAO
    ShaderProgram ssao_shader_;
    ShaderProgram ssao_blur_shader_;
    ScopedRenderTarget ssao_rt_;
    ScopedRenderTarget ssao_blur_rt_;
    ScopedTexture ssao_noise_tex_;
    TextureHandle scene_depth_tex_;

    // T3+ normal map texture
    ScopedTexture normal_map_tex_;

    bool loadSharedMeshes(Renderer* r);
    bool loadSharedTextures(Renderer* r);
    bool compileSkyShader(Renderer* r);
    bool compileTierShaders(Renderer* r, int tier);
    bool createShadowResources(Renderer* r, int shadow_size = 1024);
    bool createBloomResources(Renderer* r, int render_w, int render_h);
    bool createSSAOResources(Renderer* r, int render_w, int render_h);
};
