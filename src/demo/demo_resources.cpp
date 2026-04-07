#include "demo/demo_resources.h"
#include "demo/demo_scene.h"
#include "demo/shader_loader.h"
#include "demo/shader_feature.h"
#include "renderer/features.h"
#include "demo/fur_texture.h"
#include "geometry/mesh_gen.h"
#include "geometry/obj_loader.h"
#include "platform/data_path.h"
#include "platform/logger.h"
#include "stb_image.h"
#include <cmath>

// Terrain heightmap sampling (matches demo_scene.cpp sampleTerrainHeight)
static float terrainHeightRaw(float x, float z) {
    float h = 0.0f;
    h += sinf(x * 0.4f) * cosf(z * 0.3f) * 0.6f;
    h += sinf(x * 0.7f + 1.3f) * sinf(z * 0.5f + 0.7f) * 0.3f;
    float pdx = x - 3.5f, pdz = z - 3.5f;
    float pd = sqrtf(pdx * pdx + pdz * pdz);
    float pt = pd < 3.0f ? (3.0f - pd) / 3.0f : 0.0f;
    pt = pt * pt * (3.0f - 2.0f * pt);
    h -= pt * 0.3f;
    float cd = sqrtf(x * x + z * z);
    float ft = cd < 2.5f ? (cd < 1.0f ? 1.0f : (2.5f - cd) / 1.5f) : 0.0f;
    ft = ft * ft * (3.0f - 2.0f * ft);
    h *= (1.0f - ft);
    return h;
}

DemoResources::DemoResources()
    : renderer_(nullptr)
    , prepared_(false)
    , model_mesh_()
    , ground_mesh_()
    , rock_mesh_()
    , grass_mesh_()
    , particle_mesh_()
    , model_bounding_radius_(0.0f)
    , sky_shader_from_cache_(nullptr)
    , sky_cache_()
    , particle_cache_(nullptr)
    , torch_cache_(nullptr)
    , shadow_cache_(nullptr)
    , shadow_depth_tex_()
    , shadow_map_size_(0)
    , bloom_extract_cache_(nullptr)
    , bloom_blur_cache_(nullptr)
    , bloom_composite_cache_(nullptr)
    , bloom_strength_(0.0f)
    , grass_cache_(nullptr)
    , grass_t3_cache_(nullptr)
    , ssao_cache_(nullptr)
    , ssao_blur_cache_(nullptr)
{
    for (int i = 0; i < MAX_TIERS; i++) {
        island_cache_[i] = nullptr;
        fur_cache_[i] = nullptr;
    }
}

DemoResources::~DemoResources() {
    destroy();
}

bool DemoResources::loadSharedMeshes(Renderer* r) {
    scene_meshes_.init(r);

    // Sky mesh (vertex shader scales ×500 and uses pos.xyww trick)
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
                LOG_INF("Resources: loaded OBJ model from %s (%d verts, smooth normals, cache-optimized)",
                          obj_path.c_str(), static_cast<int>(md.vertices.size()));
            }
        }
        if (md.vertices.empty()) {
            LOG_DBG("Demo: OBJ not found, using fallback sphere");
            md = MeshGen::sphere(64, 32);
            ObjLoader::normalize(md);
        }
        model_bounding_radius_ = MeshGen::boundingRadius(md);
        model_mesh_ = scene_meshes_.add(md);
    }

    // Ground plane
    {
        MeshData gd = MeshGen::terrain(20.0f, 80);
        // Apply heightmap displacement
        for (size_t i = 0; i < gd.vertices.size(); i++) {
            float x = gd.vertices[i].pos.x;
            float z = gd.vertices[i].pos.z;
            float h = 0.0f;
            // Rolling hills
            h += sinf(x * 0.4f) * cosf(z * 0.3f) * 0.6f;
            h += sinf(x * 0.7f + 1.3f) * sinf(z * 0.5f + 0.7f) * 0.3f;
            // Flatten center for pedestal
            float cd = sqrtf(x * x + z * z);
            float flat_t = cd < 2.5f ? (cd < 1.0f ? 1.0f : (2.5f - cd) / 1.5f) : 0.0f;
            flat_t = flat_t * flat_t * (3.0f - 2.0f * flat_t);  // smoothstep
            h = h * (1.0f - flat_t);
            // Pond depression behind arch at (0.0, -3.5), radius ~3.0
            // Applied AFTER center flattening so the depression isn't attenuated
            float pdx = x - 0.0f, pdz = z - (-3.5f);
            float pond_dist = sqrtf(pdx * pdx + pdz * pdz);
            float pond_t = pond_dist < 3.0f ? (3.0f - pond_dist) / 3.0f : 0.0f;
            pond_t = pond_t * pond_t * (3.0f - 2.0f * pond_t);
            h -= pond_t * 0.5f;
            gd.vertices[i].pos.y = h;
        }
        MeshGen::recomputeNormals(gd);
        ground_mesh_ = scene_meshes_.add(gd);
    }

    // Scattered rocks
    {
        MeshData rocks = MeshGen::scatteredRocks(25, 8.0f, 0.1f, 0.3f, 137u);
        // Apply terrain heightmap to rock positions
        for (size_t i = 0; i < rocks.vertices.size(); i++) {
            rocks.vertices[i].pos.y += terrainHeightRaw(rocks.vertices[i].pos.x, rocks.vertices[i].pos.z);
        }
        rock_mesh_ = scene_meshes_.add(rocks);
        LOG_INF("Resources: generated %d scattered rocks (%d verts)",
                  25, static_cast<int>(rocks.vertices.size()));
    }

    // Scattered grass (batched for T1, same blade shape as instanced version)
    {
        MeshData grass = MeshGen::scatteredGrass(800, 20.0f, 0.30f, 0.10f, 251u);
        // Apply terrain heightmap to grass positions
        for (size_t i = 0; i < grass.vertices.size(); i++) {
            grass.vertices[i].pos.y += terrainHeightRaw(grass.vertices[i].pos.x, grass.vertices[i].pos.z);
        }
        grass_mesh_ = scene_meshes_.add(grass);
        LOG_INF("Resources: generated %d grass blades (%d verts)",
                  800, static_cast<int>(grass.vertices.size()));
    }

    // Billboard particles
    {
        MeshData particles = MeshGen::particleQuads(200, 6.0f, 2.0f, 317u);
        particle_mesh_ = scene_meshes_.add(particles);
        LOG_INF("Resources: generated %d particle quads (%d verts)",
                  200, static_cast<int>(particles.vertices.size()));
    }

    // Grass blade template (for T2+ instancing)
    {
        MeshData blade = MeshGen::grassBlade();
        grass_blade_mesh_ = scene_meshes_.add(blade);
        LOG_INF("Resources: generated grass blade template (%d verts)",
                  static_cast<int>(blade.vertices.size()));
    }

    // Pedestal: hexagonal frustum (larger, prominent base for bunny)
    {
        MeshData md = MeshGen::frustum(6, 1.1f, 1.4f, 1.1f);
        pedestal_mesh_ = scene_meshes_.add(md);
    }

    // Tall column: thick cylinder + base ring + capital (top ring)
    {
        MeshData md = MeshGen::cylinder(16, 3.0f, 0.30f);
        MeshData ring = MeshGen::torus(16, 8, 0.30f, 0.08f);
        MeshGen::appendMesh(md, ring, Mat4());  // ring at center (sinks into ground)
        // Capital at column top: wider torus to bridge column (r=0.30) → arch tube (r=0.25)
        MeshData capital = MeshGen::torus(16, 8, 0.30f, 0.12f);
        MeshGen::appendMesh(md, capital, Mat4::translate(0.0f, 1.5f, 0.0f));  // at cylinder top
        column_tall_mesh_ = scene_meshes_.add(md);
    }

    // Column stump: shorter but thick cylinder + torus base
    {
        MeshData md = MeshGen::cylinder(16, 1.2f, 0.30f);
        MeshData ring = MeshGen::torus(16, 8, 0.30f, 0.08f);
        MeshGen::appendMesh(md, ring, Mat4());
        column_stump_mesh_ = scene_meshes_.add(md);
    }

    // Arch: massive half torus (proper stone arch)
    {
        MeshData md = MeshGen::halfTorus(32, 12, 1.8f, 0.25f);
        arch_mesh_ = scene_meshes_.add(md);
    }

    // Fallen column segment (thick, lying on ground)
    {
        MeshData md = MeshGen::cylinder(16, 2.2f, 0.28f);
        fallen_column_mesh_ = scene_meshes_.add(md);
    }

    // Stone slab (cube will be scaled in scene)
    {
        MeshData md = MeshGen::cube();
        slab_mesh_ = scene_meshes_.add(md);
    }

    // Stone sphere (low-poly for carved stone look)
    {
        MeshData md = MeshGen::sphere(16, 10);
        stone_sphere_mesh_ = scene_meshes_.add(md);
    }

    // Mossy block (cube, will be scaled)
    {
        MeshData md = MeshGen::cube();
        mossy_block_mesh_ = scene_meshes_.add(md);
    }

    // Offering bowl (larger, visible)
    {
        MeshData md = MeshGen::torus(16, 10, 0.30f, 0.10f);
        bowl_mesh_ = scene_meshes_.add(md);
    }

    // Obelisk (taller, thicker)
    {
        MeshData md = MeshGen::frustum(8, 2.5f, 0.15f, 0.05f);
        obelisk_mesh_ = scene_meshes_.add(md);
    }

    // Step rings (thicker tori)
    {
        MeshData md = MeshGen::torus(32, 6, 1.5f, 0.08f);
        ring_inner_mesh_ = scene_meshes_.add(md);
    }
    {
        MeshData md = MeshGen::torus(32, 6, 2.2f, 0.08f);
        ring_outer_mesh_ = scene_meshes_.add(md);
    }

    // Pond (large disc, replaces 3 small puddles)
    {
        MeshData md = MeshGen::disc(2.5f, 64, 0);
        pond_mesh_ = scene_meshes_.add(md);
    }

    // Torch billboard quads (3 quads for 3 point lights)
    {
        MeshData md = MeshGen::torchQuads(2);
        torch_mesh_ = scene_meshes_.add(md);
    }

    // Procedural tree meshes: 3 variants with different seeds for variety
    {
        unsigned int seeds[] = { 42, 137, 293 };
        for (int i = 0; i < 3; i++) {
            MeshData md = MeshGen::proceduralTree(3.0f, 0.15f, 1.2f, 10, seeds[i]);
            tree_meshes_[i] = scene_meshes_.add(md);
        }
    }

    LOG_DBG("Demo: mesh pool loaded (%d meshes)", 21);

    return true;
}

