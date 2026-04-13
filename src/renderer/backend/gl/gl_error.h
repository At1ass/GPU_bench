#pragma once
#include "renderer/backend/gl/gl_funcs.h"
#include "platform/logger.h"

// Debug-only GL error checking after resource creation operations.
// Compiles to nothing in release builds. Do NOT use in hot paths
// (drawMesh, setViewport, etc.) — only after create/destroy/compile.
//
// Usage:
//   glTexImage2D(...);
//   CHECK_GL_ERROR("glTexImage2D");
//
// Khronos recommendation: check glGetError() after resource creation
// to catch OOM, invalid format, size exceeding GL_MAX_TEXTURE_SIZE, etc.

#ifndef NDEBUG

inline void checkGLError_(const char* op, const char* file, int line) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        const char* name = "UNKNOWN";
        switch (err) {
            case GL_INVALID_ENUM:      name = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:     name = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: name = "INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:     name = "OUT_OF_MEMORY"; break;
#ifndef CB_GLES_NATIVE
            case GL_STACK_OVERFLOW:    name = "STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW:   name = "STACK_UNDERFLOW"; break;
#endif
        }
        LOG_WRN("GL error %s (0x%04X) after %s [%s:%d]",
                name, static_cast<unsigned>(err), op, file, line);
    }
}

#define CHECK_GL_ERROR(op) checkGLError_(op, __FILE__, __LINE__)

#else
#define CHECK_GL_ERROR(op) ((void)0)
#endif
