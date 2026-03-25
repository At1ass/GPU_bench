#include "demo/demo_scene.h"
#include "demo/demo_utils.h"
#include "demo/tier_config_validate.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cmath>
#include <cstring>
#include <cstdio>

// sampleTerrainHeight() is in demo_utils.h (shared with torch_pass.cpp)

// ============================================================
// DemoScene implementation
// ============================================================

DemoScene::DemoScene()
    : r_(nullptr)
    , tier_(DemoTier::Basic)
    , viewport_w_(0), viewport_h_(0)
    , initialized_(false)
    , passes_logged_(false)
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
    placePedestal(r);
    placeColumns(r);
    placeArch(r);
    placeGroundPlane(r);
    placeRocks(r);
    placeGrass(r);
    placeRuins(r);
    placeTrees(r);
    placePond(r);
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

    // Pedestal: frustum h=1.1, half_h=0.55, at y=-1.0 → top at y=-0.45.
    // Bunny normalized to [-1,1], feet ~y=-0.85 local.
    // translate_y = pedestal_top - feet_y = -0.45 - (-0.85) = 0.40
    // Small XZ offset to visually center bunny body on pedestal.
    model_transform_ = Mat4::translate(0.0f, 0.40f, -0.20f);

    SceneObject obj;
    obj.mesh = res_.core.model_mesh;
    obj.transform = model_transform_;
    obj.material = MaterialType::Model;
    obj.color = Vec3(0.55f, 0.38f, 0.32f);  // pinkish skin visible under fur
    obj.specular = 0.08f;
    obj.metallic = 0.0f;    // dielectric (organic)
    obj.roughness = 0.85f;  // matte surface (fur/skin, not plastic)
    obj.tessellated = config_.enable_tessellation;
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
    ground.tessellated = config_.enable_tessellation;
    setBounds(ground, 12.0f);
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
    if (!config_.enable_ssr) return;

    // Pond right next to pedestal in the flat center zone.
    // Center flattening makes terrain nearly flat within radius ~2.0 from origin.
    // Place water as a horizontal plane at a fixed Y near ground level.
    if (res_.core.pond_mesh != MeshHandle()) {
        SceneObject pond;
        pond.mesh = res_.core.pond_mesh;
        float pond_x = 0.0f, pond_z = -3.5f;
        // Behind the arch/columns. Terrain has a deep depression here.
        float pond_y = sampleTerrainHeight(pond_x, pond_z) + 0.45f;
        pond.transform = Mat4::translate(pond_x, pond_y, pond_z);
        pond.material = MaterialType::Model;
        pond.color = Vec3(0.06f, 0.08f, 0.12f);
        pond.specular = 0.95f;
        pond.vertex_wind = false;
        pond.two_sided = true;
        pond.metallic = 0.0f;
        pond.roughness = 0.02f;
        pond.is_water = true;
        setBounds(pond, 2.5f);
        opaque_objects_.push_back(pond);
        return;
    }

    if (res_.t4.puddle_meshes[0] == MeshHandle()) return;

    // Fallback: 3 small puddles around the bunny
    static const float positions[][3] = {
        { 2.5f, -0.95f, 0.8f },
        { -1.8f, -0.95f, 2.2f },
        { 0.5f, -0.95f, -2.5f },
    };
    static const float scales[] = { 1.5f, 1.2f, 1.0f };

    for (int i = 0; i < 3; i++) {
        SceneObject puddle;
        puddle.mesh = res_.t4.puddle_meshes[i];
        Mat4 s = Mat4::scale(scales[i], 1.0f, scales[i]);
        Mat4 t = Mat4::translate(positions[i][0], positions[i][1], positions[i][2]);
        puddle.transform = t * s;
        puddle.material = MaterialType::Model;
        puddle.color = Vec3(0.08f, 0.10f, 0.14f);
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
// placePedestal: hexagonal stone pedestal under bunny
// ============================================================

void DemoScene::placePedestal(Renderer* r) {
    (void)r;
    if (res_.core.pedestal_mesh == MeshHandle()) return;
    SceneObject obj;
    obj.mesh = res_.core.pedestal_mesh;
    obj.transform = Mat4::translate(0.0f, -1.0f, 0.0f);
    obj.material = MaterialType::Model;
    obj.color = Vec3(0.50f, 0.47f, 0.42f);
    obj.specular = 0.06f;
    obj.metallic = 0.0f;
    obj.roughness = 0.85f;
    obj.tessellated = config_.enable_tessellation;
    setBounds(obj, 1.5f);  // covers full height (1.1) + width (1.4)
    opaque_objects_.push_back(obj);
}

// ============================================================
// placeColumns: tall columns and stumps around the sanctuary
// ============================================================

void DemoScene::placeColumns(Renderer* r) {
    (void)r;
    if (res_.core.column_tall_mesh == MeshHandle()) return;
    Vec3 col_color(0.55f, 0.52f, 0.46f);

    // Columns A & B form a unified gateway with the arch.
    // Both at Z=-2.5, X=±1.8 (matching arch halfTorus endpoints at ±R=1.8).
    // Use the LOWER terrain height so neither column floats; the other
    // column's base simply sinks into the hill.  This guarantees both
    // column tops are at the exact same Y — critical for arch alignment.
    float col_z = -2.5f;
    float col_x[2] = { -1.8f, 1.8f };
    float h0 = sampleTerrainHeight(col_x[0], col_z);
    float h1 = sampleTerrainHeight(col_x[1], col_z);
    float col_base_y = (h0 < h1) ? h0 : h1;  // lower of the two
    for (int i = 0; i < 2; i++) {
        SceneObject obj;
        obj.mesh = res_.core.column_tall_mesh;
        obj.transform = Mat4::translate(col_x[i], col_base_y, col_z);
        obj.material = MaterialType::Model;
        obj.color = col_color;
        obj.specular = 0.05f;
        obj.roughness = 0.90f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 2.0f);
        opaque_objects_.push_back(obj);
    }

    // Column stumps C, D (T3+ only) — separate ruins, not part of gateway
    if (res_.core.column_stump_mesh == MeshHandle()) return;
    if (config_.point_light_count < 3) return;  // T3+ has 3 point lights
    {
        SceneObject obj;
        obj.mesh = res_.core.column_stump_mesh;
        obj.transform = Mat4::translate(-3.0f, sampleTerrainHeight(-3.0f, -0.5f), -0.5f) * Mat4::rotateZ(5.0f);
        obj.material = MaterialType::Model;
        obj.color = col_color;
        obj.specular = 0.05f;
        obj.roughness = 0.90f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 1.0f);
        opaque_objects_.push_back(obj);
    }
    {
        SceneObject obj;
        obj.mesh = res_.core.column_stump_mesh;
        obj.transform = Mat4::translate(3.0f, sampleTerrainHeight(3.0f, -0.5f), -0.5f) * Mat4::rotateZ(-3.0f);
        obj.material = MaterialType::Model;
        obj.color = col_color;
        obj.specular = 0.05f;
        obj.roughness = 0.90f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 1.0f);
        opaque_objects_.push_back(obj);
    }
}

