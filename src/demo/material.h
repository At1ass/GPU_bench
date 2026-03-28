#pragma once
#include "geometry/math_types.h"

// Procedural surface type — selects noise function in shader
enum class ProceduralType : int {
    Flat     = 0,   // flat color + micro noise (particles, unclassified)
    Terrain  = 1,   // full terrain color (slope/height/moss/rock blend)
    Stone    = 2,   // weathered stone (Worley cracks, erosion, moss in crevices)
    Rock     = 3,   // rough boulder (stratified layers, lichen, granular)
    Foliage  = 4,   // tree canopy (leaf variation, translucency, color shift)
    Moss     = 5,   // moss/lichen covered (domain-warped organic pattern)
};

// Material definition: visual properties for a surface class
struct MaterialDef {
    Vec3 color_a;            // primary surface color
    Vec3 color_b;            // secondary color (gradient/variation endpoint)
    float specular;
    float metallic;
    float roughness;
    ProceduralType proc_type;
    float noise_scale;       // frequency multiplier for procedural detail
    float noise_intensity;   // amplitude multiplier for procedural detail
    float warp_strength;     // domain warp amount (0 = no warp)
    float detail_scale;      // secondary detail layer frequency
    bool two_sided;

    MaterialDef()
        : color_a(0.5f, 0.5f, 0.5f), color_b(0.5f, 0.5f, 0.5f)
        , specular(0.04f), metallic(0.0f), roughness(0.8f)
        , proc_type(ProceduralType::Flat)
        , noise_scale(1.0f), noise_intensity(1.0f)
        , warp_strength(0.0f), detail_scale(1.0f)
        , two_sided(false) {}
};

// Predefined material palette — factory functions (impl in materials.cpp)
namespace Materials {
    MaterialDef terrain();
    MaterialDef stone(Vec3 tint = Vec3(0.50f, 0.47f, 0.42f));
    MaterialDef rock(Vec3 tint = Vec3(0.45f, 0.42f, 0.38f));
    MaterialDef foliage(Vec3 tint = Vec3(0.25f, 0.35f, 0.14f));
    MaterialDef moss(Vec3 tint = Vec3(0.35f, 0.42f, 0.30f));
    MaterialDef skin(Vec3 tint = Vec3(0.55f, 0.38f, 0.32f));
    MaterialDef water(Vec3 tint = Vec3(0.06f, 0.08f, 0.14f));
}

// Legacy: opaque pass uses this for shader program selection (island vs fur)
enum class MaterialType { Island, Model };
