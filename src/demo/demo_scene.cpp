#include "demo/demo_scene.h"
#include "demo/tier_config_validate.h"
#include "demo/shader_loader.h"
#include "demo/uniform_id.h"
#include "renderer/features.h"
#include "renderer/gl_funcs.h"
#include "geometry/mesh_gen.h"
#include "platform/logger.h"
#include <cmath>
#include <cstring>
#include <cstdio>

// ============================================================
// Scene constants
// ============================================================

static Vec3 normalizeSafe(const Vec3& v) {
    float l = v.length();
    return l > 1e-8f ? Vec3(v.x / l, v.y / l, v.z / l) : Vec3(0.0f, 1.0f, 0.0f);
}

static FrustumPlanes extractFrustum(const Mat4& vp) {
    FrustumPlanes f;
    const float* m = vp.m;
    // Column-major: row i of matrix = m[i], m[4+i], m[8+i], m[12+i]
    // Left:   row3 + row0
    f.planes[0][0] = m[3]  + m[0];  f.planes[0][1] = m[7]  + m[4];
    f.planes[0][2] = m[11] + m[8];  f.planes[0][3] = m[15] + m[12];
    // Right:  row3 - row0
    f.planes[1][0] = m[3]  - m[0];  f.planes[1][1] = m[7]  - m[4];
    f.planes[1][2] = m[11] - m[8];  f.planes[1][3] = m[15] - m[12];
    // Bottom: row3 + row1
    f.planes[2][0] = m[3]  + m[1];  f.planes[2][1] = m[7]  + m[5];
    f.planes[2][2] = m[11] + m[9];  f.planes[2][3] = m[15] + m[13];
    // Top:    row3 - row1
    f.planes[3][0] = m[3]  - m[1];  f.planes[3][1] = m[7]  - m[5];
    f.planes[3][2] = m[11] - m[9];  f.planes[3][3] = m[15] - m[13];
    // Near:   row3 + row2
    f.planes[4][0] = m[3]  + m[2];  f.planes[4][1] = m[7]  + m[6];
    f.planes[4][2] = m[11] + m[10]; f.planes[4][3] = m[15] + m[14];
    // Far:    row3 - row2
    f.planes[5][0] = m[3]  - m[2];  f.planes[5][1] = m[7]  - m[6];
    f.planes[5][2] = m[11] - m[10]; f.planes[5][3] = m[15] - m[14];
    // Normalize each plane
    for (int i = 0; i < 6; i++) {
        float len = sqrtf(f.planes[i][0] * f.planes[i][0] +
                          f.planes[i][1] * f.planes[i][1] +
                          f.planes[i][2] * f.planes[i][2]);
        if (len > 1e-8f) {
            float inv = 1.0f / len;
            f.planes[i][0] *= inv; f.planes[i][1] *= inv;
            f.planes[i][2] *= inv; f.planes[i][3] *= inv;
        }
    }
    return f;
}

static bool sphereInFrustum(const FrustumPlanes& f, const Vec3& center, float radius) {
    for (int i = 0; i < 6; i++) {
        float dist = f.planes[i][0] * center.x + f.planes[i][1] * center.y +
                     f.planes[i][2] * center.z + f.planes[i][3];
        if (dist < -radius) return false;
    }
    return true;
}

static const Vec3 SUN_DIR_RAW(0.4f, 0.8f, 0.3f);
static const Vec3 FOG_COLOR(0.62f, 0.67f, 0.76f);

// Set bounding sphere from transform (assumes mesh is centered at origin)
static void setBounds(SceneObject& obj, float mesh_radius) {
    // Extract translation from transform matrix (column-major)
    obj.bounds_center = Vec3(obj.transform.m[12], obj.transform.m[13], obj.transform.m[14]);
    // Scale factor: max of the 3 column lengths
    float sx = sqrtf(obj.transform.m[0]*obj.transform.m[0] + obj.transform.m[1]*obj.transform.m[1] + obj.transform.m[2]*obj.transform.m[2]);
    float sy = sqrtf(obj.transform.m[4]*obj.transform.m[4] + obj.transform.m[5]*obj.transform.m[5] + obj.transform.m[6]*obj.transform.m[6]);
    float sz = sqrtf(obj.transform.m[8]*obj.transform.m[8] + obj.transform.m[9]*obj.transform.m[9] + obj.transform.m[10]*obj.transform.m[10]);
    float max_scale = sx > sy ? (sx > sz ? sx : sz) : (sy > sz ? sy : sz);
    obj.bounds_radius = mesh_radius * max_scale;
}

// ============================================================
// DemoScene implementation
// ============================================================

DemoScene::DemoScene()
    : r_(nullptr)
    , tier_(DemoTier::Basic)
    , viewport_w_(0), viewport_h_(0)
    , initialized_(false)
    , passes_logged_(false)
    , prev_exposure_(1.0f)
    , dest_rt_(INVALID_RENDER_TARGET)
{
}

DemoScene::~DemoScene() {
}

// ============================================================
// buildScene: single model + ground plane
// ============================================================

void DemoScene::buildScene(Renderer* r) {
    opaque_objects_.clear();
    cloud_objects_.clear();
    model_mesh_ = MeshHandle();

    placeModel(r);
    placeGroundPlane(r);
    placeRocks(r);
    placeGrass(r);
    placePuddles(r);

    int total = static_cast<int>(opaque_objects_.size());
    LOG_INF("Demo scene: %d objects, fur shells=%d, particles=%d",
              total, config_.fur_shells, config_.particle_count);
}

// ============================================================
// placeModel: use pre-loaded model from resources
// ============================================================

void DemoScene::placeModel(Renderer* r) {
    (void)r;

    model_mesh_ = res_.core.model_mesh;

    model_transform_ = Mat4();

    SceneObject obj;
    obj.mesh = res_.core.model_mesh;
    obj.transform = model_transform_;
    obj.material = MaterialType::Model;
    obj.color = Vec3(0.55f, 0.38f, 0.32f);  // pinkish skin visible under fur
    obj.specular = 0.08f;
    setBounds(obj, res_.core.model_bounding_radius);
    opaque_objects_.push_back(obj);
}

// ============================================================
// placeGroundPlane: use pre-loaded ground mesh from resources
// ============================================================

void DemoScene::placeGroundPlane(Renderer* r) {
    (void)r;

    SceneObject ground;
    ground.mesh = res_.core.ground_mesh;
    ground.transform = Mat4::translate(0.0f, -1.0f, 0.0f);
    ground.material = MaterialType::Island;
    ground.color = Vec3(0.45f, 0.42f, 0.38f);
    ground.specular = 0.05f;
    setBounds(ground, 8.5f);
    opaque_objects_.push_back(ground);
}

// ============================================================
// placeRocks: scattered rocks (batched mesh, single draw call)
// ============================================================

void DemoScene::placeRocks(Renderer* r) {
    (void)r;
    if (res_.core.rock_mesh == MeshHandle()) return;

    SceneObject rocks;
    rocks.mesh = res_.core.rock_mesh;
    rocks.transform = Mat4();
    rocks.material = MaterialType::Model;  // flat color, no procedural terrain
    rocks.color = Vec3(0.45f, 0.42f, 0.38f);
    rocks.specular = 0.08f;
    rocks.vertex_wind = false;
    rocks.two_sided = true;
    setBounds(rocks, 6.0f);
    opaque_objects_.push_back(rocks);
}

// ============================================================
// placeGrass: scattered grass tufts (batched mesh, single draw call)
// ============================================================

void DemoScene::placeGrass(Renderer* r) {
    (void)r;
    // T2+ uses instanced grass rendering instead
    if (config_.instanced_grass_count > 0) return;

    if (res_.core.grass_mesh == MeshHandle()) return;

    SceneObject grass;
    grass.mesh = res_.core.grass_mesh;
    grass.transform = Mat4();
    grass.material = MaterialType::Model;  // flat color for grass
    grass.color = Vec3(0.30f, 0.50f, 0.20f);
    grass.specular = 0.02f;
    grass.vertex_wind = true;
    grass.two_sided = true;  // grass sways with wind
    setBounds(grass, 6.0f);
    opaque_objects_.push_back(grass);
}

// ============================================================
// placePuddles: reflective water discs around bunny (T4 Ultra)
// ============================================================

