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
    if (imgl3wInit2(sdl_get_gl_proc) != 0) {
        fprintf(stderr, "FATAL: imgl3wInit2 failed\n");
        return false;
    }

    #define CB_LOAD_REQ(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name); \
        if (!cb_##name) { fprintf(stderr, "FATAL: GL func '%s' not found\n", #name); return false; }
    CB_GL_REQUIRED_FUNCS(CB_LOAD_REQ)
    #undef CB_LOAD_REQ

    #define CB_LOAD_SOFT(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name);
    CB_GL_SOFTREQ_FUNCS(CB_LOAD_SOFT)
    #undef CB_LOAD_SOFT

    return true;
}

bool loadGL3Functions() {
    #define CB_LOAD_OPT(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name);
    CB_GL3_OPTIONAL_FUNCS(CB_LOAD_OPT)
    #undef CB_LOAD_OPT
    return true;
}

bool loadGL4Functions() {
    // No GL4-specific function pointers to load yet
    return true;
}

bool hasGL3FunctionPointers() {
    bool has_vao = (imgl3wProcs.gl.GenVertexArrays != 0)
                && (imgl3wProcs.gl.BindVertexArray != 0)
                && (imgl3wProcs.gl.DeleteVertexArrays != 0);
    return has_vao && cb_glGenerateMipmap
        && cb_glDrawElementsInstanced && cb_glVertexAttribDivisor;
}

bool hasGL4FunctionPointers() {
    return (SDL_GL_GetProcAddress("glDispatchCompute") != 0);
}

// Not used on CB_NEED_GL_LOAD path, but defined to avoid link errors
bool checkGL3ByVersion() { return hasGL3FunctionPointers(); }
bool checkGL4ByVersion() { return hasGL4FunctionPointers(); }

bool loadGLFunctions() {
    return loadGL2Functions();
}

#else

bool loadGL2Functions() { return true; }
bool loadGL3Functions() { return true; }
bool loadGL4Functions() { return true; }

// Not used on Linux path, but defined to avoid link errors
bool hasGL3FunctionPointers() { return checkGL3ByVersion(); }
bool hasGL4FunctionPointers() { return checkGL4ByVersion(); }

bool checkGL3ByVersion() {
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!ver) return false;
    const char* num = ver;
    const char* es = strstr(ver, "ES ");
    if (es) num = es + 3;
    int major = 0;
    if (sscanf(num, "%d", &major) < 1 || major < 3) {
        const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (!exts || !strstr(exts, "GL_ARB_vertex_array_object"))
            return false;
    }
    return true;
}

bool checkGL4ByVersion() {
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!ver) return false;
    int major = 0, minor = 0;
    if (sscanf(ver, "%d.%d", &major, &minor) < 1) return false;
    return (major > 4 || (major == 4 && minor >= 3));
}

bool loadGLFunctions() { return true; }

#endif
