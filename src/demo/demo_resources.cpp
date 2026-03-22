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
    , shadow_depth_tex_()
    , shadow_map_size_(0)
    , bloom_strength_(0.0f)
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

    // Scattered grass (batched for T1, same blade shape as instanced version)
    {
        MeshData grass = MeshGen::scatteredGrass(600, 20.0f, 0.30f, 0.10f, 251u);
        grass_mesh_ = scene_meshes_.add(grass);
        Log::info("Resources: generated %d grass blades (%d verts)",
                  600, static_cast<int>(grass.vertices.size()));
    }

    // Billboard particles
    {
        MeshData particles = MeshGen::particleQuads(200, 6.0f, 2.0f, 317u);
        particle_mesh_ = scene_meshes_.add(particles);
        Log::info("Resources: generated %d particle quads (%d verts)",
                  200, static_cast<int>(particles.vertices.size()));
    }

    // Grass blade template (for T2+ instancing)
    {
        MeshData blade = MeshGen::grassBlade();
        grass_blade_mesh_ = scene_meshes_.add(blade);
        Log::info("Resources: generated grass blade template (%d verts)",
                  static_cast<int>(blade.vertices.size()));
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

    if (tier == 2) {
        // Island T2 shader (with shadow mapping)
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl3/island_t2_150.vert");
                fs_str = ShaderLoader::load("gl3/island_t2_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl3/island_t2.vert");
                fs_str = ShaderLoader::load("gl3/island_t2.frag");
            }
            if (!island_shaders_[idx].create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::err("Resources: failed to create island shader (tier %d)", tier);
                return false;
            }
        }

        // Fur T2 shader (with shadow mapping)
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl3/fur_t2_150.vert");
                fs_str = ShaderLoader::load("gl3/fur_t2_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl3/fur_t2.vert");
                fs_str = ShaderLoader::load("gl3/fur_t2.frag");
            }
            if (!fur_shaders_[idx].create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::err("Resources: failed to create fur shader (tier %d)", tier);
                return false;
            }
        }

        // Grass T2 instanced shader
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl3/grass_t2_150.vert");
                fs_str = ShaderLoader::load("gl3/grass_t2_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl3/grass_t2.vert");
                fs_str = ShaderLoader::load("gl3/grass_t2.frag");
            }
            if (!grass_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::warn("Resources: failed to create grass T2 shader (non-critical)");
            }
        }

        return true;
    }

    // Tiers 3-4: shaders don't exist yet
    Log::warn("Resources: tier %d shaders not implemented yet, reusing tier 1", tier);
    return false;
}

bool DemoResources::createShadowResources(Renderer* r) {
    // Shadow depth shader
    {
        std::string vs_str, fs_str;
        if (r->isCoreProfile()) {
            vs_str = ShaderLoader::load("gl3/shadow_depth_150.vert");
            fs_str = ShaderLoader::load("gl3/shadow_depth_150.frag");
        } else {
            vs_str = ShaderLoader::load("gl3/shadow_depth.vert");
            fs_str = ShaderLoader::load("gl3/shadow_depth.frag");
        }
        if (!shadow_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
            Log::warn("Resources: failed to create shadow depth shader");
            return false;
        }
    }

    // Shadow depth render target (1024x1024)
    shadow_map_size_ = 1024;
    RenderTargetHandle srt = r->createDepthRenderTarget(shadow_map_size_, shadow_map_size_);
    if (srt == INVALID_RENDER_TARGET) {
        Log::warn("Resources: failed to create shadow depth FBO");
        shadow_shader_.reset();
        shadow_map_size_ = 0;
        return false;
    }
    shadow_rt_.assign(r, srt);
    shadow_depth_tex_ = r->getDepthTexture(srt);

    Log::info("Resources: shadow map %dx%d created", shadow_map_size_, shadow_map_size_);
    return true;
}