// ============================================================
// placeArch: stone arch behind the pedestal (T2+)
// ============================================================

void DemoScene::placeArch(Renderer* r) {
    (void)r;
    if (res_.core.arch_mesh == MeshHandle()) return;
    if (!config_.enable_shadows) return;  // T2+ only
    SceneObject obj;
    obj.mesh = res_.core.arch_mesh;
    // halfTorus in XY plane: endpoints at (±1.8, 0, 0), apex at (0, 1.8, 0).
    // Arch sits on column tops. Columns use min(terrain) for base_y.
    // Column cylinder h=3.0 centered → top at base_y + 1.5.
    float h0 = sampleTerrainHeight(-1.8f, -2.5f);
    float h1 = sampleTerrainHeight(1.8f, -2.5f);
    float col_base_y = (h0 < h1) ? h0 : h1;
    float archY = col_base_y + 1.5f;  // column top = base + half_height
    obj.transform = Mat4::translate(0.0f, archY, -2.5f);
    obj.material = MaterialType::Model;
    obj.color = Vec3(0.48f, 0.45f, 0.40f);
    obj.specular = 0.04f;
    obj.roughness = 0.92f;
    obj.tessellated = config_.enable_tessellation;
    setBounds(obj, 2.5f);
    opaque_objects_.push_back(obj);
}

