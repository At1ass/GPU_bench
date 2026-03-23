#pragma once
#include "renderer/renderer.h"
#include "demo/material.h"
#include "geometry/math_types.h"
#include <vector>

// A placed object in the scene
struct SceneObject {
    MeshHandle mesh;
    Mat4 transform;
    MaterialType material;
    Vec3 color;
    float specular;
    Vec3 bounds_center;   // world-space bounding sphere center
    float bounds_radius;  // world-space bounding sphere radius
    bool vertex_wind;     // enable wind vertex displacement (grass only)
    bool two_sided;       // disable backface culling for this object
    float metallic;       // PBR metallic override (-1 = use default)
    float roughness;      // PBR roughness override (-1 = use default)
    bool is_water;        // water material with ripples + Fresnel

    SceneObject() : mesh(), transform(), material(MaterialType::Model),
        color(0.0f, 0.0f, 0.0f), specular(0.0f),
        bounds_center(0.0f, 0.0f, 0.0f), bounds_radius(0.0f),
        vertex_wind(false), two_sided(false),
        metallic(-1.0f), roughness(-1.0f), is_water(false) {}
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