bool DemoResources::createBloomResources(Renderer* r, int render_w, int render_h) {
    // Bloom extract shader
    {
        std::string vs_str, fs_str;
        if (r->isCoreProfile()) {
            vs_str = ShaderLoader::load("gl3/bloom_extract_150.vert");
            fs_str = ShaderLoader::load("gl3/bloom_extract_150.frag");
        } else {
            vs_str = ShaderLoader::load("gl3/bloom_extract.vert");
            fs_str = ShaderLoader::load("gl3/bloom_extract.frag");
        }
        if (!bloom_extract_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
            Log::warn("Resources: failed to create bloom extract shader");
            return false;
        }
    }

    // Bloom blur shader
    {
        std::string vs_str, fs_str;
        if (r->isCoreProfile()) {
            vs_str = ShaderLoader::load("gl3/bloom_blur_150.vert");
            fs_str = ShaderLoader::load("gl3/bloom_blur_150.frag");
        } else {
            vs_str = ShaderLoader::load("gl3/bloom_blur.vert");
            fs_str = ShaderLoader::load("gl3/bloom_blur.frag");
        }
        if (!bloom_blur_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
            Log::warn("Resources: failed to create bloom blur shader");
            bloom_extract_shader_.reset();
            return false;
        }
    }

    // Bloom composite shader
    {
        std::string vs_str, fs_str;
        if (r->isCoreProfile()) {
            vs_str = ShaderLoader::load("gl3/bloom_composite_150.vert");
            fs_str = ShaderLoader::load("gl3/bloom_composite_150.frag");
        } else {
            vs_str = ShaderLoader::load("gl3/bloom_composite.vert");
            fs_str = ShaderLoader::load("gl3/bloom_composite.frag");
        }
        if (!bloom_composite_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
            Log::warn("Resources: failed to create bloom composite shader");
            bloom_extract_shader_.reset();
            bloom_blur_shader_.reset();
            return false;
        }
    }

    // Fullscreen quad mesh
    {
        MeshData qd = MeshGen::quad();
        MeshHandle qh = r->createMesh(qd);
        if (qh == MeshHandle()) {
            Log::warn("Resources: failed to create fullscreen quad mesh");
            bloom_extract_shader_.reset();
            bloom_blur_shader_.reset();
            bloom_composite_shader_.reset();
            return false;
        }
        fullscreen_quad_.assign(r, qh);
    }

    // Scene FBO (full resolution, color + sampleable depth for SSAO)
    {
        RenderTargetHandle srt = r->createRenderTargetWithDepth(render_w, render_h);
        if (srt == INVALID_RENDER_TARGET) {
            Log::warn("Resources: failed to create scene FBO for bloom");
            bloom_extract_shader_.reset();
            bloom_blur_shader_.reset();
            bloom_composite_shader_.reset();
            fullscreen_quad_.reset();
            return false;
        }
        scene_rt_.assign(r, srt);
    }

    // Bright extract FBO (half resolution, color only)
    int bw = render_w / 2;
    int bh = render_h / 2;
    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;

    {
        RenderTargetHandle brt = r->createRenderTarget(bw, bh);
        if (brt == INVALID_RENDER_TARGET) {
            Log::warn("Resources: failed to create bright FBO for bloom");
            bloom_extract_shader_.reset();
            bloom_blur_shader_.reset();
            bloom_composite_shader_.reset();
            fullscreen_quad_.reset();
            scene_rt_.reset();
            return false;
        }
        bright_rt_.assign(r, brt);
    }

    // Blur FBO (half resolution, color only)
    {
        RenderTargetHandle blrt = r->createRenderTarget(bw, bh);
        if (blrt == INVALID_RENDER_TARGET) {
            Log::warn("Resources: failed to create blur FBO for bloom");
            bloom_extract_shader_.reset();
            bloom_blur_shader_.reset();
            bloom_composite_shader_.reset();
            fullscreen_quad_.reset();
            scene_rt_.reset();
            bright_rt_.reset();
            return false;
        }
        blur_rt_.assign(r, blrt);
    }

    bloom_strength_ = 0.3f;

    Log::info("Resources: bloom FBOs created (scene %dx%d, bloom %dx%d)",
              render_w, render_h, bw, bh);
    return true;
}

bool DemoResources::createSSAOResources(Renderer* r, int render_w, int render_h) {
    // SSAO shader
    {
        std::string vs_str, fs_str;
        if (r->isCoreProfile()) {
            vs_str = ShaderLoader::load("gl3/ssao_150.vert");
            fs_str = ShaderLoader::load("gl3/ssao_150.frag");
        } else {
            vs_str = ShaderLoader::load("gl3/ssao.vert");
            fs_str = ShaderLoader::load("gl3/ssao.frag");
        }
        if (!ssao_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
            Log::warn("Resources: failed to create SSAO shader");
            return false;
        }
    }

    // SSAO blur shader
    {
        std::string vs_str, fs_str;
        if (r->isCoreProfile()) {
            vs_str = ShaderLoader::load("gl3/ssao_blur_150.vert");
            fs_str = ShaderLoader::load("gl3/ssao_blur_150.frag");
        } else {
            vs_str = ShaderLoader::load("gl3/ssao_blur.vert");
            fs_str = ShaderLoader::load("gl3/ssao_blur.frag");
        }
        if (!ssao_blur_shader_.create(r, vs_str.c_str(), fs_str.c_str())) {
            Log::warn("Resources: failed to create SSAO blur shader");
            ssao_shader_.reset();
            return false;
        }
    }

    // SSAO FBO (half resolution for performance)
    int sw = render_w / 2;
    int sh = render_h / 2;
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;

    {
        RenderTargetHandle srt = r->createRenderTarget(sw, sh);
        if (srt == INVALID_RENDER_TARGET) {
            Log::warn("Resources: failed to create SSAO FBO");
            ssao_shader_.reset();
            ssao_blur_shader_.reset();
            return false;
        }
        ssao_rt_.assign(r, srt);
    }

    {
        RenderTargetHandle brt = r->createRenderTarget(sw, sh);
        if (brt == INVALID_RENDER_TARGET) {
            Log::warn("Resources: failed to create SSAO blur FBO");
            ssao_shader_.reset();
            ssao_blur_shader_.reset();
            ssao_rt_.reset();
            return false;
        }
        ssao_blur_rt_.assign(r, brt);
    }

    // 4x4 noise texture for kernel rotation
    {
        unsigned char noise[4 * 4 * 3];
        unsigned int seed = 42;
        for (int i = 0; i < 4 * 4; i++) {
            seed = seed * 1103515245u + 12345u;
            noise[i * 3 + 0] = static_cast<unsigned char>((seed >> 16) & 0xFF);
            seed = seed * 1103515245u + 12345u;
            noise[i * 3 + 1] = static_cast<unsigned char>((seed >> 16) & 0xFF);
            noise[i * 3 + 2] = 0; // Z component not needed
        }
        TextureHandle nt = r->createTexture(4, 4, 3, noise);
        if (nt == INVALID_TEXTURE) {
            Log::warn("Resources: failed to create SSAO noise texture");
            ssao_shader_.reset();
            ssao_blur_shader_.reset();
            ssao_rt_.reset();
            ssao_blur_rt_.reset();
            return false;
        }
        ssao_noise_tex_.assign(r, nt);
    }

    // Get depth texture from scene FBO
    scene_depth_tex_ = r->getRTDepthTexture(scene_rt_.get());
    if (scene_depth_tex_ == INVALID_TEXTURE) {
        Log::warn("Resources: scene FBO has no sampleable depth texture, SSAO disabled");
        ssao_shader_.reset();
        ssao_blur_shader_.reset();
        ssao_rt_.reset();
        ssao_blur_rt_.reset();
        ssao_noise_tex_.reset();
        return false;
    }

    Log::info("Resources: SSAO created (FBO %dx%d)", sw, sh);
    return true;
}

