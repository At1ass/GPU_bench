#include "demo/demo_resources.h"
#include "demo/demo_scene.h"
#include "demo/shader_loader.h"
#include "renderer/features.h"
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
        MeshData gd = MeshGen::terrain(14.0f, 40);
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

    if (tier == 3) {
        int idx = 2; // tier 3 -> index 2
        // Island shader
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl3/island_t3_150.vert");
                fs_str = ShaderLoader::load("gl3/island_t3_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl3/island_t3.vert");
                fs_str = ShaderLoader::load("gl3/island_t3.frag");
            }
            if (!island_shaders_[idx].create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::err("Resources: failed to create island shader (tier 3)");
                return false;
            }
        }
        // Fur shader
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl3/fur_t3_150.vert");
                fs_str = ShaderLoader::load("gl3/fur_t3_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl3/fur_t3.vert");
                fs_str = ShaderLoader::load("gl3/fur_t3.frag");
            }
            if (!fur_shaders_[idx].create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::err("Resources: failed to create fur shader (tier 3)");
                return false;
            }
        }
        // Grass T3 shader (reuses T2 grass shader structure, just version bump + point lights)
        // T3 uses the same grass_shader_ as T2 since grass instancing is shared
        // We compile a separate T3 grass shader
        {
            std::string vs_str, fs_str;
            if (r->isCoreProfile()) {
                vs_str = ShaderLoader::load("gl3/grass_t3_150.vert");
                fs_str = ShaderLoader::load("gl3/grass_t3_150.frag");
            } else {
                vs_str = ShaderLoader::load("gl3/grass_t3.vert");
                fs_str = ShaderLoader::load("gl3/grass_t3.frag");
            }
            if (!grass_shader_t3_.create(r, vs_str.c_str(), fs_str.c_str())) {
                Log::warn("Resources: failed to create grass T3 shader (non-critical)");
            }
        }

        // Procedural normal map texture (256x256)
        {
            const int NM_SIZE = 256;
            std::vector<unsigned char> nm_data(NM_SIZE * NM_SIZE * 3);
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
                    int idx2 = (y * NM_SIZE + x) * 3;
                    nm_data[idx2 + 0] = static_cast<unsigned char>(nx * 255.0f);
                    nm_data[idx2 + 1] = static_cast<unsigned char>(ny * 255.0f);
                    nm_data[idx2 + 2] = static_cast<unsigned char>(nz * 255.0f);
                }
            }
            TextureHandle nt = r->createTexture(NM_SIZE, NM_SIZE, 3, nm_data.data());
            if (nt != INVALID_TEXTURE) {
                normal_map_tex_.assign(r, nt);
                Log::info("Resources: generated normal map %dx%d", NM_SIZE, NM_SIZE);
            }
        }

        return true;
    }

    if (tier == 4) {
        int idx = 3; // tier 4 -> index 3
        // PBR Island shader
        {
            std::string vs = ShaderLoader::load("gl4/island_t4.vert");
            std::string fs = ShaderLoader::load("gl4/island_t4.frag");
            if (!island_shaders_[idx].create(r, vs.c_str(), fs.c_str())) {
                Log::err("Resources: failed to create PBR island shader (tier 4)");
                return false;
            }
        }
        // PBR Fur shader
        {
            std::string vs = ShaderLoader::load("gl4/fur_t4.vert");
            std::string fs = ShaderLoader::load("gl4/fur_t4.frag");
            if (!fur_shaders_[idx].create(r, vs.c_str(), fs.c_str())) {
                Log::err("Resources: failed to create PBR fur shader (tier 4)");
                return false;
            }
        }
        // PBR Grass shader
        {
            std::string vs = ShaderLoader::load("gl4/grass_t4.vert");
            std::string fs = ShaderLoader::load("gl4/grass_t4.frag");
            if (!grass_shader_t4_.create(r, vs.c_str(), fs.c_str())) {
                Log::warn("Resources: failed to create PBR grass shader (tier 4)");
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
                    Log::info("Resources: tessellation shader created");
                } else {
                    Log::warn("Resources: failed to create tessellation shader");
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
                    Log::info("Resources: compute particle shader created");
                } else {
                    Log::warn("Resources: failed to create compute particle shader");
                }
            }
        }
        // Particle render shader
        {
            std::string vs = ShaderLoader::load("gl4/particle_render_t4.vert");
            std::string fs = ShaderLoader::load("gl4/particle_render_t4.frag");
            if (!particle_render_shader_.create(r, vs.c_str(), fs.c_str())) {
                Log::warn("Resources: failed to create particle render shader");
            }
        }
        // Volumetric fog shader
        {
            std::string vs = ShaderLoader::load("gl4/volumetric_fog_t4.vert");
            std::string fs = ShaderLoader::load("gl4/volumetric_fog_t4.frag");
            if (!volumetric_fog_shader_.create(r, vs.c_str(), fs.c_str())) {
                Log::warn("Resources: failed to create volumetric fog shader");
            }
        }
        // HDR tone map composite shader
        {
            std::string vs = ShaderLoader::load("gl4/tone_map_t4.vert");
            std::string fs = ShaderLoader::load("gl4/tone_map_t4.frag");
            if (!tone_map_shader_.create(r, vs.c_str(), fs.c_str())) {
                Log::warn("Resources: failed to create tone map shader");
            }
        }
        return true;
    }

    Log::warn("Resources: tier %d shaders not implemented yet, reusing tier 1", tier);
    return false;
}