void DemoScene::placePuddles(Renderer* r) {
    (void)r;
    if (res_.t4.puddle_meshes[0] == MeshHandle()) return;
    if (!config_.enable_ssr) return;

    // Place 3 puddles around the bunny (larger, slightly above ground)
    static const float positions[][3] = {
        { 2.5f, -0.95f, 0.8f },
        { -1.8f, -0.95f, 2.2f },
        { 0.5f, -0.95f, -2.5f },
    };
    static const float scales[] = { 1.5f, 1.2f, 1.0f };

    for (int i = 0; i < 3; i++) {
        SceneObject puddle;
        puddle.mesh = res_.t4.puddle_meshes[i];
        // Scale + translate
        Mat4 s = Mat4::scale(scales[i], 1.0f, scales[i]);
        Mat4 t = Mat4::translate(positions[i][0], positions[i][1], positions[i][2]);
        puddle.transform = t * s;
        puddle.material = MaterialType::Model;
        puddle.color = Vec3(0.08f, 0.10f, 0.14f); // dark water
        puddle.specular = 0.95f;
        puddle.vertex_wind = false;
        puddle.two_sided = true;
        puddle.metallic = 0.0f;
        puddle.roughness = 0.02f;
        puddle.is_water = true;
        setBounds(puddle, 0.8f * scales[i]);
        opaque_objects_.push_back(puddle);
    }
}

// ============================================================
// setup: store resources, build scene geometry
// ============================================================

bool DemoScene::setup(Renderer* r, DemoTier tier, int viewport_w, int viewport_h,
                      const TierResourceView& resources) {
    r_ = r;
    tier_ = tier;
    config_ = getTierConfig(tier);
    validateTierConfig(config_);
    viewport_w_ = viewport_w;
    viewport_h_ = viewport_h;
    res_ = resources;

    // Init uniform blocks (resolve locations lazily on first set())
    ub_island_.init(res_.core.island_shader);
    ub_fur_.init(res_.core.fur_shader);
    ub_sky_.init(res_.core.sky_shader);
    ub_particle_.init(res_.core.particle_shader);
    ub_shadow_.init(res_.shadow.shader);
    ub_grass_.init(res_.grass.shader);
    ub_ssao_.init(res_.ssao.shader);
    ub_ssao_blur_.init(res_.ssao.blur_shader);
    ub_bloom_extract_.init(res_.bloom.extract_shader);
    ub_bloom_blur_.init(res_.bloom.blur_shader);
    ub_bloom_composite_.init(res_.bloom.composite_shader);
    ub_tess_.init(res_.t4.tess_shader);
    ub_compute_particle_.init(res_.t4.compute_particle_shader);
    ub_particle_render_.init(res_.t4.particle_render_shader);
    ub_vol_fog_.init(res_.t4.volumetric_fog_shader);
    ub_tone_map_.init(res_.t4.tone_map_shader);
    ub_gtao_.init(res_.t4.gtao_shader);
    ub_gtao_blur_.init(res_.t4.gtao_blur_shader);
    ub_bloom_down_.init(res_.t4.bloom_down_compute);
    ub_bloom_up_.init(res_.t4.bloom_up_compute);
    ub_histogram_.init(res_.t4.histogram_shader);
    ub_exposure_.init(res_.t4.exposure_shader);
    ub_ssr_.init(res_.t4.ssr_shader);
    ub_dof_.init(res_.t4.dof_shader);

    int t = static_cast<int>(tier);
    LOG_INF("Demo scene setup: tier %d, viewport %dx%d", t, viewport_w, viewport_h);

    // Build scene objects (cheap -- just fills SceneObject structs)
    buildScene(r);

    initialized_ = true;
    int total_obj = static_cast<int>(opaque_objects_.size());
    LOG_INF("Demo scene setup complete: %d objects", total_obj);
    return true;
}

// ============================================================
// cleanup
// ============================================================

void DemoScene::cleanup(Renderer* r) {
    (void)r;
    if (!initialized_) return;

    // Scene object lists are per-tier, clear them
    opaque_objects_.clear();
    cloud_objects_.clear();

    // Resources are owned by DemoResources, not by DemoScene

    initialized_ = false;
}

// ============================================================
// getTechniqueInfo
// ============================================================

TechniqueInfo DemoScene::getTechniqueInfo() const {
    int total = static_cast<int>(opaque_objects_.size() + cloud_objects_.size());
    return getTierTechniqueInfo(tier_, total);
}

// ============================================================
// computeLightMatrix: orthographic projection from sun direction
// ============================================================

void DemoScene::computeLightMatrix(FrameContext& fc) {
    Vec3 light_pos = fc.sun_dir * 15.0f;
    Vec3 up(0.0f, 0.0f, 1.0f);
    if (fabsf(Vec3::dot(fc.sun_dir, up)) > 0.99f)
        up = Vec3(1.0f, 0.0f, 0.0f);
    Mat4 light_view = Mat4::lookAt(light_pos, Vec3(0, 0, 0), up);
    Mat4 light_proj = Mat4::ortho(-6.0f, 6.0f, -6.0f, 6.0f, 0.1f, 30.0f);
    fc.light_vp = light_proj * light_view;
}

// ============================================================
// renderShadowPass: depth-only from sun perspective
// ============================================================

void DemoScene::renderShadowPass(Renderer* r, const FrameContext& fc) {
    if (!res_.shadow.shader || res_.shadow.rt == INVALID_RENDER_TARGET) return;

    r->bindRenderTarget(res_.shadow.rt);
    r->setViewport(0, 0, res_.shadow.map_size, res_.shadow.map_size);
    r->clear(1.0f, 1.0f, 1.0f, 1.0f);
    r->setDepthTest(true);
    r->setDepthMask(true);
    r->setCullFace(true);

    ub_shadow_.use();
    ub_shadow_.set(U::LightVP, fc.light_vp);

    // Render all opaque objects into shadow map
    // (model base mesh is already in opaque_objects_, so fur also casts shadow)
    for (size_t i = 0; i < opaque_objects_.size(); i++) {
        const SceneObject& obj = opaque_objects_[i];
        ub_shadow_.set(U::Model, obj.transform);
        r->drawMesh(obj.mesh);
    }

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

// ============================================================
// renderSky
// ============================================================

void DemoScene::renderSky(Renderer* r, const FrameContext& fc) {
    if (!res_.core.sky_shader || res_.core.sky_mesh == MeshHandle()) return;

    r->setDepthTest(false);
    r->setCullFace(false);

    ub_sky_.use();
    ub_sky_.set(U::Proj, fc.proj);
    ub_sky_.set(U::View, fc.view);
    ub_sky_.set(U::SunDir, fc.sun_dir);
    ub_sky_.set(U::Time, fc.time);

    r->drawMesh(res_.core.sky_mesh);

    r->setDepthTest(true);
    r->setCullFace(true);
}

// ============================================================
// setPointLightUniforms: T3+ animated point lights
// ============================================================

void DemoScene::setPointLightUniforms(ShaderProgram* shader, const FrameContext& fc) {
    if (config_.point_light_count <= 0) {
        shader->set1i("u_point_light_count", 0);
        return;
    }
    shader->set1i("u_point_light_count", config_.point_light_count);
    static const Vec3 colors[] = {
        Vec3(2.0f, 1.6f, 0.8f),  // warm yellow
        Vec3(0.8f, 1.2f, 2.0f),  // cool blue
        Vec3(0.8f, 1.5f, 0.8f),  // soft green
    };
    for (int i = 0; i < config_.point_light_count && i < 3; i++) {
        float angle = fc.time * 0.5f + static_cast<float>(i) * 2.094f;
        float px = cosf(angle) * 1.8f;
        float pz = sinf(angle) * 1.8f;
        float py = -0.5f + sinf(fc.time * 0.8f + static_cast<float>(i) * 1.5f) * 0.3f;
        char name[32];
        snprintf(name, sizeof(name), "u_point_lights[%d]", i);
        shader->set3f(name, px, py, pz);
        snprintf(name, sizeof(name), "u_point_colors[%d]", i);
        shader->set3f(name, colors[i].x, colors[i].y, colors[i].z);
    }
}

// ============================================================
// renderOpaquePass
// ============================================================

void DemoScene::renderOpaquePass(Renderer* r, const FrameContext& fc) {
    if (!res_.core.island_shader) return;

    ub_island_.use();

    ub_island_.set(U::Proj, fc.proj);
    ub_island_.set(U::View, fc.view);
    ub_island_.set(U::LightDir, fc.sun_dir);
    ub_island_.set(U::CamPos, fc.cam_pos);
    ub_island_.set(U::FogColor, FOG_COLOR);
    ub_island_.set(U::FogDensity, config_.fog_density);
    ub_island_.set(U::Time, fc.time);
    ub_island_.set(U::NormalStrength, config_.enable_normal_maps ? config_.normal_map_strength : 0.0f);

    // Wind direction (same as fur pass for consistency)
    if (config_.enable_wind) {
        float wind_x = sinf(fc.time * 0.7f) * 1.8f;
        float wind_z = cosf(fc.time * 0.5f) * 1.2f;
        ub_island_.set(U::WindDir, wind_x, 0.0f, wind_z);
    } else {
        ub_island_.set(U::WindDir, 0.0f, 0.0f, 0.0f);
    }

    // Viewport size for vignette + color grading
    ub_island_.set(U::ViewportSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));

    // PBR default uniforms (T4) — overridden per-object if set
    float default_metallic = 0.0f;
    float default_roughness = 0.6f;
    if (config_.enable_pbr) {
        ub_island_.set(U::Metallic, default_metallic);
        ub_island_.set(U::Roughness, default_roughness);
    }

    // PCSS / SSS uniforms (T4 Ultra)
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        ub_island_.set(U::ShadowTexelSize, texel, texel);
        ub_island_.set(U::LightSize, config_.light_size);
    }
    if (config_.enable_sss) {
        ub_island_.set(U::SssStrength, config_.sss_strength);
    } else {
        ub_island_.set(U::SssStrength, 0.0f);
    }

    // Shadow uniforms (T2+)
    if (fc.has_shadows) {
        ub_island_.set(U::LightVP, fc.light_vp);
        r->bindTextureUnit(3, res_.shadow.depth_tex);
        ub_island_.set(U::ShadowMap, 3);
        ub_island_.set(U::HasShadow, 1.0f);
    } else {
        ub_island_.set(U::HasShadow, 0.0f);
    }

    // Normal map texture (T3+)
    if (res_.normal_map_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(4, res_.normal_map_tex);
        ub_island_.set(U::NormalMap, 4);
        ub_island_.set(U::HasNormalMap, 1.0f);
    } else {
        ub_island_.set(U::HasNormalMap, 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res_.core.island_shader, fc);

    for (size_t i = 0; i < opaque_objects_.size(); i++) {
        const SceneObject& obj = opaque_objects_[i];
        // Skip water — rendered separately in renderWaterPass after scene copy
        if (obj.is_water) continue;
        if (!sphereInFrustum(fc.frustum, obj.bounds_center, obj.bounds_radius))
            continue;

        ub_island_.set(U::Model, obj.transform);
        ub_island_.set(U::MatColor, obj.color);
        ub_island_.set(U::MatSpec, obj.specular);
        ub_island_.set(U::Alpha, 1.0f);
        ub_island_.set(U::ProcTex, obj.material == MaterialType::Island ? 1.0f : 0.0f);
        ub_island_.set(U::VertexWind, obj.vertex_wind ? 1.0f : 0.0f);

        // Per-object PBR overrides
        if (config_.enable_pbr && obj.metallic >= 0.0f) {
            ub_island_.set(U::Metallic, obj.metallic);
            ub_island_.set(U::Roughness, obj.roughness);
        }

        // Water flag
        ub_island_.set(U::IsWater, 0.0f);

        if (obj.two_sided) r->setCullFace(false);
        r->drawMesh(obj.mesh);
        if (obj.two_sided) r->setCullFace(true);

        // Restore defaults after per-object override
        if (config_.enable_pbr && obj.metallic >= 0.0f) {
            ub_island_.set(U::Metallic, default_metallic);
            ub_island_.set(U::Roughness, default_roughness);
        }
    }
}

