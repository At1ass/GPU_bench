#pragma once

// High-level GL function loader and capability query.
// Wraps the low-level gl_funcs.h X-macro system.
// Call init() once after GL context creation. Then query availability.
class GLLoader {
public:
    // Load all GL function pointers (GL2 required, GL3/GL4 optional).
    // Returns false if GL2 loading fails (fatal).
    static bool init();

    // Availability checks (valid after init)
    static bool hasGL3();
    static bool hasGL4();
    static bool isGLES();

    // Parsed GL version (valid after init)
    static int glMajor();
    static int glMinor();

private:
    static bool initialized_;
    static bool gl3_available_;
    static bool gl4_available_;
    static bool is_gles_;
    static int  gl_major_;
    static int  gl_minor_;

    static void parseVersion();
};
