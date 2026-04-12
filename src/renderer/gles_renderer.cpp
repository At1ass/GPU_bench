#include "renderer/gles_renderer.h"
#include "renderer/gl_extensions.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>
#include <utility>

// GL_GENERATE_MIPMAP is a deprecated GL 1.4 constant, not in GLES headers.
// Fallback path uses it for ancient drivers; define if missing.
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

// ---- GLES-compatible shaders ----
// GLES 2.0: #version 100, precision required in fragment shaders.
// attribute/varying/texture2D syntax is the same as GLSL 1.20.

static const char* GLES_VS_3D = R"(
#version 100
precision highp float;
attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
varying vec3 v_normal;
varying vec2 v_uv;
void main() {
    v_normal = mat3(u_model) * a_normal;
    v_uv = a_uv;
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
}
)";

static const char* GLES_FS_3D = R"(
#version 100
precision mediump float;
varying vec3 v_normal;
varying vec2 v_uv;
uniform sampler2D u_tex;
uniform float u_use_tex;
uniform vec4 u_color;
uniform vec3 u_light_dir;
void main() {
    vec3 n = normalize(v_normal);
    float d = max(dot(n, normalize(u_light_dir)), 0.0);
    float light = 0.15 + 0.85 * d;
    vec4 c = u_color;
    if (u_use_tex > 0.5)
        c = texture2D(u_tex, v_uv);
    gl_FragColor = vec4(c.rgb * light, c.a);
}
)";

static const char* GLES_VS_2D = R"(
#version 100
precision highp float;
attribute vec2 a_pos;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* GLES_FS_2D_COLOR = R"(
#version 100
precision mediump float;
uniform vec4 u_color;
void main() {
    gl_FragColor = u_color;
}
)";

static const char* GLES_VS_2D_TEX = R"(
#version 100
precision highp float;
attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* GLES_FS_2D_TEX = R"(
#version 100
precision mediump float;
varying vec2 v_uv;
uniform sampler2D u_tex;
void main() {
    gl_FragColor = texture2D(u_tex, v_uv);
}
)";

// ---- Implementation ----

GLESRenderer::GLESRenderer() : gles3_(false) {}
// ~GLESRenderer: default. shutdown() must be called explicitly before destruction.

bool GLESRenderer::init(int w, int h) {
    if (initialized_) return true;

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    gpu_vendor_   = vendor   ? vendor   : "Unknown";
    gpu_renderer_ = renderer ? renderer : "Unknown";
    gl_version_   = version  ? version  : "Unknown";

    detectCaps();

    // Determine GLES version from GL_VERSION string
    // GLES reports "OpenGL ES X.Y ..." or "OpenGL ES-CM X.Y ..."
    gles3_ = false;
    if (version) {
        const char* es = strstr(version, "ES ");
        if (es) {
            int major = 0;
            sscanf(es + 3, "%d", &major);
            if (major >= 3) gles3_ = true;
        }
    }

    // GLES 2.0 may not support 32-bit indices without extension
    if (!gles3_) {
        caps_.supports_32bit_indices = false;
        if (GLExtensions::has("GL_OES_element_index_uint"))
            caps_.supports_32bit_indices = true;
    }

    // GLES 3.0+ has FBO, glGenerateMipmap built-in
    if (gles3_) {
        caps_.has_fbo = true;
        caps_.has_generate_mipmap_func = true;
    }

    gpu_timer_.init();

    // Check if glBlitFramebuffer is available
#ifdef CB_NEED_GL_LOAD
    has_blit_framebuffer_ = (cb_glBlitFramebuffer != 0);
#else
    has_blit_framebuffer_ = gles3_;
#endif

    // Slot 0 for render targets is reserved as "invalid"
    render_targets_.emplace_back();

    // Build GLES shaders
    if (!buildShader(shader_3d_, GLES_VS_3D, GLES_FS_3D)) return false;
    if (!buildShader(shader_2d_color_, GLES_VS_2D, GLES_FS_2D_COLOR)) return false;
    if (!buildShader(shader_2d_tex_, GLES_VS_2D_TEX, GLES_FS_2D_TEX)) return false;

    // Slot 0 reserved as "invalid"
    meshes_.emplace_back();
    meshes_[0].valid = false;
    textures_.emplace_back();
    textures_[0].valid = false;
    custom_shaders_.emplace_back(0);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Set sensible default uniforms
    ShaderProg* shaders[] = {&shader_3d_, &shader_2d_color_, &shader_2d_tex_};
    for (auto* sp : shaders) {
        glUseProgram(sp->program);
        if (sp->u_color >= 0)     glUniform4f(sp->u_color, 1.0f, 1.0f, 1.0f, 1.0f);
        if (sp->u_light_dir >= 0) glUniform3f(sp->u_light_dir, 0.4f, 0.7f, 0.5f);
        if (sp->u_use_tex >= 0)   glUniform1f(sp->u_use_tex, 0.0f);
        if (sp->u_tex >= 0)       glUniform1i(sp->u_tex, 0);
    }
    glUseProgram(0);

    setViewport(0, 0, w, h);

    LOG_DBG("GLESRenderer: GLES %s, version=%s, 32bit_idx=%s, fbo=%s, max_tex=%d",
             gles3_ ? "3.0+" : "2.0",
             gl_version_.c_str(),
             caps_.supports_32bit_indices ? "yes" : "no",
             caps_.has_fbo ? "yes" : "no",
             caps_.max_texture_size);

    initialized_ = true;
    return true;
}

TextureHandle GLESRenderer::createTexture(int w, int h, int channels, const unsigned char* pixels) {
    if (w > caps_.max_texture_size || h > caps_.max_texture_size) {
        // Use GL2's downscale path
        return GL2Renderer::createTexture(w, h, channels, pixels);
    }

    GLTex gt;
    gt.valid = true;
    glGenTextures(1, &gt.id);
    glBindTexture(GL_TEXTURE_2D, gt.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // GLES format handling:
    // - GLES 2.0: GL_LUMINANCE is supported
    // - GLES 3.0+: GL_LUMINANCE still works (compat) but GL_RED is preferred
    GLenum fmt;
    if (channels == 4) {
        fmt = GL_RGBA;
    } else if (channels == 3) {
        fmt = GL_RGB;
    } else {
#ifndef GL_RED
#define GL_RED 0x1903
#endif
        fmt = gles3_ ? GL_RED : GL_LUMINANCE;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt), w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);

    // glGenerateMipmap is always available on GLES 2.0+ (core function)
    if (caps_.has_generate_mipmap_func) {
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        // GLES 2.0 fallback: GL_GENERATE_MIPMAP hint
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt), w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    TextureHandle th;
    if (!free_tex_slots_.empty()) {
        th = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[th.id] = std::move(gt);
    } else {
        th = TextureHandle(static_cast<unsigned int>(textures_.size()));
        textures_.push_back(std::move(gt));
    }
    return th;
}

void GLESRenderer::drawMesh(MeshHandle h) {
    if (!isValidMesh(h)) return;
    const GLMesh& gm = meshes_[h.id];

    if (gles3_ && gm.vao) {
        // GLES 3.0 VAO fast path
        glBindVertexArray(gm.vao);
        glDrawElements(GL_TRIANGLES, gm.index_count, gm.index_type, nullptr);
        glBindVertexArray(0);
    } else {
        // GLES 2.0 path (same as GL2)
        GL2Renderer::drawMesh(h);
    }
}

void GLESRenderer::unbindState() {
    GL2Renderer::unbindState();
    if (gles3_) {
        glBindVertexArray(0);
    }
}

const char* GLESRenderer::getRendererName() const { return "GLES"; }