bool DemoResources::loadSharedTextures(Renderer* r) {
    // Fur strand texture (128x128)
    {
        const int fur_tex_size = 128;
        std::vector<unsigned char> fur_pixels = generateFurTexture(fur_tex_size, 0.75f);
        fur_tex_.assign(r, r->createTexture(fur_tex_size, fur_tex_size, 4, fur_pixels.data()));
        LOG_INF("Resources: generated fur texture %dx%d", fur_tex_size, fur_tex_size);
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
                LOG_INF("Resources: loaded fur mask %dx%d from %s", mw, mh, mask_path.c_str());
            }
            if (pixels) stbi_image_free(pixels);
        }
    }

    return true;
}

bool DemoResources::compileSkyShader(Renderer* r) {
    ShaderFeatureSet feat = r->isCoreProfile() ? SF_GLSL_150 : SF_GLSL_120;
    sky_shader_from_cache_ = shader_cache_.get("sky", feat, feat);
    if (!sky_shader_from_cache_) {
        LOG_ERR("Resources: failed to create sky shader");
        return false;
    }
    return true;
}

bool DemoResources::compileTierShaders(Renderer* r, int tier) {
    int idx = tier - 1;  // tier 1..4 -> index 0..3
    if (idx < 0 || idx >= MAX_TIERS) return false;

    ShaderFeatureSet feat = featuresForTier(static_cast<DemoTier>(tier), r->isCoreProfile());

    // Sky shader per-tier (domain warp T2+, physical atmosphere T3+)
    sky_cache_[idx] = shader_cache_.get("sky", feat, feat);

    if (tier == 1) {
        // Island T1 via cache
        island_cache_[idx] = shader_cache_.get("island", feat, feat);
        if (!island_cache_[idx]) {
            LOG_ERR("Resources: failed to compile island shader via cache (tier 1)");
            return false;
        }

        // Fur T1 via cache
        fur_cache_[idx] = shader_cache_.get("fur", feat, feat);
        if (!fur_cache_[idx]) {
            LOG_ERR("Resources: failed to compile fur shader via cache");
            return false;
        }

        // Particle via cache
        particle_cache_ = shader_cache_.get("particle", feat, feat);
        if (!particle_cache_) {
            LOG_WRN("Resources: failed to compile particle shader via cache (non-critical)");
        }

        return true;
    }

    if (tier == 2) {
        // Island T2 via cache
        island_cache_[idx] = shader_cache_.get("island", feat, feat);
        if (!island_cache_[idx]) {
            LOG_ERR("Resources: failed to compile island shader via cache (tier 2)");
            return false;
        }

        // Fur T2 via cache (unified uber shader, features drive #ifdefs)
        fur_cache_[idx] = shader_cache_.get("fur", feat, feat);
        if (!fur_cache_[idx]) {
            LOG_ERR("Resources: failed to compile fur shader via cache (tier 2)");
            return false;
        }

        // Grass T2 via cache (unified uber shader)
        grass_cache_ = shader_cache_.get("grass", feat, feat);
        if (!grass_cache_) {
            LOG_WRN("Resources: failed to compile grass shader via cache (non-critical)");
        }

        return true;
    }

    if (tier == 3) {
        // Island T3 via cache
        island_cache_[idx] = shader_cache_.get("island", feat, feat);
        if (!island_cache_[idx]) {
            LOG_ERR("Resources: failed to compile island shader via cache (tier 3)");
            return false;
        }

        // Fur T3 via cache (unified uber shader, features drive #ifdefs)
        fur_cache_[idx] = shader_cache_.get("fur", feat, feat);
        if (!fur_cache_[idx]) {
            LOG_ERR("Resources: failed to compile fur shader via cache (tier 3)");
            return false;
        }

        // Grass T3 via cache (unified uber shader)
        grass_t3_cache_ = shader_cache_.get("grass", feat, feat);
        if (!grass_t3_cache_) {
            LOG_WRN("Resources: failed to compile grass shader via cache (non-critical)");
        }

        // Torch T3+ via cache (point light flame billboards)
        torch_cache_ = shader_cache_.get("torch", feat, feat);
        if (!torch_cache_) {
            LOG_WRN("Resources: failed to compile torch shader via cache (non-critical)");
        }

        // Procedural normal map texture (256x256)
        {
            const int NM_SIZE = 256;
            std::vector<unsigned char> nm_data(static_cast<size_t>(NM_SIZE * NM_SIZE * 3));
            for (int y = 0; y < NM_SIZE; y++) {
                for (int x = 0; x < NM_SIZE; x++) {
                    float u = static_cast<float>(x) / NM_SIZE * 8.0f;
                    float v = static_cast<float>(y) / NM_SIZE * 8.0f;
                    // Simple procedural bumps using sin waves
                    float h00 = sinf(u * 3.7f) * cosf(v * 4.1f) * 0.3f
                              + sinf(u * 7.3f + 1.0f) * cosf(v * 6.7f + 2.0f) * 0.15f
                              + sinf(u * 13.1f + 3.0f) * cosf(v * 11.3f) * 0.08f;
                    float eps = 0.05f;
                    float hx = sinf((u + eps) * 3.7f) * cosf(v * 4.1f) * 0.3f
                             + sinf((u + eps) * 7.3f + 1.0f) * cosf(v * 6.7f + 2.0f) * 0.15f
                             + sinf((u + eps) * 13.1f + 3.0f) * cosf(v * 11.3f) * 0.08f;
                    float hy = sinf(u * 3.7f) * cosf((v + eps) * 4.1f) * 0.3f
                             + sinf(u * 7.3f + 1.0f) * cosf((v + eps) * 6.7f + 2.0f) * 0.15f
                             + sinf(u * 13.1f + 3.0f) * cosf((v + eps) * 11.3f) * 0.08f;
                    float dx = (hx - h00) / eps;
                    float dy = (hy - h00) / eps;
                    // Normal from heightfield: N = normalize(-dx, 1, -dy)
                    float len = sqrtf(dx * dx + 1.0f + dy * dy);
                    float nx = -dx / len * 0.5f + 0.5f;
                    float ny = 1.0f / len * 0.5f + 0.5f;
                    float nz = -dy / len * 0.5f + 0.5f;
                    size_t idx2 = static_cast<size_t>((y * NM_SIZE + x) * 3);
                    nm_data[idx2 + 0] = static_cast<unsigned char>(nx * 255.0f);
                    nm_data[idx2 + 1] = static_cast<unsigned char>(ny * 255.0f);
                    nm_data[idx2 + 2] = static_cast<unsigned char>(nz * 255.0f);
                }
            }
            TextureHandle nt = r->createTexture(NM_SIZE, NM_SIZE, 3, nm_data.data());
            if (nt != INVALID_TEXTURE) {
                normal_map_tex_.assign(r, nt);
                LOG_INF("Resources: generated normal map %dx%d", NM_SIZE, NM_SIZE);
            }
        }

        return true;
    }

    if (tier == 4) {
        // T4 shaders are GL4-only (GLSL 430), no uber variants — keep manual loading
        // PBR Island shader
        {
            std::string vs = ShaderLoader::load("gl4/island_t4.vert");
            std::string fs = ShaderLoader::load("gl4/island_t4.frag");
            if (!island_shaders_[idx].create(r, vs.c_str(), fs.c_str())) {
                LOG_ERR("Resources: failed to create PBR island shader (tier 4)");
                return false;
            }
        }
        // PBR Fur shader
        {
            std::string vs = ShaderLoader::load("gl4/fur_t4.vert");
            std::string fs = ShaderLoader::load("gl4/fur_t4.frag");
            if (!fur_shaders_[idx].create(r, vs.c_str(), fs.c_str())) {
                LOG_ERR("Resources: failed to create PBR fur shader (tier 4)");
                return false;
            }
        }
        // PBR Grass shader
        {
            std::string vs = ShaderLoader::load("gl4/grass_t4.vert");
            std::string fs = ShaderLoader::load("gl4/grass_t4.frag");
            if (!grass_shader_t4_.create(r, vs.c_str(), fs.c_str())) {
                LOG_WRN("Resources: failed to create PBR grass shader (tier 4)");
            }
        }
        // Tessellation shader
        {
            GL4Features* g4 = r->features<GL4Features>();
            if (g4 && g4->hasTessellation()) {
                std::string vs = ShaderLoader::load("gl4/tess_t4.vert");
                std::string tcs = ShaderLoader::load("gl4/tess_t4.tcs");
                std::string tes = ShaderLoader::load("gl4/tess_t4.tes");
                std::string fs = ShaderLoader::load("gl4/tess_t4.frag");
                ShaderHandle sh = g4->createTessShader(vs.c_str(), tcs.c_str(),
                                                       tes.c_str(), fs.c_str());
                if (sh != INVALID_SHADER) {
                    tess_shader_.adopt(r, sh);
                    LOG_INF("Resources: tessellation shader created");
                } else {
                    LOG_WRN("Resources: failed to create tessellation shader");
                }
            }
        }
        // Compute particle shader
        {
            ComputeFeatures* cf = r->features<ComputeFeatures>();
            if (cf && cf->hasCompute()) {
                std::string cs = ShaderLoader::load("gl4/particles_t4.comp");
                ShaderHandle sh = cf->createComputeShader(cs.c_str());
                if (sh != INVALID_SHADER) {
                    compute_particle_shader_.adopt(r, sh);
                    LOG_INF("Resources: compute particle shader created");
                } else {
                    LOG_WRN("Resources: failed to create compute particle shader");
                }
            }
        }
        // Particle render shader
        {
            std::string vs = ShaderLoader::load("gl4/particle_render_t4.vert");
            std::string fs = ShaderLoader::load("gl4/particle_render_t4.frag");
            if (!particle_render_shader_.create(r, vs.c_str(), fs.c_str())) {
                LOG_WRN("Resources: failed to create particle render shader");
            }
        }
        // Volumetric fog shader
        {
            std::string vs = ShaderLoader::load("gl4/volumetric_fog_t4.vert");
            std::string fs = ShaderLoader::load("gl4/volumetric_fog_t4.frag");
            if (!volumetric_fog_shader_.create(r, vs.c_str(), fs.c_str())) {
                LOG_WRN("Resources: failed to create volumetric fog shader");
            }
        }
        // HDR tone map composite shader
        {
            std::string vs = ShaderLoader::load("gl4/tone_map_t4.vert");
            std::string fs = ShaderLoader::load("gl4/tone_map_t4.frag");
            if (!tone_map_shader_.create(r, vs.c_str(), fs.c_str())) {
                LOG_WRN("Resources: failed to create tone map shader");
            }
        }
        return true;
    }

    LOG_WRN("Resources: tier %d shaders not implemented yet, reusing tier 1", tier);
    return false;
}