// ============================================================
// placeRuins: scattered ruin elements around the sanctuary
// ============================================================

void DemoScene::placeRuins(Renderer* r) {
    (void)r;

    // Stone slab (T1+): angled slab on the right side of scene
    if (res_.core.slab_mesh != MeshHandle()) {
        SceneObject obj;
        obj.mesh = res_.core.slab_mesh;
        obj.transform = Mat4::translate(3.0f, sampleTerrainHeight(3.0f, 0.5f) + 0.15f, 0.5f) * Mat4::rotateZ(12.0f) * Mat4::rotateY(45.0f) * Mat4::scale(1.2f, 0.12f, 0.7f);
        obj.material = MaterialType::Model;
        obj.color = Vec3(0.52f, 0.49f, 0.44f);
        obj.specular = 0.05f;
        obj.roughness = 0.88f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 1.0f);
        opaque_objects_.push_back(obj);
    }

    // Stone spheres A, B (T1+): spread wider
    if (res_.core.stone_sphere_mesh != MeshHandle()) {
        {
            SceneObject obj;
            obj.mesh = res_.core.stone_sphere_mesh;
            obj.transform = Mat4::translate(-2.8f, sampleTerrainHeight(-2.8f, 1.5f) + 0.25f, 1.5f) * Mat4::scale(0.30f, 0.28f, 0.30f);
            obj.material = MaterialType::Model;
            obj.color = Vec3(0.50f, 0.48f, 0.43f);
            obj.specular = 0.06f;
            obj.roughness = 0.80f;
            obj.tessellated = config_.enable_tessellation;
            setBounds(obj, 0.35f);
            opaque_objects_.push_back(obj);
        }
        {
            SceneObject obj;
            obj.mesh = res_.core.stone_sphere_mesh;
            obj.transform = Mat4::translate(3.5f, sampleTerrainHeight(3.5f, -1.5f) + 0.20f, -1.5f) * Mat4::scale(0.25f, 0.25f, 0.25f);
            obj.material = MaterialType::Model;
            obj.color = Vec3(0.50f, 0.48f, 0.43f);
            obj.specular = 0.06f;
            obj.roughness = 0.80f;
            obj.tessellated = config_.enable_tessellation;
            setBounds(obj, 0.30f);
            opaque_objects_.push_back(obj);
        }
    }

    // Fallen column (T2+): off to the left side
    if (config_.enable_shadows && res_.core.fallen_column_mesh != MeshHandle()) {
        SceneObject obj;
        obj.mesh = res_.core.fallen_column_mesh;
        obj.transform = Mat4::translate(-3.2f, sampleTerrainHeight(-3.2f, 1.0f) + 0.28f, 1.0f) * Mat4::rotateZ(85.0f) * Mat4::rotateY(30.0f);
        obj.material = MaterialType::Model;
        obj.color = Vec3(0.53f, 0.50f, 0.45f);
        obj.specular = 0.04f;
        obj.roughness = 0.90f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 1.5f);
        opaque_objects_.push_back(obj);
    }

    // Mossy block (T2+): further out to the right
    if (config_.enable_shadows && res_.core.mossy_block_mesh != MeshHandle()) {
        SceneObject obj;
        obj.mesh = res_.core.mossy_block_mesh;
        obj.transform = Mat4::translate(3.8f, sampleTerrainHeight(3.8f, 2.5f) + 0.25f, 2.5f) * Mat4::rotateY(25.0f) * Mat4::scale(0.7f, 0.5f, 0.6f);
        obj.material = MaterialType::Island;
        obj.color = Vec3(0.35f, 0.42f, 0.30f);
        obj.specular = 0.03f;
        obj.roughness = 0.95f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 0.8f);
        opaque_objects_.push_back(obj);
    }

    // Bowl (T2+): near pedestal but slightly further
    if (config_.enable_shadows && res_.core.bowl_mesh != MeshHandle()) {
        SceneObject obj;
        obj.mesh = res_.core.bowl_mesh;
        obj.transform = Mat4::translate(1.8f, sampleTerrainHeight(1.8f, 1.2f) + 0.1f, 1.2f);
        obj.material = MaterialType::Model;
        obj.color = Vec3(0.45f, 0.42f, 0.38f);
        obj.specular = 0.08f;
        obj.roughness = 0.75f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 0.40f);
        opaque_objects_.push_back(obj);
    }

    // Obelisk (T3+): taller
    if (config_.point_light_count >= 3 && res_.core.obelisk_mesh != MeshHandle()) {
        SceneObject obj;
        obj.mesh = res_.core.obelisk_mesh;
        obj.transform = Mat4::translate(-3.5f, sampleTerrainHeight(-3.5f, -2.0f), -2.0f) * Mat4::rotateZ(3.0f);
        obj.material = MaterialType::Model;
        obj.color = Vec3(0.42f, 0.40f, 0.36f);
        obj.specular = 0.05f;
        obj.roughness = 0.85f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 1.5f);
        opaque_objects_.push_back(obj);
    }

    // Inner/outer rings (T3+): thicker
    if (config_.point_light_count >= 3) {
        if (res_.core.ring_inner_mesh != MeshHandle()) {
            SceneObject obj;
            obj.mesh = res_.core.ring_inner_mesh;
            obj.transform = Mat4::translate(0.0f, -0.93f, 0.0f);
            obj.material = MaterialType::Model;
            obj.color = Vec3(0.50f, 0.47f, 0.42f);
            obj.specular = 0.05f;
            obj.roughness = 0.85f;
            obj.tessellated = config_.enable_tessellation;
            setBounds(obj, 1.7f);
            opaque_objects_.push_back(obj);
        }
        if (res_.core.ring_outer_mesh != MeshHandle()) {
            SceneObject obj;
            obj.mesh = res_.core.ring_outer_mesh;
            obj.transform = Mat4::translate(0.0f, -0.95f, 0.0f);
            obj.material = MaterialType::Model;
            obj.color = Vec3(0.48f, 0.45f, 0.40f);
            obj.specular = 0.05f;
            obj.roughness = 0.87f;
            obj.tessellated = config_.enable_tessellation;
            setBounds(obj, 2.4f);
            opaque_objects_.push_back(obj);
        }
    }
}

