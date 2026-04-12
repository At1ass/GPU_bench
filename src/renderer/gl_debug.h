#pragma once

// GL debug output wrapper (GL_KHR_debug / GL_ARB_debug_output).
// All methods are noop if KHR_debug is not available, not initialized,
// or on GLES platforms where it's unsupported.

#ifdef CB_GLES_NATIVE
// GLES stub — KHR_debug not available on most GLES implementations
class GLDebug {
public:
    static void init() {}
    static bool available() { return false; }
    static void pushGroup(const char*) {}
    static void popGroup() {}
    static void labelBuffer(unsigned int, const char*) {}
    static void labelTexture(unsigned int, const char*) {}
    static void labelShader(unsigned int, const char*) {}
    static void labelFramebuffer(unsigned int, const char*) {}
};
#else

class GLDebug {
public:
    static void init();
    static bool available();

    // Debug group markers (visible in RenderDoc, NVIDIA Nsight, Intel GPA)
    static void pushGroup(const char* name);
    static void popGroup();

    // Object naming (visible in debug tools)
    static void labelBuffer(unsigned int id, const char* name);
    static void labelTexture(unsigned int id, const char* name);
    static void labelShader(unsigned int program, const char* name);
    static void labelFramebuffer(unsigned int fbo, const char* name);

private:
    static bool available_;
};

#endif // CB_GLES_NATIVE