bool DemoResources::createShadowResources(Renderer* r, int shadow_size) {
    shadow_map_size_ = shadow_size;
    // Shadow depth shader via cache (T2+ feature set)
    {
        ShaderFeatureSet feat = featuresForTier(DemoTier::Enhanced, r->isCoreProfile());
        shadow_cache_ = shader_cache_.get("shadow_depth", feat, feat);
        if (!shadow_cache_) {
            LOG_WRN("Resources: failed to compile shadow_depth shader via cache");
            return false;
        }
    }

    // Shadow depth render target (size set by caller)
    RenderTargetHandle srt = r->createDepthRenderTarget(shadow_map_size_, shadow_map_size_);
    if (srt == INVALID_RENDER_TARGET) {
        LOG_WRN("Resources: failed to create shadow depth FBO");
        shadow_cache_ = nullptr;
        shadow_map_size_ = 0;
        return false;
    }
    shadow_rt_.assign(r, srt);
    shadow_depth_tex_ = r->getDepthTexture(srt);

    LOG_INF("Resources: shadow map %dx%d created", shadow_map_size_, shadow_map_size_);
    return true;
}

bool DemoResources::createBloomResources(Renderer* r, int render_w, int render_h) {
    ShaderFeatureSet feat = featuresForTier(DemoTier::Enhanced, r->isCoreProfile());

    // Bloom extract shader via cache
    bloom_extract_cache_ = shader_cache_.get("bloom_extract", feat, feat);
    if (!bloom_extract_cache_) {
        LOG_WRN("Resources: failed to compile bloom_extract shader via cache");
        return false;
    }

    // Bloom blur shader via cache
    bloom_blur_cache_ = shader_cache_.get("bloom_blur", feat, feat);
    if (!bloom_blur_cache_) {
        LOG_WRN("Resources: failed to compile bloom_blur shader via cache");
        bloom_extract_cache_ = nullptr;
        return false;
    }

    // Bloom composite shader via cache
    bloom_composite_cache_ = shader_cache_.get("bloom_composite", feat, feat);
    if (!bloom_composite_cache_) {
        LOG_WRN("Resources: failed to compile bloom_composite shader via cache");
        bloom_extract_cache_ = nullptr;
        bloom_blur_cache_ = nullptr;
        return false;
    }

    // Fullscreen quad mesh
    {
        MeshData qd = MeshGen::quad();
        MeshHandle qh = r->createMesh(qd);
        if (qh == MeshHandle()) {
            LOG_WRN("Resources: failed to create fullscreen quad mesh");
            bloom_extract_cache_ = nullptr;
            bloom_blur_cache_ = nullptr;
            bloom_composite_cache_ = nullptr;
            return false;
        }
        fullscreen_quad_.assign(r, qh);
    }

    // Scene FBO (full resolution, color + sampleable depth for SSAO)
    {
        RenderTargetHandle srt = r->createRenderTargetWithDepth(render_w, render_h);
        if (srt == INVALID_RENDER_TARGET) {
            LOG_WRN("Resources: failed to create scene FBO for bloom");
            bloom_extract_cache_ = nullptr;
            bloom_blur_cache_ = nullptr;
            bloom_composite_cache_ = nullptr;
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
            LOG_WRN("Resources: failed to create bright FBO for bloom");
            bloom_extract_cache_ = nullptr;
            bloom_blur_cache_ = nullptr;
            bloom_composite_cache_ = nullptr;
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
            LOG_WRN("Resources: failed to create blur FBO for bloom");
            bloom_extract_cache_ = nullptr;
            bloom_blur_cache_ = nullptr;
            bloom_composite_cache_ = nullptr;
            fullscreen_quad_.reset();
            scene_rt_.reset();
            bright_rt_.reset();
            return false;
        }
        blur_rt_.assign(r, blrt);
    }

    bloom_strength_ = 0.3f;

    LOG_INF("Resources: bloom FBOs created (scene %dx%d, bloom %dx%d)",
              render_w, render_h, bw, bh);
    return true;
}

