#pragma once
#include "renderer/renderer.h"
#include "engine/shader_program.h"
#include "demo/tier/shader_bank.h"
#include "geometry/mesh.h"

struct TierResourceView {
    // Shader access: passes call res.shader(ShaderBank::Sky) instead of res.core.sky_shader.
    // ShaderBank handles per-tier variants and fallback automatically.
    const ShaderBank* shaders;
    DemoTier tier;

    ShaderProgram* shader(ShaderBank::Id id) const {
        return shaders ? shaders->get(id, tier) : nullptr;
    }

    // Core (always present)
    struct Core {
        MeshHandle model_mesh, sky_mesh, ground_mesh, rock_mesh, grass_mesh, particle_mesh;
        MeshHandle fullscreen_quad;  // always available, any tier
        // Sanctuary scene meshes
        MeshHandle pedestal_mesh, column_tall_mesh, column_stump_mesh;
        MeshHandle arch_mesh, fallen_column_mesh, slab_mesh;
        MeshHandle stone_sphere_mesh, mossy_block_mesh, bowl_mesh;
        MeshHandle obelisk_mesh, ring_inner_mesh, ring_outer_mesh;
        MeshHandle pond_mesh;
        MeshHandle torch_mesh;
        MeshHandle tree_meshes[3];
        TextureHandle fur_tex, fur_mask_tex;
        float model_bounding_radius;
    } core;

    // Scene FBO (any tier with post-processing)
    RenderTargetHandle scene_rt;
    TextureHandle scene_depth_tex;

    // T2+ shadow mapping
    struct Shadow {
        RenderTargetHandle rt;
        TextureHandle depth_tex;
        int map_size;
        Shadow() : rt(), depth_tex(), map_size(0) {}
    } shadow;

    // T2+ bloom
    struct Bloom {
        RenderTargetHandle bright_rt, blur_rt;
        float strength;
        Bloom() : bright_rt(), blur_rt(), strength(0) {}
    } bloom;

    // T2+ instanced grass
    struct Grass {
        MeshHandle blade_mesh;
        Grass() : blade_mesh() {}
    } grass;

    // T2+ SSAO
    struct SSAO {
        RenderTargetHandle rt, blur_rt;
        TextureHandle noise_tex, scene_depth_tex;
        SSAO() : rt(), blur_rt(), noise_tex(), scene_depth_tex() {}
    } ssao;

    // T3+ normal map
    TextureHandle normal_map_tex;

    // T4+ features
    struct T4 {
        // Compute particles
        BufferHandle particle_ssbo;
        int compute_particle_count;

        // HDR sub-struct
        struct HDR {
            RenderTargetHandle scene_rt, bright_rt;
            TextureHandle depth_tex;
            TextureHandle color_tex;
            RenderTargetHandle fog_rt;
            int fog_w, fog_h;
            HDR() : scene_rt(), bright_rt(), depth_tex(), color_tex(),
                    fog_rt(), fog_w(0), fog_h(0) {}
        } hdr;

        // GTAO sub-struct
        struct GTAO {
            TextureHandle tex, blur_tex;
            GTAO() : tex(), blur_tex() {}
        } gtao;

        // Compute Bloom sub-struct
        struct ComputeBloom {
            static const int MIP_COUNT = 6;
            TextureHandle mips[MIP_COUNT];
            ComputeBloom() {
                for (int i = 0; i < MIP_COUNT; i++) mips[i] = TextureHandle();
            }
        } bloom;

        // Auto-exposure sub-struct
        struct AutoExposure {
            BufferHandle histogram_ssbo, exposure_ssbo;
            AutoExposure() : histogram_ssbo(), exposure_ssbo() {}
        } exposure;

        // SSR sub-struct
        struct SSR {
            TextureHandle tex;
            TextureHandle color_snapshot;
            TextureHandle depth_snapshot;
            SSR() : tex(), color_snapshot(), depth_snapshot() {}
        } ssr;

        // DoF sub-struct
        struct DoF {
            TextureHandle tex;
            DoF() : tex() {}
        } dof;

        // Puddles (flat)
        static const int PUDDLE_COUNT = 3;
        MeshHandle puddle_meshes[PUDDLE_COUNT];

        T4() : particle_ssbo(), compute_particle_count(0) {
            for (int i = 0; i < PUDDLE_COUNT; i++) puddle_meshes[i] = MeshHandle();
        }
    } t4;

    TierResourceView() : shaders(nullptr), tier(DemoTier::Basic),
        scene_rt(), scene_depth_tex(), normal_map_tex() {
        core.model_bounding_radius = 0.0f;
    }
};
