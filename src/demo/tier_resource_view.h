#pragma once
#include "renderer/renderer.h"
#include "demo/shader_program.h"
#include "geometry/mesh.h"

struct TierResourceView {
    // Core (always present)
    struct Core {
        ShaderProgram* sky_shader;
        ShaderProgram* island_shader;
        ShaderProgram* fur_shader;
        ShaderProgram* particle_shader;
        MeshHandle model_mesh, sky_mesh, ground_mesh, rock_mesh, grass_mesh, particle_mesh;
        // Sanctuary scene meshes
        MeshHandle pedestal_mesh, column_tall_mesh, column_stump_mesh;
        MeshHandle arch_mesh, fallen_column_mesh, slab_mesh;
        MeshHandle stone_sphere_mesh, mossy_block_mesh, bowl_mesh;
        MeshHandle obelisk_mesh, ring_inner_mesh, ring_outer_mesh;
        MeshHandle pond_mesh;
        MeshHandle torch_mesh;
        ShaderProgram* torch_shader;
        MeshHandle tree_meshes[3];
        TextureHandle fur_tex, fur_mask_tex;
        float model_bounding_radius;
    } core;

    // T2+ shadow mapping
    struct Shadow {
        ShaderProgram* shader;
        RenderTargetHandle rt;
        TextureHandle depth_tex;
        int map_size;
        Shadow() : shader(nullptr), rt(), depth_tex(), map_size(0) {}
    } shadow;

    // T2+ bloom
    struct Bloom {
        ShaderProgram* extract_shader;
        ShaderProgram* blur_shader;
        ShaderProgram* composite_shader;
        MeshHandle fullscreen_quad;
        RenderTargetHandle scene_rt, bright_rt, blur_rt;
        float strength;
        Bloom() : extract_shader(nullptr), blur_shader(nullptr), composite_shader(nullptr),
                  fullscreen_quad(), scene_rt(), bright_rt(), blur_rt(), strength(0) {}
    } bloom;

    // T2+ instanced grass
    struct Grass {
        ShaderProgram* shader;
        MeshHandle blade_mesh;
        Grass() : shader(nullptr), blade_mesh() {}
    } grass;

    // T2+ SSAO
    struct SSAO {
        ShaderProgram* shader;
        ShaderProgram* blur_shader;
        RenderTargetHandle rt, blur_rt;
        TextureHandle noise_tex, scene_depth_tex;
        SSAO() : shader(nullptr), blur_shader(nullptr), rt(), blur_rt(), noise_tex(), scene_depth_tex() {}
    } ssao;

    // T3+ normal map
    TextureHandle normal_map_tex;

    // T4+ features
    struct T4 {
        // PBR/tessellation
        ShaderProgram* tess_shader;
        ShaderProgram* compute_particle_shader;
        ShaderProgram* particle_render_shader;
        ShaderProgram* volumetric_fog_shader;
        ShaderProgram* tone_map_shader;
        BufferHandle particle_ssbo;
        int compute_particle_count;

        // HDR
        RenderTargetHandle hdr_scene_rt, hdr_bright_rt;
        TextureHandle hdr_depth_tex;
        TextureHandle hdr_color_tex;  // non-owning view of HDR RT color
        RenderTargetHandle fog_rt;
        int fog_w, fog_h;

        // GTAO
        ShaderProgram* gtao_shader;
        ShaderProgram* gtao_blur_shader;
        TextureHandle gtao_tex, gtao_blur_tex;

        // Compute Bloom
        ShaderProgram* bloom_down_compute;
        ShaderProgram* bloom_up_compute;
        static const int BLOOM_MIP_COUNT = 6;
        TextureHandle bloom_mips[BLOOM_MIP_COUNT];

        // Auto-exposure
        ShaderProgram* histogram_shader;
        ShaderProgram* exposure_shader;
        BufferHandle histogram_ssbo, exposure_ssbo;

        // SSR
        ShaderProgram* ssr_shader;
        TextureHandle ssr_tex;
        TextureHandle ssr_color_snapshot;  // RGBA16F copy of HDR color (for water SSR)
        TextureHandle ssr_depth_snapshot;  // DEPTH_COMPONENT24 copy of HDR depth (for water SSR)

        // DoF
        ShaderProgram* dof_shader;
        TextureHandle dof_tex;

        // Puddles
        static const int PUDDLE_COUNT = 3;
        MeshHandle puddle_meshes[PUDDLE_COUNT];

        T4() : tess_shader(nullptr), compute_particle_shader(nullptr),
               particle_render_shader(nullptr), volumetric_fog_shader(nullptr),
               tone_map_shader(nullptr), particle_ssbo(), compute_particle_count(0),
               hdr_scene_rt(), hdr_bright_rt(), hdr_depth_tex(), hdr_color_tex(), fog_rt(), fog_w(0), fog_h(0),
               gtao_shader(nullptr), gtao_blur_shader(nullptr), gtao_tex(), gtao_blur_tex(),
               bloom_down_compute(nullptr), bloom_up_compute(nullptr),
               histogram_shader(nullptr), exposure_shader(nullptr),
               histogram_ssbo(), exposure_ssbo(),
               ssr_shader(nullptr), ssr_tex(), ssr_color_snapshot(), ssr_depth_snapshot(),
               dof_shader(nullptr), dof_tex() {
            for (int i = 0; i < BLOOM_MIP_COUNT; i++) bloom_mips[i] = TextureHandle();
            for (int i = 0; i < PUDDLE_COUNT; i++) puddle_meshes[i] = MeshHandle();
        }
    } t4;

    TierResourceView() : normal_map_tex() {
        core.sky_shader = nullptr;
        core.island_shader = nullptr;
        core.fur_shader = nullptr;
        core.particle_shader = nullptr;
        core.torch_shader = nullptr;
        core.model_bounding_radius = 0.0f;
    }
};
