#pragma once
#include "renderer/renderer.h"
#include "renderer/scoped_handle.h"
#include "demo/tier/shader_bank.h"
#include "engine/mesh_pool.h"
#include "demo/tier/tier_resource_view.h"
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
    // Infrastructure
    Renderer* renderer_;
    bool prepared_;
    ShaderBank shaders_;
    MeshPool scene_meshes_;

    // Core resources (T1+, always present)
    struct CoreRes {
        MeshHandle model_mesh, ground_mesh, rock_mesh, grass_mesh, particle_mesh;
        MeshHandle fullscreen_quad;  // always available, any tier
        ScopedMesh sky_mesh;
        float model_bounding_radius = 0.0f;
        ScopedTexture fur_tex, fur_mask_tex;
    } core_;

    // Sanctuary scene meshes
    struct SanctuaryMeshes {
        MeshHandle pedestal, column_tall, column_stump, arch;
        MeshHandle fallen_column, slab, stone_sphere, mossy_block;
        MeshHandle bowl, obelisk, ring_inner, ring_outer;
        MeshHandle pond, torch;
        MeshHandle trees[3];
    } sanctuary_;

    // Shadow mapping (T2+)
    struct ShadowRes {
        ScopedRenderTarget rt;
        TextureHandle depth_tex;
        int map_size = 0;
    } shadow_;

    // Scene FBO (any tier with post-processing support)
    ScopedRenderTarget scene_rt_;
    TextureHandle scene_depth_tex_;

    // Bloom post-processing (T2+)
    struct BloomRes {
        ScopedRenderTarget bright_rt, blur_rt;
        float strength = 0.0f;
    } bloom_;

    // Instanced grass (T2+)
    struct GrassRes {
        MeshHandle blade_mesh;
    } grass_;

    // SSAO (T2+)
    struct SSAORes {
        ScopedRenderTarget rt, blur_rt;
        ScopedTexture noise_tex;
        TextureHandle scene_depth_tex;
    } ssao_;

    // Normal map (T3+)
    ScopedTexture normal_map_tex_;

    // T4 resources
    struct T4Res {
        ScopedBuffer particle_ssbo;
        int compute_particle_count = 0;

        // HDR
        ScopedRenderTarget hdr_scene_rt, hdr_bright_rt, fog_rt;
        TextureHandle hdr_depth_tex, hdr_color_tex;
        int fog_w = 0, fog_h = 0;

        // GTAO
        ScopedTexture gtao_tex, gtao_blur_tex;

        // Compute Bloom
        static const int BLOOM_MIP_COUNT = 6;
        ScopedTexture bloom_mips[BLOOM_MIP_COUNT];

        // Auto-Exposure
        ScopedBuffer histogram_ssbo, exposure_ssbo;

        // SSR
        ScopedTexture ssr_tex, ssr_color_snapshot, ssr_depth_snapshot;

        // DoF
        ScopedTexture dof_tex;

        // Puddles
        static const int PUDDLE_COUNT = 3;
        MeshHandle puddle_meshes[PUDDLE_COUNT];
    } t4_;

    bool loadSharedMeshes(Renderer* r);
    bool loadSharedTextures(Renderer* r);
    bool createSceneRT(Renderer* r, int render_w, int render_h);
    bool createShadowResources(Renderer* r, int shadow_size = 1024);
    bool createBloomResources(Renderer* r, int render_w, int render_h);
    bool createSSAOResources(Renderer* r, int render_w, int render_h);
    bool createT4Resources(Renderer* r, int render_w, int render_h);

    // Normal map generation (T3+)
    void generateNormalMapTexture(Renderer* r);
};
