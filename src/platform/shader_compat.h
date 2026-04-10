#pragma once
#include "renderer/renderer.h"

// Lightweight helpers for selecting GLSL version + precision preamble
// in bench test inline shaders. Covers Desktop GL (120/140/150) and GLES (100/300 es).

// For GL2-universal tests (attribute/varying or in/out):
//   Desktop compat:  "#version 120\n"
//   Desktop core:    "#version 150\n"
//   GLES 2.0:        "#version 100\nprecision highp float;\n"
//   GLES 3.0+:       "#version 300 es\nprecision highp float;\n"
inline const char* testShaderPreamble120(const Renderer* r) {
    if (r->isGLES())
        return r->isGLES3() ? "#version 300 es\nprecision highp float;\n"
                            : "#version 100\nprecision highp float;\n";
    return r->isCoreProfile() ? "#version 150\n" : "#version 120\n";
}

// For GL3+ tests (in/out only, no legacy path):
//   Desktop compat:  "#version 140\n"
//   Desktop core:    "#version 150\n"
//   GLES 3.0+:       "#version 300 es\nprecision highp float;\n"
inline const char* testShaderPreamble140(const Renderer* r) {
    if (r->isGLES()) return "#version 300 es\nprecision highp float;\n";
    return r->isCoreProfile() ? "#version 150\n" : "#version 140\n";
}

// True when legacy GLSL syntax is needed (attribute/varying/gl_FragColor/texture2D).
// False for core profile, GLES 3.0+ (in/out/FragColor/texture).
// GLES 2.0 also uses legacy syntax.
inline bool testShaderUsesLegacy(const Renderer* r) {
    if (r->isGLES()) return !r->isGLES3();
    return !r->isCoreProfile();
}
