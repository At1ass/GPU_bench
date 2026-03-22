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

    // T4+ PBR / compute / tessellation / HDR
    ShaderProgram grass_shader_t4_;
    ShaderProgram tess_shader_;
    ShaderProgram compute_particle_shader_;
    ShaderProgram particle_render_shader_;
    ShaderProgram volumetric_fog_shader_;
    ShaderProgram tone_map_shader_;
    BufferHandle particle_ssbo_;
    int compute_particle_count_;
    ScopedRenderTarget hdr_scene_rt_;
    ScopedRenderTarget hdr_bright_rt_;
    TextureHandle hdr_depth_tex_;
    ScopedRenderTarget fog_rt_;

    // T4 Ultra: Compute GTAO
    ShaderProgram gtao_shader_;
    ShaderProgram gtao_blur_shader_;
    ScopedTexture gtao_tex_;      // full-res float AO output
    ScopedTexture gtao_blur_tex_; // full-res float blurred AO

    // T4 Ultra: Compute Bloom (mip chain)
    ShaderProgram bloom_down_compute_;
    ShaderProgram bloom_up_compute_;
    static const int BLOOM_MIP_COUNT = 6;
    ScopedTexture bloom_mips_[BLOOM_MIP_COUNT]; // progressively smaller float textures

    // T4 Ultra: Auto-Exposure
    ShaderProgram histogram_shader_;
    ShaderProgram exposure_shader_;
    BufferHandle histogram_ssbo_;  // 256 uint bins
    BufferHandle exposure_ssbo_;   // 1 float

    // T4 Ultra: SSR
    ShaderProgram ssr_shader_;
    ScopedTexture ssr_tex_;  // float texture for SSR result

    // T4 Ultra: DoF
    ShaderProgram dof_shader_;
    ScopedTexture dof_tex_;  // float texture for DoF result

    // T4 Ultra: Puddle meshes (3 unique shapes)
    static const int PUDDLE_COUNT = 3;
    MeshHandle puddle_meshes_[PUDDLE_COUNT];

    bool loadSharedMeshes(Renderer* r);
    bool loadSharedTextures(Renderer* r);
    bool compileSkyShader(Renderer* r);
    bool compileTierShaders(Renderer* r, int tier);
    bool createShadowResources(Renderer* r, int shadow_size = 1024);
    bool createBloomResources(Renderer* r, int render_w, int render_h);
    bool createSSAOResources(Renderer* r, int render_w, int render_h);
    bool createT4Resources(Renderer* r, int render_w, int render_h);
};
