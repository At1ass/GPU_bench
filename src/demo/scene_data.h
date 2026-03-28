#pragma once
#include "renderer/renderer.h"
#include "demo/material.h"
#include "geometry/math_types.h"
#include <vector>

// A placed object in the scene
struct SceneObject {
    MeshHandle mesh;
    Mat4 transform;
    MaterialType shader_type;   // pipeline routing: Island (terrain shader) or Model
    MaterialDef mat;            // all visual material properties
    bool vertex_wind;           // enable wind vertex displacement (grass only)
    bool is_water;              // water material with ripples + Fresnel
    bool tessellated;           // render via TessellatedModelPass with displacement (T4)
    Vec3 bounds_center;         // world-space bounding sphere center
    float bounds_radius;        // world-space bounding sphere radius

    SceneObject() : mesh(), transform(), shader_type(MaterialType::Model),
        mat(), vertex_wind(false), is_water(false), tessellated(false),
        bounds_center(0.0f, 0.0f, 0.0f), bounds_radius(0.0f) {}
};

// Shared scene geometry data — passed to render passes by const reference.
struct SceneData {
    const std::vector<SceneObject>* opaque_objects;
    const std::vector<SceneObject>* cloud_objects;
    MeshHandle model_mesh;        // fur target mesh
    Mat4 model_transform;         // model world transform

    SceneData()
        : opaque_objects(nullptr), cloud_objects(nullptr)
        , model_mesh(), model_transform() {}
};
