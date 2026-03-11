#include "renderer_factory.h"
#include "gl2_renderer.h"
#include "gl3_renderer.h"
#include "gl_funcs.h"
#include <cstdio>
#include <cstring>

int detectGLMajorVersion() {
    const char* ver = (const char*)glGetString(GL_VERSION);
    if (!ver) return 2;
    int major = 0, minor = 0;
    if (sscanf(ver, "%d.%d", &major, &minor) >= 1) {
        return major;
    }
    return 2;
}

Renderer* createBestRenderer() {
    return createRenderer(0);
}

Renderer* createRenderer(int force_gl) {
    int gl_major = detectGLMajorVersion();

    if (force_gl == 3) {
        if (gl_major < 3) {
            fprintf(stderr, "Warning: GL3 requested but only GL %d available\n", gl_major);
        }
        if (!loadGL3Functions()) {
            fprintf(stderr, "Warning: Could not load GL3 functions, using GL2\n");
            return new GL2Renderer();
        }
        return new GL3Renderer();
    }

    if (force_gl == 2) {
        return new GL2Renderer();
    }

    // Auto-detect
    if (gl_major >= 3) {
        if (loadGL3Functions()) {
            fprintf(stderr, "Auto-detected GL %d, using GL3 renderer\n", gl_major);
            return new GL3Renderer();
        }
    }

    fprintf(stderr, "Using GL2 renderer\n");
    return new GL2Renderer();
}