// ============================================================
// placeTrees: 5 procedural trees at medium distance (T2+)
// ============================================================

void DemoScene::placeTrees(Renderer* r) {
    (void)r;
    if (res_.core.tree_meshes[0] == MeshHandle()) return;
    if (!config_.enable_shadows) return;  // T2+ only

    // 5 trees at distance 5-7 from center for depth layering (DoF)
    static const float positions[][2] = {
        { -5.0f, -4.0f },
        { -6.5f,  1.0f },
        {  5.5f, -3.0f },
        { -4.0f,  5.0f },
        {  6.0f,  2.5f },
    };
    static const float scales[] = { 1.0f, 0.85f, 1.15f, 0.75f, 0.95f };
    static const float rotations[] = { 0.0f, 72.0f, 144.0f, 216.0f, 288.0f };
    // Cycle through 3 mesh variants for visual variety
    static const int mesh_idx[] = { 0, 1, 2, 0, 1 };
    // Vary trunk color slightly per tree
    static const Vec3 colors[] = {
        Vec3(0.22f, 0.32f, 0.12f),  // dark forest green
        Vec3(0.28f, 0.38f, 0.16f),  // medium green
        Vec3(0.20f, 0.30f, 0.14f),  // dark green
        Vec3(0.26f, 0.36f, 0.18f),  // warm green
        Vec3(0.24f, 0.34f, 0.10f),  // olive green
    };

    for (int i = 0; i < 5; i++) {
        float x = positions[i][0];
        float z = positions[i][1];
        float y = sampleTerrainHeight(x, z);
        float s = scales[i];

        SceneObject obj;
        obj.mesh = res_.core.tree_meshes[mesh_idx[i]];
        obj.transform = Mat4::translate(x, y, z) * Mat4::rotateY(rotations[i]) * Mat4::scale(s, s, s);
        obj.material = MaterialType::Model;
        obj.color = colors[i];
        obj.specular = 0.03f;
        obj.roughness = 0.85f;
        obj.vertex_wind = false;
        setBounds(obj, 3.5f * s);
        opaque_objects_.push_back(obj);
    }
}