// ============================================================
// renderGrassInstanced: T2+ instanced grass blades
// ============================================================

void DemoScene::renderGrassInstanced(Renderer* r, const FrameContext& fc) {
    if (!res_.grass.shader || res_.grass.blade_mesh == MeshHandle()) return;
    if (config_.instanced_grass_count <= 0) return;

    GL3Features* g3 = r->features<GL3Features>();
    if (!g3 || !g3->hasInstancing()) return;

    ub_grass_.use();
    ub_grass_.set(U::Proj, fc.proj);
    ub_grass_.set(U::View, fc.view);
    ub_grass_.set(U::Model, Mat4());
    ub_grass_.set(U::LightDir, fc.sun_dir);
    ub_grass_.set(U::CamPos, fc.cam_pos);
    ub_grass_.set(U::FogColor, FOG_COLOR);
    ub_grass_.set(U::FogDensity, config_.fog_density);
    ub_grass_.set(U::Time, fc.time);
    ub_grass_.set(U::GrassCount, config_.instanced_grass_count);
    ub_grass_.set(U::AreaSize, config_.grass_area_size);

    // Wind (same as fur)
    if (config_.enable_wind) {
        float wind_x = sinf(fc.time * 0.7f) * 1.8f;
        float wind_z = cosf(fc.time * 0.5f) * 1.2f;
        ub_grass_.set(U::WindDir, wind_x, 0.0f, wind_z);
    } else {
        ub_grass_.set(U::WindDir, 0.0f, 0.0f, 0.0f);
    }

    // PCSS uniforms (T4 Ultra)
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        ub_grass_.set(U::ShadowTexelSize, texel, texel);
        ub_grass_.set(U::LightSize, config_.light_size);
    }

    // Puddle exclusion zones (T4 Ultra) — array uniforms, keep string-based
    if (config_.enable_ssr) {
        ub_grass_.set(U::PuddleCount, 3);
        res_.grass.shader->set3f("u_puddle_pos[0]", 2.5f, 0.0f, 0.8f);
        res_.grass.shader->set3f("u_puddle_pos[1]", -1.8f, 0.0f, 2.2f);
        res_.grass.shader->set3f("u_puddle_pos[2]", 0.5f, 0.0f, -2.5f);
        res_.grass.shader->set1f("u_puddle_radius[0]", 1.5f);
        res_.grass.shader->set1f("u_puddle_radius[1]", 1.2f);
        res_.grass.shader->set1f("u_puddle_radius[2]", 1.0f);
    } else {
        ub_grass_.set(U::PuddleCount, 0);
    }

    // Shadow
    if (fc.has_shadows) {
        ub_grass_.set(U::LightVP, fc.light_vp);
        r->bindTextureUnit(3, res_.shadow.depth_tex);
        ub_grass_.set(U::ShadowMap, 3);
        ub_grass_.set(U::HasShadow, 1.0f);
    } else {
        ub_grass_.set(U::HasShadow, 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res_.grass.shader, fc);

    r->setDepthTest(true);
    r->setDepthMask(true);
    r->setBlending(true);   // for tip alpha fade
    r->setCullFace(false);  // grass visible from both sides

    g3->drawMeshInstanced(res_.grass.blade_mesh, config_.instanced_grass_count);

    r->setBlending(false);
    r->setCullFace(true);
}

// ============================================================
// renderFurPass: shell-based fur rendering
// ============================================================

void DemoScene::renderFurPass(Renderer* r, const FrameContext& fc) {
    if (!res_.core.fur_shader || model_mesh_ == MeshHandle() || res_.core.fur_tex == INVALID_TEXTURE) return;

    ub_fur_.use();
    ub_fur_.set(U::Proj, fc.proj);
    ub_fur_.set(U::View, fc.view);
    ub_fur_.set(U::Model, model_transform_);
    ub_fur_.set(U::LightDir, fc.sun_dir);
    ub_fur_.set(U::CamPos, fc.cam_pos);
    ub_fur_.set(U::FogColor, FOG_COLOR);
    ub_fur_.set(U::FogDensity, config_.fog_density);
    ub_fur_.set(U::FurLength, config_.fur_length);
    ub_fur_.set(U::FurAoPower, 1.5f);
    ub_fur_.set(U::Time, fc.time);

    // Breeze: slowly varying direction
    float wind_x = sinf(fc.time * 0.7f) * 1.8f;
    float wind_z = cosf(fc.time * 0.5f) * 1.2f;
    ub_fur_.set(U::WindDir, wind_x, 0.0f, wind_z);

    // Viewport size for vignette + color grading
    ub_fur_.set(U::ViewportSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));

    // PCSS uniforms (T4 Ultra)
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        ub_fur_.set(U::ShadowTexelSize, texel, texel);
        ub_fur_.set(U::LightSize, config_.light_size);
    }

    // Shadow uniforms (T2+)
    if (fc.has_shadows) {
        ub_fur_.set(U::LightVP, fc.light_vp);
        r->bindTextureUnit(3, res_.shadow.depth_tex);
        ub_fur_.set(U::ShadowMap, 3);
        ub_fur_.set(U::HasShadow, 1.0f);
    } else {
        ub_fur_.set(U::HasShadow, 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res_.core.fur_shader, fc);

    // Fur strand texture: unit 0
    r->bindTextureUnit(0, res_.core.fur_tex);
    ub_fur_.set(U::FurTex, 0);

    // Fur intensity mask: unit 1
    if (res_.core.fur_mask_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(1, res_.core.fur_mask_tex);
        ub_fur_.set(U::FurMask, 1);
        ub_fur_.set(U::HasFurMask, 1.0f);
    } else {
        ub_fur_.set(U::HasFurMask, 0.0f);
    }

    float tex_scale = config_.fur_density * 0.05f;
    ub_fur_.set(U::FurTexScale, tex_scale);

    // PBR material: fur should be rough and non-metallic
    ub_fur_.set(U::Metallic, 0.0f);
    ub_fur_.set(U::Roughness, 0.85f);

    ub_fur_.set(U::FurColorRoot, 0.30f, 0.18f, 0.08f);
    ub_fur_.set(U::FurColorTip, 0.72f, 0.55f, 0.32f);

    r->setDepthTest(true);
    r->setDepthMask(false); // fur shells don't write depth (opaque pass already did)
    r->setBlending(true);   // alpha blend for soft fur edges
    r->setCullFace(false);  // fur visible from both sides

    // Shell 0 is the base mesh (already rendered in opaque pass) -- skip it.
    // Shells 1..N-1 are the fur layers, rendered inner to outer.
    int num_shells = config_.fur_shells;

    // Check for instancing support (T2+ with GL3Features)
    GL3Features* g3 = r->features<GL3Features>();
    bool use_instancing = (tier_ >= DemoTier::Enhanced) && g3 && g3->hasInstancing();

    if (use_instancing) {
        ub_fur_.set(U::UseInstancing, 1.0f);
        ub_fur_.set(U::FurShells, static_cast<float>(num_shells));
        g3->drawMeshInstanced(model_mesh_, num_shells - 1);
    } else {
        ub_fur_.set(U::UseInstancing, 0.0f);
        for (int i = 1; i < num_shells; i++) {
            float shell_index = static_cast<float>(i) / static_cast<float>(num_shells - 1);
            ub_fur_.set(U::ShellIndex, shell_index);
            r->drawMesh(model_mesh_);
        }
    }

    r->setBlending(false);
    r->setCullFace(true);
    r->setDepthMask(true);
}

