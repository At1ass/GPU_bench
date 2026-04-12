#pragma once

// GL version -> core feature mapping (from OpenGL spec).
// Does NOT reflect extensions — only guaranteed core features.
struct GLProfile {
    bool vao = false;
    bool instancing = false;
    bool mrt = false;
    bool texture_array = false;
    bool transform_feedback = false;
    bool ubo = false;
    bool geometry_shader = false;
    bool compute = false;
    bool ssbo = false;
    bool indirect_draw = false;
    bool tessellation = false;
    bool texture_gather = false;
    bool image_load_store = false;
    bool buffer_storage = false;

    static GLProfile coreProfile(int major, int minor) {
        GLProfile p;
        if (major >= 3) {                               // GL 3.0
            p.vao = true;
            p.instancing = true;
            p.mrt = true;
            p.texture_array = true;
            p.transform_feedback = true;
        }
        if (major > 3 || (major == 3 && minor >= 1))    // GL 3.1
            p.ubo = true;
        if (major > 3 || (major == 3 && minor >= 2))    // GL 3.2
            p.geometry_shader = true;
        if (major >= 4) {                               // GL 4.0
            p.tessellation = true;
            p.texture_gather = true;
        }
        if (major > 4 || (major == 4 && minor >= 2))    // GL 4.2
            p.image_load_store = true;
        if (major > 4 || (major == 4 && minor >= 3)) {  // GL 4.3
            p.compute = true;
            p.ssbo = true;
            p.indirect_draw = true;
        }
        if (major > 4 || (major == 4 && minor >= 4))    // GL 4.4
            p.buffer_storage = true;
        return p;
    }
};