bool DemoResources::createSSAOResources(Renderer* r, int render_w, int render_h) {
    ShaderFeatureSet feat = featuresForTier(DemoTier::Enhanced, r->isCoreProfile());

    // SSAO shader via cache
    ssao_cache_ = shader_cache_.get("ssao", feat, feat);
    if (!ssao_cache_) {
        LOG_WRN("Resources: failed to compile ssao shader via cache");
        return false;
    }

    // SSAO blur shader via cache
    ssao_blur_cache_ = shader_cache_.get("ssao_blur", feat, feat);
    if (!ssao_blur_cache_) {
        LOG_WRN("Resources: failed to compile ssao_blur shader via cache");
        ssao_cache_ = nullptr;
        return false;
    }

    // SSAO FBO (half resolution for performance)
    int sw = render_w / 2;
    int sh = render_h / 2;
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;

    {
        RenderTargetHandle srt = r->createRenderTarget(sw, sh);
        if (srt == INVALID_RENDER_TARGET) {
            LOG_WRN("Resources: failed to create SSAO FBO");
            ssao_cache_ = nullptr;
            ssao_blur_cache_ = nullptr;
            return false;
        }
        ssao_rt_.assign(r, srt);
    }

    {
        RenderTargetHandle brt = r->createRenderTarget(sw, sh);
        if (brt == INVALID_RENDER_TARGET) {
            LOG_WRN("Resources: failed to create SSAO blur FBO");
            ssao_cache_ = nullptr;
            ssao_blur_cache_ = nullptr;
            ssao_rt_.reset();
            return false;
        }
        ssao_blur_rt_.assign(r, brt);
    }

    // 4x4 noise texture for kernel rotation
    {
        unsigned char noise[4 * 4 * 3];
        unsigned int seed = 42;
        for (size_t i = 0; i < 4 * 4; i++) {
            seed = seed * 1103515245u + 12345u;
            noise[i * 3 + 0] = static_cast<unsigned char>((seed >> 16) & 0xFF);
            seed = seed * 1103515245u + 12345u;
            noise[i * 3 + 1] = static_cast<unsigned char>((seed >> 16) & 0xFF);
            noise[i * 3 + 2] = 0; // Z component not needed
        }
        TextureHandle nt = r->createTexture(4, 4, 3, noise);
        if (nt == INVALID_TEXTURE) {
            LOG_WRN("Resources: failed to create SSAO noise texture");
            ssao_cache_ = nullptr;
            ssao_blur_cache_ = nullptr;
            ssao_rt_.reset();
            ssao_blur_rt_.reset();
            return false;
        }
        ssao_noise_tex_.assign(r, nt);
    }

    // Get depth texture from scene FBO
    scene_depth_tex_ = r->getRTDepthTexture(scene_rt_.get());
    if (scene_depth_tex_ == INVALID_TEXTURE) {
        LOG_WRN("Resources: scene FBO has no sampleable depth texture, SSAO disabled");
        ssao_cache_ = nullptr;
        ssao_blur_cache_ = nullptr;
        ssao_rt_.reset();
        ssao_blur_rt_.reset();
        ssao_noise_tex_.reset();
        return false;
    }

    LOG_INF("Resources: SSAO created (FBO %dx%d)", sw, sh);
    return true;
}

