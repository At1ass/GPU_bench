#include "demo/demo_scene.h"
#include "demo/demo_utils.h"
#include "demo/scene_loader.h"
#include "demo/tier_config_validate.h"
#include "engine/pass_context.h"
#include "geometry/mesh_gen.h"
#include "renderer/features.h"
#include "platform/data_path.h"
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
// Mesh name -> MeshHandle lookup
// ============================================================

static MeshHandle lookupMesh(const std::string& name, const TierResourceView& res) {
    // Core meshes
    if (name == "model")          return res.core.model_mesh;
    if (name == "ground")         return res.core.ground_mesh;
    if (name == "rock")           return res.core.rock_mesh;
    if (name == "grass")          return res.core.grass_mesh;
    if (name == "pedestal")       return res.core.pedestal_mesh;
    if (name == "column_tall")    return res.core.column_tall_mesh;
    if (name == "column_stump")   return res.core.column_stump_mesh;
    if (name == "arch")           return res.core.arch_mesh;
    if (name == "fallen_column")  return res.core.fallen_column_mesh;
    if (name == "slab")           return res.core.slab_mesh;
    if (name == "stone_sphere")   return res.core.stone_sphere_mesh;
    if (name == "mossy_block")    return res.core.mossy_block_mesh;
    if (name == "bowl")           return res.core.bowl_mesh;
    if (name == "obelisk")        return res.core.obelisk_mesh;
    if (name == "ring_inner")     return res.core.ring_inner_mesh;
    if (name == "ring_outer")     return res.core.ring_outer_mesh;
    if (name == "pond")           return res.core.pond_mesh;
    if (name == "torch")          return res.core.torch_mesh;
    // Tree variants
    if (name == "tree0")          return res.core.tree_meshes[0];
    if (name == "tree1")          return res.core.tree_meshes[1];
    if (name == "tree2")          return res.core.tree_meshes[2];
    // T4 puddles
    if (name == "puddle0")        return res.t4.puddle_meshes[0];
    if (name == "puddle1")        return res.t4.puddle_meshes[1];
    if (name == "puddle2")        return res.t4.puddle_meshes[2];
    return MeshHandle();
}

// ============================================================
// Material name -> MaterialDef lookup
// ============================================================

static MaterialDef lookupMaterial(const std::string& name, const Vec3& tint, bool has_tint) {
    if (name == "skin")    return has_tint ? Materials::skin(tint) : Materials::skin();
    if (name == "stone")   return has_tint ? Materials::stone(tint) : Materials::stone();
    if (name == "rock")    return has_tint ? Materials::rock(tint) : Materials::rock();
    if (name == "foliage") return has_tint ? Materials::foliage(tint) : Materials::foliage();
    if (name == "moss")    return has_tint ? Materials::moss(tint) : Materials::moss();
    if (name == "water")   return has_tint ? Materials::water(tint) : Materials::water();
    if (name == "terrain") return Materials::terrain();
    // Default
    return Materials::stone();
}

// ============================================================
// loadSceneFromFile: data-driven scene construction
// ============================================================