bool DemoResources::createShadowResources(Renderer* r, int shadow_size) {
    shadow_map_size_ = shadow_size;
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

    // Shadow depth render target (size set by caller)
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

bool DemoResources::createT4Resources(Renderer* r, int render_w, int render_h) {
    ComputeFeatures* cf = r->features<ComputeFeatures>();

    // Create particle SSBO
    if (cf && cf->hasCompute() && compute_particle_shader_) {
        compute_particle_count_ = 2048;
        int particle_size = 12 * sizeof(float); // 3 vec4s
        BufferHandle ssbo = cf->createSSBO(compute_particle_count_ * particle_size);
        if (ssbo != INVALID_BUFFER) {
            // Initialize particle data
            std::vector<float> init_data(compute_particle_count_ * 12);
            unsigned int seed = 12345;
            for (int i = 0; i < compute_particle_count_; i++) {
                int base = i * 12;
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
            particle_ssbo_ = ssbo;
            Log::info("Resources: compute particle SSBO created (%d particles)", compute_particle_count_);
        }
    }

    // HDR scene FBO (float, with sampleable depth)
    {
        RenderTargetHandle hrt = r->createFloatRenderTargetWithDepth(render_w, render_h);
        if (hrt != INVALID_RENDER_TARGET) {
            hdr_scene_rt_.assign(r, hrt);
            hdr_depth_tex_ = r->getRTDepthTexture(hrt);
            Log::info("Resources: HDR scene FBO created %dx%d", render_w, render_h);
        } else {
            Log::warn("Resources: failed to create HDR FBO, falling back to LDR");
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
            Log::info("Resources: HDR fog FBO created %dx%d", fw, fh);
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
                Log::info("Resources: GTAO compute shader created");
            }
        }
        std::string gtao_blur_src = ShaderLoader::load("gl4/gtao_blur_t4.comp");
        if (!gtao_blur_src.empty()) {
            ShaderHandle sh = cf->createComputeShader(gtao_blur_src.c_str());
            if (sh != INVALID_SHADER) {
                gtao_blur_shader_.adopt(r, sh);
                Log::info("Resources: GTAO blur compute shader created");
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
                Log::info("Resources: GTAO textures created %dx%d", render_w, render_h);
            } else {
                Log::warn("Resources: failed to create GTAO textures");
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
                Log::info("Resources: compute bloom mip chain created (%d levels)", BLOOM_MIP_COUNT);
            } else {
                Log::warn("Resources: failed to create bloom mip chain");
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
            BufferHandle hist = cf->createSSBO(256 * sizeof(unsigned int));
            if (hist != INVALID_BUFFER) {
                // Zero-initialize histogram
                std::vector<unsigned int> zeros(256, 0);
                cf->updateSSBO(hist, zeros.data(), 256 * sizeof(unsigned int));
                histogram_ssbo_ = hist;
            }
            // Single float for exposure
            BufferHandle exp_buf = cf->createSSBO(sizeof(float));
            if (exp_buf != INVALID_BUFFER) {
                float init_exp = 1.0f;
                cf->updateSSBO(exp_buf, &init_exp, sizeof(float));
                exposure_ssbo_ = exp_buf;
            }
            if (histogram_ssbo_ != INVALID_BUFFER && exposure_ssbo_ != INVALID_BUFFER) {
                Log::info("Resources: auto-exposure SSBOs created");
            } else {
                Log::warn("Resources: failed to create exposure SSBOs");
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
                    Log::info("Resources: SSR compute shader + texture created");
                }
            }
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
                    Log::info("Resources: DoF compute shader + texture created");
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
        Log::info("Resources: %d puddle meshes created", PUDDLE_COUNT);
    }

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
    if (idx >= 1) {
        // Use T3 grass shader if available, otherwise T2
        if (idx >= 2 && grass_shader_t3_) {
            view.grass_shader = &grass_shader_t3_;
        } else if (grass_shader_) {
            view.grass_shader = &grass_shader_;
        }
        view.grass_blade_mesh = grass_blade_mesh_;
    }

    // T3+ normal map texture
    if (idx >= 2 && normal_map_tex_) {
        view.normal_map_tex = normal_map_tex_.get();
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

    // T4+ resources
    if (idx >= 3) {
        // Use T4 grass shader if available
        if (grass_shader_t4_) {
            view.grass_shader = &grass_shader_t4_;
        }
        if (tess_shader_) view.tess_shader = &tess_shader_;
        if (compute_particle_shader_) view.compute_particle_shader = &compute_particle_shader_;
        if (particle_render_shader_) view.particle_render_shader = &particle_render_shader_;
        if (volumetric_fog_shader_) view.volumetric_fog_shader = &volumetric_fog_shader_;
        if (tone_map_shader_) view.tone_map_shader = &tone_map_shader_;
        view.particle_ssbo = particle_ssbo_;
        view.compute_particle_count = compute_particle_count_;
        if (hdr_scene_rt_) {
            view.hdr_scene_rt = hdr_scene_rt_.get();
            view.hdr_depth_tex = hdr_depth_tex_;
            // Override scene_depth_tex for SSAO to use HDR depth
            view.scene_depth_tex = hdr_depth_tex_;
        }
        if (hdr_bright_rt_) view.hdr_bright_rt = hdr_bright_rt_.get();
        if (fog_rt_) view.fog_rt = fog_rt_.get();

        // T4 Ultra: GTAO
        if (gtao_shader_) view.gtao_shader = &gtao_shader_;
        if (gtao_blur_shader_) view.gtao_blur_shader = &gtao_blur_shader_;
        if (gtao_tex_) view.gtao_tex = gtao_tex_.get();
        if (gtao_blur_tex_) view.gtao_blur_tex = gtao_blur_tex_.get();

        // T4 Ultra: Compute Bloom
        if (bloom_down_compute_) view.bloom_down_compute = &bloom_down_compute_;
        if (bloom_up_compute_) view.bloom_up_compute = &bloom_up_compute_;
        for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
            if (bloom_mips_[i]) view.bloom_mips[i] = bloom_mips_[i].get();
        }

        // T4 Ultra: Auto-Exposure
        if (histogram_shader_) view.histogram_shader = &histogram_shader_;
        if (exposure_shader_) view.exposure_shader = &exposure_shader_;
        view.histogram_ssbo = histogram_ssbo_;
        view.exposure_ssbo = exposure_ssbo_;

        // T4 Ultra: SSR
        if (ssr_shader_) view.ssr_shader = &ssr_shader_;
        if (ssr_tex_) view.ssr_tex = ssr_tex_.get();

        // T4 Ultra: DoF
        if (dof_shader_) view.dof_shader = &dof_shader_;
        if (dof_tex_) view.dof_tex = dof_tex_.get();

        // T4 Ultra: Puddles
        for (int i = 0; i < PUDDLE_COUNT; i++)
            view.puddle_meshes[i] = puddle_meshes_[i];
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
    if (particle_ssbo_ != INVALID_BUFFER) {
        ComputeFeatures* cf = renderer_->features<ComputeFeatures>();
        if (cf) cf->destroySSBO(particle_ssbo_);
        particle_ssbo_ = BufferHandle();
    }
    compute_particle_count_ = 0;
    hdr_scene_rt_.reset();
    hdr_bright_rt_.reset();
    hdr_depth_tex_ = TextureHandle();
    fog_rt_.reset();

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
    {
        ComputeFeatures* cf = renderer_ ? renderer_->features<ComputeFeatures>() : nullptr;
        if (cf) {
            if (histogram_ssbo_ != INVALID_BUFFER) cf->destroySSBO(histogram_ssbo_);
            if (exposure_ssbo_ != INVALID_BUFFER) cf->destroySSBO(exposure_ssbo_);
        }
        histogram_ssbo_ = BufferHandle();
        exposure_ssbo_ = BufferHandle();
    }
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
    Log::info("Resources: destroyed");
}
