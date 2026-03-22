#include "demo/demo_scene.h"
#include "demo/shader_loader.h"
#include "renderer/features.h"
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
static const Vec3 FOG_COLOR(0.7f, 0.75f, 0.85f);

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
    obj.color = Vec3(0.28f, 0.16f, 0.07f);
    obj.specular = 0.15f;
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
        Vec3(3.0f, 2.4f, 1.2f),  // warm yellow
        Vec3(1.2f, 1.8f, 3.0f),  // cool blue
        Vec3(1.5f, 3.0f, 1.5f),  // soft green
    };
    for (int i = 0; i < config_.point_light_count && i < 3; i++) {
        float angle = fc.time * 0.5f + i * 2.094f;
        float px = cosf(angle) * 1.8f;
        float pz = sinf(angle) * 1.8f;
        float py = -0.5f + sinf(fc.time * 0.8f + i * 1.5f) * 0.3f;
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
        if (!sphereInFrustum(fc.frustum, obj.bounds_center, obj.bounds_radius))
            continue;

        res_.island_shader->setMat4("u_model", obj.transform);
        res_.island_shader->set3f("u_mat_color", obj.color.x, obj.color.y, obj.color.z);
        res_.island_shader->set1f("u_mat_spec", obj.specular);
        res_.island_shader->set1f("u_alpha", 1.0f);
        res_.island_shader->set1f("u_proc_tex", obj.material == MaterialType::Island ? 1.0f : 0.0f);
        res_.island_shader->set1f("u_vertex_wind", obj.vertex_wind ? 1.0f : 0.0f);

        if (obj.two_sided) r->setCullFace(false);
        r->drawMesh(obj.mesh);
        if (obj.two_sided) r->setCullFace(true);
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

void DemoScene::renderFrame(Renderer* r, float t, float time, int viewport_w, int viewport_h) {
    if (!initialized_) return;

    viewport_w_ = viewport_w;
    viewport_h_ = viewport_h;

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

    // Camera: Catmull-Rom spline path
    fc.cam_pos = camera_.getPosition(t);
    Vec3 cam_target = camera_.getTarget(t);
    float aspect = static_cast<float>(viewport_w) / static_cast<float>(viewport_h > 0 ? viewport_h : 1);
    fc.proj = Mat4::perspective(60.0f, aspect, 0.1f, 50.0f);
    fc.view = Mat4::lookAt(fc.cam_pos, cam_target, Vec3(0.0f, 1.0f, 0.0f));

    Mat4 vp = fc.proj * fc.view;
    fc.frustum = extractFrustum(vp);

    if (fc.has_bloom) {
        // T2+ pipeline: shadow -> scene FBO -> bloom -> composite
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

    // Extract bright pixels
    r->bindRenderTarget(res_.bright_rt);
    r->setViewport(0, 0, bw, bh);
    res_.bloom_extract_shader->use();
    r->bindRenderTargetTexture(res_.scene_rt, 0);
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