// ============================================================
// placePond: reflective pond in terrain depression (T4+)
// ============================================================

void DemoScene::placePond(Renderer* r) {
    (void)r;
    // Pond is handled by the modified placePuddles via res_.core.pond_mesh
    // This method exists for future pond-specific enhancements
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

    int t = static_cast<int>(tier);
    LOG_INF("Demo scene setup: tier %d, viewport %dx%d", t, viewport_w, viewport_h);

    // Build scene objects (cheap -- just fills SceneObject structs)
    buildScene(r);

    // Populate scene data for render passes
    scene_data_.opaque_objects = &opaque_objects_;
    scene_data_.cloud_objects = &cloud_objects_;
    scene_data_.model_mesh = model_mesh_;
    scene_data_.model_transform = model_transform_;

    // Create render passes and build pipeline via topological sort.
    createPasses(passes_, res_, config_);
    buildPipeline(pipeline_, passes_, config_, debug_, res_, viewport_w_, viewport_h_);

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
// buildFrameData: construct per-frame data from camera + config
// ============================================================

FrameData DemoScene::buildFrameData(float t, float time, int w, int h,
                                    RenderTargetHandle dest_rt) {
    FrameData fd;
    fd.cam_pos = camera_.getPosition(t);
    Vec3 cam_target = camera_.getTarget(t);
    fd.aspect = static_cast<float>(w) / static_cast<float>(h > 0 ? h : 1);
    fd.proj = Mat4::perspective(kDemoFovDeg, fd.aspect, kDemoNear, kDemoFar);
    fd.view = Mat4::lookAt(fd.cam_pos, cam_target, Vec3(0.0f, 1.0f, 0.0f));
    fd.sun_dir = SUN_DIR_RAW.normalized();
    fd.frustum = extractFrustum(fd.proj * fd.view);
    fd.time = time;
    fd.tier_int = static_cast<int>(tier_);
    fd.viewport_w = w;
    fd.viewport_h = h;
    fd.dest_rt = dest_rt;

    // Tier capability flags
    fd.has_shadows = config_.enable_shadows && res_.shadow.shader != nullptr
                     && res_.shadow.rt != INVALID_RENDER_TARGET;
    fd.has_bloom = config_.enable_bloom
                   && res_.bloom.extract_shader != nullptr
                   && res_.bloom.scene_rt != INVALID_RENDER_TARGET;
    fd.has_ssao = config_.enable_ssao
                  && res_.ssao.shader != nullptr
                  && res_.ssao.rt != INVALID_RENDER_TARGET;
    fd.has_pbr = config_.enable_pbr;
    fd.has_tessellation = config_.enable_tessellation && res_.t4.tess_shader != nullptr;
    fd.has_compute_particles = config_.enable_compute_particles
                               && res_.t4.compute_particle_shader != nullptr
                               && res_.t4.particle_ssbo != INVALID_BUFFER;
    fd.has_volumetric_fog = config_.enable_volumetric_fog
                            && res_.t4.volumetric_fog_shader != nullptr
                            && res_.t4.fog_rt != INVALID_RENDER_TARGET;
    fd.has_hdr = config_.enable_hdr
                 && res_.t4.tone_map_shader != nullptr
                 && res_.t4.hdr_scene_rt != INVALID_RENDER_TARGET;

    // Log active passes once per tier
    if (!passes_logged_) {
        passes_logged_ = true;
        LOG_DBG("Demo: tier %d passes: shadow=%d ssao=%d bloom=%d pbr=%d tess=%d "
                 "compute_particles=%d vol_fog=%d hdr=%d",
                 fd.tier_int, fd.has_shadows, fd.has_ssao, fd.has_bloom,
                 fd.has_pbr, fd.has_tessellation, fd.has_compute_particles,
                 fd.has_volumetric_fog, fd.has_hdr);
    }

    return fd;
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

    FrameData fd = buildFrameData(t, time, viewport_w, viewport_h, dest_rt);
    pipeline_.execute(r, fd, res_, config_, scene_data_);

    // Restore GL state
    r->setDepthTest(true);
    r->setCullFace(true);
    r->setBlending(false);
    r->setColorMask(true, true, true, true);
}
