#include "demo/scene/demo_resources.h"
#include "demo/scene/demo_scene.h"
#include "demo/tier/shader_loader.h"
#include "demo/tier/shader_feature.h"
#include "renderer/features.h"
#include "demo/scene/fur_texture.h"
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
{
}

DemoResources::~DemoResources() {
    destroy();
}

bool DemoResources::loadSharedMeshes(Renderer* r) {
    scene_meshes_.init(r);

    // Sky mesh (vertex shader scales ×500 and uses pos.xyww trick)
    {
        MeshData sd = MeshGen::sphere(32, 16);
        core_.sky_mesh.assign(r, r->createMesh(sd));
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
        core_.model_bounding_radius = MeshGen::boundingRadius(md);
        core_.model_mesh = scene_meshes_.add(md);
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
        core_.ground_mesh = scene_meshes_.add(gd);
    }

    // Scattered rocks
    {
        MeshData rocks = MeshGen::scatteredRocks(25, 8.0f, 0.1f, 0.3f, 137u);
        // Apply terrain heightmap to rock positions
        for (size_t i = 0; i < rocks.vertices.size(); i++) {
            rocks.vertices[i].pos.y += terrainHeightRaw(rocks.vertices[i].pos.x, rocks.vertices[i].pos.z);
        }
        core_.rock_mesh = scene_meshes_.add(rocks);
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
        core_.grass_mesh = scene_meshes_.add(grass);
        LOG_INF("Resources: generated %d grass blades (%d verts)",
                  800, static_cast<int>(grass.vertices.size()));
    }

    // Billboard particles
    {
        MeshData particles = MeshGen::particleQuads(200, 6.0f, 2.0f, 317u);
        core_.particle_mesh = scene_meshes_.add(particles);
        LOG_INF("Resources: generated %d particle quads (%d verts)",
                  200, static_cast<int>(particles.vertices.size()));
    }

    // Grass blade template (for T2+ instancing)
    {
        MeshData blade = MeshGen::grassBlade();
        grass_.blade_mesh = scene_meshes_.add(blade);
        LOG_INF("Resources: generated grass blade template (%d verts)",
                  static_cast<int>(blade.vertices.size()));
    }

    // Pedestal: hexagonal frustum (larger, prominent base for bunny)
    {
        MeshData md = MeshGen::frustum(6, 1.1f, 1.4f, 1.1f);
        sanctuary_.pedestal = scene_meshes_.add(md);
    }

    // Tall column: thick cylinder + base ring + capital (top ring)
    {
        MeshData md = MeshGen::cylinder(16, 3.0f, 0.30f);
        MeshData ring = MeshGen::torus(16, 8, 0.30f, 0.08f);
        MeshGen::appendMesh(md, ring, Mat4());  // ring at center (sinks into ground)
        // Capital at column top: wider torus to bridge column (r=0.30) → arch tube (r=0.25)
        MeshData capital = MeshGen::torus(16, 8, 0.30f, 0.12f);
        MeshGen::appendMesh(md, capital, Mat4::translate(0.0f, 1.5f, 0.0f));  // at cylinder top
        sanctuary_.column_tall = scene_meshes_.add(md);
    }

    // Column stump: shorter but thick cylinder + torus base
    {
        MeshData md = MeshGen::cylinder(16, 1.2f, 0.30f);
        MeshData ring = MeshGen::torus(16, 8, 0.30f, 0.08f);
        MeshGen::appendMesh(md, ring, Mat4());
        sanctuary_.column_stump = scene_meshes_.add(md);
    }

    // Arch: massive half torus (proper stone arch)
    {
        MeshData md = MeshGen::halfTorus(32, 12, 1.8f, 0.25f);
        sanctuary_.arch = scene_meshes_.add(md);
    }

    // Fallen column segment (thick, lying on ground)
    {
        MeshData md = MeshGen::cylinder(16, 2.2f, 0.28f);
        sanctuary_.fallen_column = scene_meshes_.add(md);
    }

    // Stone slab (cube will be scaled in scene)
    {
        MeshData md = MeshGen::cube();
        sanctuary_.slab = scene_meshes_.add(md);
    }

    // Stone sphere (low-poly for carved stone look)
    {
        MeshData md = MeshGen::sphere(16, 10);
        sanctuary_.stone_sphere = scene_meshes_.add(md);
    }

    // Mossy block (cube, will be scaled)
    {
        MeshData md = MeshGen::cube();
        sanctuary_.mossy_block = scene_meshes_.add(md);
    }

    // Offering bowl (larger, visible)
    {
        MeshData md = MeshGen::torus(16, 10, 0.30f, 0.10f);
        sanctuary_.bowl = scene_meshes_.add(md);
    }

    // Obelisk (taller, thicker)
    {
        MeshData md = MeshGen::frustum(8, 2.5f, 0.15f, 0.05f);
        sanctuary_.obelisk = scene_meshes_.add(md);
    }

    // Step rings (thicker tori)
    {
        MeshData md = MeshGen::torus(32, 6, 1.5f, 0.08f);
        sanctuary_.ring_inner = scene_meshes_.add(md);
    }
    {
        MeshData md = MeshGen::torus(32, 6, 2.2f, 0.08f);
        sanctuary_.ring_outer = scene_meshes_.add(md);
    }

    // Pond (large disc, replaces 3 small puddles)
    {
        MeshData md = MeshGen::disc(2.5f, 64, 0);
        sanctuary_.pond = scene_meshes_.add(md);
    }

    // Torch billboard quads (3 quads for 3 point lights)
    {
        MeshData md = MeshGen::torchQuads(2);
        sanctuary_.torch = scene_meshes_.add(md);
    }

    // Procedural tree meshes: 3 variants with different seeds for variety
    {
        unsigned int seeds[] = { 42, 137, 293 };
        for (int i = 0; i < 3; i++) {
            MeshData md = MeshGen::proceduralTree(3.0f, 0.15f, 1.2f, 10, seeds[i]);
            sanctuary_.trees[i] = scene_meshes_.add(md);
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
        core_.fur_tex.assign(r, r->createTexture(fur_tex_size, fur_tex_size, 4, fur_pixels.data()));
        LOG_INF("Resources: generated fur texture %dx%d", fur_tex_size, fur_tex_size);
    }

    // Fur mask (fur.png)
    {
        std::string mask_path = getDataPath("models/fur.png");
        if (!mask_path.empty()) {
            std::string img_data = readTextFile(mask_path.c_str());
            if (!img_data.empty()) {
                int mw = 0, mh = 0, mc = 0;
                stbi_set_flip_vertically_on_load(1);
                unsigned char* pixels = stbi_load_from_memory(
                    reinterpret_cast<const unsigned char*>(img_data.data()),
                    static_cast<int>(img_data.size()), &mw, &mh, &mc, 4);
                if (pixels && mw > 0 && mh > 0) {
                    core_.fur_mask_tex.assign(r, r->createTexture(mw, mh, 4, pixels));
                    LOG_INF("Resources: loaded fur mask %dx%d from %s", mw, mh, mask_path.c_str());
                }
                if (pixels) stbi_image_free(pixels);
            }
        }
    }

    return true;
}

bool DemoResources::compileSkyShader(Renderer* r) {
    ShaderFeatureSet feat;
    if (r->isGLES())
        feat = r->isGLES3() ? SF_GLES_300 : SF_GLES_100;
    else
        feat = r->isCoreProfile() ? SF_GLSL_150 : SF_GLSL_120;
    core_.sky_shader_from_cache = shader_cache_.get("sky", feat, feat);
    if (!core_.sky_shader_from_cache) {
        LOG_ERR("Resources: failed to create sky shader");
        return false;
    }
    return true;
}

bool DemoResources::compileTierShaders(Renderer* r, int tier) {
    int idx = tier - 1;  // tier 1..4 -> index 0..3
    if (idx < 0 || idx >= CoreRes::MAX_TIERS) return false;

    ShaderFeatureSet feat = featuresForTier(static_cast<DemoTier>(tier), r->isCoreProfile(), r->isGLES(), r->isGLES3());

    // Sky shader per-tier (domain warp T2+, physical atmosphere T3+)
    core_.sky_cache[idx] = shader_cache_.get("sky", feat, feat);

    if (tier == 1) {
        // Island T1 via cache
        core_.island_cache[idx] = shader_cache_.get("island", feat, feat);
        if (!core_.island_cache[idx]) {
            LOG_ERR("Resources: failed to compile island shader via cache (tier 1)");
            return false;
        }

        // Fur T1 via cache
        core_.fur_cache[idx] = shader_cache_.get("fur", feat, feat);
        if (!core_.fur_cache[idx]) {
            LOG_ERR("Resources: failed to compile fur shader via cache");
            return false;
        }

        // Particle via cache
        core_.particle_cache = shader_cache_.get("particle", feat, feat);
        if (!core_.particle_cache) {
            LOG_WRN("Resources: failed to compile particle shader via cache (non-critical)");
        }

        return true;
    }

    if (tier == 2) {
        // Island T2 via cache
        core_.island_cache[idx] = shader_cache_.get("island", feat, feat);
        if (!core_.island_cache[idx]) {
            LOG_ERR("Resources: failed to compile island shader via cache (tier 2)");
            return false;
        }

        // Fur T2 via cache (unified uber shader, features drive #ifdefs)
        core_.fur_cache[idx] = shader_cache_.get("fur", feat, feat);
        if (!core_.fur_cache[idx]) {
            LOG_ERR("Resources: failed to compile fur shader via cache (tier 2)");
            return false;
        }

        // Grass T2 via cache (unified uber shader)
        grass_.cache = shader_cache_.get("grass", feat, feat);
        if (!grass_.cache) {
            LOG_WRN("Resources: failed to compile grass shader via cache (non-critical)");
        }

        return true;
    }

    if (tier == 3) {
        // Island T3 via cache
        core_.island_cache[idx] = shader_cache_.get("island", feat, feat);
        if (!core_.island_cache[idx]) {
            LOG_ERR("Resources: failed to compile island shader via cache (tier 3)");
            return false;
        }

        // Fur T3 via cache (unified uber shader, features drive #ifdefs)
        core_.fur_cache[idx] = shader_cache_.get("fur", feat, feat);
        if (!core_.fur_cache[idx]) {
            LOG_ERR("Resources: failed to compile fur shader via cache (tier 3)");
            return false;
        }

        // Grass T3 via cache (unified uber shader)
        grass_.t3_cache = shader_cache_.get("grass", feat, feat);
        if (!grass_.t3_cache) {
            LOG_WRN("Resources: failed to compile grass shader via cache (non-critical)");
        }

        // Torch T3+ via cache (point light flame billboards)
        core_.torch_cache = shader_cache_.get("torch", feat, feat);
        if (!core_.torch_cache) {
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
            if (!core_.island_shaders[idx].create(r, vs.c_str(), fs.c_str())) {
                LOG_ERR("Resources: failed to create PBR island shader (tier 4)");
                return false;
            }
        }
        // PBR Fur shader
        {
            std::string vs = ShaderLoader::load("gl4/fur_t4.vert");
            std::string fs = ShaderLoader::load("gl4/fur_t4.frag");
            if (!core_.fur_shaders[idx].create(r, vs.c_str(), fs.c_str())) {
                LOG_ERR("Resources: failed to create PBR fur shader (tier 4)");
                return false;
            }
        }
        // PBR Grass shader
        {
            std::string vs = ShaderLoader::load("gl4/grass_t4.vert");
            std::string fs = ShaderLoader::load("gl4/grass_t4.frag");
            if (!t4_.grass_shader_t4.create(r, vs.c_str(), fs.c_str())) {
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
                    t4_.tess_shader.adopt(r, sh);
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
                    t4_.compute_particle_shader.adopt(r, sh);
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
            if (!t4_.particle_render_shader.create(r, vs.c_str(), fs.c_str())) {
                LOG_WRN("Resources: failed to create particle render shader");
            }
        }
        // Volumetric fog shader
        {
            std::string vs = ShaderLoader::load("gl4/volumetric_fog_t4.vert");
            std::string fs = ShaderLoader::load("gl4/volumetric_fog_t4.frag");
            if (!t4_.volumetric_fog_shader.create(r, vs.c_str(), fs.c_str())) {
                LOG_WRN("Resources: failed to create volumetric fog shader");
            }
        }
        // HDR tone map composite shader
        {
            std::string vs = ShaderLoader::load("gl4/tone_map_t4.vert");
            std::string fs = ShaderLoader::load("gl4/tone_map_t4.frag");
            if (!t4_.tone_map_shader.create(r, vs.c_str(), fs.c_str())) {
                LOG_WRN("Resources: failed to create tone map shader");
            }
        }
        return true;
    }

    LOG_WRN("Resources: tier %d shaders not implemented yet, reusing tier 1", tier);
    return false;
}

bool DemoResources::createShadowResources(Renderer* r, int shadow_size) {
    shadow_.map_size = shadow_size;
    // Shadow depth shader via cache (T2+ feature set)
    {
        ShaderFeatureSet feat = featuresForTier(DemoTier::Enhanced, r->isCoreProfile(), r->isGLES(), r->isGLES3());
        shadow_.cache = shader_cache_.get("shadow_depth", feat, feat);
        if (!shadow_.cache) {
            LOG_WRN("Resources: failed to compile shadow_depth shader via cache");
            return false;
        }
    }

    // Shadow depth render target (size set by caller)
    RenderTargetHandle srt = r->createDepthRenderTarget(shadow_.map_size, shadow_.map_size);
    if (srt == INVALID_RENDER_TARGET) {
        LOG_WRN("Resources: failed to create shadow depth FBO");
        shadow_.cache = nullptr;
        shadow_.map_size = 0;
        return false;
    }
    shadow_.rt.assign(r, srt);
    shadow_.depth_tex = r->getDepthTexture(srt);

    LOG_INF("Resources: shadow map %dx%d created", shadow_.map_size, shadow_.map_size);
    return true;
}

bool DemoResources::createBloomResources(Renderer* r, int render_w, int render_h) {
    ShaderFeatureSet feat = featuresForTier(DemoTier::Enhanced, r->isCoreProfile(), r->isGLES(), r->isGLES3());

    // Bloom extract shader via cache
    bloom_.extract_cache = shader_cache_.get("bloom_extract", feat, feat);
    if (!bloom_.extract_cache) {
        LOG_WRN("Resources: failed to compile bloom_extract shader via cache");
        return false;
    }

    // Bloom blur shader via cache
    bloom_.blur_cache = shader_cache_.get("bloom_blur", feat, feat);
    if (!bloom_.blur_cache) {
        LOG_WRN("Resources: failed to compile bloom_blur shader via cache");
        bloom_.extract_cache = nullptr;
        return false;
    }

    // Bloom composite shader via cache
    bloom_.composite_cache = shader_cache_.get("bloom_composite", feat, feat);
    if (!bloom_.composite_cache) {
        LOG_WRN("Resources: failed to compile bloom_composite shader via cache");
        bloom_.extract_cache = nullptr;
        bloom_.blur_cache = nullptr;
        return false;
    }

    // Fullscreen quad mesh
    {
        MeshData qd = MeshGen::quad();
        MeshHandle qh = r->createMesh(qd);
        if (qh == MeshHandle()) {
            LOG_WRN("Resources: failed to create fullscreen quad mesh");
            bloom_.extract_cache = nullptr;
            bloom_.blur_cache = nullptr;
            bloom_.composite_cache = nullptr;
            return false;
        }
        bloom_.fullscreen_quad.assign(r, qh);
    }

    // Scene FBO (full resolution, color + sampleable depth for SSAO)
    {
        RenderTargetHandle srt = r->createRenderTargetWithDepth(render_w, render_h);
        if (srt == INVALID_RENDER_TARGET) {
            LOG_WRN("Resources: failed to create scene FBO for bloom");
            bloom_.extract_cache = nullptr;
            bloom_.blur_cache = nullptr;
            bloom_.composite_cache = nullptr;
            bloom_.fullscreen_quad.reset();
            return false;
        }
        bloom_.scene_rt.assign(r, srt);
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
            bloom_.extract_cache = nullptr;
            bloom_.blur_cache = nullptr;
            bloom_.composite_cache = nullptr;
            bloom_.fullscreen_quad.reset();
            bloom_.scene_rt.reset();
            return false;
        }
        bloom_.bright_rt.assign(r, brt);
    }

    // Blur FBO (half resolution, color only)
    {
        RenderTargetHandle blrt = r->createRenderTarget(bw, bh);
        if (blrt == INVALID_RENDER_TARGET) {
            LOG_WRN("Resources: failed to create blur FBO for bloom");
            bloom_.extract_cache = nullptr;
            bloom_.blur_cache = nullptr;
            bloom_.composite_cache = nullptr;
            bloom_.fullscreen_quad.reset();
            bloom_.scene_rt.reset();
            bloom_.bright_rt.reset();
            return false;
        }
        bloom_.blur_rt.assign(r, blrt);
    }

    bloom_.strength = 0.3f;

    LOG_INF("Resources: bloom FBOs created (scene %dx%d, bloom %dx%d)",
              render_w, render_h, bw, bh);
    return true;
}

bool DemoResources::createSSAOResources(Renderer* r, int render_w, int render_h) {
    ShaderFeatureSet feat = featuresForTier(DemoTier::Enhanced, r->isCoreProfile(), r->isGLES(), r->isGLES3());

    // SSAO shader via cache
    ssao_.cache = shader_cache_.get("ssao", feat, feat);
    if (!ssao_.cache) {
        LOG_WRN("Resources: failed to compile ssao shader via cache");
        return false;
    }

    // SSAO blur shader via cache
    ssao_.blur_cache = shader_cache_.get("ssao_blur", feat, feat);
    if (!ssao_.blur_cache) {
        LOG_WRN("Resources: failed to compile ssao_blur shader via cache");
        ssao_.cache = nullptr;
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
            ssao_.cache = nullptr;
            ssao_.blur_cache = nullptr;
            return false;
        }
        ssao_.rt.assign(r, srt);
    }

    {
        RenderTargetHandle brt = r->createRenderTarget(sw, sh);
        if (brt == INVALID_RENDER_TARGET) {
            LOG_WRN("Resources: failed to create SSAO blur FBO");
            ssao_.cache = nullptr;
            ssao_.blur_cache = nullptr;
            ssao_.rt.reset();
            return false;
        }
        ssao_.blur_rt.assign(r, brt);
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
            ssao_.cache = nullptr;
            ssao_.blur_cache = nullptr;
            ssao_.rt.reset();
            ssao_.blur_rt.reset();
            return false;
        }
        ssao_.noise_tex.assign(r, nt);
    }

    // Get depth texture from scene FBO
    ssao_.scene_depth_tex = r->getRTDepthTexture(bloom_.scene_rt.get());
    if (ssao_.scene_depth_tex == INVALID_TEXTURE) {
        LOG_WRN("Resources: scene FBO has no sampleable depth texture, SSAO disabled");
        ssao_.cache = nullptr;
        ssao_.blur_cache = nullptr;
        ssao_.rt.reset();
        ssao_.blur_rt.reset();
        ssao_.noise_tex.reset();
        return false;
    }

    LOG_INF("Resources: SSAO created (FBO %dx%d)", sw, sh);
    return true;
}

