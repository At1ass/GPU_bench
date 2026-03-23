#include "renderer/gl_funcs.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>

#ifdef CB_NEED_GL_LOAD

// =========================================================================
// Variable definitions — generated from X-macro lists
// =========================================================================
#define CB_DEFINE_PTR(name) PFNCB_##name cb_##name = nullptr;
CB_GL_REQUIRED_FUNCS(CB_DEFINE_PTR)
CB_GL_SOFTREQ_FUNCS(CB_DEFINE_PTR)
CB_GL3_OPTIONAL_FUNCS(CB_DEFINE_PTR)
CB_GL4_OPTIONAL_FUNCS(CB_DEFINE_PTR)
CB_GL_EXT_FUNCS(CB_DEFINE_PTR)
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
    int loaded = 0, total = 0;
    #define CB_LOAD_OPT(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name); \
        total++; if (cb_##name) loaded++;
    CB_GL3_OPTIONAL_FUNCS(CB_LOAD_OPT)
    #undef CB_LOAD_OPT
    Log::dbg("GL3 funcs: %d/%d loaded", loaded, total);
    return true;
}

bool loadGL4Functions() {
    int loaded = 0, total = 0;
    #define CB_LOAD_OPT(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name); \
        total++; if (cb_##name) loaded++;
    CB_GL4_OPTIONAL_FUNCS(CB_LOAD_OPT)
    #undef CB_LOAD_OPT
    Log::dbg("GL4 funcs: %d/%d loaded", loaded, total);
    return true;
}

bool loadGLExtFunctions() {
    int loaded = 0, total = 0;
    #define CB_LOAD_OPT(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name); \
        total++; if (cb_##name) loaded++;
    CB_GL_EXT_FUNCS(CB_LOAD_OPT)
    #undef CB_LOAD_OPT
    Log::dbg("GL ext funcs: %d/%d loaded", loaded, total);
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
    // GL4 features start at 4.0 (tessellation, texture gather).
    // Compute/SSBO/indirect (4.3) are optional, checked inside GL4Renderer.
    return (SDL_GL_GetProcAddress("glPatchParameteri") != 0);  // GL 4.0 tessellation
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

// Extension-only functions must be loaded via SDL_GL_GetProcAddress even on Linux
// because they are not link-time symbols (not part of core GL).
// Variable definitions for ext funcs (on Linux, outside CB_NEED_GL_LOAD block).
#define CB_DEFINE_EXT_PTR(name) PFNCB_##name cb_##name = nullptr;
CB_GL_EXT_FUNCS(CB_DEFINE_EXT_PTR)
#undef CB_DEFINE_EXT_PTR

bool loadGLExtFunctions() {
    int loaded = 0, total = 0;
    #define CB_LOAD_OPT(name) \
        cb_##name = (PFNCB_##name)SDL_GL_GetProcAddress(#name); \
        total++; if (cb_##name) loaded++;
    CB_GL_EXT_FUNCS(CB_LOAD_OPT)
    #undef CB_LOAD_OPT
    Log::dbg("GL ext funcs: %d/%d loaded", loaded, total);
    return true;
}

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
    // GL4 features start at 4.0 (tessellation, texture gather).
    // Compute/SSBO/indirect (4.3) are optional, checked inside GL4Renderer.
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!ver) return false;
    int major = 0, minor = 0;
    if (sscanf(ver, "%d.%d", &major, &minor) < 1) return false;
    return (major >= 4);
}

bool loadGLFunctions() { return true; }

#endif