// ============================================================
// renderParticlePass: billboard dust motes
// ============================================================

void DemoScene::renderParticlePass(Renderer* r, const FrameContext& fc) {
    if (!res_.core.particle_shader || res_.core.particle_mesh == MeshHandle()) return;

    ub_particle_.use();
    ub_particle_.set(U::Proj, fc.proj);
    ub_particle_.set(U::View, fc.view);
    ub_particle_.set(U::Time, fc.time);

    r->setDepthTest(true);
    r->setDepthMask(false);  // don't write depth
    r->setBlending(true);
    r->setCullFace(false);

    r->drawMesh(res_.core.particle_mesh);

    r->setDepthMask(true);
    r->setBlending(false);
    r->setCullFace(true);
}

// ============================================================
// renderFrame: pipeline orchestrator
// ============================================================

void DemoScene::renderFrame(Renderer* r, float t, float time, int viewport_w, int viewport_h,
                            RenderTargetHandle dest_rt) {
    if (!initialized_) return;

    viewport_w_ = viewport_w;
    viewport_h_ = viewport_h;
    dest_rt_ = dest_rt;

    // Build frame context
    FrameContext fc;
    fc.tier_int = static_cast<int>(tier_);
    fc.time = time;
    fc.sun_dir = normalizeSafe(SUN_DIR_RAW);
    fc.has_shadows = config_.enable_shadows && res_.shadow.shader != nullptr
                     && res_.shadow.rt != INVALID_RENDER_TARGET;
    fc.has_bloom = config_.enable_bloom
                   && res_.bloom.extract_shader != nullptr
                   && res_.bloom.scene_rt != INVALID_RENDER_TARGET;
    fc.has_ssao = config_.enable_ssao
                  && res_.ssao.shader != nullptr
                  && res_.ssao.rt != INVALID_RENDER_TARGET;
    fc.has_pbr = config_.enable_pbr;
    fc.has_tessellation = config_.enable_tessellation && res_.t4.tess_shader != nullptr;
    fc.has_compute_particles = config_.enable_compute_particles
                               && res_.t4.compute_particle_shader != nullptr
                               && res_.t4.particle_ssbo != INVALID_BUFFER;
    fc.has_volumetric_fog = config_.enable_volumetric_fog
                            && res_.t4.volumetric_fog_shader != nullptr
                            && res_.t4.fog_rt != INVALID_RENDER_TARGET;
    fc.has_hdr = config_.enable_hdr
                 && res_.t4.tone_map_shader != nullptr
                 && res_.t4.hdr_scene_rt != INVALID_RENDER_TARGET;

    // Camera: Catmull-Rom spline path
    fc.cam_pos = camera_.getPosition(t);
    Vec3 cam_target = camera_.getTarget(t);
    float aspect = static_cast<float>(viewport_w) / static_cast<float>(viewport_h > 0 ? viewport_h : 1);
    fc.proj = Mat4::perspective(kDemoFovDeg, aspect, kDemoNear, kDemoFar);
    fc.view = Mat4::lookAt(fc.cam_pos, cam_target, Vec3(0.0f, 1.0f, 0.0f));

    Mat4 vp = fc.proj * fc.view;
    fc.frustum = extractFrustum(vp);

    // Log active render passes once per tier setup
    if (!passes_logged_) {
        passes_logged_ = true;
        LOG_DBG("Demo: tier %d passes: shadow=%d ssao=%d bloom=%d pbr=%d tess=%d "
                 "compute_particles=%d vol_fog=%d hdr=%d",
                 fc.tier_int, fc.has_shadows, fc.has_ssao, fc.has_bloom,
                 fc.has_pbr, fc.has_tessellation, fc.has_compute_particles,
                 fc.has_volumetric_fog, fc.has_hdr);
    }

    if (fc.has_hdr && !debug_.skip_hdr) {
        // T4 pipeline: auto-exposure -> compute particles -> shadow -> HDR scene ->
        //              GTAO/SSAO -> vol fog -> compute bloom -> HDR composite
        if (fc.has_compute_particles)
            renderComputeParticles(r, fc);
        if (fc.has_shadows) {
            computeLightMatrix(fc);
            renderShadowPass(r, fc);
        }
        // Render scene to HDR FBO
        r->bindRenderTarget(res_.t4.hdr_scene_rt);
        r->setViewport(0, 0, viewport_w_, viewport_h_);
        r->clear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f);
        r->setDepthTest(true);
        r->setCullFace(true);
        renderSky(r, fc);
        renderOpaquePass(r, fc);
        renderGrassInstanced(r, fc);
        if (fc.has_tessellation)
            renderTessellatedModel(r, fc);
        else
            renderFurPass(r, fc);
        if (fc.has_compute_particles)
            renderComputeParticlesDraw(r, fc);
        else
            renderParticlePass(r, fc);

        // Copy scene color to ssr_tex_ for water reflections, then render water
        if (config_.enable_ssr && res_.t4.ssr_tex != INVALID_TEXTURE) {
            // Copy current FBO color into ssr_tex_ for SSR lookups
            r->copyFramebufferToTexture(res_.t4.ssr_tex, viewport_w_, viewport_h_);
            // Render water with scene reflection
            renderWaterPass(r, fc);
        } else {
            // Fallback: render water without reflections
            renderWaterPass(r, fc);
        }

        r->bindRenderTarget(INVALID_RENDER_TARGET);

        // Auto-exposure (histogram from HDR scene)
        if (config_.enable_auto_exposure && !debug_.skip_auto_exposure)
            computeAutoExposure(r, fc);

        // AO: use Compute GTAO if available, otherwise fragment SSAO
        if (!debug_.skip_gtao && config_.enable_gtao && res_.t4.gtao_shader && res_.t4.gtao_tex != INVALID_TEXTURE) {
            renderGTAOPass(r, fc);
            renderGTAOBlur(r, fc);
        } else if (fc.has_ssao) {
            renderSSAOPass(r, fc);
            renderSSAOBlur(r, fc);
        }

        // Volumetric fog
        if (fc.has_volumetric_fog && !debug_.skip_vol_fog)
            renderVolumetricFog(r, fc);

        // Restore full-res viewport after half-res fog pass
        r->setViewport(0, 0, viewport_w_, viewport_h_);

        // Bloom: compute bloom if available, otherwise fragment bloom
        if (!debug_.skip_compute_bloom && config_.enable_compute_bloom && res_.t4.bloom_down_compute && res_.t4.bloom_mips[0] != INVALID_TEXTURE) {
            renderBloomCompute(r, fc);
        } else {
            renderBloomPasses(r, fc);
        }

        // DoF (after bloom, before composite)
        if (config_.enable_dof && res_.t4.dof_shader && !debug_.skip_dof)
            renderDoF(r, fc);

        // Ensure all compute writes are visible before fragment shader composite.
        // GL4.3 spec: imageStore writes need GL_TEXTURE_FETCH_BARRIER_BIT for
        // subsequent texture() reads.
        {
            GL4Features* g4 = r->features<GL4Features>();
            if (g4) {
                // Unbind ALL image units used by compute passes to prevent
                // leftover bindings from interfering with texture sampler state
                for (int iu = 0; iu < 4; iu++)
                    g4->bindImageTexture(INVALID_TEXTURE, iu, false, false);
            }
            ComputeFeatures* cf = r->features<ComputeFeatures>();
            if (cf) cf->computeMemoryBarrier();
        }

        // Restore destination render target for final composite output
        r->bindRenderTarget(dest_rt_);
        r->setViewport(0, 0, viewport_w_, viewport_h_);

        // HDR composite with tone mapping
        renderHDRComposite(r, fc);
    } else if (fc.has_bloom) {
        // T2/T3 pipeline: shadow -> scene FBO -> bloom -> composite
        if (fc.has_shadows) {
            computeLightMatrix(fc);
            renderShadowPass(r, fc);
        }
        renderSceneToFBO(r, fc);
        if (fc.has_ssao) {
            renderSSAOPass(r, fc);
            renderSSAOBlur(r, fc);
        }
        renderBloomPasses(r, fc);
        renderComposite(r, fc);
    } else {
        // T1 pipeline (unchanged)
        if (fc.has_shadows) {
            computeLightMatrix(fc);
            renderShadowPass(r, fc);
        }

        r->setViewport(0, 0, viewport_w, viewport_h);
        r->clear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f);
        r->setDepthTest(true);
        r->setCullFace(true);

        renderSky(r, fc);
        renderOpaquePass(r, fc);
        renderGrassInstanced(r, fc);
        renderFurPass(r, fc);
        renderParticlePass(r, fc);
    }

    // Restore state
    r->setDepthTest(true);
    r->setCullFace(true);
    r->setBlending(false);
    r->setColorMask(true, true, true, true);
}

