#include "gl_funcs.h"
#include <cstdio>
#include <cstring>

#ifdef CB_NEED_GL_LOAD

// Define function pointers for GL functions NOT in imgl3w.
// Functions covered by imgl3w (glDrawElements, glCreateShader, glClear, etc.)
// are loaded by imgl3wInit2() and accessed via imgl3wProcs macros.

// GL 1.x extras
PFNCB_glFinish                  cb_glFinish = 0;
PFNCB_glCullFace                cb_glCullFace = 0;
PFNCB_glFrontFace               cb_glFrontFace = 0;
PFNCB_glBlendFunc               cb_glBlendFunc = 0;
PFNCB_glColorMask               cb_glColorMask = 0;
PFNCB_glDrawArrays              cb_glDrawArrays = 0;
PFNCB_glTexSubImage2D           cb_glTexSubImage2D = 0;

// GL 2.0 extras
PFNCB_glUniform1f               cb_glUniform1f = 0;
PFNCB_glUniform3f               cb_glUniform3f = 0;
PFNCB_glUniform4f               cb_glUniform4f = 0;
PFNCB_glBindAttribLocation      cb_glBindAttribLocation = 0;

// GL 3.0+ extras
PFNCB_glGenerateMipmap          cb_glGenerateMipmap = 0;
PFNCB_glDrawElementsInstanced   cb_glDrawElementsInstanced = 0;
PFNCB_glVertexAttribDivisor     cb_glVertexAttribDivisor = 0;

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

    // Load extra functions not covered by imgl3w
    #define LOAD(ptr, name) \
        ptr = (decltype(ptr))SDL_GL_GetProcAddress(#name); \
        if (!ptr) { fprintf(stderr, "FATAL: GL func '%s' not found\n", #name); return false; }

    // GL 1.x extras (must also go through loaded pointers for Wine compatibility)
    LOAD(cb_glFinish,           glFinish)
    LOAD(cb_glCullFace,         glCullFace)
    LOAD(cb_glFrontFace,        glFrontFace)
    LOAD(cb_glBlendFunc,        glBlendFunc)
    LOAD(cb_glColorMask,        glColorMask)
    LOAD(cb_glDrawArrays,       glDrawArrays)

    // glTexSubImage2D — not fatal if missing (GL 1.1, rare to be absent)
    cb_glTexSubImage2D = (PFNCB_glTexSubImage2D)SDL_GL_GetProcAddress("glTexSubImage2D");

    // GL 2.0 extras
    LOAD(cb_glUniform1f,        glUniform1f)
    LOAD(cb_glUniform3f,        glUniform3f)
    LOAD(cb_glUniform4f,        glUniform4f)
    LOAD(cb_glBindAttribLocation, glBindAttribLocation)

    #undef LOAD
    return true;
}

bool loadGL3Functions() {
    #define LOAD_OPT(ptr, name) \
        ptr = (decltype(ptr))SDL_GL_GetProcAddress(#name);

    LOAD_OPT(cb_glGenerateMipmap,         glGenerateMipmap)
    LOAD_OPT(cb_glDrawElementsInstanced,  glDrawElementsInstanced)
    LOAD_OPT(cb_glVertexAttribDivisor,    glVertexAttribDivisor)

    #undef LOAD_OPT

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
