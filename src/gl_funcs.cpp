#include "gl_funcs.h"
#include <cstdio>
#include <cstring>

#ifdef CB_NEED_GL_LOAD

// =========================================================================
// Variable definitions — generated from X-macro lists
// =========================================================================
#define CB_DEFINE_PTR(name) PFNCB_##name cb_##name = 0;
CB_GL_REQUIRED_FUNCS(CB_DEFINE_PTR)
CB_GL_SOFTREQ_FUNCS(CB_DEFINE_PTR)
CB_GL3_OPTIONAL_FUNCS(CB_DEFINE_PTR)
#undef CB_DEFINE_PTR

// SDL-based proc address loader for imgl3w
static GL3WglProc sdl_get_gl_proc(const char* name) {
    return (GL3WglProc)SDL_GL_GetProcAddress(name);
}

bool loadGL2Functions() {
    // Initialize imgl3w with SDL's GL proc loader.
    // This loads ALL GL functions that imgl3w covers (~50 functions including
    // glDrawElements, glClear, glEnable, glViewport, glGetString, etc.)
    // through SDL_GL_GetProcAddress, ensuring the same dispatch path as ImGui GL3.
    if (imgl3wInit2(sdl_get_gl_proc) != 0) {
        fprintf(stderr, "FATAL: imgl3wInit2 failed\n");
        return false;
    }

    // Load required functions — fatal if missing
    #define CB_LOAD_REQ(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name); \
        if (!cb_##name) { fprintf(stderr, "FATAL: GL func '%s' not found\n", #name); return false; }
    CB_GL_REQUIRED_FUNCS(CB_LOAD_REQ)
    #undef CB_LOAD_REQ

    // Load soft-required functions — non-fatal
    #define CB_LOAD_SOFT(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name);
    CB_GL_SOFTREQ_FUNCS(CB_LOAD_SOFT)
    #undef CB_LOAD_SOFT

    return true;
}

bool loadGL3Functions() {
    // Load all GL3 optional functions
    #define CB_LOAD_OPT(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name);
    CB_GL3_OPTIONAL_FUNCS(CB_LOAD_OPT)
    #undef CB_LOAD_OPT

    // VAO functions are in imgl3w — check if they were loaded
    bool has_vao = (imgl3wProcs.gl.GenVertexArrays != 0)
                && (imgl3wProcs.gl.BindVertexArray != 0)
                && (imgl3wProcs.gl.DeleteVertexArrays != 0);

    bool all_ok = has_vao && cb_glGenerateMipmap
               && cb_glDrawElementsInstanced && cb_glVertexAttribDivisor;
    return all_ok;
}

bool loadGLFunctions() {
    return loadGL2Functions();
}

#else

bool loadGL2Functions() { return true; }

bool loadGL3Functions() {
    // On Linux with GL_GLEXT_PROTOTYPES, symbols exist at link time but may
    // not be functional if the driver only supports GL 2.x. Verify by checking
    // that the GL version is at least 3.0 or that VAO extensions exist.
    const char* ver = (const char*)glGetString(GL_VERSION);
    if (!ver) return false;
    int major = 0;
    if (sscanf(ver, "%d", &major) < 1 || major < 3) {
        // Check ARB extension as fallback
        const char* exts = (const char*)glGetString(GL_EXTENSIONS);
        if (!exts || !strstr(exts, "GL_ARB_vertex_array_object"))
            return false;
    }
    return true;
}

bool loadGLFunctions() { return true; }

#endif
