#include "gpu_timer.h"
#include "logger.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>

#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#endif

// GLuint64 type — use unsigned long long if not already defined
#if !defined(GL_GLEXT_PROTOTYPES) && !defined(__glext_h_) && !defined(GL_GLEXT_VERSION)
using GLuint64 = unsigned long long;
#endif

GPUTimer::GPUTimer()
    : available_(false), initialized_(false), query_(0),
      gen_queries_(nullptr), delete_queries_(nullptr), begin_query_(nullptr),
      end_query_(nullptr), get_query_ui64v_(nullptr), get_query_iv_(nullptr) {}

GPUTimer::~GPUTimer() {
    if (initialized_ && query_ && delete_queries_) {
        delete_queries_(1, &query_);
    }
}

void GPUTimer::init() {
    available_ = false;
    initialized_ = true;

    // Check GL version or ARB_timer_query extension
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    bool has_timer = false;
    if (ver) {
        int major = 0, minor = 0;
        if (sscanf(ver, "%d.%d", &major, &minor) >= 2) {
            if (major > 3 || (major == 3 && minor >= 3))
                has_timer = true;
        }
    }

    if (!has_timer) {
        const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (exts && strstr(exts, "GL_ARB_timer_query"))
            has_timer = true;
    }

    if (!has_timer) {
        Log::info("GPUTimer: GL_ARB_timer_query not available");
        return;
    }

    // Load function pointers
    gen_queries_ = reinterpret_cast<PFN_glGenQueries>(SDL_GL_GetProcAddress("glGenQueries"));
    delete_queries_ = reinterpret_cast<PFN_glDeleteQueries>(SDL_GL_GetProcAddress("glDeleteQueries"));
    begin_query_ = reinterpret_cast<PFN_glBeginQuery>(SDL_GL_GetProcAddress("glBeginQuery"));
    end_query_ = reinterpret_cast<PFN_glEndQuery>(SDL_GL_GetProcAddress("glEndQuery"));
    get_query_ui64v_ = reinterpret_cast<PFN_glGetQueryObjectui64v>(SDL_GL_GetProcAddress("glGetQueryObjectui64v"));
    get_query_iv_ = reinterpret_cast<PFN_glGetQueryObjectiv>(SDL_GL_GetProcAddress("glGetQueryObjectiv"));

    if (!gen_queries_ || !delete_queries_ || !begin_query_ || !end_query_ || !get_query_ui64v_) {
        Log::warn("GPUTimer: failed to load query functions");
        return;
    }

    gen_queries_(1, &query_);
    if (!query_) {
        Log::warn("GPUTimer: glGenQueries failed");
        return;
    }

    available_ = true;
    Log::info("GPUTimer: GL_TIME_ELAPSED available");
}

void GPUTimer::begin() {
    if (available_) begin_query_(GL_TIME_ELAPSED, query_);
}

void GPUTimer::end() {
    if (available_) end_query_(GL_TIME_ELAPSED);
}

double GPUTimer::elapsed_ms() {
    if (!available_) return 0;

    // Wait for result with timeout (~100ms / 10000 iterations)
    if (get_query_iv_) {
        GLint ready = 0;
        int timeout = 10000;
        while (!ready && timeout-- > 0) {
            get_query_iv_(query_, GL_QUERY_RESULT_AVAILABLE, &ready);
        }
        if (!ready) return 0.0;
    }

    GLuint64 ns = 0;
    get_query_ui64v_(query_, GL_QUERY_RESULT, &ns);
    return static_cast<double>(ns) / 1000000.0;
}