bool DemoResources::prepare(Renderer* r, int max_tier, int render_w, int render_h) {
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

    // Create shadow resources for T2+ (non-critical: T2 falls back to unshadowed)
    if (max_tier >= 2) {
        createShadowResources(r);
    }

    // Create bloom resources for T2+ (non-critical: T2 falls back to no bloom)
    if (max_tier >= 2 && render_w > 0 && render_h > 0) {
        createBloomResources(r, render_w, render_h);
    }

    // Create SSAO resources for T2+ (non-critical, requires bloom's scene FBO with depth tex)
    if (max_tier >= 2 && scene_rt_ && render_w > 0 && render_h > 0) {
        createSSAOResources(r, render_w, render_h);
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

    // T2+ shadow mapping resources
    if (idx >= 1 && shadow_shader_ && shadow_rt_) {
        view.shadow_shader = &shadow_shader_;
        view.shadow_rt = shadow_rt_.get();
        view.shadow_depth_tex = shadow_depth_tex_;
        view.shadow_map_size = shadow_map_size_;
    }

    // T2+ bloom post-processing resources
    if (idx >= 1 && bloom_extract_shader_ && bloom_blur_shader_
        && bloom_composite_shader_ && scene_rt_ && bright_rt_ && blur_rt_) {
        view.bloom_extract_shader = &bloom_extract_shader_;
        view.bloom_blur_shader = &bloom_blur_shader_;
        view.bloom_composite_shader = &bloom_composite_shader_;
        view.fullscreen_quad = fullscreen_quad_.get();
        view.scene_rt = scene_rt_.get();
        view.bright_rt = bright_rt_.get();
        view.blur_rt = blur_rt_.get();
        view.bloom_strength = bloom_strength_;
    }

    // T2+ instanced grass resources
    if (idx >= 1 && grass_shader_) {
        view.grass_shader = &grass_shader_;
        view.grass_blade_mesh = grass_blade_mesh_;
    }

    // T2+ SSAO resources
    if (idx >= 1 && ssao_shader_ && ssao_blur_shader_ && ssao_rt_ && ssao_blur_rt_) {
        view.ssao_shader = &ssao_shader_;
        view.ssao_blur_shader = &ssao_blur_shader_;
        view.ssao_rt = ssao_rt_.get();
        view.ssao_blur_rt = ssao_blur_rt_.get();
        view.ssao_noise_tex = ssao_noise_tex_.get();
        view.scene_depth_tex = scene_depth_tex_;
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

    // Shadow resources
    shadow_shader_.reset();
    shadow_rt_.reset();
    shadow_depth_tex_ = TextureHandle();
    shadow_map_size_ = 0;

    // Bloom resources
    bloom_extract_shader_.reset();
    bloom_blur_shader_.reset();
    bloom_composite_shader_.reset();
    fullscreen_quad_.reset();
    scene_rt_.reset();
    bright_rt_.reset();
    blur_rt_.reset();
    bloom_strength_ = 0.0f;

    // Grass resources
    grass_shader_.reset();

    // SSAO resources
    ssao_shader_.reset();
    ssao_blur_shader_.reset();
    ssao_rt_.reset();
    ssao_blur_rt_.reset();
    ssao_noise_tex_.reset();
    scene_depth_tex_ = TextureHandle();

    prepared_ = false;
    renderer_ = nullptr;
    Log::info("Resources: destroyed");
}
