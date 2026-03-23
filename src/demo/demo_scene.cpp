#include "demo/demo_scene.h"
#include "demo/shader_loader.h"
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
    Log::info("Demo scene: %d objects, fur shells=%d, particles=%d",
              total, config_.fur_shells, config_.particle_count);
}

// ============================================================
// placeModel: use pre-loaded model from resources
// ============================================================

void DemoScene::placeModel(Renderer* r) {
    (void)r;

    model_mesh_ = res_.model_mesh;

    model_transform_ = Mat4();

    SceneObject obj;
    obj.mesh = res_.model_mesh;
    obj.transform = model_transform_;
    obj.material = MaterialType::Model;
    obj.color = Vec3(0.55f, 0.38f, 0.32f);  // pinkish skin visible under fur
    obj.specular = 0.08f;
    setBounds(obj, res_.model_bounding_radius);
    opaque_objects_.push_back(obj);
}

// ============================================================
// placeGroundPlane: use pre-loaded ground mesh from resources
// ============================================================

void DemoScene::placeGroundPlane(Renderer* r) {
    (void)r;

    SceneObject ground;
    ground.mesh = res_.ground_mesh;
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
    if (res_.rock_mesh == MeshHandle()) return;

    SceneObject rocks;
    rocks.mesh = res_.rock_mesh;
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

    if (res_.grass_mesh == MeshHandle()) return;

    SceneObject grass;
    grass.mesh = res_.grass_mesh;
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
    if (res_.puddle_meshes[0] == MeshHandle()) return;
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
        puddle.mesh = res_.puddle_meshes[i];
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
    viewport_w_ = viewport_w;
    viewport_h_ = viewport_h;
    res_ = resources;

    int t = static_cast<int>(tier);
    Log::info("Demo scene setup: tier %d, viewport %dx%d", t, viewport_w, viewport_h);

    // Build scene objects (cheap -- just fills SceneObject structs)
    buildScene(r);

    initialized_ = true;
    int total_obj = static_cast<int>(opaque_objects_.size());
    Log::info("Demo scene setup complete: %d objects", total_obj);
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
    if (!res_.shadow_shader || res_.shadow_rt == INVALID_RENDER_TARGET) return;

    r->bindRenderTarget(res_.shadow_rt);
    r->setViewport(0, 0, res_.shadow_map_size, res_.shadow_map_size);
    r->clear(1.0f, 1.0f, 1.0f, 1.0f);
    r->setDepthTest(true);
    r->setDepthMask(true);
    r->setCullFace(true);

    res_.shadow_shader->use();
    res_.shadow_shader->setMat4("u_light_vp", fc.light_vp);

    // Render all opaque objects into shadow map
    // (model base mesh is already in opaque_objects_, so fur also casts shadow)
    for (size_t i = 0; i < opaque_objects_.size(); i++) {
        const SceneObject& obj = opaque_objects_[i];
        res_.shadow_shader->setMat4("u_model", obj.transform);
        r->drawMesh(obj.mesh);
    }

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

// ============================================================
// renderSky
// ============================================================

void DemoScene::renderSky(Renderer* r, const FrameContext& fc) {
    if (!res_.sky_shader || res_.sky_mesh == MeshHandle()) return;

    r->setDepthTest(false);
    r->setCullFace(false);

    res_.sky_shader->use();
    res_.sky_shader->setMat4("u_proj", fc.proj);
    res_.sky_shader->setMat4("u_view", fc.view);
    res_.sky_shader->set3f("u_sun_dir", fc.sun_dir.x, fc.sun_dir.y, fc.sun_dir.z);
    res_.sky_shader->set1f("u_time", fc.time);

    r->drawMesh(res_.sky_mesh);

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
    if (!res_.island_shader) return;

    res_.island_shader->use();

    res_.island_shader->setMat4("u_proj", fc.proj);
    res_.island_shader->setMat4("u_view", fc.view);
    res_.island_shader->set3f("u_light_dir", fc.sun_dir.x, fc.sun_dir.y, fc.sun_dir.z);
    res_.island_shader->set3f("u_cam_pos", fc.cam_pos.x, fc.cam_pos.y, fc.cam_pos.z);
    res_.island_shader->set3f("u_fog_color", FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z);
    res_.island_shader->set1f("u_fog_density", config_.fog_density);
    res_.island_shader->set1f("u_time", fc.time);
    res_.island_shader->set1f("u_normal_strength", config_.enable_normal_maps ? config_.normal_map_strength : 0.0f);

    // Wind direction (same as fur pass for consistency)
    if (config_.enable_wind) {
        float wind_x = sinf(fc.time * 0.7f) * 1.8f;
        float wind_z = cosf(fc.time * 0.5f) * 1.2f;
        res_.island_shader->set3f("u_wind_dir", wind_x, 0.0f, wind_z);
    } else {
        res_.island_shader->set3f("u_wind_dir", 0.0f, 0.0f, 0.0f);
    }

    // Viewport size for vignette + color grading
    res_.island_shader->set2f("u_viewport_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));

    // PBR default uniforms (T4) — overridden per-object if set
    float default_metallic = 0.0f;
    float default_roughness = 0.6f;
    if (config_.enable_pbr) {
        res_.island_shader->set1f("u_metallic", default_metallic);
        res_.island_shader->set1f("u_roughness", default_roughness);
    }

    // PCSS / SSS uniforms (T4 Ultra)
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        res_.island_shader->set2f("u_shadow_texel_size", texel, texel);
        res_.island_shader->set1f("u_light_size", config_.light_size);
    }
    if (config_.enable_sss) {
        res_.island_shader->set1f("u_sss_strength", config_.sss_strength);
    } else {
        res_.island_shader->set1f("u_sss_strength", 0.0f);
    }

    // Shadow uniforms (T2+)
    if (fc.has_shadows) {
        res_.island_shader->setMat4("u_light_vp", fc.light_vp);
        r->bindTextureUnit(3, res_.shadow_depth_tex);
        res_.island_shader->set1i("u_shadow_map", 3);
        res_.island_shader->set1f("u_has_shadow", 1.0f);
    } else {
        res_.island_shader->set1f("u_has_shadow", 0.0f);
    }

    // Normal map texture (T3+)
    if (res_.normal_map_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(4, res_.normal_map_tex);
        res_.island_shader->set1i("u_normal_map", 4);
        res_.island_shader->set1f("u_has_normal_map", 1.0f);
    } else {
        res_.island_shader->set1f("u_has_normal_map", 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res_.island_shader, fc);

    for (size_t i = 0; i < opaque_objects_.size(); i++) {
        const SceneObject& obj = opaque_objects_[i];
        // Skip water — rendered separately in renderWaterPass after scene copy
        if (obj.is_water) continue;
        if (!sphereInFrustum(fc.frustum, obj.bounds_center, obj.bounds_radius))
            continue;

        res_.island_shader->setMat4("u_model", obj.transform);
        res_.island_shader->set3f("u_mat_color", obj.color.x, obj.color.y, obj.color.z);
        res_.island_shader->set1f("u_mat_spec", obj.specular);
        res_.island_shader->set1f("u_alpha", 1.0f);
        res_.island_shader->set1f("u_proc_tex", obj.material == MaterialType::Island ? 1.0f : 0.0f);
        res_.island_shader->set1f("u_vertex_wind", obj.vertex_wind ? 1.0f : 0.0f);

        // Per-object PBR overrides
        if (config_.enable_pbr && obj.metallic >= 0.0f) {
            res_.island_shader->set1f("u_metallic", obj.metallic);
            res_.island_shader->set1f("u_roughness", obj.roughness);
        }

        // Water flag
        res_.island_shader->set1f("u_is_water", 0.0f);

        if (obj.two_sided) r->setCullFace(false);
        r->drawMesh(obj.mesh);
        if (obj.two_sided) r->setCullFace(true);

        // Restore defaults after per-object override
        if (config_.enable_pbr && obj.metallic >= 0.0f) {
            res_.island_shader->set1f("u_metallic", default_metallic);
            res_.island_shader->set1f("u_roughness", default_roughness);
        }
    }
}

// ============================================================
// renderGrassInstanced: T2+ instanced grass blades
// ============================================================

void DemoScene::renderGrassInstanced(Renderer* r, const FrameContext& fc) {
    if (!res_.grass_shader || res_.grass_blade_mesh == MeshHandle()) return;
    if (config_.instanced_grass_count <= 0) return;

    GL3Features* g3 = r->features<GL3Features>();
    if (!g3 || !g3->hasInstancing()) return;

    res_.grass_shader->use();
    res_.grass_shader->setMat4("u_proj", fc.proj);
    res_.grass_shader->setMat4("u_view", fc.view);
    res_.grass_shader->setMat4("u_model", Mat4());
    res_.grass_shader->set3f("u_light_dir", fc.sun_dir.x, fc.sun_dir.y, fc.sun_dir.z);
    res_.grass_shader->set3f("u_cam_pos", fc.cam_pos.x, fc.cam_pos.y, fc.cam_pos.z);
    res_.grass_shader->set3f("u_fog_color", FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z);
    res_.grass_shader->set1f("u_fog_density", config_.fog_density);
    res_.grass_shader->set1f("u_time", fc.time);
    res_.grass_shader->set1i("u_grass_count", config_.instanced_grass_count);
    res_.grass_shader->set1f("u_area_size", config_.grass_area_size);

    // Wind (same as fur)
    if (config_.enable_wind) {
        float wind_x = sinf(fc.time * 0.7f) * 1.8f;
        float wind_z = cosf(fc.time * 0.5f) * 1.2f;
        res_.grass_shader->set3f("u_wind_dir", wind_x, 0.0f, wind_z);
    } else {
        res_.grass_shader->set3f("u_wind_dir", 0.0f, 0.0f, 0.0f);
    }

    // PCSS uniforms (T4 Ultra)
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        res_.grass_shader->set2f("u_shadow_texel_size", texel, texel);
        res_.grass_shader->set1f("u_light_size", config_.light_size);
    }

    // Puddle exclusion zones (T4 Ultra)
    if (config_.enable_ssr) {
        res_.grass_shader->set1i("u_puddle_count", 3);
        res_.grass_shader->set3f("u_puddle_pos[0]", 2.5f, 0.0f, 0.8f);
        res_.grass_shader->set3f("u_puddle_pos[1]", -1.8f, 0.0f, 2.2f);
        res_.grass_shader->set3f("u_puddle_pos[2]", 0.5f, 0.0f, -2.5f);
        res_.grass_shader->set1f("u_puddle_radius[0]", 1.5f);
        res_.grass_shader->set1f("u_puddle_radius[1]", 1.2f);
        res_.grass_shader->set1f("u_puddle_radius[2]", 1.0f);
    } else {
        res_.grass_shader->set1i("u_puddle_count", 0);
    }

    // Shadow
    if (fc.has_shadows) {
        res_.grass_shader->setMat4("u_light_vp", fc.light_vp);
        r->bindTextureUnit(3, res_.shadow_depth_tex);
        res_.grass_shader->set1i("u_shadow_map", 3);
        res_.grass_shader->set1f("u_has_shadow", 1.0f);
    } else {
        res_.grass_shader->set1f("u_has_shadow", 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res_.grass_shader, fc);

    r->setDepthTest(true);
    r->setDepthMask(true);
    r->setBlending(true);   // for tip alpha fade
    r->setCullFace(false);  // grass visible from both sides

    g3->drawMeshInstanced(res_.grass_blade_mesh, config_.instanced_grass_count);

    r->setBlending(false);
    r->setCullFace(true);
}

// ============================================================
// renderFurPass: shell-based fur rendering
// ============================================================

void DemoScene::renderFurPass(Renderer* r, const FrameContext& fc) {
    if (!res_.fur_shader || model_mesh_ == MeshHandle() || res_.fur_tex == INVALID_TEXTURE) return;

    res_.fur_shader->use();
    res_.fur_shader->setMat4("u_proj", fc.proj);
    res_.fur_shader->setMat4("u_view", fc.view);
    res_.fur_shader->setMat4("u_model", model_transform_);
    res_.fur_shader->set3f("u_light_dir", fc.sun_dir.x, fc.sun_dir.y, fc.sun_dir.z);
    res_.fur_shader->set3f("u_cam_pos", fc.cam_pos.x, fc.cam_pos.y, fc.cam_pos.z);
    res_.fur_shader->set3f("u_fog_color", FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z);
    res_.fur_shader->set1f("u_fog_density", config_.fog_density);
    res_.fur_shader->set1f("u_fur_length", config_.fur_length);
    res_.fur_shader->set1f("u_fur_ao_power", 1.5f);
    res_.fur_shader->set1f("u_time", fc.time);

    // Breeze: slowly varying direction
    float wind_x = sinf(fc.time * 0.7f) * 1.8f;
    float wind_z = cosf(fc.time * 0.5f) * 1.2f;
    res_.fur_shader->set3f("u_wind_dir", wind_x, 0.0f, wind_z);

    // Viewport size for vignette + color grading
    res_.fur_shader->set2f("u_viewport_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));

    // PCSS uniforms (T4 Ultra)
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        res_.fur_shader->set2f("u_shadow_texel_size", texel, texel);
        res_.fur_shader->set1f("u_light_size", config_.light_size);
    }

    // Shadow uniforms (T2+)
    if (fc.has_shadows) {
        res_.fur_shader->setMat4("u_light_vp", fc.light_vp);
        r->bindTextureUnit(3, res_.shadow_depth_tex);
        res_.fur_shader->set1i("u_shadow_map", 3);
        res_.fur_shader->set1f("u_has_shadow", 1.0f);
    } else {
        res_.fur_shader->set1f("u_has_shadow", 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res_.fur_shader, fc);

    // Fur strand texture: unit 0
    r->bindTextureUnit(0, res_.fur_tex);
    res_.fur_shader->set1i("u_fur_tex", 0);

    // Fur intensity mask: unit 1
    if (res_.fur_mask_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(1, res_.fur_mask_tex);
        res_.fur_shader->set1i("u_fur_mask", 1);
        res_.fur_shader->set1f("u_has_fur_mask", 1.0f);
    } else {
        res_.fur_shader->set1f("u_has_fur_mask", 0.0f);
    }

    float tex_scale = config_.fur_density * 0.05f;
    res_.fur_shader->set1f("u_fur_tex_scale", tex_scale);

    // PBR material: fur should be rough and non-metallic
    res_.fur_shader->set1f("u_metallic", 0.0f);
    res_.fur_shader->set1f("u_roughness", 0.85f);

    res_.fur_shader->set3f("u_fur_color_root", 0.30f, 0.18f, 0.08f);
    res_.fur_shader->set3f("u_fur_color_tip", 0.72f, 0.55f, 0.32f);

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
        res_.fur_shader->set1i("u_use_instancing", 1);
        res_.fur_shader->set1i("u_fur_shells", num_shells);
        g3->drawMeshInstanced(model_mesh_, num_shells - 1);
    } else {
        res_.fur_shader->set1i("u_use_instancing", 0);
        for (int i = 1; i < num_shells; i++) {
            float shell_index = static_cast<float>(i) / static_cast<float>(num_shells - 1);
            res_.fur_shader->set1f("u_shell_index", shell_index);
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
    if (!res_.particle_shader || res_.particle_mesh == MeshHandle()) return;

    res_.particle_shader->use();
    res_.particle_shader->setMat4("u_proj", fc.proj);
    res_.particle_shader->setMat4("u_view", fc.view);
    res_.particle_shader->set1f("u_time", fc.time);

    r->setDepthTest(true);
    r->setDepthMask(false);  // don't write depth
    r->setBlending(true);
    r->setCullFace(false);

    r->drawMesh(res_.particle_mesh);

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
    fc.has_shadows = config_.enable_shadows && res_.shadow_shader != nullptr
                     && res_.shadow_rt != INVALID_RENDER_TARGET;
    fc.has_bloom = config_.enable_bloom
                   && res_.bloom_extract_shader != nullptr
                   && res_.scene_rt != INVALID_RENDER_TARGET;
    fc.has_ssao = config_.enable_ssao
                  && res_.ssao_shader != nullptr
                  && res_.ssao_rt != INVALID_RENDER_TARGET;
    fc.has_pbr = config_.enable_pbr;
    fc.has_tessellation = config_.enable_tessellation && res_.tess_shader != nullptr;
    fc.has_compute_particles = config_.enable_compute_particles
                               && res_.compute_particle_shader != nullptr
                               && res_.particle_ssbo != INVALID_BUFFER;
    fc.has_volumetric_fog = config_.enable_volumetric_fog
                            && res_.volumetric_fog_shader != nullptr
                            && res_.fog_rt != INVALID_RENDER_TARGET;
    fc.has_hdr = config_.enable_hdr
                 && res_.tone_map_shader != nullptr
                 && res_.hdr_scene_rt != INVALID_RENDER_TARGET;

    // Camera: Catmull-Rom spline path
    fc.cam_pos = camera_.getPosition(t);
    Vec3 cam_target = camera_.getTarget(t);
    float aspect = static_cast<float>(viewport_w) / static_cast<float>(viewport_h > 0 ? viewport_h : 1);
    fc.proj = Mat4::perspective(60.0f, aspect, 0.1f, 50.0f);
    fc.view = Mat4::lookAt(fc.cam_pos, cam_target, Vec3(0.0f, 1.0f, 0.0f));

    Mat4 vp = fc.proj * fc.view;
    fc.frustum = extractFrustum(vp);

    // === DEBUG: set to 1 to disable specific T4 features ===
    // Disable one at a time, rebuild, and test to find the culprit
    #define DBG_SKIP_HDR          0  // 1 = skip entire HDR pipeline (use T3 path)
    #define DBG_SKIP_AUTO_EXPOSE  0  // 1 = skip auto-exposure
    #define DBG_SKIP_GTAO         0  // 1 = skip compute GTAO (use fragment SSAO)
    #define DBG_SKIP_VOL_FOG      1  // 1 = skip volumetric fog
    #define DBG_SKIP_SSR          1  // 1 = skip SSR compute (water does its own SSR now)
    #define DBG_SKIP_COMPUTE_BLOOM 0 // 1 = skip compute bloom (use fragment bloom)
    #define DBG_SKIP_DOF          0  // 1 = skip depth of field

    if (fc.has_hdr && !DBG_SKIP_HDR) {
        // T4 pipeline: auto-exposure -> compute particles -> shadow -> HDR scene ->
        //              GTAO/SSAO -> vol fog -> compute bloom -> HDR composite
        if (fc.has_compute_particles)
            renderComputeParticles(r, fc);
        if (fc.has_shadows) {
            computeLightMatrix(fc);
            renderShadowPass(r, fc);
        }
        // Render scene to HDR FBO
        r->bindRenderTarget(res_.hdr_scene_rt);
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
        if (config_.enable_ssr && res_.ssr_tex != INVALID_TEXTURE) {
            // Copy current FBO color into ssr_tex_ for SSR lookups
            r->copyFramebufferToTexture(res_.ssr_tex, viewport_w_, viewport_h_);
            // Render water with scene reflection
            renderWaterPass(r, fc);
        } else {
            // Fallback: render water without reflections
            renderWaterPass(r, fc);
        }

        r->bindRenderTarget(INVALID_RENDER_TARGET);

        // Auto-exposure (histogram from HDR scene)
        if (config_.enable_auto_exposure && !DBG_SKIP_AUTO_EXPOSE)
            computeAutoExposure(r, fc);

        // AO: use Compute GTAO if available, otherwise fragment SSAO
        if (!DBG_SKIP_GTAO && config_.enable_gtao && res_.gtao_shader && res_.gtao_tex != INVALID_TEXTURE) {
            renderGTAOPass(r, fc);
            renderGTAOBlur(r, fc);
        } else if (fc.has_ssao) {
            renderSSAOPass(r, fc);
            renderSSAOBlur(r, fc);
        }

        // Volumetric fog
        if (fc.has_volumetric_fog && !DBG_SKIP_VOL_FOG)
            renderVolumetricFog(r, fc);

        // Restore full-res viewport after half-res fog pass
        r->setViewport(0, 0, viewport_w_, viewport_h_);

        // Bloom: compute bloom if available, otherwise fragment bloom
        if (!DBG_SKIP_COMPUTE_BLOOM && config_.enable_compute_bloom && res_.bloom_down_compute && res_.bloom_mips[0] != INVALID_TEXTURE) {
            renderBloomCompute(r, fc);
        } else {
            renderBloomPasses(r, fc);
        }

        // DoF (after bloom, before composite)
        if (config_.enable_dof && res_.dof_shader && !DBG_SKIP_DOF)
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
    r->bindRenderTarget(res_.scene_rt);
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
    r->bindRenderTarget(res_.ssao_rt);
    r->setViewport(0, 0, viewport_w_ / 2, viewport_h_ / 2);
    r->clear(1.0f, 1.0f, 1.0f, 1.0f);  // white = no occlusion
    r->setDepthTest(false);
    r->setCullFace(false);

    res_.ssao_shader->use();

    // Bind depth texture from scene FBO
    r->bindTextureUnit(0, res_.scene_depth_tex);
    res_.ssao_shader->set1i("u_depth_tex", 0);

    // Bind noise texture
    r->bindTextureUnit(1, res_.ssao_noise_tex);
    res_.ssao_shader->set1i("u_noise_tex", 1);

    // Projection parameters (perspective: fov=60, near=0.1, far=50)
    float fov_rad = 60.0f * 3.14159265f / 180.0f;
    float aspect = static_cast<float>(viewport_w_) / static_cast<float>(viewport_h_ > 0 ? viewport_h_ : 1);
    res_.ssao_shader->set2f("u_screen_size",
                            static_cast<float>(viewport_w_),
                            static_cast<float>(viewport_h_));
    res_.ssao_shader->set1f("u_near", 0.1f);
    res_.ssao_shader->set1f("u_far", 50.0f);
    res_.ssao_shader->set1f("u_aspect", aspect);
    res_.ssao_shader->set1f("u_tan_half_fov", tanf(fov_rad * 0.5f));
    res_.ssao_shader->set1f("u_radius", config_.ssao_radius);
    res_.ssao_shader->set1f("u_bias", 0.025f);
    res_.ssao_shader->set1f("u_intensity", config_.ssao_intensity);

    r->drawMesh(res_.fullscreen_quad);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

// ============================================================
// renderSSAOBlur: blur SSAO to smooth noise artifacts
// ============================================================

void DemoScene::renderSSAOBlur(Renderer* r, const FrameContext& fc) {
    (void)fc;
    r->bindRenderTarget(res_.ssao_blur_rt);
    r->setViewport(0, 0, viewport_w_ / 2, viewport_h_ / 2);
    r->setDepthTest(false);
    r->setCullFace(false);

    res_.ssao_blur_shader->use();

    r->bindRenderTargetTexture(res_.ssao_rt, 0);
    res_.ssao_blur_shader->set1i("u_ssao_tex", 0);
    res_.ssao_blur_shader->set2f("u_texel_size",
                                 2.0f / static_cast<float>(viewport_w_),
                                 2.0f / static_cast<float>(viewport_h_));

    r->drawMesh(res_.fullscreen_quad);

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
    r->bindRenderTarget(res_.bright_rt);
    r->setViewport(0, 0, bw, bh);
    res_.bloom_extract_shader->use();
    RenderTargetHandle bloom_source = (res_.hdr_scene_rt != INVALID_RENDER_TARGET)
                                       ? res_.hdr_scene_rt : res_.scene_rt;
    r->bindRenderTargetTexture(bloom_source, 0);
    res_.bloom_extract_shader->set1i("u_scene_tex", 0);
    res_.bloom_extract_shader->set1f("u_threshold", 0.8f);
    r->drawMesh(res_.fullscreen_quad);

    // Horizontal blur -> blur_rt
    r->bindRenderTarget(res_.blur_rt);
    res_.bloom_blur_shader->use();
    r->bindRenderTargetTexture(res_.bright_rt, 0);
    res_.bloom_blur_shader->set1i("u_tex", 0);
    res_.bloom_blur_shader->set1f("u_horizontal", 1.0f);
    res_.bloom_blur_shader->set2f("u_texel_size", 1.0f / static_cast<float>(bw),
                                                   1.0f / static_cast<float>(bh));
    r->drawMesh(res_.fullscreen_quad);

    // Vertical blur -> bright_rt (ping-pong back)
    r->bindRenderTarget(res_.bright_rt);
    r->bindRenderTargetTexture(res_.blur_rt, 0);
    res_.bloom_blur_shader->set1f("u_horizontal", 0.0f);
    r->drawMesh(res_.fullscreen_quad);

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

    res_.bloom_composite_shader->use();
    r->bindRenderTargetTexture(res_.scene_rt, 0);
    res_.bloom_composite_shader->set1i("u_scene_tex", 0);
    r->bindRenderTargetTexture(res_.bright_rt, 1);
    res_.bloom_composite_shader->set1i("u_bloom_tex", 1);
    res_.bloom_composite_shader->set1f("u_bloom_strength", res_.bloom_strength);
    res_.bloom_composite_shader->set2f("u_viewport_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));

    // SSAO: bind blurred AO texture
    if (fc.has_ssao) {
        r->bindRenderTargetTexture(res_.ssao_blur_rt, 2);
        res_.bloom_composite_shader->set1i("u_ssao_tex", 2);
        res_.bloom_composite_shader->set1f("u_has_ssao", 1.0f);
    } else {
        res_.bloom_composite_shader->set1f("u_has_ssao", 0.0f);
    }

    r->drawMesh(res_.fullscreen_quad);

    r->setDepthTest(true);
    r->setCullFace(true);
}

// ============================================================
// T4: Compute particle physics update
// ============================================================

void DemoScene::renderComputeParticles(Renderer* r, const FrameContext& fc) {
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf) return;

    res_.compute_particle_shader->use();
    cf->bindSSBO(res_.particle_ssbo, 0);
    res_.compute_particle_shader->set1f("u_time", fc.time);
    res_.compute_particle_shader->set1f("u_dt", 1.0f / 60.0f);
    res_.compute_particle_shader->set3f("u_emitter_pos", 0.0f, -0.5f, 0.0f);

    int groups = (res_.compute_particle_count + 255) / 256;
    cf->dispatchCompute(groups, 1, 1);
    cf->computeMemoryBarrier();
}

// ============================================================
// T4: Draw compute particles as billboards
// ============================================================

void DemoScene::renderComputeParticlesDraw(Renderer* r, const FrameContext& fc) {
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf || !res_.particle_render_shader) return;

    res_.particle_render_shader->use();
    cf->bindSSBO(res_.particle_ssbo, 0);
    res_.particle_render_shader->setMat4("u_proj", fc.proj);
    res_.particle_render_shader->setMat4("u_view", fc.view);

    // Camera right/up vectors from view matrix
    Vec3 cam_right(fc.view.m[0], fc.view.m[4], fc.view.m[8]);
    Vec3 cam_up(fc.view.m[1], fc.view.m[5], fc.view.m[9]);
    res_.particle_render_shader->set3f("u_cam_right", cam_right.x, cam_right.y, cam_right.z);
    res_.particle_render_shader->set3f("u_cam_up", cam_up.x, cam_up.y, cam_up.z);

    r->setDepthTest(true);
    r->setDepthMask(false);
    r->setBlending(true);
    r->setCullFace(false);

    // Draw quads: 6 vertices per particle (2 triangles each, no strip artifacts)
    glDrawArrays(GL_TRIANGLES, 0, res_.compute_particle_count * 6);

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

    res_.tess_shader->use();
    res_.tess_shader->setMat4("u_proj", fc.proj);
    res_.tess_shader->setMat4("u_view", fc.view);
    res_.tess_shader->setMat4("u_model", model_transform_);
    res_.tess_shader->set3f("u_light_dir", fc.sun_dir.x, fc.sun_dir.y, fc.sun_dir.z);
    res_.tess_shader->set3f("u_cam_pos", fc.cam_pos.x, fc.cam_pos.y, fc.cam_pos.z);
    res_.tess_shader->set3f("u_fog_color", FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z);
    res_.tess_shader->set1f("u_fog_density", config_.fog_density);
    res_.tess_shader->set1f("u_time", fc.time);
    res_.tess_shader->set1i("u_tess_inner", config_.tess_level);
    res_.tess_shader->set1i("u_tess_outer", config_.tess_level);
    res_.tess_shader->set1f("u_displacement_strength", config_.displacement_strength);
    res_.tess_shader->set1f("u_metallic", 0.0f);
    res_.tess_shader->set1f("u_roughness", 0.6f);
    res_.tess_shader->set3f("u_mat_color", 0.55f, 0.35f, 0.20f);
    res_.tess_shader->set1f("u_proc_tex", 0.0f);
    res_.tess_shader->set1f("u_normal_strength", 0.0f);

    // PCSS / SSS uniforms
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        res_.tess_shader->set2f("u_shadow_texel_size", texel, texel);
        res_.tess_shader->set1f("u_light_size", config_.light_size);
    }
    if (config_.enable_sss) {
        res_.tess_shader->set1f("u_sss_strength", config_.sss_strength);
    } else {
        res_.tess_shader->set1f("u_sss_strength", 0.0f);
    }

    if (fc.has_shadows) {
        res_.tess_shader->setMat4("u_light_vp", fc.light_vp);
        r->bindTextureUnit(3, res_.shadow_depth_tex);
        res_.tess_shader->set1i("u_shadow_map", 3);
        res_.tess_shader->set1f("u_has_shadow", 1.0f);
    } else {
        res_.tess_shader->set1f("u_has_shadow", 0.0f);
    }

    setPointLightUniforms(res_.tess_shader, fc);
    res_.tess_shader->set1f("u_has_normal_map", 0.0f);

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
    r->bindRenderTarget(res_.fog_rt);
    r->setViewport(0, 0, viewport_w_ / 2, viewport_h_ / 2);
    r->clear(0.0f, 0.0f, 0.0f, 0.0f);
    r->setDepthTest(false);
    r->setCullFace(false);

    res_.volumetric_fog_shader->use();

    r->bindTextureUnit(0, res_.hdr_depth_tex);
    res_.volumetric_fog_shader->set1i("u_depth_tex", 0);

    float fov_rad = 60.0f * 3.14159265f / 180.0f;
    float aspect = static_cast<float>(viewport_w_) / static_cast<float>(viewport_h_ > 0 ? viewport_h_ : 1);
    res_.volumetric_fog_shader->set1f("u_near", 0.1f);
    res_.volumetric_fog_shader->set1f("u_far", 50.0f);
    res_.volumetric_fog_shader->set1f("u_aspect", aspect);
    res_.volumetric_fog_shader->set1f("u_tan_half_fov", tanf(fov_rad * 0.5f));
    res_.volumetric_fog_shader->set3f("u_sun_dir", fc.sun_dir.x, fc.sun_dir.y, fc.sun_dir.z);
    res_.volumetric_fog_shader->set3f("u_cam_pos", fc.cam_pos.x, fc.cam_pos.y, fc.cam_pos.z);
    res_.volumetric_fog_shader->set1f("u_time", fc.time);
    res_.volumetric_fog_shader->set1f("u_fog_density", config_.fog_density * 0.5f);
    res_.volumetric_fog_shader->set3f("u_fog_color", FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z);
    res_.volumetric_fog_shader->set1i("u_fog_steps", config_.fog_steps);

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
    res_.volumetric_fog_shader->setMat4("u_view_inv", vi);

    r->drawMesh(res_.fullscreen_quad);
    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

// ============================================================
// T4 Ultra: Compute GTAO pass
// ============================================================

void DemoScene::renderGTAOPass(Renderer* r, const FrameContext& fc) {
    if (!res_.gtao_shader || res_.gtao_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    res_.gtao_shader->use();

    r->bindTextureUnit(0, res_.hdr_depth_tex);
    res_.gtao_shader->set1i("u_depth_tex", 0);

    g4->bindImageTexture(res_.gtao_tex, 0, false, true); // write-only

    float fov_rad = 60.0f * 3.14159265f / 180.0f;
    float aspect = static_cast<float>(viewport_w_) / static_cast<float>(viewport_h_ > 0 ? viewport_h_ : 1);
    res_.gtao_shader->set2f("u_screen_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    res_.gtao_shader->set1f("u_near", 0.1f);
    res_.gtao_shader->set1f("u_far", 50.0f);
    res_.gtao_shader->set1f("u_aspect", aspect);
    res_.gtao_shader->set1f("u_tan_half_fov", tanf(fov_rad * 0.5f));
    res_.gtao_shader->set1f("u_ao_radius", config_.ssao_radius);
    res_.gtao_shader->set1f("u_ao_intensity", config_.ssao_intensity);

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
    if (!res_.gtao_blur_shader || res_.gtao_blur_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    res_.gtao_blur_shader->use();

    g4->bindImageTexture(res_.gtao_tex, 0, true, false);      // read-only input
    g4->bindImageTexture(res_.gtao_blur_tex, 1, false, true);  // write-only output

    r->bindTextureUnit(0, res_.hdr_depth_tex);
    res_.gtao_blur_shader->set1i("u_depth_tex", 0);
    res_.gtao_blur_shader->set2f("u_screen_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    res_.gtao_blur_shader->set1f("u_near", 0.1f);
    res_.gtao_blur_shader->set1f("u_far", 50.0f);

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
    if (!res_.bloom_down_compute || !res_.bloom_up_compute) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    static const int MIP_COUNT = TierResourceView::BLOOM_MIP_COUNT;

    // Verify all mips exist
    for (int i = 0; i < MIP_COUNT; i++) {
        if (res_.bloom_mips[i] == INVALID_TEXTURE) return;
    }

    // --- Downsample chain ---
    res_.bloom_down_compute->use();

    for (int i = 0; i < MIP_COUNT; i++) {
        // Source: either HDR scene texture (level 0) or previous mip
        if (i == 0) {
            r->bindRenderTargetTexture(res_.hdr_scene_rt, 0);
            res_.bloom_down_compute->set2f("u_src_size",
                static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
            res_.bloom_down_compute->set1i("u_first_pass", 1); // Karis average
            res_.bloom_down_compute->set1f("u_bloom_threshold", 1.5f);
        } else {
            r->bindTextureUnit(0, res_.bloom_mips[i - 1]);
            int prev_w = viewport_w_ >> i;
            int prev_h = viewport_h_ >> i;
            if (prev_w < 1) prev_w = 1;
            if (prev_h < 1) prev_h = 1;
            res_.bloom_down_compute->set2f("u_src_size",
                static_cast<float>(prev_w), static_cast<float>(prev_h));
            res_.bloom_down_compute->set1i("u_first_pass", 0);
        }
        res_.bloom_down_compute->set1i("u_src_tex", 0);

        g4->bindImageTexture(res_.bloom_mips[i], 0, false, true); // write-only

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
    res_.bloom_up_compute->use();
    res_.bloom_up_compute->set1f("u_bloom_radius", 1.0f);

    for (int i = MIP_COUNT - 2; i >= 0; i--) {
        // Source: lower (smaller) mip
        r->bindTextureUnit(0, res_.bloom_mips[i + 1]);
        res_.bloom_up_compute->set1i("u_src_tex", 0);

        // Destination: current mip (read-write for additive blend)
        g4->bindImageTexture(res_.bloom_mips[i], 0, true, true);

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
    if (!res_.histogram_shader || !res_.exposure_shader) return;
    if (res_.histogram_ssbo == INVALID_BUFFER || res_.exposure_ssbo == INVALID_BUFFER) return;

    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf) return;

    float min_log_lum = -8.0f;
    float log_lum_range = 12.0f;
    float total_pixels = static_cast<float>(viewport_w_ * viewport_h_);

    // Step 1: Build histogram
    res_.histogram_shader->use();
    r->bindRenderTargetTexture(res_.hdr_scene_rt, 0);
    res_.histogram_shader->set1i("u_scene_tex", 0);
    res_.histogram_shader->set2f("u_screen_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    res_.histogram_shader->set1f("u_min_log_lum", min_log_lum);
    res_.histogram_shader->set1f("u_log_lum_range", log_lum_range);

    cf->bindSSBO(res_.histogram_ssbo, 1);

    int gx = (viewport_w_ + 15) / 16;
    int gy = (viewport_h_ + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    cf->computeMemoryBarrier();

    // Step 2: Reduce histogram to exposure value
    res_.exposure_shader->use();
    cf->bindSSBO(res_.histogram_ssbo, 1);
    cf->bindSSBO(res_.exposure_ssbo, 2);
    res_.exposure_shader->set1f("u_min_log_lum", min_log_lum);
    res_.exposure_shader->set1f("u_log_lum_range", log_lum_range);
    res_.exposure_shader->set1f("u_total_pixels", total_pixels);
    res_.exposure_shader->set1f("u_adapt_speed", 2.0f);
    res_.exposure_shader->set1f("u_dt", 1.0f / 60.0f);

    cf->dispatchCompute(1, 1, 1);
    cf->computeMemoryBarrier();
}

// ============================================================
// T4 Ultra: Water pass with screen-space reflections
// ============================================================

void DemoScene::renderWaterPass(Renderer* r, const FrameContext& fc) {
    if (!res_.island_shader) return;

    // Count water objects
    bool has_water = false;
    for (size_t i = 0; i < opaque_objects_.size(); i++) {
        if (opaque_objects_[i].is_water) { has_water = true; break; }
    }
    if (!has_water) return;

    res_.island_shader->use();

    // Set all standard uniforms (same as renderOpaquePass)
    res_.island_shader->setMat4("u_proj", fc.proj);
    res_.island_shader->setMat4("u_view", fc.view);
    res_.island_shader->set3f("u_light_dir", fc.sun_dir.x, fc.sun_dir.y, fc.sun_dir.z);
    res_.island_shader->set3f("u_cam_pos", fc.cam_pos.x, fc.cam_pos.y, fc.cam_pos.z);
    res_.island_shader->set3f("u_fog_color", FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z);
    res_.island_shader->set1f("u_fog_density", config_.fog_density);
    res_.island_shader->set1f("u_time", fc.time);
    res_.island_shader->set1f("u_normal_strength", 0.0f);

    if (config_.enable_pbr) {
        res_.island_shader->set1f("u_metallic", 0.0f);
        res_.island_shader->set1f("u_roughness", 0.02f);
    }
    if (config_.enable_pcss) {
        float texel = 1.0f / static_cast<float>(config_.shadow_map_size);
        res_.island_shader->set2f("u_shadow_texel_size", texel, texel);
        res_.island_shader->set1f("u_light_size", config_.light_size);
    }
    res_.island_shader->set1f("u_sss_strength", 0.0f);

    if (fc.has_shadows) {
        res_.island_shader->setMat4("u_light_vp", fc.light_vp);
        r->bindTextureUnit(3, res_.shadow_depth_tex);
        res_.island_shader->set1i("u_shadow_map", 3);
        res_.island_shader->set1f("u_has_shadow", 1.0f);
    } else {
        res_.island_shader->set1f("u_has_shadow", 0.0f);
    }
    res_.island_shader->set1f("u_has_normal_map", 0.0f);
    res_.island_shader->set1i("u_point_light_count", 0);

    // Bind scene copy for water reflections (ssr_tex_ has the scene before water)
    bool has_reflection = (res_.ssr_tex != INVALID_TEXTURE);
    if (has_reflection) {
        r->bindTextureUnit(5, res_.ssr_tex);
        res_.island_shader->set1i("u_reflection_tex", 5);
        r->bindTextureUnit(6, res_.hdr_depth_tex);
        res_.island_shader->set1i("u_depth_tex", 6);
        res_.island_shader->set1f("u_has_reflection", 1.0f);
        res_.island_shader->set2f("u_screen_size",
            static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));

        res_.island_shader->set1f("u_near", 0.1f);
        res_.island_shader->set1f("u_far", 50.0f);
    } else {
        res_.island_shader->set1f("u_has_reflection", 0.0f);
    }

    // Render only water objects
    for (size_t i = 0; i < opaque_objects_.size(); i++) {
        const SceneObject& obj = opaque_objects_[i];
        if (!obj.is_water) continue;
        if (!sphereInFrustum(fc.frustum, obj.bounds_center, obj.bounds_radius))
            continue;

        res_.island_shader->setMat4("u_model", obj.transform);
        res_.island_shader->set3f("u_mat_color", obj.color.x, obj.color.y, obj.color.z);
        res_.island_shader->set1f("u_mat_spec", obj.specular);
        res_.island_shader->set1f("u_alpha", 1.0f);
        res_.island_shader->set1f("u_proc_tex", 0.0f);
        res_.island_shader->set1f("u_vertex_wind", 0.0f);
        res_.island_shader->set1f("u_is_water", 1.0f);

        if (obj.two_sided) r->setCullFace(false);
        r->drawMesh(obj.mesh);
        if (obj.two_sided) r->setCullFace(true);
    }
}

// ============================================================
// T4 Ultra: Screen-Space Reflections (compute — legacy)
// ============================================================

void DemoScene::renderSSR(Renderer* r, const FrameContext& fc) {
    if (!res_.ssr_shader || res_.ssr_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    res_.ssr_shader->use();

    r->bindRenderTargetTexture(res_.hdr_scene_rt, 0);
    res_.ssr_shader->set1i("u_scene_tex", 0);
    r->bindTextureUnit(1, res_.hdr_depth_tex);
    res_.ssr_shader->set1i("u_depth_tex", 1);

    g4->bindImageTexture(res_.ssr_tex, 0, false, true); // write-only

    res_.ssr_shader->setMat4("u_proj", fc.proj);

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
    res_.ssr_shader->setMat4("u_view_inv", vi);

    float fov_rad = 60.0f * 3.14159265f / 180.0f;
    float aspect = static_cast<float>(viewport_w_) / static_cast<float>(viewport_h_ > 0 ? viewport_h_ : 1);
    res_.ssr_shader->set2f("u_screen_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    res_.ssr_shader->set1f("u_near", 0.1f);
    res_.ssr_shader->set1f("u_far", 50.0f);
    res_.ssr_shader->set1f("u_aspect", aspect);
    res_.ssr_shader->set1f("u_tan_half_fov", tanf(fov_rad * 0.5f));

    // Pass puddle positions so SSR skips water (water has its own reflections)
    if (config_.enable_ssr) {
        res_.ssr_shader->set1i("u_puddle_count", 3);
        res_.ssr_shader->set3f("u_puddle_pos[0]", 2.5f, 0.0f, 0.8f);
        res_.ssr_shader->set3f("u_puddle_pos[1]", -1.8f, 0.0f, 2.2f);
        res_.ssr_shader->set3f("u_puddle_pos[2]", 0.5f, 0.0f, -2.5f);
        res_.ssr_shader->set1f("u_puddle_radius[0]", 1.5f);
        res_.ssr_shader->set1f("u_puddle_radius[1]", 1.2f);
        res_.ssr_shader->set1f("u_puddle_radius[2]", 1.0f);
    } else {
        res_.ssr_shader->set1i("u_puddle_count", 0);
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
    if (!res_.dof_shader || res_.dof_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    res_.dof_shader->use();

    r->bindRenderTargetTexture(res_.hdr_scene_rt, 0);
    res_.dof_shader->set1i("u_scene_tex", 0);
    r->bindTextureUnit(1, res_.hdr_depth_tex);
    res_.dof_shader->set1i("u_depth_tex", 1);

    g4->bindImageTexture(res_.dof_tex, 0, false, true); // write-only

    res_.dof_shader->set2f("u_screen_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    res_.dof_shader->set1f("u_near", 0.1f);
    res_.dof_shader->set1f("u_far", 50.0f);
    res_.dof_shader->set1f("u_focal_distance", config_.dof_focal_distance);
    res_.dof_shader->set1f("u_focal_range", 5.0f);
    res_.dof_shader->set1f("u_max_blur", 5.0f);
    res_.dof_shader->set1f("u_dof_strength", config_.dof_strength);

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

    res_.tone_map_shader->use();

    r->bindRenderTargetTexture(res_.hdr_scene_rt, 0);
    res_.tone_map_shader->set1i("u_scene_tex", 0);

    // Bloom: prefer compute bloom mip[0], otherwise fragment bloom
    if (config_.enable_compute_bloom && res_.bloom_mips[0] != INVALID_TEXTURE) {
        r->bindTextureUnit(1, res_.bloom_mips[0]);
    } else {
        r->bindRenderTargetTexture(res_.bright_rt, 1);
    }
    res_.tone_map_shader->set1i("u_bloom_tex", 1);
    res_.tone_map_shader->set1f("u_bloom_strength", res_.bloom_strength);

    // AO: prefer compute GTAO, otherwise fragment SSAO
    if (config_.enable_gtao && res_.gtao_blur_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(2, res_.gtao_blur_tex);
        res_.tone_map_shader->set1i("u_ssao_tex", 2);
        res_.tone_map_shader->set1f("u_has_ssao", 1.0f);
    } else if (fc.has_ssao) {
        r->bindRenderTargetTexture(res_.ssao_blur_rt, 2);
        res_.tone_map_shader->set1i("u_ssao_tex", 2);
        res_.tone_map_shader->set1f("u_has_ssao", 1.0f);
    } else {
        res_.tone_map_shader->set1f("u_has_ssao", 0.0f);
    }

    if (fc.has_volumetric_fog) {
        r->bindRenderTargetTexture(res_.fog_rt, 3);
        res_.tone_map_shader->set1i("u_fog_tex", 3);
        res_.tone_map_shader->set1f("u_has_fog", 1.0f);
    } else {
        res_.tone_map_shader->set1f("u_has_fog", 0.0f);
    }

    // SSR texture
    if (config_.enable_ssr && res_.ssr_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(4, res_.ssr_tex);
        res_.tone_map_shader->set1i("u_ssr_tex", 4);
        res_.tone_map_shader->set1f("u_has_ssr", 1.0f);
    } else {
        res_.tone_map_shader->set1f("u_has_ssr", 0.0f);
    }

    // DoF texture
    if (config_.enable_dof && res_.dof_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(5, res_.dof_tex);
        res_.tone_map_shader->set1i("u_dof_tex", 5);
        res_.tone_map_shader->set1f("u_has_dof", 1.0f);
    } else {
        res_.tone_map_shader->set1f("u_has_dof", 0.0f);
    }

    res_.tone_map_shader->set2f("u_viewport_size",
        static_cast<float>(viewport_w_), static_cast<float>(viewport_h_));
    res_.tone_map_shader->set1f("u_time", fc.time);

    // Auto-exposure: read exposure from SSBO
    float exposure = 1.0f;
    if (config_.enable_auto_exposure && res_.exposure_ssbo != INVALID_BUFFER) {
        ComputeFeatures* cf = r->features<ComputeFeatures>();
        if (cf) {
            cf->readSSBO(res_.exposure_ssbo, &exposure, 0, sizeof(float));
            if (exposure < 0.01f) exposure = 1.0f;
        }
    }
    res_.tone_map_shader->set1f("u_exposure", exposure);
    res_.tone_map_shader->set1f("u_chromatic_strength", config_.chromatic_strength);
    res_.tone_map_shader->set1f("u_grain_strength", config_.grain_strength);

    r->drawMesh(res_.fullscreen_quad);

    r->setDepthTest(true);
    r->setCullFace(true);
}
