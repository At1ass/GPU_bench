#include "demo/demo_resources.h"
#include "demo/demo_scene.h"
#include "demo/shader_loader.h"
#include "demo/fur_texture.h"
#include "geometry/mesh_gen.h"
#include "geometry/obj_loader.h"
#include "platform/data_path.h"
#include "platform/logger.h"
#include "stb_image.h"
#include <cmath>

DemoResources::DemoResources()
    : renderer_(nullptr)
    , prepared_(false)
    , model_mesh_()
    , ground_mesh_()
    , rock_mesh_()
    , grass_mesh_()
    , particle_mesh_()
    , model_bounding_radius_(0.0f)
{
}

DemoResources::~DemoResources() {
    destroy();
}

bool DemoResources::loadSharedMeshes(Renderer* r) {
    scene_meshes_.init(r);

    // Sky mesh
    {
        MeshData sd = MeshGen::sphere(32, 16);
        sky_mesh_.assign(r, r->createMesh(sd));
    }

    // Model (OBJ or fallback sphere)
    {
        MeshData md;
        std::string obj_path = getDataPath("models/bunny.obj");
        if (!obj_path.empty()) {
            md = ObjLoader::load(obj_path.c_str());
            if (!md.vertices.empty()) {
                ObjLoader::normalize(md);
                MeshGen::smoothNormals(md);
                MeshGen::optimizeVertexCache(md);
                Log::info("Resources: loaded OBJ model from %s (%d verts, smooth normals, cache-optimized)",
                          obj_path.c_str(), static_cast<int>(md.vertices.size()));
            }
        }
        if (md.vertices.empty()) {
            Log::info("Resources: OBJ not found, using fallback sphere");
            md = MeshGen::sphere(64, 32);
            ObjLoader::normalize(md);
        }
        model_bounding_radius_ = MeshGen::boundingRadius(md);
        model_mesh_ = scene_meshes_.add(md);
    }

    // Ground plane
    {
        MeshData gd = MeshGen::terrain(20.0f, 40);
        ground_mesh_ = scene_meshes_.add(gd);
    }

    // Scattered rocks
    {
        MeshData rocks = MeshGen::scatteredRocks(10, 8.0f, 0.1f, 0.3f, 137u);
        rock_mesh_ = scene_meshes_.add(rocks);
        Log::info("Resources: generated %d scattered rocks (%d verts)",
                  10, static_cast<int>(rocks.vertices.size()));
    }

    // Scattered grass
    {
        MeshData grass = MeshGen::scatteredGrass(30, 8.0f, 0.15f, 0.06f, 251u);
        grass_mesh_ = scene_meshes_.add(grass);
        Log::info("Resources: generated %d grass tufts (%d verts)",
                  30, static_cast<int>(grass.vertices.size()));
    }

    // Billboard particles
    {
        MeshData particles = MeshGen::particleQuads(200, 6.0f, 2.0f, 317u);
        particle_mesh_ = scene_meshes_.add(particles);
        Log::info("Resources: generated %d particle quads (%d verts)",
                  200, static_cast<int>(particles.vertices.size()));
    }

    return true;
}

bool DemoResources::loadSharedTextures(Renderer* r) {
    // Fur strand texture (128x128)
    {
        const int fur_tex_size = 128;
        std::vector<unsigned char> fur_pixels = generateFurTexture(fur_tex_size, 0.75f);
        fur_tex_.assign(r, r->createTexture(fur_tex_size, fur_tex_size, 4, fur_pixels.data()));
        Log::info("Resources: generated fur texture %dx%d", fur_tex_size, fur_tex_size);
    }

    // Fur mask (fur.png)
    {
        std::string mask_path = getDataPath("models/fur.png");
        if (!mask_path.empty()) {
            int mw = 0, mh = 0, mc = 0;
            stbi_set_flip_vertically_on_load(1);
            unsigned char* pixels = stbi_load(mask_path.c_str(), &mw, &mh, &mc, 4);
            if (pixels && mw > 0 && mh > 0) {
                fur_mask_tex_.assign(r, r->createTexture(mw, mh, 4, pixels));
                Log::info("Resources: loaded fur mask %dx%d from %s", mw, mh, mask_path.c_str());
            }
            if (pixels) stbi_image_free(pixels);
        }
    }

    return true;
}

