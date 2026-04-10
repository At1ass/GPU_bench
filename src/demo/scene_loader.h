#pragma once
#include "engine/scene_data.h"
#include "engine/material.h"
#include "geometry/math_types.h"
#include <vector>
#include <string>

// Definition of a scene object as read from a .scene file.
// All placement data is stored here; conversion to SceneObject
// happens in DemoScene using the mesh/material registries.
struct SceneObjectDef {
    std::string name;
    std::string mesh;                      // mesh registry key
    Vec3 pos;
    Vec3 rotation;                         // euler angles in degrees (X, Y, Z)
    Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
    std::string material;                  // material factory name
    Vec3 tint;                             // override tint (0,0,0 = use factory default)
    bool has_tint = false;

    MaterialType shader_type = MaterialType::Model;

    bool fur = false;
    bool vertex_wind = false;
    bool is_water = false;
    bool tessellated = false;              // true/false explicit
    bool tessellated_auto = false;         // follow tier config
    bool two_sided = false;
    bool snap_to_terrain = false;
    float snap_to_terrain_offset = 0.0f;
    std::string snap_to_terrain_pair;      // name of paired object for min(height)
    bool arch_on_columns = false;          // special: compute Y from column tops

    float roughness = -1.0f;               // -1 = use factory default
    float noise_scale = -1.0f;
    float warp_strength = -1.0f;

    float bounds_radius = 0.0f;            // 0 = auto from mesh
    bool bounds_radius_is_model = false;   // "model" = use model_bounding_radius

    bool require_shadows = false;
    bool require_ssr = false;
    int require_point_lights = 0;          // minimum point_light_count
    int max_instanced_grass = -1;          // -1 = no check
    bool fallback_for_pond = false;
};

// Camera keypoint as read from a .scene file.
struct SceneCameraKeypoint {
    Vec3 position;
    Vec3 target;
};

// Load a .scene file (INI-like format).
// Returns true on success; objects and camera keypoints are appended.
class SceneLoader {
public:
    static bool load(const char* path,
                     std::vector<SceneObjectDef>& objects,
                     std::vector<SceneCameraKeypoint>& camera);
};