bool DemoScene::loadSceneFromFile(Renderer* r) {
    (void)r;
    std::string scene_path = getDataPath("scenes/sanctuary.scene");
    if (scene_path.empty()) {
        LOG_DBG("SceneLoader: no scene file found, using built-in layout");
        return false;
    }

    std::vector<SceneObjectDef> defs;
    std::vector<SceneCameraKeypoint> cam_kps;
    if (!SceneLoader::load(scene_path.c_str(), defs, cam_kps)) {
        LOG_WRN("SceneLoader: failed to parse scene file, using built-in layout");
        return false;
    }

    // Build mesh name -> SceneObjectDef index map for pair lookups
    // (used for snap_to_terrain_pair: both columns use min height)
    // Simple O(n^2) is fine for ~30 objects.

    bool has_pond_mesh = (res_.core.pond_mesh != MeshHandle());

    int placed = 0;
    for (size_t i = 0; i < defs.size(); i++) {
        const SceneObjectDef& def = defs[i];

        // --- Conditional checks ---
        if (def.require_shadows && !config_.enable_shadows) continue;
        if (def.require_ssr && !config_.enable_ssr) continue;
        if (def.require_point_lights > 0 && config_.point_light_count < def.require_point_lights) continue;
        if (def.max_instanced_grass >= 0 && config_.instanced_grass_count > def.max_instanced_grass) continue;

        // Pond/puddle fallback logic: if pond mesh exists, skip puddle fallbacks;
        // if pond mesh absent, skip the pond entry but allow puddle fallbacks.
        if (def.mesh == "pond" && !has_pond_mesh) continue;
        if (def.fallback_for_pond && has_pond_mesh) continue;

        // Resolve mesh
        MeshHandle mesh = lookupMesh(def.mesh, res_);
        if (mesh == MeshHandle()) {
            LOG_DBG("SceneLoader: skipping [%s] — mesh '%s' not loaded", def.name.c_str(), def.mesh.c_str());
            continue;
        }

        // Build transform: T * Rz * Ry * Rx * S
        float px = def.pos.x;
        float py = def.pos.y;
        float pz = def.pos.z;

        // Snap to terrain
        if (def.snap_to_terrain) {
            float h = sampleTerrainHeight(px, pz);
            // If paired with another object, use min of both heights
            if (!def.snap_to_terrain_pair.empty()) {
                for (size_t j = 0; j < defs.size(); j++) {
                    if (defs[j].name == def.snap_to_terrain_pair) {
                        float h2 = sampleTerrainHeight(defs[j].pos.x, defs[j].pos.z);
                        h = (h < h2) ? h : h2;
                        break;
                    }
                }
            }
            py = h + def.snap_to_terrain_offset;
        }

        // Arch special: position on top of column pair
        if (def.arch_on_columns) {
            // Find paired columns (column_left/column_right at z=-2.5)
            float h0 = sampleTerrainHeight(-1.8f, -2.5f);
            float h1 = sampleTerrainHeight(1.8f, -2.5f);
            float col_base_y = (h0 < h1) ? h0 : h1;
            py = col_base_y + 1.5f;  // column top = base + half_height
        }

        Mat4 transform = Mat4::translate(px, py, pz);
        if (def.rotation.z != 0.0f) transform = transform * Mat4::rotateZ(def.rotation.z);
        if (def.rotation.y != 0.0f) transform = transform * Mat4::rotateY(def.rotation.y);
        if (def.rotation.x != 0.0f) transform = transform * Mat4::rotateX(def.rotation.x);
        if (def.scale.x != 1.0f || def.scale.y != 1.0f || def.scale.z != 1.0f)
            transform = transform * Mat4::scale(def.scale.x, def.scale.y, def.scale.z);

        // Build SceneObject
        SceneObject obj;
        obj.mesh = mesh;
        obj.transform = transform;
        obj.shader_type = def.shader_type;
        obj.mat = lookupMaterial(def.material, def.tint, def.has_tint);

        // Material property overrides
        if (def.roughness >= 0.0f) obj.mat.roughness = def.roughness;
        if (def.noise_scale >= 0.0f) obj.mat.noise_scale = def.noise_scale;
        if (def.warp_strength >= 0.0f) obj.mat.warp_strength = def.warp_strength;
        if (def.two_sided) obj.mat.two_sided = true;

        // Flags
        obj.vertex_wind = def.vertex_wind;
        obj.is_water = def.is_water;
        if (def.tessellated_auto)
            obj.tessellated = config_.enable_tessellation;
        else
            obj.tessellated = def.tessellated;

        // Bounds
        float br = def.bounds_radius;
        if (def.bounds_radius_is_model)
            br = res_.core.model_bounding_radius;
        if (br <= 0.0f) br = 1.0f;  // safe default
        setBounds(obj, br);

        // Track the model mesh/transform for fur passes
        if (def.name == "model") {
            model_mesh_ = mesh;
            model_transform_ = transform;
        }

        opaque_objects_.push_back(obj);
        placed++;
    }

    // Override camera keypoints if scene file provided them
    if (static_cast<int>(cam_kps.size()) == CameraPath::NUM_KEYPOINTS) {
        CameraKeypoint kp[CameraPath::NUM_KEYPOINTS];
        for (int i = 0; i < CameraPath::NUM_KEYPOINTS; i++) {
            kp[i].position = cam_kps[static_cast<size_t>(i)].position;
            kp[i].target = cam_kps[static_cast<size_t>(i)].target;
        }
        camera_.setKeypoints(kp, CameraPath::NUM_KEYPOINTS);
        LOG_INF("SceneLoader: loaded %d camera keypoints", CameraPath::NUM_KEYPOINTS);
    } else if (!cam_kps.empty()) {
        LOG_WRN("SceneLoader: expected %d camera keypoints, got %d — using defaults",
                CameraPath::NUM_KEYPOINTS, static_cast<int>(cam_kps.size()));
    }

    LOG_INF("SceneLoader: placed %d/%d objects from scene file", placed, static_cast<int>(defs.size()));
    return placed > 0;
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
    obj.shader_type = MaterialType::Model;
    obj.mat = Materials::skin();
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
    ground.shader_type = MaterialType::Island;
    ground.mat = Materials::terrain();
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
    rocks.shader_type = MaterialType::Model;
    rocks.mat = Materials::rock();
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
    grass.shader_type = MaterialType::Model;
    grass.mat = Materials::foliage(Vec3(0.30f, 0.50f, 0.20f));
    grass.mat.two_sided = true;
    grass.vertex_wind = true;
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
        pond.shader_type = MaterialType::Model;
        pond.mat = Materials::water();
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
        puddle.shader_type = MaterialType::Model;
        puddle.mat = Materials::water(Vec3(0.08f, 0.10f, 0.14f));
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
    obj.shader_type = MaterialType::Model;
    obj.mat = Materials::stone(Vec3(0.50f, 0.47f, 0.42f));
    obj.mat.roughness = 0.85f;
    obj.mat.noise_scale = 1.5f;  // finer grain on pedestal
    obj.tessellated = false;
    setBounds(obj, 1.5f);  // covers full height (1.1) + width (1.4)
    opaque_objects_.push_back(obj);
}

