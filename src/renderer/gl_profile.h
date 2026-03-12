#pragma once

// GL version -> core feature mapping (from OpenGL spec)
struct GLProfile {
    bool vao;
    bool instancing;
    bool mrt;
    bool texture_array;
    bool transform_feedback;
    bool ubo;
    bool geometry_shader;
    bool compute;
    bool ssbo;
    bool indirect_draw;
    bool tessellation;
    bool texture_gather;
    bool image_load_store;
    bool buffer_storage;

    // What the GL spec guarantees for a given version.
    // Does NOT reflect extensions -- only core features.
    static GLProfile coreProfile(int major, int minor) {
        GLProfile p = {};
        // GL 3.0 core
        if (major > 3 || (major == 3)) {
            p.vao = true;
            p.instancing = true;
            p.mrt = true;
            p.texture_array = true;
            p.transform_feedback = true;
        }
        // GL 3.1 core
        if (major > 3 || (major == 3 && minor >= 1)) {
            p.ubo = true;
        }
        // GL 3.2 core
        if (major > 3 || (major == 3 && minor >= 2)) {
            p.geometry_shader = true;
        }
        // GL 4.0 core
        if (major > 4 || (major == 4 && minor >= 0)) {
            p.tessellation = true;
            p.texture_gather = true;
        }
        // GL 4.2 core
        if (major > 4 || (major == 4 && minor >= 2)) {
            p.image_load_store = true;
        }
        // GL 4.3 core
        if (major > 4 || (major == 4 && minor >= 3)) {
            p.compute = true;
            p.ssbo = true;
            p.indirect_draw = true;
        }
        // GL 4.4 core
        if (major > 4 || (major == 4 && minor >= 4)) {
            p.buffer_storage = true;
        }
        return p;
    }
};