// ============================================================
// renderSceneToFBO: render full scene into scene_rt for bloom
// ============================================================

void DemoScene::renderSceneToFBO(Renderer* r, const FrameContext& fc) {
    r->bindRenderTarget(res_.bloom.scene_rt);
    r->setViewport(0, 0, viewport_w_, viewport_h_);
    r->clear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f);
    r->setDepthTest(true);
    r->setCullFace(true);

    renderSky(r, fc);
    r->setBlending(false);
    renderOpaquePass(r, fc);
    renderGrassInstanced(r, fc);
    renderFurPass(r, fc);
    renderParticlePass(r, fc);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

// ============================================================
// renderSSAOPass: compute ambient occlusion from scene depth
// ============================================================

void DemoScene::renderSSAOPass(Renderer* r, const FrameContext& fc) {
    r->bindRenderTarget(res_.ssao.rt);
    r->setViewport(0, 0, viewport_w_ / 2, viewport_h_ / 2);
    r->clear(1.0f, 1.0f, 1.0f, 1.0f);  // white = no occlusion
    r->setDepthTest(false);
    r->setCullFace(false);

    ub_ssao_.use();

    // Bind depth texture from scene FBO
    r->bindTextureUnit(0, res_.ssao.scene_depth_tex);
    ub_ssao_.set(U::DepthTex, 0);

    // Bind noise texture
    r->bindTextureUnit(1, res_.ssao.noise_tex);
    ub_ssao_.set(U::NoiseTex, 1);

    // Projection parameters (perspective: fov=60, near=0.1, far=50)
    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(viewport_w_) / static_cast<float>(viewport_h_ > 0 ? viewport_h_ : 1);
    ub_ssao_.set(U::ScreenSize,
                 static_cast<float>(viewport_w_),
                 static_cast<float>(viewport_h_));
    ub_ssao_.set(U::Near, kDemoNear);
    ub_ssao_.set(U::Far, kDemoFar);
    ub_ssao_.set(U::Aspect, aspect);
    ub_ssao_.set(U::TanHalfFov, tanf(fov_rad * 0.5f));
    ub_ssao_.set(U::Radius, config_.ssao_radius);
    ub_ssao_.set(U::Bias, 0.025f);
    ub_ssao_.set(U::Intensity, config_.ssao_intensity);

    r->drawMesh(res_.bloom.fullscreen_quad);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

// ============================================================
// renderSSAOBlur: blur SSAO to smooth noise artifacts
// ============================================================

void DemoScene::renderSSAOBlur(Renderer* r, const FrameContext& fc) {
    (void)fc;
    r->bindRenderTarget(res_.ssao.blur_rt);
    r->setViewport(0, 0, viewport_w_ / 2, viewport_h_ / 2);
    r->setDepthTest(false);
    r->setCullFace(false);

    ub_ssao_blur_.use();

    r->bindRenderTargetTexture(res_.ssao.rt, 0);
    ub_ssao_blur_.set(U::SsaoTex, 0);
    ub_ssao_blur_.set(U::TexelSize,
                      2.0f / static_cast<float>(viewport_w_),
                      2.0f / static_cast<float>(viewport_h_));

    r->drawMesh(res_.bloom.fullscreen_quad);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

// ============================================================
// renderBloomPasses: extract bright, blur H, blur V (ping-pong)
// ============================================================

void DemoScene::renderBloomPasses(Renderer* r, const FrameContext& fc) {
    (void)fc;
    int bw = viewport_w_ / 2;
    int bh = viewport_h_ / 2;
    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;

    r->setDepthTest(false);
    r->setBlending(false);
    r->setCullFace(false);

    // Extract bright pixels (use HDR FBO if available, otherwise scene FBO)
    r->bindRenderTarget(res_.bloom.bright_rt);
    r->setViewport(0, 0, bw, bh);
    ub_bloom_extract_.use();
    RenderTargetHandle bloom_source = (res_.t4.hdr_scene_rt != INVALID_RENDER_TARGET)
                                       ? res_.t4.hdr_scene_rt : res_.bloom.scene_rt;
    r->bindRenderTargetTexture(bloom_source, 0);
    ub_bloom_extract_.set(U::SceneTex, 0);
    ub_bloom_extract_.set(U::Threshold, 0.8f);
    r->drawMesh(res_.bloom.fullscreen_quad);

    // Horizontal blur -> blur_rt
    r->bindRenderTarget(res_.bloom.blur_rt);
    ub_bloom_blur_.use();
    r->bindRenderTargetTexture(res_.bloom.bright_rt, 0);
    ub_bloom_blur_.set(U::Tex, 0);
    ub_bloom_blur_.set(U::Horizontal, 1);
    ub_bloom_blur_.set(U::TexelSize, 1.0f / static_cast<float>(bw),
                                      1.0f / static_cast<float>(bh));
    r->drawMesh(res_.bloom.fullscreen_quad);

    // Vertical blur -> bright_rt (ping-pong back)
    r->bindRenderTarget(res_.bloom.bright_rt);
    r->bindRenderTargetTexture(res_.bloom.blur_rt, 0);
    ub_bloom_blur_.set(U::Horizontal, 0);
    r->drawMesh(res_.bloom.fullscreen_quad);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
    r->setCullFace(true);
}

// ============================================================
// renderComposite: combine scene + bloom + vignette + color grade
// ============================================================

void DemoScene::renderComposite(Renderer* r, const FrameContext& fc) {
    // Restore destination render target for composite output
    r->bindRenderTarget(dest_rt_);
    r->setViewport(0, 0, viewport_w_, viewport_h_);
    r->setDepthTest(false);
    r->setCullFace(false);

    ub_bloom_composite_.use();
    r->bindRenderTargetTexture(res_.bloom.scene_rt, 0);
    ub_bloom_composite_.set(U::SceneTex, 0);
    r->bindRenderTargetTexture(res_.bloom.bright_rt, 1);
    ub_bloom_composite_.set(U::BloomTex, 1);
    ub_bloom_composite_.set(U::BloomStrength, res_.bloom.strength);
    ub_bloom_composite_.set(U::ViewportSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));

    // SSAO: bind blurred AO texture
    if (fc.has_ssao) {
        r->bindRenderTargetTexture(res_.ssao.blur_rt, 2);
        ub_bloom_composite_.set(U::SsaoTex, 2);
        ub_bloom_composite_.set(U::HasSsao, 1.0f);
    } else {
        ub_bloom_composite_.set(U::HasSsao, 0.0f);
    }

    r->drawMesh(res_.bloom.fullscreen_quad);

    r->setDepthTest(true);
    r->setCullFace(true);
}

// ============================================================
// T4: Compute particle physics update
// ============================================================

void DemoScene::renderComputeParticles(Renderer* r, const FrameContext& fc) {
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf) return;

    ub_compute_particle_.use();
    cf->bindSSBO(res_.t4.particle_ssbo, 0);
    ub_compute_particle_.set(U::Time, fc.time);
    ub_compute_particle_.set(U::Dt, 1.0f / 60.0f);
    ub_compute_particle_.set(U::EmitterPos, 0.0f, -0.5f, 0.0f);

    int groups = (res_.t4.compute_particle_count + 255) / 256;
    cf->dispatchCompute(groups, 1, 1);
    cf->computeMemoryBarrier();
}

