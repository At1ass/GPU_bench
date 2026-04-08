#pragma once
#include "renderer/renderer.h"
#include "demo/material.h"
#include "geometry/math_types.h"
#include <vector>

// A placed object in the scene.
// POD-like aggregate: default-constructible, copyable, movable.
struct SceneObject {
    MeshHandle mesh;
    Mat4 transform;
    MaterialType shader_type = MaterialType::Model;  // pipeline routing
    MaterialDef mat;
    bool vertex_wind = false;   // enable wind vertex displacement (grass only)
    bool is_water = false;      // water material with ripples + Fresnel
    bool tessellated = false;   // render via TessellatedModelPass (T4)
    Vec3 bounds_center;         // world-space bounding sphere center
    float bounds_radius = 0.0f; // world-space bounding sphere radius
};

// Shared scene geometry — passed to render passes by const reference.
// Pointer members are non-owning observers into DemoScene::opaque_objects_
// and cloud_objects_ vectors. Valid for the lifetime of the DemoScene.
struct SceneData {
    const std::vector<SceneObject>* opaque_objects = nullptr;  // non-owning
    const std::vector<SceneObject>* cloud_objects = nullptr;   // non-owning
    MeshHandle model_mesh;        // fur target mesh
    Mat4 model_transform;         // model world transform
};
