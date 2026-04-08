#include "renderer/gl_extensions.h"
#include "renderer/gl_funcs.h"
#include "renderer/gl_loader.h"
#include "platform/logger.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>

bool GLExtensions::initialized_ = false;

static bool s_modern = false;
static std::vector<std::string> s_list;
static std::string s_legacy;

void GLExtensions::init() {
    s_list.clear();
    s_legacy.clear();
    s_modern = false;

    int major = GLLoader::glMajor();

    // GL 3.0+ or GLES 3.0+: use glGetStringi (modern path)
    bool use_modern = (major >= 3);
#ifdef CB_NEED_GL_LOAD
    use_modern = use_modern && (cb_glGetStringi != nullptr);
#endif

    if (use_modern) {
        s_modern = true;
        int n = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        s_list.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) {
            const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
            if (ext) s_list.push_back(ext);
        }
        LOG_DBG("GLExtensions: modern path, %d extensions enumerated", n);
    } else {
        // GL 2.x / GLES 2.0: legacy single-string path
        const char* raw = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (raw) {
            s_legacy = raw;
            // Count for logging
            int count = 0;
            const char* p = raw;
            while (*p) { if (*p == ' ') count++; p++; }
            if (p != raw) count++;
            LOG_DBG("GLExtensions: legacy path, ~%d extensions", count);
        } else {
            LOG_WRN("GLExtensions: glGetString(GL_EXTENSIONS) returned null");
        }
    }

    initialized_ = true;
}

bool GLExtensions::has(const char* name) {
    if (!initialized_) return false;

    if (s_modern) {
        return std::find(s_list.begin(), s_list.end(), name) != s_list.end();
    }

    // Legacy: word-boundary matching (not simple strstr)
    if (s_legacy.empty()) return false;
    const char* hay = s_legacy.c_str();
    size_t len = strlen(name);
    const char* p = hay;
    while ((p = strstr(p, name)) != nullptr) {
        bool start_ok = (p == hay || p[-1] == ' ');
        bool end_ok = (p[len] == '\0' || p[len] == ' ');
        if (start_ok && end_ok) return true;
        p += len;
    }
    return false;
}
