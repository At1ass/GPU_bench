#pragma once
#include "engine/resource_id.h"
#include "engine/shader_feature_set.h"
#include "engine/shader_feature_define.h"
#include "demo/tier/demo_tier.h"

// Demo-specific shader feature flags — combinable with engine version flags.
// Each bit enables one preprocessor #define in uber shaders via ShaderCache.
// Bit positions 0-4, 20-21 are reserved for engine GLSL version flags.

enum ShaderFeature : uint32_t {
    SF_NONE           = 0,

    // Rendering features (demo-specific, combinable)
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

// Demo feature→define mapping table (passed to ShaderCache::init).
static const FeatureDefine kDemoFeatures[] = {
    { SF_SHADOWS,        "HAS_SHADOWS"        },
    { SF_SHADOW_PCF3,    "HAS_SHADOW_PCF3"    },
    { SF_SHADOW_PCF5,    "HAS_SHADOW_PCF5"    },
    { SF_SHADOW_PCSS,    "HAS_SHADOW_PCSS"    },
    { SF_NORMAL_MAP,     "HAS_NORMAL_MAP"     },
    { SF_POINT_LIGHTS,   "HAS_POINT_LIGHTS"   },
    { SF_PBR,            "HAS_PBR"            },
    { SF_SSS,            "HAS_SSS"            },
    { SF_WATER,          "HAS_WATER"          },
    { SF_VIGNETTE,       "HAS_VIGNETTE"       },
    { SF_INSTANCING,     "HAS_INSTANCING"     },
    { SF_PUDDLE_EXCLUDE, "HAS_PUDDLE_EXCLUDE" },
    { SF_DOMAIN_WARP,    "HAS_DOMAIN_WARP"    },
    { SF_PHYSICAL_SKY,   "HAS_PHYSICAL_SKY"   },
    { SF_FILM_GRAIN,     "HAS_FILM_GRAIN"     },
};
static const int kDemoFeatureCount = static_cast<int>(sizeof(kDemoFeatures) / sizeof(kDemoFeatures[0]));

// Convert tier + renderer profile to shader feature set (demo-specific).
ShaderFeatureSet featuresForTier(DemoTier tier, bool core_profile, bool is_gles = false, bool gles3 = false);