bool DemoResources::createT4Resources(Renderer* r, int render_w, int render_h) {
    ComputeFeatures* cf = r->features<ComputeFeatures>();

    // Create particle SSBO
    if (cf && cf->hasCompute() && compute_particle_shader_) {
        compute_particle_count_ = 1024;
        int particle_size = static_cast<int>(12 * sizeof(float)); // 3 vec4s
        BufferHandle ssbo = cf->createSSBO(compute_particle_count_ * particle_size);
        if (ssbo != INVALID_BUFFER) {
            // Initialize particle data
            std::vector<float> init_data(static_cast<size_t>(compute_particle_count_) * 12);
            unsigned int seed = 12345;
            for (int i = 0; i < compute_particle_count_; i++) {
                size_t base = static_cast<size_t>(i) * 12;
                seed = seed * 1103515245u + 12345u;
                float rx = (static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f - 0.5f) * 6.0f;
                seed = seed * 1103515245u + 12345u;
                float ry = static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f * 3.0f - 1.0f;
                seed = seed * 1103515245u + 12345u;
                float rz = (static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f - 0.5f) * 6.0f;
                seed = seed * 1103515245u + 12345u;
                float lifetime = static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f * 3.0f;
                // pos.xyz, pos.w=lifetime
                init_data[base + 0] = rx;
                init_data[base + 1] = ry;
                init_data[base + 2] = rz;
                init_data[base + 3] = lifetime;
                // vel.xyz, vel.w=max_lifetime
                seed = seed * 1103515245u + 12345u;
                init_data[base + 4] = (static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f - 0.5f) * 0.5f;
                init_data[base + 5] = static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f * 1.0f + 0.5f;
                seed = seed * 1103515245u + 12345u;
                init_data[base + 6] = (static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f - 0.5f) * 0.5f;
                init_data[base + 7] = 2.0f + static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f * 2.0f;
                // color.rgba
                init_data[base + 8] = 1.0f;
                init_data[base + 9] = 0.6f;
                init_data[base + 10] = 0.2f;
                init_data[base + 11] = 1.0f;
            }
            cf->updateSSBO(ssbo, init_data.data(), compute_particle_count_ * particle_size);
            particle_ssbo_.assign(renderer_, ssbo);
            LOG_INF("Resources: compute particle SSBO created (%d particles)", compute_particle_count_);
        }
    }

    // HDR scene FBO (float, with sampleable depth)
    {
        RenderTargetHandle hrt = r->createFloatRenderTargetWithDepth(render_w, render_h);
        if (hrt != INVALID_RENDER_TARGET) {
            hdr_scene_rt_.assign(r, hrt);
            hdr_depth_tex_ = r->getRTDepthTexture(hrt);
            hdr_color_tex_ = r->getRTColorTexture(hrt);
            LOG_INF("Resources: HDR scene FBO created %dx%d", render_w, render_h);
        } else {
            LOG_WRN("Resources: failed to create HDR FBO, falling back to LDR");
        }
    }

    // HDR bright extract FBO (float, half-res)
    {
        int bw = render_w / 2, bh = render_h / 2;
        if (bw < 1) bw = 1;
        if (bh < 1) bh = 1;
        RenderTargetHandle brt = r->createFloatRenderTarget(bw, bh);
        if (brt != INVALID_RENDER_TARGET) {
            hdr_bright_rt_.assign(r, brt);
        }
    }

    // Volumetric fog FBO (float for HDR fog)
    {
        int fw = render_w / 2, fh = render_h / 2;
        if (fw < 1) fw = 1;
        if (fh < 1) fh = 1;
        RenderTargetHandle frt = r->createFloatRenderTarget(fw, fh);
        if (frt != INVALID_RENDER_TARGET) {
            fog_rt_.assign(r, frt);
            fog_w_ = fw;
            fog_h_ = fh;
            LOG_INF("Resources: HDR fog FBO created %dx%d", fw, fh);
        }
    }

    GL4Features* g4 = r->features<GL4Features>();

    // Compute GTAO resources
    if (cf && cf->hasCompute() && g4 && g4->hasImageLoadStore()) {
        std::string gtao_src = ShaderLoader::load("gl4/gtao_t4.comp");
        if (!gtao_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(gtao_src.c_str());
            if (sh != INVALID_SHADER) {
                gtao_shader_.adopt(r, sh);
                LOG_INF("Resources: GTAO compute shader created");
            }
        }
        std::string gtao_blur_src = ShaderLoader::load("gl4/gtao_blur_t4.comp");
        if (!gtao_blur_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(gtao_blur_src.c_str());
            if (sh != INVALID_SHADER) {
                gtao_blur_shader_.adopt(r, sh);
                LOG_INF("Resources: GTAO blur compute shader created");
            }
        }

        if (gtao_shader_ && gtao_blur_shader_) {
            TextureHandle ao_tex = r->createFloatTexture(render_w, render_h);
            if (ao_tex != INVALID_TEXTURE) {
                gtao_tex_.assign(r, ao_tex);
            }
            TextureHandle ao_blur = r->createFloatTexture(render_w, render_h);
            if (ao_blur != INVALID_TEXTURE) {
                gtao_blur_tex_.assign(r, ao_blur);
            }
            if (gtao_tex_ && gtao_blur_tex_) {
                LOG_INF("Resources: GTAO textures created %dx%d", render_w, render_h);
            } else {
                LOG_WRN("Resources: failed to create GTAO textures");
                gtao_shader_.reset();
                gtao_blur_shader_.reset();
                gtao_tex_.reset();
                gtao_blur_tex_.reset();
            }
        }
    }

    // Compute Bloom mip chain
    if (cf && cf->hasCompute() && g4 && g4->hasImageLoadStore()) {
        std::string bloom_down_src = ShaderLoader::load("gl4/bloom_downsample_t4.comp");
        if (!bloom_down_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(bloom_down_src.c_str());
            if (sh != INVALID_SHADER) {
                bloom_down_compute_.adopt(r, sh);
            }
        }
        std::string bloom_up_src = ShaderLoader::load("gl4/bloom_upsample_t4.comp");
        if (!bloom_up_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(bloom_up_src.c_str());
            if (sh != INVALID_SHADER) {
                bloom_up_compute_.adopt(r, sh);
            }
        }

        if (bloom_down_compute_ && bloom_up_compute_) {
            int mw = render_w / 2, mh = render_h / 2;
            bool all_ok = true;
            for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
                if (mw < 1) mw = 1;
                if (mh < 1) mh = 1;
                TextureHandle mt = r->createFloatTexture(mw, mh);
                if (mt != INVALID_TEXTURE) {
                    bloom_mips_[i].assign(r, mt);
                } else {
                    all_ok = false;
                    break;
                }
                mw /= 2;
                mh /= 2;
            }
            if (all_ok) {
                LOG_INF("Resources: compute bloom mip chain created (%d levels)", BLOOM_MIP_COUNT);
            } else {
                LOG_WRN("Resources: failed to create bloom mip chain");
                bloom_down_compute_.reset();
                bloom_up_compute_.reset();
                for (int i = 0; i < BLOOM_MIP_COUNT; i++) bloom_mips_[i].reset();
            }
        }
    }

    // Auto-Exposure (histogram + exposure SSBOs)
    if (cf && cf->hasCompute()) {
        std::string hist_src = ShaderLoader::load("gl4/histogram_t4.comp");
        if (!hist_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(hist_src.c_str());
            if (sh != INVALID_SHADER) {
                histogram_shader_.adopt(r, sh);
            }
        }
        std::string exp_src = ShaderLoader::load("gl4/exposure_t4.comp");
        if (!exp_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(exp_src.c_str());
            if (sh != INVALID_SHADER) {
                exposure_shader_.adopt(r, sh);
            }
        }

        if (histogram_shader_ && exposure_shader_) {
            // 256 bins * 4 bytes = 1024 bytes
            BufferHandle hist = cf->createSSBO(static_cast<int>(256 * sizeof(unsigned int)));
            if (hist != INVALID_BUFFER) {
                // Zero-initialize histogram
                std::vector<unsigned int> zeros(256, 0);
                cf->updateSSBO(hist, zeros.data(), static_cast<int>(256 * sizeof(unsigned int)));
                histogram_ssbo_.assign(renderer_, hist);
            }
            // Single float for exposure
            BufferHandle exp_buf = cf->createSSBO(static_cast<int>(sizeof(float)),
                                                   ComputeFeatures::SSBOUsage::CpuReadBack);
            if (exp_buf != INVALID_BUFFER) {
                float init_exp = 1.0f;
                cf->updateSSBO(exp_buf, &init_exp, static_cast<int>(sizeof(float)));
                exposure_ssbo_.assign(renderer_, exp_buf);
            }
            if (histogram_ssbo_ && exposure_ssbo_) {
                LOG_INF("Resources: auto-exposure SSBOs created");
            } else {
                LOG_WRN("Resources: failed to create exposure SSBOs");
                histogram_shader_.reset();
                exposure_shader_.reset();
            }
        }
    }

    // SSR compute shader + output texture
    if (cf && cf->hasCompute() && g4 && g4->hasImageLoadStore()) {
        std::string ssr_src = ShaderLoader::load("gl4/ssr_t4.comp");
        if (!ssr_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(ssr_src.c_str());
            if (sh != INVALID_SHADER) {
                ssr_shader_.adopt(r, sh);
                TextureHandle st = r->createFloatTexture(render_w, render_h);
                if (st != INVALID_TEXTURE) {
                    ssr_tex_.assign(r, st);
                    LOG_INF("Resources: SSR compute shader + texture created");
                }
            }
        }
    }

    // SSR snapshot textures for water fragment SSR (format must match HDR RT)
    if (g4) {
        TextureHandle col_snap = g4->createFloat16Texture(render_w, render_h);
        if (col_snap != INVALID_TEXTURE) {
            ssr_color_snapshot_.assign(r, col_snap);
        }
        TextureHandle depth_snap = g4->createDepthTexture(render_w, render_h);
        if (depth_snap != INVALID_TEXTURE) {
            ssr_depth_snapshot_.assign(r, depth_snap);
        }
        if (ssr_color_snapshot_ && ssr_depth_snapshot_) {
            LOG_INF("Resources: SSR snapshot textures created (RGBA16F + DEPTH24) %dx%d",
                    render_w, render_h);
        }
    }

    // DoF compute shader + output texture
    if (cf && cf->hasCompute() && g4 && g4->hasImageLoadStore()) {
        std::string dof_src = ShaderLoader::load("gl4/dof_t4.comp");
        if (!dof_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(dof_src.c_str());
            if (sh != INVALID_SHADER) {
                dof_shader_.adopt(r, sh);
                TextureHandle dt = r->createFloatTexture(render_w, render_h);
                if (dt != INVALID_TEXTURE) {
                    dof_tex_.assign(r, dt);
                    LOG_INF("Resources: DoF compute shader + texture created");
                }
            }
        }
    }

    // Puddle disc meshes (3 unique organic shapes)
    for (int i = 0; i < PUDDLE_COUNT; i++) {
        MeshData pd = MeshGen::disc(0.8f, 48, static_cast<unsigned int>(i));
        puddle_meshes_[i] = scene_meshes_.add(pd);
    }
    if (puddle_meshes_[0] != MeshHandle()) {
        LOG_INF("Resources: %d puddle meshes created", PUDDLE_COUNT);
    }

    return true;
}

