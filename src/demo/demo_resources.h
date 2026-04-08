#pragma once
#include "renderer/renderer.h"
#include "renderer/scoped_handle.h"
#include "demo/shader_program.h"
#include "demo/shader_cache.h"
#include "demo/mesh_pool.h"
#include "demo/scoped_ssbo.h"
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
    // Infrastructure
    Renderer* renderer_;
    bool prepared_;
    ShaderCache shader_cache_;
    MeshPool scene_meshes_;

    // Core resources (T1+, always present)
    struct CoreRes {
        MeshHandle model_mesh, ground_mesh, rock_mesh, grass_mesh, particle_mesh;
        ScopedMesh sky_mesh;
        float model_bounding_radius = 0.0f;
        ScopedTexture fur_tex, fur_mask_tex;

        // Per-tier shader variants
        static const int MAX_TIERS = 4;
        ShaderProgram sky_shader;              // legacy fallback
        ShaderProgram* sky_shader_from_cache = nullptr;
        ShaderProgram* sky_cache[MAX_TIERS] = {};

        ShaderProgram island_shaders[MAX_TIERS];   // legacy (T4 only)
        ShaderProgram fur_shaders[MAX_TIERS];      // legacy (T4 only)
        ShaderProgram* island_cache[MAX_TIERS] = {};
        ShaderProgram* fur_cache[MAX_TIERS] = {};

        ShaderProgram particle_shader;         // legacy fallback
        ShaderProgram* particle_cache = nullptr;
        ShaderProgram* torch_cache = nullptr;
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
        ShaderProgram shader;
        ShaderProgram* cache = nullptr;
        ScopedRenderTarget rt;
        TextureHandle depth_tex;
        int map_size = 0;
    } shadow_;

    // Bloom post-processing (T2+)
    struct BloomRes {
        ShaderProgram extract_shader, blur_shader, composite_shader;
        ShaderProgram* extract_cache = nullptr;
        ShaderProgram* blur_cache = nullptr;
        ShaderProgram* composite_cache = nullptr;
        ScopedMesh fullscreen_quad;
        ScopedRenderTarget scene_rt, bright_rt, blur_rt;
        float strength = 0.0f;
    } bloom_;

    // Instanced grass (T2+)
    struct GrassRes {
        ShaderProgram shader, shader_t3;
        ShaderProgram* cache = nullptr;
        ShaderProgram* t3_cache = nullptr;
        MeshHandle blade_mesh;
    } grass_;

    // SSAO (T2+)
    struct SSAORes {
        ShaderProgram shader, blur_shader;
        ShaderProgram* cache = nullptr;
        ShaderProgram* blur_cache = nullptr;
        ScopedRenderTarget rt, blur_rt;
        ScopedTexture noise_tex;
        TextureHandle scene_depth_tex;
    } ssao_;

    // Normal map (T3+)
    ScopedTexture normal_map_tex_;

    // T4 resources
    struct T4Res {
        ShaderProgram grass_shader_t4;
        ShaderProgram tess_shader;
        ShaderProgram compute_particle_shader, particle_render_shader;
        ShaderProgram volumetric_fog_shader, tone_map_shader;
        ScopedSSBO particle_ssbo;
        int compute_particle_count = 0;

        // HDR
        ScopedRenderTarget hdr_scene_rt, hdr_bright_rt, fog_rt;
        TextureHandle hdr_depth_tex, hdr_color_tex;
        int fog_w = 0, fog_h = 0;

        // GTAO
        ShaderProgram gtao_shader, gtao_blur_shader;
        ScopedTexture gtao_tex, gtao_blur_tex;

        // Compute Bloom
        ShaderProgram bloom_down_compute, bloom_up_compute;
        static const int BLOOM_MIP_COUNT = 6;
        ScopedTexture bloom_mips[BLOOM_MIP_COUNT];

        // Auto-Exposure
        ShaderProgram histogram_shader, exposure_shader;
        ScopedSSBO histogram_ssbo, exposure_ssbo;

        // SSR
        ShaderProgram ssr_shader;
        ScopedTexture ssr_tex, ssr_color_snapshot, ssr_depth_snapshot;

        // DoF
        ShaderProgram dof_shader;
        ScopedTexture dof_tex;

        // Puddles
        static const int PUDDLE_COUNT = 3;
        MeshHandle puddle_meshes[PUDDLE_COUNT];
    } t4_;

    bool loadSharedMeshes(Renderer* r);
    bool loadSharedTextures(Renderer* r);
    bool compileSkyShader(Renderer* r);
    bool compileTierShaders(Renderer* r, int tier);
    int validateShaders(int max_tier);
    bool createShadowResources(Renderer* r, int shadow_size = 1024);
    bool createBloomResources(Renderer* r, int render_w, int render_h);
    bool createSSAOResources(Renderer* r, int render_w, int render_h);
    bool createT4Resources(Renderer* r, int render_w, int render_h);
};
