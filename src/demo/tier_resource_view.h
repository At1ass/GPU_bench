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
        // PBR/tessellation (flat)
        ShaderProgram* tess_shader;
        ShaderProgram* compute_particle_shader;
        ShaderProgram* particle_render_shader;
        BufferHandle particle_ssbo;
        int compute_particle_count;

        // HDR sub-struct
        struct HDR {
            RenderTargetHandle scene_rt, bright_rt;
            TextureHandle depth_tex;
            TextureHandle color_tex;  // non-owning view of HDR RT color
            RenderTargetHandle fog_rt;
            int fog_w, fog_h;
            ShaderProgram* tone_map_shader;
            ShaderProgram* volumetric_fog_shader;
            HDR() : scene_rt(), bright_rt(), depth_tex(), color_tex(),
                    fog_rt(), fog_w(0), fog_h(0),
                    tone_map_shader(nullptr), volumetric_fog_shader(nullptr) {}
        } hdr;

        // GTAO sub-struct
        struct GTAO {
            ShaderProgram* shader;
            ShaderProgram* blur_shader;
            TextureHandle tex, blur_tex;
            GTAO() : shader(nullptr), blur_shader(nullptr), tex(), blur_tex() {}
        } gtao;

        // Compute Bloom sub-struct
        struct ComputeBloom {
            ShaderProgram* down_compute;
            ShaderProgram* up_compute;
            static const int MIP_COUNT = 6;
            TextureHandle mips[MIP_COUNT];
            ComputeBloom() : down_compute(nullptr), up_compute(nullptr) {
                for (int i = 0; i < MIP_COUNT; i++) mips[i] = TextureHandle();
            }
        } bloom;

        // Auto-exposure sub-struct
        struct AutoExposure {
            ShaderProgram* histogram_shader;
            ShaderProgram* exposure_shader;
            BufferHandle histogram_ssbo, exposure_ssbo;
            AutoExposure() : histogram_shader(nullptr), exposure_shader(nullptr),
                             histogram_ssbo(), exposure_ssbo() {}
        } exposure;

        // SSR sub-struct
        struct SSR {
            ShaderProgram* shader;
            TextureHandle tex;
            TextureHandle color_snapshot;  // RGBA16F copy of HDR color (for water SSR)
            TextureHandle depth_snapshot;  // DEPTH_COMPONENT24 copy of HDR depth (for water SSR)
            SSR() : shader(nullptr), tex(), color_snapshot(), depth_snapshot() {}
        } ssr;

        // DoF sub-struct
        struct DoF {
            ShaderProgram* shader;
            TextureHandle tex;
            DoF() : shader(nullptr), tex() {}
        } dof;

        // Puddles (flat)
        static const int PUDDLE_COUNT = 3;
        MeshHandle puddle_meshes[PUDDLE_COUNT];

        T4() : tess_shader(nullptr), compute_particle_shader(nullptr),
               particle_render_shader(nullptr), particle_ssbo(), compute_particle_count(0) {
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
