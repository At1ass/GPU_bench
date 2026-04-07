#include "renderer/gl_debug.h"
#include "renderer/gl_funcs.h"
#include "renderer/gl_extensions.h"
#include "renderer/gl_loader.h"
#include "platform/logger.h"
#include <cstring>
#include <unordered_map>

bool GLDebug::available_ = false;

// Rate-limit repeated messages: log each unique ID up to MAX_REPEATS times,
// then one "suppressed N more" summary. Prevents log spam from per-frame warnings.
static const int MAX_REPEATS = 3;
static std::unordered_map<GLuint, int> s_msg_counts;

static void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id,
                                   GLenum severity, GLsizei /*length*/,
                                   const GLchar* message, const void* /*userParam*/) {
    // Filter notifications unless Trace level is active (--debug=verbose)
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        if (!Log::levelEnabled(Log::Level::Trace)) return;
        LOG_TRC("GL [notification #%u]: %s", id, message);
        return;
    }

    // Rate limiting: after MAX_REPEATS, log one suppression notice, then silence
    int& count = s_msg_counts[id];
    count++;
    if (count == MAX_REPEATS + 1) {
        LOG_DBG("GL [#%u]: repeated %d times, suppressing further", id, MAX_REPEATS);
        return;
    }
    if (count > MAX_REPEATS + 1) return;

    const char* src_str = "?";
    switch (source) {
    case GL_DEBUG_SOURCE_API:             src_str = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   src_str = "Window"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: src_str = "Shader"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     src_str = "3rdParty"; break;
    case GL_DEBUG_SOURCE_APPLICATION:      src_str = "App"; break;
    case GL_DEBUG_SOURCE_OTHER:           src_str = "Other"; break;
    }

    const char* type_str = "?";
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:               type_str = "ERROR"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "DEPRECATED"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = "UNDEFINED"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         type_str = "PORTABILITY"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         type_str = "PERF"; break;
    case GL_DEBUG_TYPE_MARKER:              type_str = "MARKER"; break;
    case GL_DEBUG_TYPE_OTHER:               type_str = "OTHER"; break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        LOG_ERR("GL [%s/%s #%u]: %s", src_str, type_str, id, message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        LOG_WRN("GL [%s/%s #%u]: %s", src_str, type_str, id, message);
        break;
    case GL_DEBUG_SEVERITY_LOW:
        LOG_DBG("GL [%s/%s #%u]: %s", src_str, type_str, id, message);
        break;
    default:
        LOG_DBG("GL [%s/%s #%u]: %s", src_str, type_str, id, message);
        break;
    }
}

void GLDebug::init() {
    available_ = false;

    // KHR_debug is core in GL 4.3+ and available as extension on earlier versions
    bool has_debug = (GLLoader::glMajor() > 4 ||
                      (GLLoader::glMajor() == 4 && GLLoader::glMinor() >= 3));
    if (!has_debug)
        has_debug = GLExtensions::has("GL_KHR_debug") || GLExtensions::has("GL_ARB_debug_output");

#ifdef CB_NEED_GL_LOAD
    if (!cb_glDebugMessageCallback || !cb_glDebugMessageControl) {
        if (has_debug) LOG_DBG("GLDebug: extension detected but function pointers missing");
        return;
    }
#endif

    if (!has_debug) {
        LOG_DBG("GLDebug: KHR_debug not available");
        return;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debugCallback, nullptr);

    // Enable all messages except notifications
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

    available_ = true;
    LOG_INF("GLDebug: KHR_debug enabled (synchronous mode)");
}

bool GLDebug::available() {
    return available_;
}

void GLDebug::pushGroup(const char* name) {
    if (!available_) return;
#ifdef CB_NEED_GL_LOAD
    if (!cb_glPushDebugGroup) return;
#endif
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0,
                     static_cast<GLsizei>(strlen(name)), name);
}

void GLDebug::popGroup() {
    if (!available_) return;
#ifdef CB_NEED_GL_LOAD
    if (!cb_glPopDebugGroup) return;
#endif
    glPopDebugGroup();
}

void GLDebug::labelBuffer(unsigned int id, const char* name) {
    if (!available_ || !id) return;
#ifdef CB_NEED_GL_LOAD
    if (!cb_glObjectLabel) return;
#endif
    glObjectLabel(GL_BUFFER, id, static_cast<GLsizei>(strlen(name)), name);
}

void GLDebug::labelTexture(unsigned int id, const char* name) {
    if (!available_ || !id) return;
#ifdef CB_NEED_GL_LOAD
    if (!cb_glObjectLabel) return;
#endif
    glObjectLabel(GL_TEXTURE, id, static_cast<GLsizei>(strlen(name)), name);
}

void GLDebug::labelShader(unsigned int program, const char* name) {
    if (!available_ || !program) return;
#ifdef CB_NEED_GL_LOAD
    if (!cb_glObjectLabel) return;
#endif
    glObjectLabel(GL_PROGRAM, program, static_cast<GLsizei>(strlen(name)), name);
}

void GLDebug::labelFramebuffer(unsigned int fbo, const char* name) {
    if (!available_ || !fbo) return;
#ifdef CB_NEED_GL_LOAD
    if (!cb_glObjectLabel) return;
#endif
    glObjectLabel(GL_FRAMEBUFFER, fbo, static_cast<GLsizei>(strlen(name)), name);
}
