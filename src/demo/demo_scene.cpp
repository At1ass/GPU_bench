#include "demo/demo_scene.h"
#include "demo/demo_shaders.h"
#include "geometry/mesh_gen.h"
#include <cstdint>
#include "renderer/features.h"
#include "platform/logger.h"
#include <cmath>

// --- Tier configuration ---

DemoTierConfig getTierConfig(DemoTier tier) {
    DemoTierConfig c;
    c.tier = tier;
    c.terrain_resolution = 48;
    c.shadow_map_size = 0;
    c.enable_bloom = false;
    c.bloom_passes = 0;
    c.use_pcf = false;
    c.use_pbr = false;
    c.use_god_rays = false;

    switch (tier) {
        case DemoTier::Basic:
            c.terrain_resolution = 48;
            break;
        case DemoTier::Enhanced:
            c.terrain_resolution = 64;
            c.shadow_map_size = 1024;
            c.enable_bloom = true;
            c.bloom_passes = 1;
            break;
        case DemoTier::Quality:
            c.terrain_resolution = 96;
            c.shadow_map_size = 2048;
            c.enable_bloom = true;
            c.bloom_passes = 2;
            c.use_pcf = true;
            break;
        case DemoTier::Ultra:
            c.terrain_resolution = 128;
            c.shadow_map_size = 4096;
            c.enable_bloom = true;
            c.bloom_passes = 3;
            c.use_pcf = true;
            c.use_pbr = true;
            c.use_god_rays = true;
            break;
    }
    return c;
}

int maxSupportedTier(const Renderer& r) {
    if (r.features<GL4Features>()) return 4;
    if (r.features<GL3Features>()) {
        const RenderCaps& caps = r.getCaps();
        if (caps.gl_major >= 3 && caps.gl_minor >= 3) return 3;
        return 2;
    }
    return 1;
}

// --- Scene constants ---

static const float PI_F = 3.14159265359f;
static const Vec3 SUN_DIR(0.4f, 0.8f, 0.3f);
static const Vec3 FOG_COLOR(0.18f, 0.18f, 0.24f);
static const float FOG_DENSITY = 0.008f;