bool DemoResources::createT4Resources(Renderer* r, int render_w, int render_h) {
    ComputeFeatures* cf = r->features<ComputeFeatures>();

    // Create particle SSBO
    if (cf && cf->hasCompute() && t4_.compute_particle_shader) {
        t4_.compute_particle_count = 1024;
        int particle_size = static_cast<int>(12 * sizeof(float)); // 3 vec4s
        BufferHandle ssbo = cf->createSSBO(t4_.compute_particle_count * particle_size);
        if (ssbo != INVALID_BUFFER) {
            // Initialize particle data
            std::vector<float> init_data(static_cast<size_t>(t4_.compute_particle_count) * 12);
            unsigned int seed = 12345;
            for (int i = 0; i < t4_.compute_particle_count; i++) {
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
            cf->updateSSBO(ssbo, init_data.data(), t4_.compute_particle_count * particle_size);
            t4_.particle_ssbo.assign(renderer_, ssbo);
            LOG_INF("Resources: compute particle SSBO created (%d particles)", t4_.compute_particle_count);
        }
    }

    // HDR scene FBO (float, with sampleable depth)
    {
        RenderTargetHandle hrt = r->createFloatRenderTargetWithDepth(render_w, render_h);
        if (hrt != INVALID_RENDER_TARGET) {
            t4_.hdr_scene_rt.assign(r, hrt);
            t4_.hdr_depth_tex = r->getRTDepthTexture(hrt);
            t4_.hdr_color_tex = r->getRTColorTexture(hrt);
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
            t4_.hdr_bright_rt.assign(r, brt);
        }
    }

    // Volumetric fog FBO (float for HDR fog)
    {
        int fw = render_w / 2, fh = render_h / 2;
        if (fw < 1) fw = 1;
        if (fh < 1) fh = 1;
        RenderTargetHandle frt = r->createFloatRenderTarget(fw, fh);
        if (frt != INVALID_RENDER_TARGET) {
            t4_.fog_rt.assign(r, frt);
            t4_.fog_w = fw;
            t4_.fog_h = fh;
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
                t4_.gtao_shader.adopt(r, sh);
                LOG_INF("Resources: GTAO compute shader created");
            }
        }
        std::string gtao_blur_src = ShaderLoader::load("gl4/gtao_blur_t4.comp");
        if (!gtao_blur_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(gtao_blur_src.c_str());
            if (sh != INVALID_SHADER) {
                t4_.gtao_blur_shader.adopt(r, sh);
                LOG_INF("Resources: GTAO blur compute shader created");
            }
        }

        if (t4_.gtao_shader && t4_.gtao_blur_shader) {
            TextureHandle ao_tex = r->createFloatTexture(render_w, render_h);
            if (ao_tex != INVALID_TEXTURE) {
                t4_.gtao_tex.assign(r, ao_tex);
            }
            TextureHandle ao_blur = r->createFloatTexture(render_w, render_h);
            if (ao_blur != INVALID_TEXTURE) {
                t4_.gtao_blur_tex.assign(r, ao_blur);
            }
            if (t4_.gtao_tex && t4_.gtao_blur_tex) {
                LOG_INF("Resources: GTAO textures created %dx%d", render_w, render_h);
            } else {
                LOG_WRN("Resources: failed to create GTAO textures");
                t4_.gtao_shader.reset();
                t4_.gtao_blur_shader.reset();
                t4_.gtao_tex.reset();
                t4_.gtao_blur_tex.reset();
            }
        }
    }

    // Compute Bloom mip chain
    if (cf && cf->hasCompute() && g4 && g4->hasImageLoadStore()) {
        std::string bloom_down_src = ShaderLoader::load("gl4/bloom_downsample_t4.comp");
        if (!bloom_down_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(bloom_down_src.c_str());
            if (sh != INVALID_SHADER) {
                t4_.bloom_down_compute.adopt(r, sh);
            }
        }
        std::string bloom_up_src = ShaderLoader::load("gl4/bloom_upsample_t4.comp");
        if (!bloom_up_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(bloom_up_src.c_str());
            if (sh != INVALID_SHADER) {
                t4_.bloom_up_compute.adopt(r, sh);
            }
        }

        if (t4_.bloom_down_compute && t4_.bloom_up_compute) {
            int mw = render_w / 2, mh = render_h / 2;
            bool all_ok = true;
            for (int i = 0; i < T4Res::BLOOM_MIP_COUNT; i++) {
                if (mw < 1) mw = 1;
                if (mh < 1) mh = 1;
                TextureHandle mt = r->createFloatTexture(mw, mh);
                if (mt != INVALID_TEXTURE) {
                    t4_.bloom_mips[i].assign(r, mt);
                } else {
                    all_ok = false;
                    break;
                }
                mw /= 2;
                mh /= 2;
            }
            if (all_ok) {
                LOG_INF("Resources: compute bloom mip chain created (%d levels)", T4Res::BLOOM_MIP_COUNT);
            } else {
                LOG_WRN("Resources: failed to create bloom mip chain");
                t4_.bloom_down_compute.reset();
                t4_.bloom_up_compute.reset();
                for (int i = 0; i < T4Res::BLOOM_MIP_COUNT; i++) t4_.bloom_mips[i].reset();
            }
        }
    }

    // Auto-Exposure (histogram + exposure SSBOs)
    if (cf && cf->hasCompute()) {
        std::string hist_src = ShaderLoader::load("gl4/histogram_t4.comp");
        if (!hist_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(hist_src.c_str());
            if (sh != INVALID_SHADER) {
                t4_.histogram_shader.adopt(r, sh);
            }
        }
        std::string exp_src = ShaderLoader::load("gl4/exposure_t4.comp");
        if (!exp_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(exp_src.c_str());
            if (sh != INVALID_SHADER) {
                t4_.exposure_shader.adopt(r, sh);
            }
        }

        if (t4_.histogram_shader && t4_.exposure_shader) {
            // 256 bins * 4 bytes = 1024 bytes
            BufferHandle hist = cf->createSSBO(static_cast<int>(256 * sizeof(unsigned int)));
            if (hist != INVALID_BUFFER) {
                // Zero-initialize histogram
                std::vector<unsigned int> zeros(256, 0);
                cf->updateSSBO(hist, zeros.data(), static_cast<int>(256 * sizeof(unsigned int)));
                t4_.histogram_ssbo.assign(renderer_, hist);
            }
            // Single float for exposure
            BufferHandle exp_buf = cf->createSSBO(static_cast<int>(sizeof(float)),
                                                   ComputeFeatures::SSBOUsage::CpuReadBack);
            if (exp_buf != INVALID_BUFFER) {
                float init_exp = 1.0f;
                cf->updateSSBO(exp_buf, &init_exp, static_cast<int>(sizeof(float)));
                t4_.exposure_ssbo.assign(renderer_, exp_buf);
            }
            if (t4_.histogram_ssbo && t4_.exposure_ssbo) {
                LOG_INF("Resources: auto-exposure SSBOs created");
            } else {
                LOG_WRN("Resources: failed to create exposure SSBOs");
                t4_.histogram_shader.reset();
                t4_.exposure_shader.reset();
            }
        }
    }

    // SSR compute shader + output texture
    if (cf && cf->hasCompute() && g4 && g4->hasImageLoadStore()) {
        std::string ssr_src = ShaderLoader::load("gl4/ssr_t4.comp");
        if (!ssr_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(ssr_src.c_str());
            if (sh != INVALID_SHADER) {
                t4_.ssr_shader.adopt(r, sh);
                TextureHandle st = r->createFloatTexture(render_w, render_h);
                if (st != INVALID_TEXTURE) {
                    t4_.ssr_tex.assign(r, st);
                    LOG_INF("Resources: SSR compute shader + texture created");
                }
            }
        }
    }

    // SSR snapshot textures for water fragment SSR (format must match HDR RT)
    if (g4) {
        TextureHandle col_snap = g4->createFloat16Texture(render_w, render_h);
        if (col_snap != INVALID_TEXTURE) {
            t4_.ssr_color_snapshot.assign(r, col_snap);
        }
        TextureHandle depth_snap = g4->createDepthTexture(render_w, render_h);
        if (depth_snap != INVALID_TEXTURE) {
            t4_.ssr_depth_snapshot.assign(r, depth_snap);
        }
        if (t4_.ssr_color_snapshot && t4_.ssr_depth_snapshot) {
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
                t4_.dof_shader.adopt(r, sh);
                TextureHandle dt = r->createFloatTexture(render_w, render_h);
                if (dt != INVALID_TEXTURE) {
                    t4_.dof_tex.assign(r, dt);
                    LOG_INF("Resources: DoF compute shader + texture created");
                }
            }
        }
    }

    // Puddle disc meshes (3 unique organic shapes)
    for (int i = 0; i < T4Res::PUDDLE_COUNT; i++) {
        MeshData pd = MeshGen::disc(0.8f, 48, static_cast<unsigned int>(i));
        t4_.puddle_meshes[i] = scene_meshes_.add(pd);
    }
    if (t4_.puddle_meshes[0] != MeshHandle()) {
        LOG_INF("Resources: %d puddle meshes created", T4Res::PUDDLE_COUNT);
    }

    return true;
}