// ============================================================
// T4: Draw compute particles as billboards
// ============================================================

void DemoScene::renderComputeParticlesDraw(Renderer* r, const FrameContext& fc) {
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf || !res_.t4.particle_render_shader) return;

    ub_particle_render_.use();
    cf->bindSSBO(res_.t4.particle_ssbo, 0);
    ub_particle_render_.set(U::Proj, fc.proj);
    ub_particle_render_.set(U::View, fc.view);

    // Camera right/up vectors from view matrix
    Vec3 cam_right(fc.view.m[0], fc.view.m[4], fc.view.m[8]);
    Vec3 cam_up(fc.view.m[1], fc.view.m[5], fc.view.m[9]);
    ub_particle_render_.set(U::CamRight, cam_right);
    ub_particle_render_.set(U::CamUp, cam_up);

    r->setDepthTest(true);
    r->setDepthMask(false);
    r->setBlending(true);
    r->setCullFace(false);

    // Draw quads: 6 vertices per particle (2 triangles each, no strip artifacts)
    glDrawArrays(GL_TRIANGLES, 0, res_.t4.compute_particle_count * 6);

    r->setDepthMask(true);
    r->setBlending(false);
    r->setCullFace(true);
}

// ============================================================
// T4: Tessellated model rendering
// ============================================================

void DemoScene::renderTessellatedModel(Renderer* r, const FrameContext& fc) {
    GL4Features* g4 = r->features<GL4Features>();
    if (!g4 || model_mesh_ == MeshHandle()) return;

    ub_tess_.use();
    ub_tess_.set(U::Proj, fc.proj);
    ub_tess_.set(U::View, fc.view);
    ub_tess_.set(U::Model, model_transform_);
    ub_tess_.set(U::LightDir, fc.sun_dir);
    ub_tess_.set(U::CamPos, fc.cam_pos);
    ub_tess_.set(U::FogColor, FOG_COLOR);
    ub_tess_.set(U::FogDensity, config_.fog_density);
    ub_tess_.set(U::Time, fc.time);
    ub_tess_.set(U::TessInner, static_cast<float>(config_.tess_level));
    ub_tess_.set(U::TessOuter, static_cast<float>(config_.tess_level));
    ub_tess_.set(U::DisplacementStr, config_.displacement_strength);
    ub_tess_.set(U::Metallic, 0.0f);
    ub_tess_.set(U::Roughness, 0.6f);
    ub_tess_.set(U::MatColor, 0.55f, 0.35f, 0.20f);
    ub_tess_.set(U::ProcTex, 0.0f);
    ub_tess_.set(U::NormalStrength, 0.0f);

    // PCSS / SSS uniforms
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        ub_tess_.set(U::ShadowTexelSize, texel, texel);
        ub_tess_.set(U::LightSize, config_.light_size);
    }
    if (config_.enable_sss) {
        ub_tess_.set(U::SssStrength, config_.sss_strength);
    } else {
        ub_tess_.set(U::SssStrength, 0.0f);
    }

    if (fc.has_shadows) {
        ub_tess_.set(U::LightVP, fc.light_vp);
        r->bindTextureUnit(3, res_.shadow.depth_tex);
        ub_tess_.set(U::ShadowMap, 3);
        ub_tess_.set(U::HasShadow, 1.0f);
    } else {
        ub_tess_.set(U::HasShadow, 0.0f);
    }

    setPointLightUniforms(res_.t4.tess_shader, fc);
    ub_tess_.set(U::HasNormalMap, 0.0f);

    g4->setPatchVertices(3);
    r->setDepthTest(true);
    r->setCullFace(true);
    g4->drawMeshAsPatches(model_mesh_);

    // Still draw fur on top of tessellated model
    renderFurPass(r, fc);
}

// ============================================================
// T4: Volumetric fog raymarch
// ============================================================

