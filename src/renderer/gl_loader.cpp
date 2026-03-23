#include "renderer/gl_loader.h"
#include "renderer/gl_funcs.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>

bool GLLoader::initialized_   = false;
bool GLLoader::gl3_available_  = false;
bool GLLoader::gl4_available_  = false;
bool GLLoader::is_gles_        = false;
int  GLLoader::gl_major_       = 2;
int  GLLoader::gl_minor_       = 0;

void GLLoader::parseVersion() {
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!ver) return;

    is_gles_ = (strstr(ver, "OpenGL ES") != 0);

    // Find the numeric part
    const char* num = ver;
    const char* es = strstr(ver, "ES ");
    if (es) num = es + 3;
    const char* escm = strstr(ver, "ES-CM ");
    if (escm) num = escm + 6;

    int major = 0, minor = 0;
    if (sscanf(num, "%d.%d", &major, &minor) >= 1) {
        gl_major_ = major;
        gl_minor_ = minor;
    }
}

bool GLLoader::init() {
    if (initialized_) return true;

    // GL2 — required
    if (!loadGL2Functions()) {
        Log::err("GLLoader: failed to load GL2 functions");
        return false;
    }

    // Parse version before GL3/GL4 checks (needed on Linux path)
    parseVersion();
    Log::dbg("GLLoader: GL %d.%d, GLES=%s", gl_major_, gl_minor_, is_gles_ ? "yes" : "no");

    // GL3 — optional, load function pointers
    loadGL3Functions();
    Log::dbg("GLLoader: GL3 function pointers loaded");

    // GL4 — optional, load function pointers
    loadGL4Functions();
    Log::dbg("GLLoader: GL4 function pointers loaded");

    // Extension-only functions (ARB_bindless_texture etc)
    loadGLExtFunctions();
    Log::dbg("GLLoader: extension function pointers loaded");

    // Cache availability
#ifdef CB_NEED_GL_LOAD
    // Windows: check actual function pointers
    gl3_available_ = hasGL3FunctionPointers();
    gl4_available_ = hasGL4FunctionPointers();
#else
    // Linux: functions are link-time symbols, check version/extensions
    gl3_available_ = checkGL3ByVersion();
    gl4_available_ = checkGL4ByVersion();
#endif

    Log::dbg("GLLoader: GL3=%s, GL4=%s", gl3_available_ ? "yes" : "no", gl4_available_ ? "yes" : "no");
    initialized_ = true;
    return true;
}

bool GLLoader::hasGL3() { return gl3_available_; }
bool GLLoader::hasGL4() { return gl4_available_; }
bool GLLoader::isGLES() { return is_gles_; }
int  GLLoader::glMajor() { return gl_major_; }
int  GLLoader::glMinor() { return gl_minor_; }
