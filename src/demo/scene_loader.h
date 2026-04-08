#pragma once
#include "demo/scene_data.h"
#include "demo/material.h"
#include "geometry/math_types.h"
#include <vector>
#include <string>

// Definition of a scene object as read from a .scene file.
// All placement data is stored here; conversion to SceneObject
// happens in DemoScene using the mesh/material registries.
struct SceneObjectDef {
    std::string name;
    std::string mesh;           // mesh registry key
    Vec3 pos;
    Vec3 rotation;              // euler angles in degrees (X, Y, Z)
    Vec3 scale;
    std::string material;       // material factory name
    Vec3 tint;                  // override tint (0,0,0 = use factory default)
    bool has_tint;

    // Shader routing
    MaterialType shader_type;

    // Flags
    bool fur;
    bool vertex_wind;
    bool is_water;
    bool tessellated;           // true/false explicit
    bool tessellated_auto;      // follow tier config
    bool two_sided;
    bool snap_to_terrain;
    float snap_to_terrain_offset;
    std::string snap_to_terrain_pair;  // name of paired object for min(height) alignment
    bool arch_on_columns;       // special: compute Y from column tops

    // Material property overrides (-1 = not set, use factory default)
    float roughness;
    float noise_scale;
    float warp_strength;

    // Bounds override
    float bounds_radius;        // explicit radius; 0 = auto from mesh
    bool bounds_radius_is_model; // "model" = use model_bounding_radius

    // Conditional placement
    bool require_shadows;
    bool require_ssr;
    int require_point_lights;   // minimum point_light_count (0 = no requirement)

    // Grass special: only place if instanced grass count <= this value
    int max_instanced_grass;    // -1 = no check

    // Puddle fallback: only used if pond mesh is absent
    bool fallback_for_pond;

    SceneObjectDef()
        : pos(), rotation(), scale(1.0f, 1.0f, 1.0f)
        , has_tint(false)
        , shader_type(MaterialType::Model)
        , fur(false), vertex_wind(false), is_water(false)
        , tessellated(false), tessellated_auto(false)
        , two_sided(false)
        , snap_to_terrain(false), snap_to_terrain_offset(0.0f)
        , arch_on_columns(false)
        , roughness(-1.0f), noise_scale(-1.0f), warp_strength(-1.0f)
        , bounds_radius(0.0f), bounds_radius_is_model(false)
        , require_shadows(false), require_ssr(false), require_point_lights(0)
        , max_instanced_grass(-1)
        , fallback_for_pond(false) {}
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