bool DemoResources::prepare(Renderer* r, int max_tier, int render_w, int render_h) {
    if (prepared_) return true;
    renderer_ = r;

    // Reset all cache pointers
    core_.sky_shader_from_cache = nullptr;
    core_.particle_cache = nullptr;
    core_.torch_cache = nullptr;
    shadow_.cache = nullptr;
    bloom_.extract_cache = nullptr;
    bloom_.blur_cache = nullptr;
    bloom_.composite_cache = nullptr;
    grass_.cache = nullptr;
    grass_.t3_cache = nullptr;
    ssao_.cache = nullptr;
    ssao_.blur_cache = nullptr;
    for (int i = 0; i < CoreRes::MAX_TIERS; i++) {
        core_.island_cache[i] = nullptr;
        core_.fur_cache[i] = nullptr;
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
    for (int t = 2; t <= max_tier && t <= CoreRes::MAX_TIERS; t++) {
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
    if (max_tier >= 2 && bloom_.scene_rt && render_w > 0 && render_h > 0) {
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
        bool has_island = (t <= 3) ? (core_.island_cache[idx] != nullptr)
                                   : static_cast<bool>(core_.island_shaders[idx]);
        if (!has_island) {
            LOG_WRN("Shader validation: tier %d missing island shader", t);
            valid = false;
        }

        // Sky shader: per-tier cache, with fallback to sky_shader_from_cache
        bool has_sky = (core_.sky_cache[idx] != nullptr);
        if (!has_sky && t == 1 && core_.sky_shader_from_cache)
            has_sky = true;
        if (!has_sky) {
            LOG_WRN("Shader validation: tier %d missing sky shader", t);
            valid = false;
        }

        // Fur shader: same pattern as island
        bool has_fur = (t <= 3) ? (core_.fur_cache[idx] != nullptr)
                                : static_cast<bool>(core_.fur_shaders[idx]);
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
        if (si >= 0 && si < CoreRes::MAX_TIERS && core_.sky_cache[si])
            view.core.sky_shader = core_.sky_cache[si];
        else
            view.core.sky_shader = core_.sky_shader_from_cache ? core_.sky_shader_from_cache : &core_.sky_shader;
    }
    view.core.model_mesh = core_.model_mesh;
    view.core.sky_mesh = core_.sky_mesh.get();
    view.core.ground_mesh = core_.ground_mesh;
    view.core.rock_mesh = core_.rock_mesh;
    view.core.grass_mesh = core_.grass_mesh;
    view.core.particle_mesh = core_.particle_mesh;
    view.core.fur_tex = core_.fur_tex.get();
    view.core.fur_mask_tex = core_.fur_mask_tex.get();
    view.core.pedestal_mesh = sanctuary_.pedestal;
    view.core.column_tall_mesh = sanctuary_.column_tall;
    view.core.column_stump_mesh = sanctuary_.column_stump;
    view.core.arch_mesh = sanctuary_.arch;
    view.core.fallen_column_mesh = sanctuary_.fallen_column;
    view.core.slab_mesh = sanctuary_.slab;
    view.core.stone_sphere_mesh = sanctuary_.stone_sphere;
    view.core.mossy_block_mesh = sanctuary_.mossy_block;
    view.core.bowl_mesh = sanctuary_.bowl;
    view.core.obelisk_mesh = sanctuary_.obelisk;
    view.core.ring_inner_mesh = sanctuary_.ring_inner;
    view.core.ring_outer_mesh = sanctuary_.ring_outer;
    view.core.pond_mesh = sanctuary_.pond;
    view.core.torch_mesh = sanctuary_.torch;
    view.core.torch_shader = core_.torch_cache;
    for (int i = 0; i < 3; i++) view.core.tree_meshes[i] = sanctuary_.trees[i];
    view.core.model_bounding_radius = core_.model_bounding_radius;
    view.core.particle_shader = core_.particle_cache ? core_.particle_cache
                               : (core_.particle_shader ? &core_.particle_shader : nullptr);

    int idx = static_cast<int>(tier) - 1;
    if (idx < 0 || idx >= CoreRes::MAX_TIERS) idx = 0;

    // Use cached tier-specific shaders (T1-T3), fall back to legacy owned (T4), then tier 1
    if (core_.island_cache[idx]) {
        view.core.island_shader = core_.island_cache[idx];
    } else if (core_.island_shaders[idx]) {
        view.core.island_shader = &core_.island_shaders[idx];
    } else if (core_.island_cache[0]) {
        view.core.island_shader = core_.island_cache[0];
    } else {
        view.core.island_shader = &core_.island_shaders[0];
    }

    if (core_.fur_cache[idx]) {
        view.core.fur_shader = core_.fur_cache[idx];
    } else if (core_.fur_shaders[idx]) {
        view.core.fur_shader = &core_.fur_shaders[idx];
    } else if (core_.fur_cache[0]) {
        view.core.fur_shader = core_.fur_cache[0];
    } else {
        view.core.fur_shader = &core_.fur_shaders[0];
    }

    // T2+ shadow mapping resources (prefer cache, fallback to legacy)
    if (idx >= 1 && (shadow_.cache || shadow_.shader) && shadow_.rt) {
        view.shadow.shader = shadow_.cache ? shadow_.cache : &shadow_.shader;
        view.shadow.rt = shadow_.rt.get();
        view.shadow.depth_tex = shadow_.depth_tex;
        view.shadow.map_size = shadow_.map_size;
    }

    // T2+ bloom post-processing resources (prefer cache, fallback to legacy)
    {
        ShaderProgram* bex = bloom_.extract_cache ? bloom_.extract_cache
                           : (bloom_.extract_shader ? &bloom_.extract_shader : nullptr);
        ShaderProgram* bbl = bloom_.blur_cache ? bloom_.blur_cache
                           : (bloom_.blur_shader ? &bloom_.blur_shader : nullptr);
        ShaderProgram* bco = bloom_.composite_cache ? bloom_.composite_cache
                           : (bloom_.composite_shader ? &bloom_.composite_shader : nullptr);
        if (idx >= 1 && bex && bbl && bco && bloom_.scene_rt && bloom_.bright_rt && bloom_.blur_rt) {
            view.bloom.extract_shader = bex;
            view.bloom.blur_shader = bbl;
            view.bloom.composite_shader = bco;
            view.bloom.fullscreen_quad = bloom_.fullscreen_quad.get();
            view.bloom.scene_rt = bloom_.scene_rt.get();
            view.bloom.bright_rt = bloom_.bright_rt.get();
            view.bloom.blur_rt = bloom_.blur_rt.get();
            view.bloom.strength = bloom_.strength;
        }
    }

    // T2+ instanced grass resources (prefer cache, fallback to legacy)
    if (idx >= 1) {
        if (idx >= 2 && (grass_.t3_cache || grass_.shader_t3)) {
            view.grass.shader = grass_.t3_cache ? grass_.t3_cache : &grass_.shader_t3;
        } else if (grass_.cache || grass_.shader) {
            view.grass.shader = grass_.cache ? grass_.cache : &grass_.shader;
        }
        view.grass.blade_mesh = grass_.blade_mesh;
    }

    // T3+ normal map texture
    if (idx >= 2 && normal_map_tex_) {
        view.normal_map_tex = normal_map_tex_.get();
    }

    // T2+ SSAO resources (prefer cache, fallback to legacy)
    {
        ShaderProgram* ssao_prog = ssao_.cache ? ssao_.cache
                                 : (ssao_.shader ? &ssao_.shader : nullptr);
        ShaderProgram* ssao_blur_prog = ssao_.blur_cache ? ssao_.blur_cache
                                      : (ssao_.blur_shader ? &ssao_.blur_shader : nullptr);
        if (idx >= 1 && ssao_prog && ssao_blur_prog && ssao_.rt && ssao_.blur_rt) {
            view.ssao.shader = ssao_prog;
            view.ssao.blur_shader = ssao_blur_prog;
            view.ssao.rt = ssao_.rt.get();
            view.ssao.blur_rt = ssao_.blur_rt.get();
            view.ssao.noise_tex = ssao_.noise_tex.get();
            view.ssao.scene_depth_tex = ssao_.scene_depth_tex;
        }
    }

    // T4+ resources
    if (idx >= 3) {
        // Use T4 grass shader if available
        if (t4_.grass_shader_t4) {
            view.grass.shader = &t4_.grass_shader_t4;
        }
        if (t4_.tess_shader) view.t4.tess_shader = &t4_.tess_shader;
        if (t4_.compute_particle_shader) view.t4.compute_particle_shader = &t4_.compute_particle_shader;
        if (t4_.particle_render_shader) view.t4.particle_render_shader = &t4_.particle_render_shader;
        if (t4_.volumetric_fog_shader) view.t4.hdr.volumetric_fog_shader = &t4_.volumetric_fog_shader;
        if (t4_.tone_map_shader) view.t4.hdr.tone_map_shader = &t4_.tone_map_shader;
        view.t4.particle_ssbo = t4_.particle_ssbo.get();
        view.t4.compute_particle_count = t4_.compute_particle_count;
        if (t4_.hdr_scene_rt) {
            view.t4.hdr.scene_rt = t4_.hdr_scene_rt.get();
            view.t4.hdr.depth_tex = t4_.hdr_depth_tex;
            view.t4.hdr.color_tex = t4_.hdr_color_tex;
            // Override scene_depth_tex for SSAO to use HDR depth
            view.ssao.scene_depth_tex = t4_.hdr_depth_tex;
        }
        if (t4_.hdr_bright_rt) view.t4.hdr.bright_rt = t4_.hdr_bright_rt.get();
        if (t4_.fog_rt) {
            view.t4.hdr.fog_rt = t4_.fog_rt.get();
            view.t4.hdr.fog_w = t4_.fog_w;
            view.t4.hdr.fog_h = t4_.fog_h;
        }

        // T4 Ultra: GTAO
        if (t4_.gtao_shader) view.t4.gtao.shader = &t4_.gtao_shader;
        if (t4_.gtao_blur_shader) view.t4.gtao.blur_shader = &t4_.gtao_blur_shader;
        if (t4_.gtao_tex) view.t4.gtao.tex = t4_.gtao_tex.get();
        if (t4_.gtao_blur_tex) view.t4.gtao.blur_tex = t4_.gtao_blur_tex.get();

        // T4 Ultra: Compute Bloom
        if (t4_.bloom_down_compute) view.t4.bloom.down_compute = &t4_.bloom_down_compute;
        if (t4_.bloom_up_compute) view.t4.bloom.up_compute = &t4_.bloom_up_compute;
        for (int i = 0; i < T4Res::BLOOM_MIP_COUNT; i++) {
            if (t4_.bloom_mips[i]) view.t4.bloom.mips[i] = t4_.bloom_mips[i].get();
        }

        // T4 Ultra: Auto-Exposure
        if (t4_.histogram_shader) view.t4.exposure.histogram_shader = &t4_.histogram_shader;
        if (t4_.exposure_shader) view.t4.exposure.exposure_shader = &t4_.exposure_shader;
        view.t4.exposure.histogram_ssbo = t4_.histogram_ssbo.get();
        view.t4.exposure.exposure_ssbo = t4_.exposure_ssbo.get();

        // T4 Ultra: SSR
        if (t4_.ssr_shader) view.t4.ssr.shader = &t4_.ssr_shader;
        if (t4_.ssr_tex) view.t4.ssr.tex = t4_.ssr_tex.get();
        if (t4_.ssr_color_snapshot) view.t4.ssr.color_snapshot = t4_.ssr_color_snapshot.get();
        if (t4_.ssr_depth_snapshot) view.t4.ssr.depth_snapshot = t4_.ssr_depth_snapshot.get();

        // T4 Ultra: DoF
        if (t4_.dof_shader) view.t4.dof.shader = &t4_.dof_shader;
        if (t4_.dof_tex) view.t4.dof.tex = t4_.dof_tex.get();

        // T4 Ultra: Puddles
        for (int i = 0; i < T4Res::PUDDLE_COUNT; i++)
            view.t4.puddle_meshes[i] = t4_.puddle_meshes[i];
    }

    return view;
}

void DemoResources::destroy() {
    if (!prepared_) return;

    scene_meshes_.destroyAll();
    core_.sky_mesh.reset();
    core_.fur_tex.reset();
    core_.fur_mask_tex.reset();
    core_.sky_shader.reset();
    core_.sky_shader_from_cache = nullptr;
    for (int i = 0; i < CoreRes::MAX_TIERS; i++) core_.sky_cache[i] = nullptr;

    // Null out all cache pointers BEFORE destroying the cache (which frees the programs)
    for (int i = 0; i < CoreRes::MAX_TIERS; i++) {
        core_.island_cache[i] = nullptr;
        core_.fur_cache[i] = nullptr;
    }
    core_.particle_cache = nullptr;
    core_.torch_cache = nullptr;
    shadow_.cache = nullptr;
    bloom_.extract_cache = nullptr;
    bloom_.blur_cache = nullptr;
    bloom_.composite_cache = nullptr;
    grass_.cache = nullptr;
    grass_.t3_cache = nullptr;
    ssao_.cache = nullptr;
    ssao_.blur_cache = nullptr;

    shader_cache_.destroy();

    for (int i = 0; i < CoreRes::MAX_TIERS; i++) {
        core_.island_shaders[i].reset();
        core_.fur_shaders[i].reset();
    }
    core_.particle_shader.reset();

    // Shadow resources
    shadow_.shader.reset();
    shadow_.rt.reset();
    shadow_.depth_tex = TextureHandle();
    shadow_.map_size = 0;

    // Bloom resources
    bloom_.extract_shader.reset();
    bloom_.blur_shader.reset();
    bloom_.composite_shader.reset();
    bloom_.fullscreen_quad.reset();
    bloom_.scene_rt.reset();
    bloom_.bright_rt.reset();
    bloom_.blur_rt.reset();
    bloom_.strength = 0.0f;

    // Grass resources
    grass_.shader.reset();
    grass_.shader_t3.reset();

    // Normal map
    normal_map_tex_.reset();

    // T4 resources
    t4_.grass_shader_t4.reset();
    t4_.tess_shader.reset();
    t4_.compute_particle_shader.reset();
    t4_.particle_render_shader.reset();
    t4_.volumetric_fog_shader.reset();
    t4_.tone_map_shader.reset();
    t4_.particle_ssbo.reset();
    t4_.compute_particle_count = 0;
    t4_.hdr_scene_rt.reset();
    t4_.hdr_bright_rt.reset();
    t4_.hdr_depth_tex = TextureHandle();
    t4_.fog_rt.reset();
    t4_.fog_w = 0;
    t4_.fog_h = 0;

    // T4 Ultra resources
    t4_.gtao_shader.reset();
    t4_.gtao_blur_shader.reset();
    t4_.gtao_tex.reset();
    t4_.gtao_blur_tex.reset();
    t4_.bloom_down_compute.reset();
    t4_.bloom_up_compute.reset();
    for (int i = 0; i < T4Res::BLOOM_MIP_COUNT; i++) t4_.bloom_mips[i].reset();
    t4_.histogram_shader.reset();
    t4_.exposure_shader.reset();
    t4_.histogram_ssbo.reset();
    t4_.exposure_ssbo.reset();
    t4_.ssr_shader.reset();
    t4_.ssr_tex.reset();
    t4_.dof_shader.reset();
    t4_.dof_tex.reset();

    // SSAO resources
    ssao_.shader.reset();
    ssao_.blur_shader.reset();
    ssao_.rt.reset();
    ssao_.blur_rt.reset();
    ssao_.noise_tex.reset();
    ssao_.scene_depth_tex = TextureHandle();

    prepared_ = false;
    renderer_ = nullptr;
    LOG_INF("Resources: destroyed");
}
