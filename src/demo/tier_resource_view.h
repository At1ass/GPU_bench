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

    TierResourceView()
        : sky_shader(nullptr), island_shader(nullptr), fur_shader(nullptr)
        , particle_shader(nullptr)
        , model_mesh(), sky_mesh(), ground_mesh()
        , rock_mesh(), grass_mesh(), particle_mesh()
        , fur_tex(), fur_mask_tex()
        , model_bounding_radius(0.0f) {}
};