bool DemoResources::compileSkyShader(Renderer* r) {
    std::string vs_str, fs_str;
    if (r->isCoreProfile()) {
        vs_str = ShaderLoader::load("gl2/sky_150.vert");
        fs_str = ShaderLoader::load("gl2/sky_150.frag");
    } else {
        vs_str = ShaderLoader::load("gl2/sky.vert");
        fs_str = ShaderLoader::load("gl2/sky.frag");
    }
    if (!sky_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
        Log::err("Resources: failed to create sky shader");
        return false;
    }
    return true;
}

bool DemoResources::compileTierShaders(Renderer* r, int tier) {
    int idx = tier - 1;  // tier 1..4 -> index 0..3
    if (idx < 0 || idx >= MAX_TIERS) return false;

    if (tier == 1) {
        // Island shader
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl2/island_t1_150.vert");
                fs_str = ShaderLoader::load("gl2/island_t1_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl2/island_t1.vert");
                fs_str = ShaderLoader::load("gl2/island_t1.frag");
            }
            if (!island_shaders_[idx].create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::err("Resources: failed to create island shader (tier %d)", tier);
                return false;
            }
        }

        // Fur shader
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl2/fur_150.vert");
                fs_str = ShaderLoader::load("gl2/fur_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl2/fur.vert");
                fs_str = ShaderLoader::load("gl2/fur.frag");
            }
            if (!fur_shaders_[idx].create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::err("Resources: failed to create fur shader (tier %d)", tier);
                return false;
            }
        }

        // Particle shader
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl2/particle_150.vert");
                fs_str = ShaderLoader::load("gl2/particle_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl2/particle.vert");
                fs_str = ShaderLoader::load("gl2/particle.frag");
            }
            if (!particle_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::warn("Resources: failed to create particle shader (non-critical)");
                // Non-critical: particles just won't render
            }
        }

        return true;
    }

    // Tiers 2-4: shaders don't exist yet
    Log::warn("Resources: tier %d shaders not implemented yet, reusing tier 1", tier);
    return false;
}

bool DemoResources::prepare(Renderer* r, int max_tier) {
    if (prepared_) return true;
    renderer_ = r;

    if (!loadSharedMeshes(r)) {
        Log::err("Resources: failed to load shared meshes");
        return false;
    }

    if (!loadSharedTextures(r)) {
        Log::err("Resources: failed to load shared textures");
        return false;
    }

    if (!compileSkyShader(r)) {
        return false;
    }

    // Compile tier 1 shaders (required)
    if (!compileTierShaders(r, 1)) {
        return false;
    }

    // Compile higher tier shaders (non-critical: fall back to tier 1)
    for (int t = 2; t <= max_tier && t <= MAX_TIERS; t++) {
        compileTierShaders(r, t);
    }

    prepared_ = true;
    Log::info("Resources: all resources prepared (max_tier=%d)", max_tier);
    return true;
}

TierResourceView DemoResources::viewForTier(DemoTier tier) {
    TierResourceView view;

    view.sky_shader = &sky_shader_;
    view.model_mesh = model_mesh_;
    view.sky_mesh = sky_mesh_.get();
    view.ground_mesh = ground_mesh_;
    view.rock_mesh = rock_mesh_;
    view.grass_mesh = grass_mesh_;
    view.particle_mesh = particle_mesh_;
    view.fur_tex = fur_tex_.get();
    view.fur_mask_tex = fur_mask_tex_.get();
    view.model_bounding_radius = model_bounding_radius_;
    view.particle_shader = particle_shader_ ? &particle_shader_ : nullptr;

    int idx = static_cast<int>(tier) - 1;
    if (idx < 0 || idx >= MAX_TIERS) idx = 0;

    // Use tier-specific shaders if available, otherwise fall back to tier 1
    if (island_shaders_[idx]) {
        view.island_shader = &island_shaders_[idx];
    } else {
        view.island_shader = &island_shaders_[0];
    }

    if (fur_shaders_[idx]) {
        view.fur_shader = &fur_shaders_[idx];
    } else {
        view.fur_shader = &fur_shaders_[0];
    }

    return view;
}

void DemoResources::destroy() {
    if (!prepared_) return;

    scene_meshes_.destroyAll();
    sky_mesh_.reset();
    fur_tex_.reset();
    fur_mask_tex_.reset();
    sky_shader_.reset();

    for (int i = 0; i < MAX_TIERS; i++) {
        island_shaders_[i].reset();
        fur_shaders_[i].reset();
    }
    particle_shader_.reset();

    prepared_ = false;
    renderer_ = nullptr;
    Log::info("Resources: destroyed");
}