bool DemoResources::prepare(Renderer* r, int max_tier, int render_w, int render_h) {
    if (prepared_) return true;
    renderer_ = r;

    // Reset all cache pointers
    sky_shader_from_cache_ = nullptr;
    particle_cache_ = nullptr;
    torch_cache_ = nullptr;
    shadow_cache_ = nullptr;
    bloom_extract_cache_ = nullptr;
    bloom_blur_cache_ = nullptr;
    bloom_composite_cache_ = nullptr;
    grass_cache_ = nullptr;
    grass_t3_cache_ = nullptr;
    ssao_cache_ = nullptr;
    ssao_blur_cache_ = nullptr;
    for (int i = 0; i < MAX_TIERS; i++) {
        island_cache_[i] = nullptr;
        fur_cache_[i] = nullptr;
    }

    shader_cache_.init(r);

    if (!loadSharedMeshes(r)) {
        LOG_ERR("Resources: failed to load shared meshes");
        return false;
    }

    if (!loadSharedTextures(r)) {
        LOG_ERR("Resources: failed to load shared textures");
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

    // Validate shader availability per tier — fall back if critical shaders missing
    int valid_tier = validateShaders(max_tier);
    if (valid_tier < max_tier) {
        LOG_WRN("Shader validation: falling back from tier %d to %d", max_tier, valid_tier);
        max_tier = valid_tier;
    }

    // Create shadow resources for T2+ (non-critical: T2 falls back to unshadowed)
    if (max_tier >= 2) {
        int shadow_size = (max_tier >= 4) ? 4096 : ((max_tier >= 3) ? 2048 : 1024);
        createShadowResources(r, shadow_size);
    }

    // Create bloom resources for T2+ (non-critical: T2 falls back to no bloom)
    if (max_tier >= 2 && render_w > 0 && render_h > 0) {
        createBloomResources(r, render_w, render_h);
    }

    // Create SSAO resources for T2+ (non-critical, requires bloom's scene FBO with depth tex)
    if (max_tier >= 2 && scene_rt_ && render_w > 0 && render_h > 0) {
        createSSAOResources(r, render_w, render_h);
    }

    // Create T4 resources (HDR FBO, SSBO, fog FBO)
    if (max_tier >= 4 && render_w > 0 && render_h > 0) {
        createT4Resources(r, render_w, render_h);
    }

    prepared_ = true;
    LOG_INF("Resources: all resources prepared (max_tier=%d)", max_tier);
    return true;
}

int DemoResources::validateShaders(int max_tier) {
    // Check critical shaders for each tier, top-down.
    // Returns the highest tier where all required shaders are available.
    for (int t = max_tier; t >= 1; t--) {
        int idx = t - 1;
        bool valid = true;

        // Island shader: T1-T3 use cache, T4 uses legacy owned ShaderProgram
        bool has_island = (t <= 3) ? (island_cache_[idx] != nullptr)
                                   : static_cast<bool>(island_shaders_[idx]);
        if (!has_island) {
            LOG_WRN("Shader validation: tier %d missing island shader", t);
            valid = false;
        }

        // Sky shader: per-tier cache, with fallback to sky_shader_from_cache_
        bool has_sky = (sky_cache_[idx] != nullptr);
        if (!has_sky && t == 1 && sky_shader_from_cache_)
            has_sky = true;
        if (!has_sky) {
            LOG_WRN("Shader validation: tier %d missing sky shader", t);
            valid = false;
        }

        // Fur shader: same pattern as island
        bool has_fur = (t <= 3) ? (fur_cache_[idx] != nullptr)
                                : static_cast<bool>(fur_shaders_[idx]);
        if (!has_fur) {
            LOG_WRN("Shader validation: tier %d missing fur shader", t);
            valid = false;
        }

        if (valid) {
            LOG_DBG("Shader validation: tier %d OK", t);
            return t;
        }
    }
    LOG_WRN("Shader validation: no tier fully valid, defaulting to tier 1");
    return 1;
}

TierResourceView DemoResources::viewForTier(DemoTier tier) {
    TierResourceView view;

    // Use per-tier sky shader (with domain warp / physical atmosphere), fallback to base
    {
        int si = static_cast<int>(tier) - 1;
        if (si >= 0 && si < MAX_TIERS && sky_cache_[si])
            view.core.sky_shader = sky_cache_[si];
        else
            view.core.sky_shader = sky_shader_from_cache_ ? sky_shader_from_cache_ : &sky_shader_;
    }
    view.core.model_mesh = model_mesh_;
    view.core.sky_mesh = sky_mesh_.get();
    view.core.ground_mesh = ground_mesh_;
    view.core.rock_mesh = rock_mesh_;
    view.core.grass_mesh = grass_mesh_;
    view.core.particle_mesh = particle_mesh_;
    view.core.fur_tex = fur_tex_.get();
    view.core.fur_mask_tex = fur_mask_tex_.get();
    view.core.pedestal_mesh = pedestal_mesh_;
    view.core.column_tall_mesh = column_tall_mesh_;
    view.core.column_stump_mesh = column_stump_mesh_;
    view.core.arch_mesh = arch_mesh_;
    view.core.fallen_column_mesh = fallen_column_mesh_;
    view.core.slab_mesh = slab_mesh_;
    view.core.stone_sphere_mesh = stone_sphere_mesh_;
    view.core.mossy_block_mesh = mossy_block_mesh_;
    view.core.bowl_mesh = bowl_mesh_;
    view.core.obelisk_mesh = obelisk_mesh_;
    view.core.ring_inner_mesh = ring_inner_mesh_;
    view.core.ring_outer_mesh = ring_outer_mesh_;
    view.core.pond_mesh = pond_mesh_;
    view.core.torch_mesh = torch_mesh_;
    view.core.torch_shader = torch_cache_;
    for (int i = 0; i < 3; i++) view.core.tree_meshes[i] = tree_meshes_[i];
    view.core.model_bounding_radius = model_bounding_radius_;
    view.core.particle_shader = particle_cache_ ? particle_cache_
                               : (particle_shader_ ? &particle_shader_ : nullptr);

    int idx = static_cast<int>(tier) - 1;
    if (idx < 0 || idx >= MAX_TIERS) idx = 0;

    // Use cached tier-specific shaders (T1-T3), fall back to legacy owned (T4), then tier 1
    if (island_cache_[idx]) {
        view.core.island_shader = island_cache_[idx];
    } else if (island_shaders_[idx]) {
        view.core.island_shader = &island_shaders_[idx];
    } else if (island_cache_[0]) {
        view.core.island_shader = island_cache_[0];
    } else {
        view.core.island_shader = &island_shaders_[0];
    }

    if (fur_cache_[idx]) {
        view.core.fur_shader = fur_cache_[idx];
    } else if (fur_shaders_[idx]) {
        view.core.fur_shader = &fur_shaders_[idx];
    } else if (fur_cache_[0]) {
        view.core.fur_shader = fur_cache_[0];
    } else {
        view.core.fur_shader = &fur_shaders_[0];
    }

    // T2+ shadow mapping resources (prefer cache, fallback to legacy)
    if (idx >= 1 && (shadow_cache_ || shadow_shader_) && shadow_rt_) {
        view.shadow.shader = shadow_cache_ ? shadow_cache_ : &shadow_shader_;
        view.shadow.rt = shadow_rt_.get();
        view.shadow.depth_tex = shadow_depth_tex_;
        view.shadow.map_size = shadow_map_size_;
    }

    // T2+ bloom post-processing resources (prefer cache, fallback to legacy)
    {
        ShaderProgram* bex = bloom_extract_cache_ ? bloom_extract_cache_
                           : (bloom_extract_shader_ ? &bloom_extract_shader_ : nullptr);
        ShaderProgram* bbl = bloom_blur_cache_ ? bloom_blur_cache_
                           : (bloom_blur_shader_ ? &bloom_blur_shader_ : nullptr);
        ShaderProgram* bco = bloom_composite_cache_ ? bloom_composite_cache_
                           : (bloom_composite_shader_ ? &bloom_composite_shader_ : nullptr);
        if (idx >= 1 && bex && bbl && bco && scene_rt_ && bright_rt_ && blur_rt_) {
            view.bloom.extract_shader = bex;
            view.bloom.blur_shader = bbl;
            view.bloom.composite_shader = bco;
            view.bloom.fullscreen_quad = fullscreen_quad_.get();
            view.bloom.scene_rt = scene_rt_.get();
            view.bloom.bright_rt = bright_rt_.get();
            view.bloom.blur_rt = blur_rt_.get();
            view.bloom.strength = bloom_strength_;
        }
    }

    // T2+ instanced grass resources (prefer cache, fallback to legacy)
    if (idx >= 1) {
        if (idx >= 2 && (grass_t3_cache_ || grass_shader_t3_)) {
            view.grass.shader = grass_t3_cache_ ? grass_t3_cache_ : &grass_shader_t3_;
        } else if (grass_cache_ || grass_shader_) {
            view.grass.shader = grass_cache_ ? grass_cache_ : &grass_shader_;
        }
        view.grass.blade_mesh = grass_blade_mesh_;
    }

    // T3+ normal map texture
    if (idx >= 2 && normal_map_tex_) {
        view.normal_map_tex = normal_map_tex_.get();
    }

    // T2+ SSAO resources (prefer cache, fallback to legacy)
    {
        ShaderProgram* ssao_prog = ssao_cache_ ? ssao_cache_
                                 : (ssao_shader_ ? &ssao_shader_ : nullptr);
        ShaderProgram* ssao_blur_prog = ssao_blur_cache_ ? ssao_blur_cache_
                                      : (ssao_blur_shader_ ? &ssao_blur_shader_ : nullptr);
        if (idx >= 1 && ssao_prog && ssao_blur_prog && ssao_rt_ && ssao_blur_rt_) {
            view.ssao.shader = ssao_prog;
            view.ssao.blur_shader = ssao_blur_prog;
            view.ssao.rt = ssao_rt_.get();
            view.ssao.blur_rt = ssao_blur_rt_.get();
            view.ssao.noise_tex = ssao_noise_tex_.get();
            view.ssao.scene_depth_tex = scene_depth_tex_;
        }
    }

    // T4+ resources
    if (idx >= 3) {
        // Use T4 grass shader if available
        if (grass_shader_t4_) {
            view.grass.shader = &grass_shader_t4_;
        }
        if (tess_shader_) view.t4.tess_shader = &tess_shader_;
        if (compute_particle_shader_) view.t4.compute_particle_shader = &compute_particle_shader_;
        if (particle_render_shader_) view.t4.particle_render_shader = &particle_render_shader_;
        if (volumetric_fog_shader_) view.t4.volumetric_fog_shader = &volumetric_fog_shader_;
        if (tone_map_shader_) view.t4.tone_map_shader = &tone_map_shader_;
        view.t4.particle_ssbo = particle_ssbo_.get();
        view.t4.compute_particle_count = compute_particle_count_;
        if (hdr_scene_rt_) {
            view.t4.hdr_scene_rt = hdr_scene_rt_.get();
            view.t4.hdr_depth_tex = hdr_depth_tex_;
            view.t4.hdr_color_tex = hdr_color_tex_;
            // Override scene_depth_tex for SSAO to use HDR depth
            view.ssao.scene_depth_tex = hdr_depth_tex_;
        }
        if (hdr_bright_rt_) view.t4.hdr_bright_rt = hdr_bright_rt_.get();
        if (fog_rt_) {
            view.t4.fog_rt = fog_rt_.get();
            view.t4.fog_w = fog_w_;
            view.t4.fog_h = fog_h_;
        }

        // T4 Ultra: GTAO
        if (gtao_shader_) view.t4.gtao_shader = &gtao_shader_;
        if (gtao_blur_shader_) view.t4.gtao_blur_shader = &gtao_blur_shader_;
        if (gtao_tex_) view.t4.gtao_tex = gtao_tex_.get();
        if (gtao_blur_tex_) view.t4.gtao_blur_tex = gtao_blur_tex_.get();

        // T4 Ultra: Compute Bloom
        if (bloom_down_compute_) view.t4.bloom_down_compute = &bloom_down_compute_;
        if (bloom_up_compute_) view.t4.bloom_up_compute = &bloom_up_compute_;
        for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
            if (bloom_mips_[i]) view.t4.bloom_mips[i] = bloom_mips_[i].get();
        }

        // T4 Ultra: Auto-Exposure
        if (histogram_shader_) view.t4.histogram_shader = &histogram_shader_;
        if (exposure_shader_) view.t4.exposure_shader = &exposure_shader_;
        view.t4.histogram_ssbo = histogram_ssbo_.get();
        view.t4.exposure_ssbo = exposure_ssbo_.get();

        // T4 Ultra: SSR
        if (ssr_shader_) view.t4.ssr_shader = &ssr_shader_;
        if (ssr_tex_) view.t4.ssr_tex = ssr_tex_.get();
        if (ssr_color_snapshot_) view.t4.ssr_color_snapshot = ssr_color_snapshot_.get();
        if (ssr_depth_snapshot_) view.t4.ssr_depth_snapshot = ssr_depth_snapshot_.get();

        // T4 Ultra: DoF
        if (dof_shader_) view.t4.dof_shader = &dof_shader_;
        if (dof_tex_) view.t4.dof_tex = dof_tex_.get();

        // T4 Ultra: Puddles
        for (int i = 0; i < PUDDLE_COUNT; i++)
            view.t4.puddle_meshes[i] = puddle_meshes_[i];
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
    sky_shader_from_cache_ = nullptr;
    for (int i = 0; i < MAX_TIERS; i++) sky_cache_[i] = nullptr;

    // Null out all cache pointers BEFORE destroying the cache (which frees the programs)
    for (int i = 0; i < MAX_TIERS; i++) {
        island_cache_[i] = nullptr;
        fur_cache_[i] = nullptr;
    }
    particle_cache_ = nullptr;
    torch_cache_ = nullptr;
    shadow_cache_ = nullptr;
    bloom_extract_cache_ = nullptr;
    bloom_blur_cache_ = nullptr;
    bloom_composite_cache_ = nullptr;
    grass_cache_ = nullptr;
    grass_t3_cache_ = nullptr;
    ssao_cache_ = nullptr;
    ssao_blur_cache_ = nullptr;

    shader_cache_.destroy();

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
    grass_shader_t3_.reset();

    // Normal map
    normal_map_tex_.reset();

    // T4 resources
    grass_shader_t4_.reset();
    tess_shader_.reset();
    compute_particle_shader_.reset();
    particle_render_shader_.reset();
    volumetric_fog_shader_.reset();
    tone_map_shader_.reset();
    particle_ssbo_.reset();
    compute_particle_count_ = 0;
    hdr_scene_rt_.reset();
    hdr_bright_rt_.reset();
    hdr_depth_tex_ = TextureHandle();
    fog_rt_.reset();
    fog_w_ = 0;
    fog_h_ = 0;

    // T4 Ultra resources
    gtao_shader_.reset();
    gtao_blur_shader_.reset();
    gtao_tex_.reset();
    gtao_blur_tex_.reset();
    bloom_down_compute_.reset();
    bloom_up_compute_.reset();
    for (int i = 0; i < BLOOM_MIP_COUNT; i++) bloom_mips_[i].reset();
    histogram_shader_.reset();
    exposure_shader_.reset();
    histogram_ssbo_.reset();
    exposure_ssbo_.reset();
    ssr_shader_.reset();
    ssr_tex_.reset();
    dof_shader_.reset();
    dof_tex_.reset();

    // SSAO resources
    ssao_shader_.reset();
    ssao_blur_shader_.reset();
    ssao_rt_.reset();
    ssao_blur_rt_.reset();
    ssao_noise_tex_.reset();
    scene_depth_tex_ = TextureHandle();

    prepared_ = false;
    renderer_ = nullptr;
    LOG_INF("Resources: destroyed");
}
