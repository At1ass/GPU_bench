#pragma once
#include <cstdint>

// Engine-level mapping from feature bitmask to GLSL #define string.
// Passed to ShaderCache at init time so the cache has no application-specific knowledge.
// The application (demo, benchmark, etc.) declares its own feature flag enum and table.
struct FeatureDefine {
    uint32_t mask;       // bit(s) in ShaderFeatureSet to test
    const char* define;  // GLSL preprocessor symbol (emitted as "#define <define>\n")
};