// ============================================================
// placeColumns: tall columns and stumps around the sanctuary
// ============================================================

void DemoScene::placeColumns(Renderer* r) {
    (void)r;
    if (res_.core.column_tall_mesh == MeshHandle()) return;
    MaterialDef col_mat = Materials::stone(Vec3(0.55f, 0.52f, 0.46f));
    col_mat.roughness = 0.90f;

    float col_z = -2.5f;
    float col_x[2] = { -1.8f, 1.8f };
    float h0 = sampleTerrainHeight(col_x[0], col_z);
    float h1 = sampleTerrainHeight(col_x[1], col_z);
    float col_base_y = (h0 < h1) ? h0 : h1;
    for (int i = 0; i < 2; i++) {
        SceneObject obj;
        obj.mesh = res_.core.column_tall_mesh;
        obj.transform = Mat4::translate(col_x[i], col_base_y, col_z);
        obj.shader_type = MaterialType::Model;
        obj.mat = col_mat;
        obj.tessellated = false;
        setBounds(obj, 2.0f);
        opaque_objects_.push_back(obj);
    }

    if (res_.core.column_stump_mesh == MeshHandle()) return;
    if (config_.point_light_count < 3) return;
    {
        SceneObject obj;
        obj.mesh = res_.core.column_stump_mesh;
        obj.transform = Mat4::translate(-3.0f, sampleTerrainHeight(-3.0f, -0.5f), -0.5f) * Mat4::rotateZ(5.0f);
        obj.shader_type = MaterialType::Model;
        obj.mat = col_mat;
        obj.mat.noise_scale = 0.8f;  // coarser weathered look
        obj.tessellated = false;
        setBounds(obj, 1.0f);
        opaque_objects_.push_back(obj);
    }
    {
        SceneObject obj;
        obj.mesh = res_.core.column_stump_mesh;
        obj.transform = Mat4::translate(3.0f, sampleTerrainHeight(3.0f, -0.5f), -0.5f) * Mat4::rotateZ(-3.0f);
        obj.shader_type = MaterialType::Model;
        obj.mat = col_mat;
        obj.mat.noise_scale = 1.2f;
        obj.tessellated = false;
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
    obj.shader_type = MaterialType::Model;
    obj.mat = Materials::stone(Vec3(0.48f, 0.45f, 0.40f));
    obj.mat.roughness = 0.92f;
    obj.mat.warp_strength = 0.4f;  // more erosion on arch
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
        obj.shader_type = MaterialType::Model;
        obj.mat = Materials::stone(Vec3(0.52f, 0.49f, 0.44f));
        obj.tessellated = false;
        setBounds(obj, 1.0f);
        opaque_objects_.push_back(obj);
    }

    // Stone spheres A, B (T1+): spread wider
    if (res_.core.stone_sphere_mesh != MeshHandle()) {
        {
            SceneObject obj;
            obj.mesh = res_.core.stone_sphere_mesh;
            obj.transform = Mat4::translate(-2.8f, sampleTerrainHeight(-2.8f, 1.5f) + 0.25f, 1.5f) * Mat4::scale(0.30f, 0.28f, 0.30f);
            obj.shader_type = MaterialType::Model;
            obj.mat = Materials::stone(Vec3(0.50f, 0.48f, 0.43f));
            obj.mat.roughness = 0.80f;
            obj.tessellated = config_.enable_tessellation;
            setBounds(obj, 0.35f);
            opaque_objects_.push_back(obj);
        }
        {
            SceneObject obj;
            obj.mesh = res_.core.stone_sphere_mesh;
            obj.transform = Mat4::translate(3.5f, sampleTerrainHeight(3.5f, -1.5f) + 0.20f, -1.5f) * Mat4::scale(0.25f, 0.25f, 0.25f);
            obj.shader_type = MaterialType::Model;
            obj.mat = Materials::stone(Vec3(0.50f, 0.48f, 0.43f));
            obj.mat.roughness = 0.80f;
            obj.mat.noise_scale = 1.3f;  // slightly different grain
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
        obj.shader_type = MaterialType::Model;
        obj.mat = Materials::stone(Vec3(0.53f, 0.50f, 0.45f));
        obj.mat.roughness = 0.90f;
        obj.mat.warp_strength = 0.5f;  // more weathered
        obj.tessellated = false;
        setBounds(obj, 1.5f);
        opaque_objects_.push_back(obj);
    }

    // Mossy block (T2+): further out to the right
    if (config_.enable_shadows && res_.core.mossy_block_mesh != MeshHandle()) {
        SceneObject obj;
        obj.mesh = res_.core.mossy_block_mesh;
        obj.transform = Mat4::translate(3.8f, sampleTerrainHeight(3.8f, 2.5f) + 0.25f, 2.5f) * Mat4::rotateY(25.0f) * Mat4::scale(0.7f, 0.5f, 0.6f);
        obj.shader_type = MaterialType::Island;
        obj.mat = Materials::moss();
        obj.tessellated = false;
        setBounds(obj, 0.8f);
        opaque_objects_.push_back(obj);
    }

    // Bowl (T2+): near pedestal but slightly further
    if (config_.enable_shadows && res_.core.bowl_mesh != MeshHandle()) {
        SceneObject obj;
        obj.mesh = res_.core.bowl_mesh;
        obj.transform = Mat4::translate(1.8f, sampleTerrainHeight(1.8f, 1.2f) + 0.1f, 1.2f);
        obj.shader_type = MaterialType::Model;
        obj.mat = Materials::stone(Vec3(0.45f, 0.42f, 0.38f));
        obj.mat.roughness = 0.75f;
        obj.tessellated = config_.enable_tessellation;
        setBounds(obj, 0.40f);
        opaque_objects_.push_back(obj);
    }

    // Obelisk (T3+): taller
    if (config_.point_light_count >= 3 && res_.core.obelisk_mesh != MeshHandle()) {
        SceneObject obj;
        obj.mesh = res_.core.obelisk_mesh;
        obj.transform = Mat4::translate(-3.5f, sampleTerrainHeight(-3.5f, -2.0f), -2.0f) * Mat4::rotateZ(3.0f);
        obj.shader_type = MaterialType::Model;
        obj.mat = Materials::stone(Vec3(0.42f, 0.40f, 0.36f));
        obj.mat.noise_scale = 0.7f;  // larger stone blocks
        obj.tessellated = false;
        setBounds(obj, 1.5f);
        opaque_objects_.push_back(obj);
    }

    // Inner/outer rings (T3+): thicker
    if (config_.point_light_count >= 3) {
        if (res_.core.ring_inner_mesh != MeshHandle()) {
            SceneObject obj;
            obj.mesh = res_.core.ring_inner_mesh;
            obj.transform = Mat4::translate(0.0f, -0.93f, 0.0f);
            obj.shader_type = MaterialType::Model;
            obj.mat = Materials::stone(Vec3(0.50f, 0.47f, 0.42f));
            obj.tessellated = config_.enable_tessellation;
            setBounds(obj, 1.7f);
            opaque_objects_.push_back(obj);
        }
        if (res_.core.ring_outer_mesh != MeshHandle()) {
            SceneObject obj;
            obj.mesh = res_.core.ring_outer_mesh;
            obj.transform = Mat4::translate(0.0f, -0.95f, 0.0f);
            obj.shader_type = MaterialType::Model;
            obj.mat = Materials::stone(Vec3(0.48f, 0.45f, 0.40f));
            obj.mat.roughness = 0.87f;
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
        obj.shader_type = MaterialType::Model;
        obj.mat = Materials::foliage(colors[i]);
        obj.mat.noise_scale = 0.8f + 0.4f * (static_cast<float>(i) / 4.0f);  // vary per tree
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

    // Try loading scene from data file; fall back to hardcoded layout
    if (!loadSceneFromFile(r)) {
        buildScene(r);
    }

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
    computeLightMatrix(fd);
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
                            && res_.t4.hdr.volumetric_fog_shader != nullptr
                            && res_.t4.hdr.fog_rt != INVALID_RENDER_TARGET;
    fd.has_hdr = config_.enable_hdr
                 && res_.t4.hdr.tone_map_shader != nullptr
                 && res_.t4.hdr.scene_rt != INVALID_RENDER_TARGET;

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
    PassContext ctx(r);
    ctx.beginFrame();
    pipeline_.execute(ctx, fd, res_, config_, scene_data_);
    last_frame_stats_ = ctx.stats();
    last_renderer_stats_ = r->rendererStats();

    // Restore GL state
    r->setDepthTest(true);
    r->setCullFace(true);
    r->setBlending(false);
    r->setColorMask(true, true, true, true);
}