static float smoothstep_f(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

// Analytical terrain height — must match MeshGen::terrain + cityTerrain flattening
static float terrainHeight(float wx, float wz) {
    float h = sinf(wx * 0.5f) * cosf(wz * 0.5f) * 2.0f
            + sinf(wx * 1.3f + 0.7f) * 0.5f
            + cosf(wz * 0.9f + 1.2f) * 0.8f;
    float ax = fabsf(wx);
    float az = fabsf(wz);
    float sf = smoothstep_f(6.0f, 18.0f, ax);
    float cf = smoothstep_f(28.0f, 44.0f, (ax > az * 0.7f) ? ax : az * 0.7f);
    float factor = (sf < cf) ? sf : cf;
    return h * factor * 0.15f;
}

// --- City layout data ---

struct BuildingDef  { float x, z, w, h, d; unsigned int seed; };
struct TreeDef      { float x, z, h, r; int depth; unsigned int seed; };
struct RockDef      { float x, z, radius; unsigned int seed; };
struct LampDef      { float x, z; unsigned int seed; };
struct FenceDef     { float x, z, length, height, angle; unsigned int seed; };
struct ArchDef      { float x, z, w, h, t; int seg; unsigned int seed; };

// Buildings: two rows along main street (Z axis), plus off-street cabins
static const BuildingDef BUILDINGS[] = {
    // Left side (negative X)
    { -14, -22,  6, 10, 8,  100 },
    { -16, -10,  8,  7, 6,  201 },
    { -12,   2,  5, 12, 7,  302 },
    { -15,  14,  7,  8, 8,  403 },
    { -12,  24,  6, 11, 7,  504 },
    // Right side (positive X)
    {  14, -20,  6, 11, 7,  605 },
    {  12,  -8,  5, 14, 8,  706 },
    {  20,  10,  6,  8, 5,  807 },
    {  14,  20,  5,  7, 6,  908 },
    {  16,  28,  5,  9, 6, 1009 },
    // Off-street
    { -26,  -5,  4,  5, 4, 1110 },
    {  26, -14,  4,  6, 5, 1211 },
};
static const int NUM_BUILDINGS = sizeof(BUILDINGS) / sizeof(BUILDINGS[0]);

// Trees: park cluster, perimeter, near buildings
static const TreeDef TREES[] = {
    // Park cluster near center-east
    {   6,  -3, 7.0f, 0.35f, 3, 2000 },
    {   9,   1, 8.0f, 0.40f, 3, 2001 },
    {   4,   5, 6.0f, 0.30f, 3, 2002 },
    // North perimeter
    { -25, -32, 8.0f, 0.45f, 3, 2100 },
    { -18, -36, 6.5f, 0.35f, 3, 2101 },
    {  24, -30, 7.0f, 0.40f, 3, 2102 },
    // South perimeter
    { -28,  32, 9.0f, 0.45f, 3, 2200 },
    { -20,  36, 6.5f, 0.35f, 3, 2201 },
    {  25,  34, 7.0f, 0.40f, 3, 2202 },
    // Near buildings
    { -22, -18, 6.0f, 0.30f, 3, 2300 },
    { -23,   8, 6.5f, 0.35f, 3, 2301 },
    {  24,  -4, 5.5f, 0.30f, 3, 2302 },
    {  22,  16, 6.0f, 0.35f, 3, 2303 },
    // Lone trees
    {  32,   0, 8.5f, 0.45f, 3, 2403 },  // dead (seed & 3 == 3)
    { -34,  12, 7.5f, 0.40f, 3, 2500 },
};
static const int NUM_TREES = sizeof(TREES) / sizeof(TREES[0]);

// Rocks: rubble near buildings, boulders on perimeter
static const RockDef ROCKS[] = {
    // Rubble near buildings
    {  -8, -20, 0.5f, 3000 },
    { -10,  -4, 0.4f, 3001 },
    {  10, -16, 0.6f, 3002 },
    {  17,   6, 0.3f, 3003 },
    // Street rubble
    {  -2, -13, 0.35f, 3100 },
    {   3,   9, 0.40f, 3101 },
    {  -1,  21, 0.30f, 3102 },
    {   5, -26, 0.35f, 3103 },
    // Perimeter boulders
    { -36, -20, 1.5f, 3200 },
    {  36,  16, 1.2f, 3201 },
    { -32,  26, 1.8f, 3202 },
    {  30, -32, 1.0f, 3203 },
};
static const int NUM_ROCKS = sizeof(ROCKS) / sizeof(ROCKS[0]);

// Street lamps: along the main street
static const LampDef LAMPS[] = {
    {  4, -20, 4000 }, {  4, -10, 4001 }, {  4,   0, 4002 },
    {  4,  10, 4003 }, {  4,  20, 4004 }, { -4, -15, 4005 },
};
static const int NUM_LAMPS = sizeof(LAMPS) / sizeof(LAMPS[0]);

// Fences: near off-street buildings and park
static const FenceDef FENCES[] = {
    { -26, -10,  8.0f, 1.6f,   0, 5000 },
    { -26,   0,  8.0f, 1.6f,   0, 5001 },
    {  26, -20,  6.0f, 1.4f,  90, 5002 },
    {   0,  -6, 10.0f, 1.6f,  90, 5003 },
};
static const int NUM_FENCES = sizeof(FENCES) / sizeof(FENCES[0]);

// Arches: entrance gates at north and south
static const ArchDef ARCHES[] = {
    { 0, -30, 5.0f, 6.0f, 0.8f, 12, 6000 },
    { 0,  30, 4.5f, 5.5f, 0.7f, 10, 6001 },
};
static const int NUM_ARCHES = sizeof(ARCHES) / sizeof(ARCHES[0]);

// --- Water quad mesh (XZ plane) ---

static MeshData waterQuad() {
    MeshData m;
    Vertex v;
    v.normal = Vec3(0, 1, 0);
    v.pos = Vec3(-1, 0, -1); v.uv = Vec2(0, 0); m.vertices.push_back(v);
    v.pos = Vec3( 1, 0, -1); v.uv = Vec2(1, 0); m.vertices.push_back(v);
    v.pos = Vec3( 1, 0,  1); v.uv = Vec2(1, 1); m.vertices.push_back(v);
    v.pos = Vec3(-1, 0,  1); v.uv = Vec2(0, 1); m.vertices.push_back(v);
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

// --- City terrain: flatten city area ---

static MeshData cityTerrain(int resolution) {
    MeshData m = MeshGen::terrain(100.0f, resolution);
    for (size_t i = 0; i < m.vertices.size(); i++) {
        Vertex& v = m.vertices[i];
        float ax = fabsf(v.pos.x);
        float az = fabsf(v.pos.z);
        // Street corridor: flat (wider flat zone)
        float sf = smoothstep_f(6.0f, 18.0f, ax);
        // City bounds: suppress terrain inside (wider)
        float cf = smoothstep_f(28.0f, 44.0f, (ax > az * 0.7f) ? ax : az * 0.7f);
        float factor = (sf < cf) ? sf : cf;
        v.pos.y *= factor * 0.15f;
    }
    MeshGen::recomputeNormals(m);
    return m;
}

// --- DemoScene implementation ---

DemoScene::DemoScene()
    : r_(nullptr),
      tier_(DemoTier::Basic),
      water_index_(-1),
      u_proj_(-1), u_view_(-1), u_model_(-1),
      u_light_dir_(-1), u_cam_pos_(-1),
      u_fog_color_(-1), u_fog_density_(-1),
      u_time_(-1),
      u_mat_color_(-1), u_mat_spec_(-1), u_alpha_(-1),
      u_light_vp_(-1), u_shadow_map_(-1),
      u_roughness_(-1), u_metallic_(-1),
      u_shd_lvp_(-1), u_shd_model_(-1),
      u_bl_tex_(-1), u_bl_dir_(-1), u_bl_threshold_(-1),
      u_bc_scene_(-1), u_bc_bloom_(-1), u_bc_intensity_(-1),
      u_bc_sun_pos_(-1), u_bc_ray_density_(-1), u_bc_ray_decay_(-1),
      u_bc_time_(-1),
      shadow_tex_(INVALID_TEXTURE),
      viewport_w_(0), viewport_h_(0),
      initialized_(false)
{}

DemoScene::~DemoScene() {
    if (initialized_ && r_)
        cleanup(r_);
}

void DemoScene::buildCity(Renderer* r) {
    // Terrain
    {
        MeshData md = cityTerrain(config_.terrain_resolution);
        SceneObject obj;
        obj.mesh = r->createMesh(md);
        obj.transform = Mat4();
        obj.material = MaterialType::Terrain;
        obj.color = Vec3(0.35f, 0.42f, 0.25f);
        obj.specular = 0.05f;
        objects_.push_back(obj);
    }

    // Buildings
    for (int i = 0; i < NUM_BUILDINGS; i++) {
        const BuildingDef& b = BUILDINGS[i];
        MeshData md = MeshGen::building_v2(b.w, b.h, b.d, b.seed);
        SceneObject obj;
        obj.mesh = r->createMesh(md);
        float y = terrainHeight(b.x, b.z);
        obj.transform = Mat4::translate(b.x, y, b.z);
        obj.material = MaterialType::Building;
        float tint = 0.85f + static_cast<float>(b.seed % 30) / 100.0f;
        obj.color = Vec3(0.55f * tint, 0.50f * tint, 0.45f * tint);
        obj.specular = 0.15f;
        objects_.push_back(obj);
    }

    // Trees
    for (int i = 0; i < NUM_TREES; i++) {
        const TreeDef& t = TREES[i];
        MeshData md = MeshGen::tree_v2(t.h, t.r, t.depth, t.seed);
        SceneObject obj;
        obj.mesh = r->createMesh(md);
        float y = terrainHeight(t.x, t.z);
        obj.transform = Mat4::translate(t.x, y, t.z);
        obj.material = MaterialType::Foliage;
        float tint = 0.9f + static_cast<float>(t.seed % 20) / 100.0f;
        obj.color = Vec3(0.28f * tint, 0.48f * tint, 0.18f * tint);
        obj.specular = 0.05f;
        objects_.push_back(obj);
    }

    // Rocks
    for (int i = 0; i < NUM_ROCKS; i++) {
        const RockDef& rk = ROCKS[i];
        int detail = (rk.radius > 1.0f) ? 6 : 4;
        MeshData md = MeshGen::rock_v2(detail, rk.radius, rk.seed);
        SceneObject obj;
        obj.mesh = r->createMesh(md);
        float y = terrainHeight(rk.x, rk.z) + rk.radius * 0.3f;
        obj.transform = Mat4::translate(rk.x, y, rk.z);
        obj.material = MaterialType::Stone;
        float tint = 0.88f + static_cast<float>(rk.seed % 25) / 100.0f;
        obj.color = Vec3(0.45f * tint, 0.43f * tint, 0.40f * tint);
        obj.specular = 0.20f;
        objects_.push_back(obj);
    }

    // Street lamps
    for (int i = 0; i < NUM_LAMPS; i++) {
        const LampDef& l = LAMPS[i];
        MeshData md = MeshGen::street_lamp(3.5f, l.seed);
        SceneObject obj;
        obj.mesh = r->createMesh(md);
        float y = terrainHeight(l.x, l.z);
        obj.transform = Mat4::translate(l.x, y, l.z);
        obj.material = MaterialType::Metal;
        obj.color = Vec3(0.30f, 0.30f, 0.32f);
        obj.specular = 0.60f;
        objects_.push_back(obj);
    }

    // Fences
    for (int i = 0; i < NUM_FENCES; i++) {
        const FenceDef& f = FENCES[i];
        MeshData md = MeshGen::fence_segment(f.length, f.height, f.seed);
        SceneObject obj;
        obj.mesh = r->createMesh(md);
        float y = terrainHeight(f.x, f.z);
        Mat4 rot = (fabsf(f.angle) > 0.1f) ? Mat4::rotateY(f.angle) : Mat4();
        obj.transform = Mat4::translate(f.x, y, f.z) * rot;
        obj.material = MaterialType::Wood;
        obj.color = Vec3(0.40f, 0.28f, 0.15f);
        obj.specular = 0.10f;
        objects_.push_back(obj);
    }

    // Arches
    for (int i = 0; i < NUM_ARCHES; i++) {
        const ArchDef& a = ARCHES[i];
        MeshData md = MeshGen::arch_v2(a.w, a.h, a.t, a.seg, a.seed);
        SceneObject obj;
        obj.mesh = r->createMesh(md);
        float y = terrainHeight(a.x, a.z);
        obj.transform = Mat4::translate(a.x, y, a.z);
        obj.material = MaterialType::Stone;
        obj.color = Vec3(0.50f, 0.48f, 0.44f);
        obj.specular = 0.18f;
        objects_.push_back(obj);
    }

    // Water (puddle in the east plaza) — rendered last with blending
    {
        MeshData md = waterQuad();
        SceneObject obj;
        obj.mesh = r->createMesh(md);
        float wy = terrainHeight(22.0f, 5.0f) + 0.02f;
        obj.transform = Mat4::translate(22.0f, wy, 5.0f) * Mat4::scale(6.0f, 1.0f, 5.0f);
        obj.material = MaterialType::Water;
        obj.color = Vec3(0.10f, 0.13f, 0.20f);
        obj.specular = 0.80f;
        water_index_ = static_cast<int>(objects_.size());
        objects_.push_back(obj);
    }

    Log::info("Demo: city built — %d objects (%d buildings, %d trees, %d rocks, %d lamps, %d fences, %d arches)",
              static_cast<int>(objects_.size()), NUM_BUILDINGS, NUM_TREES, NUM_ROCKS,
              NUM_LAMPS, NUM_FENCES, NUM_ARCHES);
}

bool DemoScene::setup(Renderer* r, DemoTier tier, int viewport_w, int viewport_h) {
    if (initialized_) cleanup(r);
    r_ = r;
    tier_ = tier;
    config_ = getTierConfig(tier);
    viewport_w_ = viewport_w;
    viewport_h_ = viewport_h;

    // Build all city geometry
    buildCity(r);

    screen_quad_.assign(r, r->createMesh(MeshGen::quad()));

    // Create city shader based on tier
    bool use_core = r->isCoreProfile();

    if (static_cast<int>(tier) >= 4) {
        city_shader_.assign(r, r->createCustomShader(DemoShaders::city_t4_vs, DemoShaders::city_t4_fs));
    } else if (static_cast<int>(tier) >= 3) {
        city_shader_.assign(r, r->createCustomShader(DemoShaders::city_t3_vs, DemoShaders::city_t3_fs));
    } else if (static_cast<int>(tier) >= 2) {
        city_shader_.assign(r, r->createCustomShader(DemoShaders::city_t2_vs, DemoShaders::city_t2_fs));
    } else {
        const char* vs = use_core ? DemoShaders::city_t1_vs_150 : DemoShaders::city_t1_vs_120;
        const char* fs = use_core ? DemoShaders::city_t1_fs_150 : DemoShaders::city_t1_fs_120;
        city_shader_.assign(r, r->createCustomShader(vs, fs));
    }

    if (!city_shader_) {
        Log::err("DemoScene: failed to create city shader for tier %d", static_cast<int>(tier));
        cleanup(r);
        return false;
    }

    // City shader uniforms
    u_proj_ = r->getCustomUniformLoc(city_shader_, "u_proj");
    u_view_ = r->getCustomUniformLoc(city_shader_, "u_view");
    u_model_ = r->getCustomUniformLoc(city_shader_, "u_model");
    u_light_dir_ = r->getCustomUniformLoc(city_shader_, "u_light_dir");
    u_cam_pos_ = r->getCustomUniformLoc(city_shader_, "u_cam_pos");
    u_fog_color_ = r->getCustomUniformLoc(city_shader_, "u_fog_color");
    u_fog_density_ = r->getCustomUniformLoc(city_shader_, "u_fog_density");
    u_time_ = r->getCustomUniformLoc(city_shader_, "u_time");
    u_mat_color_ = r->getCustomUniformLoc(city_shader_, "u_material_color");
    u_mat_spec_ = r->getCustomUniformLoc(city_shader_, "u_material_specular");
    u_alpha_ = r->getCustomUniformLoc(city_shader_, "u_alpha");
    u_light_vp_ = r->getCustomUniformLoc(city_shader_, "u_light_vp");
    u_shadow_map_ = r->getCustomUniformLoc(city_shader_, "u_shadow_map");
    u_roughness_ = r->getCustomUniformLoc(city_shader_, "u_roughness");
    u_metallic_ = r->getCustomUniformLoc(city_shader_, "u_metallic");

    // Shadow shader (T2+)
    if (config_.shadow_map_size > 0) {
        shadow_shader_.assign(r, r->createCustomShader(DemoShaders::city_shadow_vs, DemoShaders::city_shadow_fs));
        if (shadow_shader_) {
            u_shd_lvp_ = r->getCustomUniformLoc(shadow_shader_, "u_light_vp");
            u_shd_model_ = r->getCustomUniformLoc(shadow_shader_, "u_model");
        }

        shadow_rt_.assign(r, r->createDepthRenderTarget(config_.shadow_map_size, config_.shadow_map_size));
        if (shadow_rt_)
            shadow_tex_ = r->getDepthTexture(shadow_rt_);
        if (!shadow_rt_) {
            Log::warn("DemoScene: shadow map not available, disabling shadows");
            config_.shadow_map_size = 0;
        }
    }

    // Bloom (T2+)
    if (config_.enable_bloom && r->supportsRenderTargets()) {
        bloom_blur_shader_.assign(r, r->createCustomShader(DemoShaders::bloom_blur_vs, DemoShaders::bloom_blur_fs));
        if (config_.use_god_rays)
            bloom_composite_shader_.assign(r, r->createCustomShader(DemoShaders::bloom_composite_t4_vs, DemoShaders::bloom_composite_t4_fs));
        else if (static_cast<int>(tier) >= 3)
            bloom_composite_shader_.assign(r, r->createCustomShader(DemoShaders::bloom_composite_t3_vs, DemoShaders::bloom_composite_t3_fs));
        else
            bloom_composite_shader_.assign(r, r->createCustomShader(DemoShaders::bloom_composite_vs, DemoShaders::bloom_composite_fs));

        if (bloom_blur_shader_ && bloom_composite_shader_) {
            scene_rt_.assign(r, r->createRenderTarget(viewport_w, viewport_h));
            bloom_rt_a_.assign(r, r->createRenderTarget(viewport_w / 2, viewport_h / 2));
            bloom_rt_b_.assign(r, r->createRenderTarget(viewport_w / 2, viewport_h / 2));
            u_bl_tex_ = r->getCustomUniformLoc(bloom_blur_shader_, "u_tex");
            u_bl_dir_ = r->getCustomUniformLoc(bloom_blur_shader_, "u_direction");
            u_bl_threshold_ = r->getCustomUniformLoc(bloom_blur_shader_, "u_threshold");
            u_bc_scene_ = r->getCustomUniformLoc(bloom_composite_shader_, "u_scene");
            u_bc_bloom_ = r->getCustomUniformLoc(bloom_composite_shader_, "u_bloom");
            u_bc_intensity_ = r->getCustomUniformLoc(bloom_composite_shader_, "u_bloom_intensity");
            u_bc_sun_pos_ = r->getCustomUniformLoc(bloom_composite_shader_, "u_sun_pos");
            u_bc_ray_density_ = r->getCustomUniformLoc(bloom_composite_shader_, "u_ray_density");
            u_bc_ray_decay_ = r->getCustomUniformLoc(bloom_composite_shader_, "u_ray_decay");
            u_bc_time_ = r->getCustomUniformLoc(bloom_composite_shader_, "u_time");
        }
        if (!scene_rt_ || !bloom_rt_a_) {
            Log::warn("DemoScene: bloom FBOs not available, disabling bloom");
            config_.enable_bloom = false;
        }
    }

    initialized_ = true;
    return true;
}

Mat4 DemoScene::computeLightVP() const {
    Vec3 light_pos = SUN_DIR.normalized() * 80.0f;
    Mat4 light_view = Mat4::lookAt(light_pos, Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 light_proj = Mat4::ortho(-55, 55, -55, 55, 10.0f, 160.0f);
    return light_proj * light_view;
}

void DemoScene::renderShadowPass(Renderer* r, const Mat4& light_vp) {
    if (!shadow_rt_ || !shadow_shader_) return;

    r->bindRenderTarget(shadow_rt_);
    r->setViewport(0, 0, config_.shadow_map_size, config_.shadow_map_size);
    r->clear(1, 1, 1, 1);
    r->setDepthTest(true);
    r->setCullFace(true);
    r->setColorMask(false, false, false, false);

    r->useCustomShader(shadow_shader_);
    r->setUniformMat4(u_shd_lvp_, light_vp);

    for (int i = 0; i < static_cast<int>(objects_.size()); i++) {
        if (i == water_index_) continue;
        r->setUniformMat4(u_shd_model_, objects_[i].transform);
        r->drawMesh(objects_[i].mesh);
    }

    r->setColorMask(true, true, true, true);
    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

void DemoScene::renderBloom(Renderer* r, float time) {
    if (!config_.enable_bloom || !bloom_blur_shader_ || !scene_rt_) return;

    int half_w = viewport_w_ / 2, half_h = viewport_h_ / 2;
    float texel_w = 1.0f / static_cast<float>(half_w);
    float texel_h = 1.0f / static_cast<float>(half_h);
    int passes = (config_.bloom_passes > 0) ? config_.bloom_passes : 1;

    r->setDepthTest(false);
    r->useCustomShader(bloom_blur_shader_);
    r->setUniform1i(u_bl_tex_, 0);
    r->setViewport(0, 0, half_w, half_h);

    // First H pass: extract + blur (from full-res scene)
    r->bindRenderTarget(bloom_rt_b_);
    r->bindRenderTargetTexture(scene_rt_, 0);
    r->setUniform1f(u_bl_threshold_, 0.65f);
    r->setUniform2f(u_bl_dir_, texel_w, 0.0f);
    r->drawMesh(screen_quad_);

    // First V pass
    r->bindRenderTarget(bloom_rt_a_);
    r->bindRenderTargetTexture(bloom_rt_b_, 0);
    r->setUniform1f(u_bl_threshold_, -1.0f);
    r->setUniform2f(u_bl_dir_, 0.0f, texel_h);
    r->drawMesh(screen_quad_);

    // Additional blur passes
    for (int p = 1; p < passes; p++) {
        r->bindRenderTarget(bloom_rt_b_);
        r->bindRenderTargetTexture(bloom_rt_a_, 0);
        r->setUniform2f(u_bl_dir_, texel_w, 0.0f);
        r->drawMesh(screen_quad_);
        r->bindRenderTarget(bloom_rt_a_);
        r->bindRenderTargetTexture(bloom_rt_b_, 0);
        r->setUniform2f(u_bl_dir_, 0.0f, texel_h);
        r->drawMesh(screen_quad_);
    }

    // Composite
    r->bindRenderTarget(INVALID_RENDER_TARGET);
    r->setViewport(0, 0, viewport_w_, viewport_h_);
    r->useCustomShader(bloom_composite_shader_);
    r->bindRenderTargetTexture(scene_rt_, 0);
    r->bindRenderTargetTexture(bloom_rt_a_, 1);
    r->setUniform1i(u_bc_scene_, 0);
    r->setUniform1i(u_bc_bloom_, 1);
    r->setUniform1f(u_bc_intensity_, 0.30f);
    if (u_bc_sun_pos_ >= 0) {
        r->setUniform2f(u_bc_sun_pos_, 0.65f, 0.85f);
        r->setUniform1f(u_bc_ray_density_, 0.5f);
        r->setUniform1f(u_bc_ray_decay_, 0.96f);
    }
    if (u_bc_time_ >= 0)
        r->setUniform1f(u_bc_time_, time);
    r->drawMesh(screen_quad_);
}

void DemoScene::renderFrame(Renderer* r, float t, float time, int viewport_w, int viewport_h) {
    if (!initialized_) return;

    // Camera: Catmull-Rom spline path through the city
    Vec3 cam_pos = camera_.getPosition(t);
    Vec3 cam_target = camera_.getTarget(t);
    Mat4 view = Mat4::lookAt(cam_pos, cam_target, Vec3(0, 1, 0));

    float aspect = static_cast<float>(viewport_w) / static_cast<float>(viewport_h);
    Mat4 proj = Mat4::perspective(60.0f, aspect, 0.5f, 200.0f);

    Mat4 light_vp = computeLightVP();

    // Shadow pass (T2+)
    if (config_.shadow_map_size > 0)
        renderShadowPass(r, light_vp);

    // Bind bloom FBO if needed
    if (config_.enable_bloom && scene_rt_)
        r->bindRenderTarget(scene_rt_);

    r->setViewport(0, 0, viewport_w, viewport_h);
    r->clear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f);
    r->setDepthTest(true);
    r->setCullFace(true);

    // Bind city shader and set per-frame uniforms
    r->useCustomShader(city_shader_);
    r->setUniformMat4(u_proj_, proj);
    r->setUniformMat4(u_view_, view);
    r->setUniform3f(u_light_dir_, SUN_DIR.x, SUN_DIR.y, SUN_DIR.z);
    r->setUniform3f(u_cam_pos_, cam_pos.x, cam_pos.y, cam_pos.z);
    r->setUniform3f(u_fog_color_, FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z);
    r->setUniform1f(u_fog_density_, FOG_DENSITY);
    r->setUniform1f(u_time_, time);

    // Shadow map binding (T2+)
    if (config_.shadow_map_size > 0 && shadow_tex_ != INVALID_TEXTURE) {
        r->setUniformMat4(u_light_vp_, light_vp);
        r->bindTextureUnit(1, shadow_tex_);
        r->setUniform1i(u_shadow_map_, 1);
    }

    // Draw opaque objects
    for (int i = 0; i < static_cast<int>(objects_.size()); i++) {
        if (i == water_index_) continue;
        const SceneObject& obj = objects_[i];
        r->setUniformMat4(u_model_, obj.transform);
        r->setUniform3f(u_mat_color_, obj.color.x, obj.color.y, obj.color.z);
        r->setUniform1f(u_mat_spec_, obj.specular);
        r->setUniform1f(u_alpha_, 1.0f);

        if (config_.use_pbr) {
            float rough = 0.7f, metal = 0.0f;
            if (obj.material == MaterialType::Metal) { rough = 0.3f; metal = 0.8f; }
            else if (obj.material == MaterialType::Stone) { rough = 0.9f; }
            else if (obj.material == MaterialType::Foliage) { rough = 0.8f; }
            else if (obj.material == MaterialType::Wood) { rough = 0.85f; }
            else if (obj.material == MaterialType::Terrain) { rough = 0.95f; }
            r->setUniform1f(u_roughness_, rough);
            r->setUniform1f(u_metallic_, metal);
        }

        r->drawMesh(obj.mesh);
    }

    // Draw water (blended, last)
    if (water_index_ >= 0 && water_index_ < static_cast<int>(objects_.size())) {
        r->setBlending(true);
        const SceneObject& w = objects_[water_index_];
        r->setUniformMat4(u_model_, w.transform);
        r->setUniform3f(u_mat_color_, w.color.x, w.color.y, w.color.z);
        r->setUniform1f(u_mat_spec_, w.specular);
        r->setUniform1f(u_alpha_, 0.6f);
        if (config_.use_pbr) {
            r->setUniform1f(u_roughness_, 0.1f);
            r->setUniform1f(u_metallic_, 0.0f);
        }
        r->drawMesh(w.mesh);
        r->setBlending(false);
    }

    // Bloom post-process
    if (config_.enable_bloom && scene_rt_)
        renderBloom(r, time);
}

void DemoScene::cleanup(Renderer* r) {
    if (!initialized_) return;

    // Objects contain raw MeshHandles — must destroy manually
    for (size_t i = 0; i < objects_.size(); i++) {
        if (objects_[i].mesh != INVALID_MESH) r->destroyMesh(objects_[i].mesh);
    }
    objects_.clear();
    water_index_ = -1;

    // ScopedHandle members auto-destroy via reset()
    screen_quad_.reset();
    city_shader_.reset();
    shadow_shader_.reset();
    bloom_blur_shader_.reset();
    bloom_composite_shader_.reset();
    shadow_rt_.reset();
    shadow_tex_ = INVALID_TEXTURE;
    scene_rt_.reset();
    bloom_rt_a_.reset();
    bloom_rt_b_.reset();

    r_ = nullptr;
    initialized_ = false;
}
