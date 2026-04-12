#pragma once

#ifdef CB_GLES_NATIVE
// GLES stub — GPU timer queries not available on GLES 2.0/3.0
class GPUTimer {
public:
    void init() {}
    bool available() const { return false; }
    void begin() {}
    void end() {}
    double elapsed_ms() { return 0.0; }
};
#else

#include "renderer/backend/gl/gl_funcs.h"

// GPU Timer using GL_TIME_ELAPSED queries.
// Requires GL 3.3 or GL_ARB_timer_query extension.
class GPUTimer {
public:
    GPUTimer();
    ~GPUTimer();

    GPUTimer(const GPUTimer&) = delete;
    GPUTimer& operator=(const GPUTimer&) = delete;
    GPUTimer(GPUTimer&&) = delete;
    GPUTimer& operator=(GPUTimer&&) = delete;

    // Initialize and check availability. Call after GL context is created.
    void init();

    // Returns true if GPU timer queries are available.
    bool available() const { return available_; }

    // Begin/end a GPU timing measurement.
    void begin();
    void end();

    // Get elapsed time in milliseconds. Blocks until result is ready.
    double elapsed_ms();

private:
    bool available_;
    bool initialized_;
    unsigned int query_;

    using PFN_glGenQueries            = void (APIENTRY *)(GLsizei, GLuint*);
    using PFN_glDeleteQueries         = void (APIENTRY *)(GLsizei, const GLuint*);
    using PFN_glBeginQuery            = void (APIENTRY *)(GLenum, GLuint);
    using PFN_glEndQuery              = void (APIENTRY *)(GLenum);
    using PFN_glGetQueryObjectui64v   = void (APIENTRY *)(GLuint, GLenum, GLuint64*);
    using PFN_glGetQueryObjectiv      = void (APIENTRY *)(GLuint, GLenum, GLint*);

    PFN_glGenQueries gen_queries_;
    PFN_glDeleteQueries delete_queries_;
    PFN_glBeginQuery begin_query_;
    PFN_glEndQuery end_query_;
    PFN_glGetQueryObjectui64v get_query_ui64v_;
    PFN_glGetQueryObjectiv get_query_iv_;
};

#endif // CB_GLES_NATIVE
