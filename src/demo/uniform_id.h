#pragma once

// Type-safe uniform identifiers for demo shaders.
// Generated from uniform_registry.def via X-macro.
//
// Usage:
//   UniformBlock ub;
//   ub.init(&shader);
//   ub.set(U::Proj, proj_matrix);
//   ub.set(U::Time, 1.5f);
//
// Adding a new uniform = one line in uniform_registry.def.

namespace U {
    enum Id {
        #define U(id, name, type) id,
        #include "demo/uniform_registry.def"
        #undef U
        COUNT
    };
}

// Lookup GLSL name by enum ID
inline const char* uniformName(U::Id id) {
    static const char* names[] = {
        #define U(id, name, type) name,
        #include "demo/uniform_registry.def"
        #undef U
    };
    return names[id];
}
