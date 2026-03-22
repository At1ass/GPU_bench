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

    bool prepare(Renderer* r, int max_tier);
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

    bool loadSharedMeshes(Renderer* r);
    bool loadSharedTextures(Renderer* r);
    bool compileSkyShader(Renderer* r);
    bool compileTierShaders(Renderer* r, int tier);
};