void DemoScene::renderVolumetricFog(Renderer* r, const FrameContext& fc) {
    r->bindRenderTarget(res_.t4.fog_rt);
    r->setViewport(0, 0, viewport_w_ / 2, viewport_h_ / 2);
    r->clear(0.0f, 0.0f, 0.0f, 0.0f);
    r->setDepthTest(false);
    r->setCullFace(false);

    ub_vol_fog_.use();

    r->bindTextureUnit(0, res_.t4.hdr_depth_tex);
    ub_vol_fog_.set(U::DepthTex, 0);

    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(viewport_w_) / static_cast<float>(viewport_h_ > 0 ? viewport_h_ : 1);
    ub_vol_fog_.set(U::Near, kDemoNear);
    ub_vol_fog_.set(U::Far, kDemoFar);
    ub_vol_fog_.set(U::Aspect, aspect);
    ub_vol_fog_.set(U::TanHalfFov, tanf(fov_rad * 0.5f));
    ub_vol_fog_.set(U::SunDir, fc.sun_dir);
    ub_vol_fog_.set(U::CamPos, fc.cam_pos);
    ub_vol_fog_.set(U::Time, fc.time);
    ub_vol_fog_.set(U::FogDensity, config_.fog_density * 0.5f);
    ub_vol_fog_.set(U::FogColor, FOG_COLOR);
    ub_vol_fog_.set(U::FogSteps, config_.fog_steps);

    // Inverse view matrix (column-major: rotation = transpose, translation = -R^T * t)
    Mat4 vi;
    // Transpose 3x3 rotation
    vi.m[0] = fc.view.m[0]; vi.m[1] = fc.view.m[4]; vi.m[2] = fc.view.m[8];  vi.m[3]  = 0;
    vi.m[4] = fc.view.m[1]; vi.m[5] = fc.view.m[5]; vi.m[6] = fc.view.m[9];  vi.m[7]  = 0;
    vi.m[8] = fc.view.m[2]; vi.m[9] = fc.view.m[6]; vi.m[10] = fc.view.m[10]; vi.m[11] = 0;
    // Translation: -R^T * t  (t is in column 3: m[12], m[13], m[14])
    vi.m[12] = -(vi.m[0]*fc.view.m[12] + vi.m[4]*fc.view.m[13] + vi.m[8]*fc.view.m[14]);
    vi.m[13] = -(vi.m[1]*fc.view.m[12] + vi.m[5]*fc.view.m[13] + vi.m[9]*fc.view.m[14]);
    vi.m[14] = -(vi.m[2]*fc.view.m[12] + vi.m[6]*fc.view.m[13] + vi.m[10]*fc.view.m[14]);
    vi.m[15] = 1;
    ub_vol_fog_.set(U::ViewInv, vi);

    r->drawMesh(res_.bloom.fullscreen_quad);
    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

// ============================================================
// T4 Ultra: Compute GTAO pass
// ============================================================

void DemoScene::renderGTAOPass(Renderer* r, const FrameContext& fc) {
    if (!res_.t4.gtao_shader || res_.t4.gtao_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    ub_gtao_.use();

    r->bindTextureUnit(0, res_.t4.hdr_depth_tex);
    ub_gtao_.set(U::DepthTex, 0);

    g4->bindImageTexture(res_.t4.gtao_tex, 0, false, true); // write-only

    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(viewport_w_) / static_cast<float>(viewport_h_ > 0 ? viewport_h_ : 1);
    ub_gtao_.set(U::ScreenSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    ub_gtao_.set(U::Near, kDemoNear);
    ub_gtao_.set(U::Far, kDemoFar);
    ub_gtao_.set(U::Aspect, aspect);
    ub_gtao_.set(U::TanHalfFov, tanf(fov_rad * 0.5f));
    ub_gtao_.set(U::AoRadius, config_.ssao_radius);
    ub_gtao_.set(U::AoIntensity, config_.ssao_intensity);

    int gx = (viewport_w_ + 15) / 16;
    int gy = (viewport_h_ + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    g4->imageMemoryBarrier();
}

// ============================================================
// T4 Ultra: Compute GTAO bilateral blur
// ============================================================

void DemoScene::renderGTAOBlur(Renderer* r, const FrameContext& fc) {
    (void)fc;
    if (!res_.t4.gtao_blur_shader || res_.t4.gtao_blur_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    ub_gtao_blur_.use();

    g4->bindImageTexture(res_.t4.gtao_tex, 0, true, false);      // read-only input
    g4->bindImageTexture(res_.t4.gtao_blur_tex, 1, false, true);  // write-only output

    r->bindTextureUnit(0, res_.t4.hdr_depth_tex);
    ub_gtao_blur_.set(U::DepthTex, 0);
    ub_gtao_blur_.set(U::ScreenSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    ub_gtao_blur_.set(U::Near, kDemoNear);
    ub_gtao_blur_.set(U::Far, kDemoFar);

    int gx = (viewport_w_ + 15) / 16;
    int gy = (viewport_h_ + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    g4->imageMemoryBarrier();
}

// ============================================================
// T4 Ultra: Compute Bloom (mip chain downsample + upsample)
// ============================================================

void DemoScene::renderBloomCompute(Renderer* r, const FrameContext& fc) {
    (void)fc;
    if (!res_.t4.bloom_down_compute || !res_.t4.bloom_up_compute) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    static const int MIP_COUNT = TierResourceView::T4::BLOOM_MIP_COUNT;

    // Verify all mips exist
    for (int i = 0; i < MIP_COUNT; i++) {
        if (res_.t4.bloom_mips[i] == INVALID_TEXTURE) return;
    }

    // --- Downsample chain ---
    ub_bloom_down_.use();

    for (int i = 0; i < MIP_COUNT; i++) {
        // Source: either HDR scene texture (level 0) or previous mip
        if (i == 0) {
            r->bindRenderTargetTexture(res_.t4.hdr_scene_rt, 0);
            ub_bloom_down_.set(U::SrcSize,
                static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
            ub_bloom_down_.set(U::FirstPass, 1); // Karis average
            ub_bloom_down_.set(U::BloomThreshold, 1.5f);
        } else {
            r->bindTextureUnit(0, res_.t4.bloom_mips[i - 1]);
            int prev_w = viewport_w_ >> i;
            int prev_h = viewport_h_ >> i;
            if (prev_w < 1) prev_w = 1;
            if (prev_h < 1) prev_h = 1;
            ub_bloom_down_.set(U::SrcSize,
                static_cast<float>(prev_w), static_cast<float>(prev_h));
            ub_bloom_down_.set(U::FirstPass, 0);
        }
        ub_bloom_down_.set(U::SrcTex, 0);

        g4->bindImageTexture(res_.t4.bloom_mips[i], 0, false, true); // write-only

        int mw = viewport_w_ >> (i + 1);
        int mh = viewport_h_ >> (i + 1);
        if (mw < 1) mw = 1;
        if (mh < 1) mh = 1;

        int gx = (mw + 15) / 16;
        int gy = (mh + 15) / 16;
        cf->dispatchCompute(gx, gy, 1);
        g4->imageMemoryBarrier();
    }

    // --- Upsample chain (bottom-up, additive) ---
    ub_bloom_up_.use();
    ub_bloom_up_.set(U::BloomRadius, 1.0f);

    for (int i = MIP_COUNT - 2; i >= 0; i--) {
        // Source: lower (smaller) mip
        r->bindTextureUnit(0, res_.t4.bloom_mips[i + 1]);
        ub_bloom_up_.set(U::SrcTex, 0);

        // Destination: current mip (read-write for additive blend)
        g4->bindImageTexture(res_.t4.bloom_mips[i], 0, true, true);

        int mw = viewport_w_ >> (i + 1);
        int mh = viewport_h_ >> (i + 1);
        if (mw < 1) mw = 1;
        if (mh < 1) mh = 1;

        int gx = (mw + 15) / 16;
        int gy = (mh + 15) / 16;
        cf->dispatchCompute(gx, gy, 1);
        g4->imageMemoryBarrier();
    }
}

// ============================================================
// T4 Ultra: Compute Auto-Exposure (histogram)
// ============================================================

void DemoScene::computeAutoExposure(Renderer* r, const FrameContext& fc) {
    if (!res_.t4.histogram_shader || !res_.t4.exposure_shader) return;
    if (res_.t4.histogram_ssbo == INVALID_BUFFER || res_.t4.exposure_ssbo == INVALID_BUFFER) return;

    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf) return;

    float min_log_lum = -8.0f;
    float log_lum_range = 12.0f;

    // Step 1: Build histogram
    ub_histogram_.use();
    r->bindRenderTargetTexture(res_.t4.hdr_scene_rt, 0);
    ub_histogram_.set(U::SceneTex, 0);
    ub_histogram_.set(U::ScreenSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    ub_histogram_.set(U::MinLogLum, min_log_lum);
    ub_histogram_.set(U::LogLumRange, log_lum_range);

    cf->bindSSBO(res_.t4.histogram_ssbo, 1);

    int gx = (viewport_w_ + 15) / 16;
    int gy = (viewport_h_ + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    cf->computeMemoryBarrier();

    // Step 2: Reduce histogram to exposure value
    ub_exposure_.use();
    cf->bindSSBO(res_.t4.histogram_ssbo, 1);
    cf->bindSSBO(res_.t4.exposure_ssbo, 2);
    ub_exposure_.set(U::MinLogLum, min_log_lum);
    ub_exposure_.set(U::LogLumRange, log_lum_range);
    ub_exposure_.set(U::TotalPixels, static_cast<float>(viewport_w_ * viewport_h_));
    ub_exposure_.set(U::AdaptSpeed, 2.0f);
    ub_exposure_.set(U::Dt, 1.0f / 60.0f);

    cf->dispatchCompute(1, 1, 1);
    cf->computeMemoryBarrier();
}

// ============================================================
// T4 Ultra: Water pass with screen-space reflections
// ============================================================

void DemoScene::renderWaterPass(Renderer* r, const FrameContext& fc) {
    if (!res_.core.island_shader) return;

    // Count water objects
    bool has_water = false;
    for (size_t i = 0; i < opaque_objects_.size(); i++) {
        if (opaque_objects_[i].is_water) { has_water = true; break; }
    }
    if (!has_water) return;

    ub_island_.use();

    // Set all standard uniforms (same as renderOpaquePass)
    ub_island_.set(U::Proj, fc.proj);
    ub_island_.set(U::View, fc.view);
    ub_island_.set(U::LightDir, fc.sun_dir);
    ub_island_.set(U::CamPos, fc.cam_pos);
    ub_island_.set(U::FogColor, FOG_COLOR);
    ub_island_.set(U::FogDensity, config_.fog_density);
    ub_island_.set(U::Time, fc.time);
    ub_island_.set(U::NormalStrength, 0.0f);

    if (config_.enable_pbr) {
        ub_island_.set(U::Metallic, 0.0f);
        ub_island_.set(U::Roughness, 0.02f);
    }
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        ub_island_.set(U::ShadowTexelSize, texel, texel);
        ub_island_.set(U::LightSize, config_.light_size);
    }
    ub_island_.set(U::SssStrength, 0.0f);

    if (fc.has_shadows) {
        ub_island_.set(U::LightVP, fc.light_vp);
        r->bindTextureUnit(3, res_.shadow.depth_tex);
        ub_island_.set(U::ShadowMap, 3);
        ub_island_.set(U::HasShadow, 1.0f);
    } else {
        ub_island_.set(U::HasShadow, 0.0f);
    }
    ub_island_.set(U::HasNormalMap, 0.0f);
    ub_island_.set(U::PointLightCount, 0);

    // Bind scene copy for water reflections (ssr_tex_ has the scene before water)
    bool has_reflection = (res_.t4.ssr_tex != INVALID_TEXTURE);
    if (has_reflection) {
        r->bindTextureUnit(5, res_.t4.ssr_tex);
        ub_island_.set(U::ReflectionTex, 5);
        r->bindTextureUnit(6, res_.t4.hdr_depth_tex);
        ub_island_.set(U::DepthTex, 6);
        ub_island_.set(U::HasReflection, 1.0f);
        ub_island_.set(U::ScreenSize,
            static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));

        ub_island_.set(U::Near, kDemoNear);
        ub_island_.set(U::Far, kDemoFar);
    } else {
        ub_island_.set(U::HasReflection, 0.0f);
    }

    // Render only water objects
    for (size_t i = 0; i < opaque_objects_.size(); i++) {
        const SceneObject& obj = opaque_objects_[i];
        if (!obj.is_water) continue;
        if (!sphereInFrustum(fc.frustum, obj.bounds_center, obj.bounds_radius))
            continue;

        ub_island_.set(U::Model, obj.transform);
        ub_island_.set(U::MatColor, obj.color);
        ub_island_.set(U::MatSpec, obj.specular);
        ub_island_.set(U::Alpha, 1.0f);
        ub_island_.set(U::ProcTex, 0.0f);
        ub_island_.set(U::VertexWind, 0.0f);
        ub_island_.set(U::IsWater, 1.0f);

        if (obj.two_sided) r->setCullFace(false);
        r->drawMesh(obj.mesh);
        if (obj.two_sided) r->setCullFace(true);
    }
}

// ============================================================
// T4 Ultra: Screen-Space Reflections (compute — legacy)
// ============================================================

void DemoScene::renderSSR(Renderer* r, const FrameContext& fc) {
    if (!res_.t4.ssr_shader || res_.t4.ssr_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    ub_ssr_.use();

    r->bindRenderTargetTexture(res_.t4.hdr_scene_rt, 0);
    ub_ssr_.set(U::SceneTex, 0);
    r->bindTextureUnit(1, res_.t4.hdr_depth_tex);
    ub_ssr_.set(U::DepthTex, 1);

    g4->bindImageTexture(res_.t4.ssr_tex, 0, false, true); // write-only

    ub_ssr_.set(U::Proj, fc.proj);

    // Inverse view matrix (column-major: rotation = transpose, translation = -R^T * t)
    Mat4 vi;
    // Transpose 3x3 rotation
    vi.m[0] = fc.view.m[0]; vi.m[1] = fc.view.m[4]; vi.m[2] = fc.view.m[8];  vi.m[3]  = 0;
    vi.m[4] = fc.view.m[1]; vi.m[5] = fc.view.m[5]; vi.m[6] = fc.view.m[9];  vi.m[7]  = 0;
    vi.m[8] = fc.view.m[2]; vi.m[9] = fc.view.m[6]; vi.m[10] = fc.view.m[10]; vi.m[11] = 0;
    // Translation: -R^T * t  (t is in column 3: m[12], m[13], m[14])
    vi.m[12] = -(vi.m[0]*fc.view.m[12] + vi.m[4]*fc.view.m[13] + vi.m[8]*fc.view.m[14]);
    vi.m[13] = -(vi.m[1]*fc.view.m[12] + vi.m[5]*fc.view.m[13] + vi.m[9]*fc.view.m[14]);
    vi.m[14] = -(vi.m[2]*fc.view.m[12] + vi.m[6]*fc.view.m[13] + vi.m[10]*fc.view.m[14]);
    vi.m[15] = 1;
    ub_ssr_.set(U::ViewInv, vi);

    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(viewport_w_) / static_cast<float>(viewport_h_ > 0 ? viewport_h_ : 1);
    ub_ssr_.set(U::ScreenSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    ub_ssr_.set(U::Near, kDemoNear);
    ub_ssr_.set(U::Far, kDemoFar);
    ub_ssr_.set(U::Aspect, aspect);
    ub_ssr_.set(U::TanHalfFov, tanf(fov_rad * 0.5f));

    // Pass puddle positions so SSR skips water (water has its own reflections)
    // Array uniforms kept as string-based calls
    if (config_.enable_ssr) {
        ub_ssr_.set(U::PuddleCount, 3);
        res_.t4.ssr_shader->set3f("u_puddle_pos[0]", 2.5f, 0.0f, 0.8f);
        res_.t4.ssr_shader->set3f("u_puddle_pos[1]", -1.8f, 0.0f, 2.2f);
        res_.t4.ssr_shader->set3f("u_puddle_pos[2]", 0.5f, 0.0f, -2.5f);
        res_.t4.ssr_shader->set1f("u_puddle_radius[0]", 1.5f);
        res_.t4.ssr_shader->set1f("u_puddle_radius[1]", 1.2f);
        res_.t4.ssr_shader->set1f("u_puddle_radius[2]", 1.0f);
    } else {
        ub_ssr_.set(U::PuddleCount, 0);
    }

    int gx = (viewport_w_ + 15) / 16;
    int gy = (viewport_h_ + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    g4->imageMemoryBarrier();
}

// ============================================================
// T4 Ultra: Depth of Field
// ============================================================

void DemoScene::renderDoF(Renderer* r, const FrameContext& fc) {
    (void)fc;
    if (!res_.t4.dof_shader || res_.t4.dof_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    ub_dof_.use();

    r->bindRenderTargetTexture(res_.t4.hdr_scene_rt, 0);
    ub_dof_.set(U::SceneTex, 0);
    r->bindTextureUnit(1, res_.t4.hdr_depth_tex);
    ub_dof_.set(U::DepthTex, 1);

    g4->bindImageTexture(res_.t4.dof_tex, 0, false, true); // write-only

    ub_dof_.set(U::ScreenSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    ub_dof_.set(U::Near, kDemoNear);
    ub_dof_.set(U::Far, kDemoFar);
    ub_dof_.set(U::FocalDistance, config_.dof_focal_distance);
    ub_dof_.set(U::FocalRange, 5.0f);
    ub_dof_.set(U::MaxBlur, 5.0f);
    ub_dof_.set(U::DofStrength, config_.dof_strength);

    int gx = (viewport_w_ + 15) / 16;
    int gy = (viewport_h_ + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    g4->imageMemoryBarrier();
}

// ============================================================
// T4: HDR composite with ACES tone mapping
// ============================================================

void DemoScene::renderHDRComposite(Renderer* r, const FrameContext& fc) {
    r->setViewport(0, 0, viewport_w_, viewport_h_);
    r->setDepthTest(false);
    r->setCullFace(false);
    r->setBlending(false);

    ub_tone_map_.use();

    r->bindRenderTargetTexture(res_.t4.hdr_scene_rt, 0);
    ub_tone_map_.set(U::SceneTex, 0);

    // Bloom: prefer compute bloom mip[0], otherwise fragment bloom
    if (config_.enable_compute_bloom && res_.t4.bloom_mips[0] != INVALID_TEXTURE) {
        r->bindTextureUnit(1, res_.t4.bloom_mips[0]);
    } else {
        r->bindRenderTargetTexture(res_.bloom.bright_rt, 1);
    }
    ub_tone_map_.set(U::BloomTex, 1);
    ub_tone_map_.set(U::BloomStrength, res_.bloom.strength);

    // AO: prefer compute GTAO, otherwise fragment SSAO
    if (config_.enable_gtao && res_.t4.gtao_blur_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(2, res_.t4.gtao_blur_tex);
        ub_tone_map_.set(U::SsaoTex, 2);
        ub_tone_map_.set(U::HasSsao, 1.0f);
    } else if (fc.has_ssao) {
        r->bindRenderTargetTexture(res_.ssao.blur_rt, 2);
        ub_tone_map_.set(U::SsaoTex, 2);
        ub_tone_map_.set(U::HasSsao, 1.0f);
    } else {
        ub_tone_map_.set(U::HasSsao, 0.0f);
    }

    if (fc.has_volumetric_fog) {
        r->bindRenderTargetTexture(res_.t4.fog_rt, 3);
        ub_tone_map_.set(U::FogTex, 3);
        ub_tone_map_.set(U::HasFog, 1.0f);
    } else {
        ub_tone_map_.set(U::HasFog, 0.0f);
    }

    // SSR texture
    if (config_.enable_ssr && res_.t4.ssr_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(4, res_.t4.ssr_tex);
        ub_tone_map_.set(U::SsrTex, 4);
        ub_tone_map_.set(U::HasSsr, 1.0f);
    } else {
        ub_tone_map_.set(U::HasSsr, 0.0f);
    }

    // DoF texture
    if (config_.enable_dof && res_.t4.dof_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(5, res_.t4.dof_tex);
        ub_tone_map_.set(U::DofTex, 5);
        ub_tone_map_.set(U::HasDof, 1.0f);
    } else {
        ub_tone_map_.set(U::HasDof, 0.0f);
    }

    ub_tone_map_.set(U::ViewportSize,
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    ub_tone_map_.set(U::Time, fc.time);

    // Auto-exposure: read exposure from SSBO
    float exposure = 1.0f;
    if (config_.enable_auto_exposure && res_.t4.exposure_ssbo != INVALID_BUFFER) {
        ComputeFeatures* cf = r->features<ComputeFeatures>();
        if (cf) {
            cf->readSSBO(res_.t4.exposure_ssbo, &exposure, 0, sizeof(float));
            if (exposure < 0.01f) exposure = 1.0f;
        }
    }
    ub_tone_map_.set(U::Exposure, exposure);
    ub_tone_map_.set(U::ChromaticStrength, config_.chromatic_strength);
    ub_tone_map_.set(U::GrainStrength, config_.grain_strength);

    r->drawMesh(res_.bloom.fullscreen_quad);

    r->setDepthTest(true);
    r->setCullFace(true);
}
