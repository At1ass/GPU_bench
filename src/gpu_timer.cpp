#include "gpu_timer.h"
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
typedef unsigned long long GLuint64;
#endif

GPUTimer::GPUTimer()
    : available_(false), initialized_(false), query_(0),
      gen_queries_(0), delete_queries_(0), begin_query_(0),
      end_query_(0), get_query_ui64v_(0), get_query_iv_(0) {}

GPUTimer::~GPUTimer() {
    if (initialized_ && query_ && delete_queries_) {
        delete_queries_(1, &query_);
    }
}

void GPUTimer::init() {
    available_ = false;
    initialized_ = true;

    // Check GL version or ARB_timer_query extension
    const char* ver = (const char*)glGetString(GL_VERSION);
    bool has_timer = false;
    if (ver) {
        int major = 0, minor = 0;
        if (sscanf(ver, "%d.%d", &major, &minor) >= 2) {
            if (major > 3 || (major == 3 && minor >= 3))
                has_timer = true;
        }
    }

    if (!has_timer) {
        const char* exts = (const char*)glGetString(GL_EXTENSIONS);
        if (exts && strstr(exts, "GL_ARB_timer_query"))
            has_timer = true;
    }

    if (!has_timer) {
        fprintf(stderr, "GPUTimer: GL_ARB_timer_query not available\n");
        return;
    }

    // Load function pointers
    gen_queries_ = (PFN_glGenQueries)SDL_GL_GetProcAddress("glGenQueries");
    delete_queries_ = (PFN_glDeleteQueries)SDL_GL_GetProcAddress("glDeleteQueries");
    begin_query_ = (PFN_glBeginQuery)SDL_GL_GetProcAddress("glBeginQuery");
    end_query_ = (PFN_glEndQuery)SDL_GL_GetProcAddress("glEndQuery");
    get_query_ui64v_ = (PFN_glGetQueryObjectui64v)SDL_GL_GetProcAddress("glGetQueryObjectui64v");
    get_query_iv_ = (PFN_glGetQueryObjectiv)SDL_GL_GetProcAddress("glGetQueryObjectiv");

    if (!gen_queries_ || !delete_queries_ || !begin_query_ || !end_query_ || !get_query_ui64v_) {
        fprintf(stderr, "GPUTimer: failed to load query functions\n");
        return;
    }

    gen_queries_(1, &query_);
    if (!query_) {
        fprintf(stderr, "GPUTimer: glGenQueries failed\n");
        return;
    }

    available_ = true;
    fprintf(stderr, "GPUTimer: GL_TIME_ELAPSED available\n");
}

void GPUTimer::begin() {
    if (available_) begin_query_(GL_TIME_ELAPSED, query_);
}

void GPUTimer::end() {
    if (available_) end_query_(GL_TIME_ELAPSED);
}

double GPUTimer::elapsed_ms() {
    if (!available_) return 0;

    // Wait for result
    if (get_query_iv_) {
        GLint ready = 0;
        while (!ready) {
            get_query_iv_(query_, GL_QUERY_RESULT_AVAILABLE, &ready);
        }
    }

    GLuint64 ns = 0;
    get_query_ui64v_(query_, GL_QUERY_RESULT, &ns);
    return (double)ns / 1000000.0;
}
