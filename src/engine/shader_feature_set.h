#pragma once
#include <cstdint>

// Engine-level shader feature set — opaque bitfield.
// Specific flag values are defined by the application (e.g., demo/tier/shader_feature.h).
using ShaderFeatureSet = uint32_t;

// GLSL version flags (engine-level, shared across all applications).
// Exactly one version flag must be set in any ShaderFeatureSet.
enum ShaderVersionFlag : uint32_t {
    SF_GLSL_120 = 1u << 0,
    SF_GLSL_150 = 1u << 1,
    SF_GLSL_140 = 1u << 2,
    SF_GLSL_330 = 1u << 3,
    SF_GLSL_430 = 1u << 4,
    SF_GLES_100 = 1u << 20,
    SF_GLES_300 = 1u << 21,
};

static const ShaderFeatureSet SF_VERSION_MASK =
    SF_GLSL_120 | SF_GLSL_150 | SF_GLSL_140 | SF_GLSL_330 | SF_GLSL_430 |
    SF_GLES_100 | SF_GLES_300;
