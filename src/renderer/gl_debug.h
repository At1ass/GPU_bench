#pragma once

// GL debug output wrapper (GL_KHR_debug / GL_ARB_debug_output).
// Khronos: "Use KHR_debug. It is the single most useful tool for OpenGL development."
//
// Usage:
//   GLDebug::init();                          // after GL context creation
//   GLDebug::pushGroup("ShadowPass");         // visible in RenderDoc/Nsight
//   ... render ...
//   GLDebug::popGroup();
//   GLDebug::labelTexture(tex_id, "shadow_depth");
//
// All methods are noop if KHR_debug is not available or not initialized.
// Ref: https://www.khronos.org/opengl/wiki/Debug_Output

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
