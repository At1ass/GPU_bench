#pragma once
#include "engine/resource_id.h"
#include <cstdint>

// Shader feature flags — bitfield encoding of active features.
// Used as variant key for ShaderCache.
// Each bit enables one preprocessor #define in the shader.

enum ShaderFeature : uint32_t {
    SF_NONE           = 0,

    // GLSL version (mutually exclusive — exactly one must be set)
    SF_GLSL_120       = 1u << 0,   // GLSL 1.20 compatibility profile
    SF_GLSL_150       = 1u << 1,   // GLSL 1.50 core profile
    SF_GLSL_140       = 1u << 2,   // GLSL 1.40 (GL 3.1, supports gl_InstanceID)
    SF_GLSL_330       = 1u << 3,   // GLSL 3.30
    SF_GLSL_430       = 1u << 4,   // GLSL 4.30
    SF_GLES_100       = 1u << 20,  // GLES 2.0 (#version 100)
    SF_GLES_300       = 1u << 21,  // GLES 3.0+ (#version 300 es)

    // Rendering features (combinable)
    SF_SHADOWS        = 1u << 5,   // Shadow mapping (T2+)
    SF_SHADOW_PCF3    = 1u << 6,   // PCF 3x3 kernel (T2)
    SF_SHADOW_PCF5    = 1u << 7,   // PCF 5x5 kernel (T3)
    SF_SHADOW_PCSS    = 1u << 8,   // PCSS with textureGather (T4)
    SF_NORMAL_MAP     = 1u << 9,   // Normal map texture (T3+)
    SF_POINT_LIGHTS   = 1u << 10,  // Point light array up to 3 (T3+)
    SF_PBR            = 1u << 11,  // Cook-Torrance GGX (T4)
    SF_SSS            = 1u << 12,  // Subsurface scattering (T4)
    SF_WATER          = 1u << 13,  // Water material path (T4)
    SF_VIGNETTE       = 1u << 14,  // Vignette + color grading (T1 forward)
    SF_INSTANCING     = 1u << 15,  // gl_InstanceID-based rendering (T2+ grass/fur)
    SF_PUDDLE_EXCLUDE = 1u << 16,  // Puddle exclusion zones in grass (T4)
    SF_DOMAIN_WARP    = 1u << 17,  // Domain-warped clouds (T2+)
    SF_PHYSICAL_SKY   = 1u << 18,  // Physical atmosphere Rayleigh+Mie (T3+)
    SF_FILM_GRAIN     = 1u << 19,  // Luminance-aware film grain (T2+)
};

using ShaderFeatureSet = uint32_t;

// Version flag mask (exactly one must be set)
static const ShaderFeatureSet SF_VERSION_MASK =
    SF_GLSL_120 | SF_GLSL_150 | SF_GLSL_140 | SF_GLSL_330 | SF_GLSL_430 |
    SF_GLES_100 | SF_GLES_300;

// Convert tier + renderer profile to shader feature set.
// Encapsulates all tier→feature mapping in one place.
ShaderFeatureSet featuresForTier(DemoTier tier, bool core_profile, bool is_gles = false, bool gles3 = false);
