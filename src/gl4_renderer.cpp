#include "gl4_renderer.h"
#include "logger.h"
#include <cstdio>
#include <cstring>

GL4Renderer::GL4Renderer() {}
GL4Renderer::~GL4Renderer() { shutdown(); }

bool GL4Renderer::init(int w, int h) {
    if (!GL3Renderer::init(w, h)) return false;

    // Detect GL4-specific capabilities
    caps_.has_compute = false;

    // Compute shaders require GL 4.3+
    if (caps_.gl_major > 4 || (caps_.gl_major == 4 && caps_.gl_minor >= 3)) {
        caps_.has_compute = true;
    }

    // Extension fallback for GL 4.0-4.2
    if (!caps_.has_compute) {
#ifdef CB_NEED_GL_LOAD
        caps_.has_compute = (SDL_GL_GetProcAddress("glDispatchCompute") != 0);
#else
        const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (exts && strstr(exts, "GL_ARB_compute_shader"))
            caps_.has_compute = true;
#endif
    }

    Log::info("GL4Renderer: compute=%s",
            caps_.has_compute ? "yes" : "no");
    return true;
}

const char* GL4Renderer::getRendererName() const { return "GL4"; }
