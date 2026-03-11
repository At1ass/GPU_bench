#pragma once
#include "gl_funcs.h"

// GPU Timer using GL_TIME_ELAPSED queries.
// Requires GL 3.3 or GL_ARB_timer_query extension.
class GPUTimer {
public:
    GPUTimer();
    ~GPUTimer();

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

    // Function pointers for query functions
    typedef void (APIENTRY *PFN_glGenQueries)(GLsizei, GLuint*);
    typedef void (APIENTRY *PFN_glDeleteQueries)(GLsizei, const GLuint*);
    typedef void (APIENTRY *PFN_glBeginQuery)(GLenum, GLuint);
    typedef void (APIENTRY *PFN_glEndQuery)(GLenum);
    typedef void (APIENTRY *PFN_glGetQueryObjectui64v)(GLuint, GLenum, GLuint64*);
    typedef void (APIENTRY *PFN_glGetQueryObjectiv)(GLuint, GLenum, GLint*);

    PFN_glGenQueries gen_queries_;
    PFN_glDeleteQueries delete_queries_;
    PFN_glBeginQuery begin_query_;
    PFN_glEndQuery end_query_;
    PFN_glGetQueryObjectui64v get_query_ui64v_;
    PFN_glGetQueryObjectiv get_query_iv_;
};
