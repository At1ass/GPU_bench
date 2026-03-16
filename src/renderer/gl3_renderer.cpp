#include "renderer/gl3_renderer.h"
#include "renderer/gl_profile.h"
#include "platform/logger.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

GL3Renderer::GL3Renderer() {}
// ~GL3Renderer: default. shutdown() must be called explicitly before destruction.

bool GL3Renderer::init(int w, int h) {
    if (!GL2Renderer::init(w, h)) return false;

    // Baseline from GL spec
    GLProfile baseline = GLProfile::coreProfile(caps_.gl_major, caps_.gl_minor);
    has_vao_ = baseline.vao;
    has_instancing_ = baseline.instancing;
    has_mrt_ = baseline.mrt;
    has_texture_array_ = baseline.texture_array;
    has_transform_feedback_ = baseline.transform_feedback;
    has_ubo_ = baseline.ubo;
    has_geometry_shader_ = baseline.geometry_shader;

#ifdef CB_NEED_GL_LOAD
    // Windows: verify function pointers (driver could be broken)
    if (has_vao_ && !imgl3wProcs.gl.GenVertexArrays) {
        Log::warn("GL%d.%d claims VAO but glGenVertexArrays missing", caps_.gl_major, caps_.gl_minor);
        has_vao_ = false;
    }
    if (has_instancing_ && !cb_glDrawElementsInstanced) {
        has_instancing_ = false;
    }
    if (has_texture_array_ && !cb_glTexImage3D) {
        has_texture_array_ = false;
    }
    if (has_mrt_ && !cb_glDrawBuffers) {
        has_mrt_ = false;
    }
    if (has_ubo_ && (!cb_glUniformBlockBinding || !cb_glGetUniformBlockIndex)) {
        has_ubo_ = false;
    }
    if (has_transform_feedback_ && (!cb_glBeginTransformFeedback || !cb_glEndTransformFeedback || !cb_glTransformFeedbackVaryings)) {
        has_transform_feedback_ = false;
    }
    if (has_geometry_shader_ && !SDL_GL_GetProcAddress("glProgramParameteri")) {
        has_geometry_shader_ = false;
    }

    // Extensions: enable features below core version
    if (!has_vao_) {
        has_vao_ = (imgl3wProcs.gl.GenVertexArrays != 0)
                && (imgl3wProcs.gl.BindVertexArray != 0)
                && (imgl3wProcs.gl.DeleteVertexArrays != 0);
    }
    if (!has_instancing_) {
        has_instancing_ = (cb_glDrawElementsInstanced != 0)
                       && (cb_glVertexAttribDivisor != 0);
    }
    if (!has_texture_array_)
        has_texture_array_ = (cb_glTexImage3D != 0);
    if (!has_mrt_)
        has_mrt_ = (cb_glDrawBuffers != 0);
    if (!has_ubo_)
        has_ubo_ = (cb_glUniformBlockBinding != 0) && (cb_glGetUniformBlockIndex != 0);
    if (!has_transform_feedback_) {
        has_transform_feedback_ = (cb_glBeginTransformFeedback != 0)
                               && (cb_glEndTransformFeedback != 0)
                               && (cb_glTransformFeedbackVaryings != 0);
    }
    if (!has_geometry_shader_)
        has_geometry_shader_ = (SDL_GL_GetProcAddress("glProgramParameteri") != 0);
#else
    // Linux: GL spec guarantees are reliable, but check GL2 extensions too
    if (!has_vao_ && caps_.gl_major == 2) {
        const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (exts && strstr(exts, "GL_ARB_vertex_array_object"))
            has_vao_ = true;
        if (exts && strstr(exts, "GL_ARB_draw_instanced"))
            has_instancing_ = true;
    }
#endif

    // Reserve slot 0 as invalid for UBO/TF handles
    GLBuffer invalid_buf;
    ubos_.push_back(invalid_buf);
    tf_buffers_.push_back(invalid_buf);

    if (!has_vao_) {
        Log::warn("GL3Renderer: VAO not available, falling back to GL2 behavior");
    }
    Log::info("GL3Renderer: vao=%s, instancing=%s, tex_array=%s, mrt=%s, ubo=%s, tf=%s, gs=%s",
            has_vao_ ? "yes" : "no",
            has_instancing_ ? "yes" : "no",
            has_texture_array_ ? "yes" : "no",
            has_mrt_ ? "yes" : "no",
            has_ubo_ ? "yes" : "no",
            has_transform_feedback_ ? "yes" : "no",
            has_geometry_shader_ ? "yes" : "no");
    return true;
}

void* GL3Renderer::queryFeature(int id) {
    if (id == FeatureTag<GL3Features>::id)
        return static_cast<GL3Features*>(this);
    return GL2Renderer::queryFeature(id);
}

void GL3Renderer::shutdown() {
    // Clean up UBOs
    for (size_t i = 1; i < ubos_.size(); i++) {
        if (ubos_[i].valid) {
            glDeleteBuffers(1, &ubos_[i].id);
            ubos_[i].valid = false;
        }
    }
    ubos_.clear();
    free_ubo_slots_.clear();

    // Clean up TF buffers
    for (size_t i = 1; i < tf_buffers_.size(); i++) {
        if (tf_buffers_[i].valid) {
            glDeleteBuffers(1, &tf_buffers_[i].id);
            tf_buffers_[i].valid = false;
        }
    }
    tf_buffers_.clear();
    free_tf_slots_.clear();

    // Clean up VAOs before GL2Renderer::shutdown deletes VBOs/IBOs
    for (size_t i = 1; i < meshes_.size(); i++) {
        if (meshes_[i].valid && meshes_[i].vao) {
            glDeleteVertexArrays(1, &meshes_[i].vao);
            meshes_[i].vao = 0;
        }
    }
    GL2Renderer::shutdown();
}

MeshHandle GL3Renderer::createMesh(const MeshData& data) {
    MeshHandle h = GL2Renderer::createMesh(data);
    if (h == INVALID_MESH || !has_vao_) return h;

    GLMesh& gm = meshes_[h];

    // Create VAO to capture vertex state
    glGenVertexArrays(1, &gm.vao);
    glBindVertexArray(gm.vao);

    glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ibo);

    GLsizei stride = sizeof(Vertex);

    // Use fixed attribute locations matching built-in shaders
    // a_pos = 0, a_normal = 1, a_uv = 2
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return h;
}

void GL3Renderer::destroyMesh(MeshHandle h) {
    if (isValidMesh(h) && meshes_[h].vao) {
        glDeleteVertexArrays(1, &meshes_[h].vao);
        meshes_[h].vao = 0;
    }
    GL2Renderer::destroyMesh(h);
}

TextureHandle GL3Renderer::createTexture(int w, int h, int channels, const unsigned char* pixels) {
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

    GLenum fmt = (channels == 4) ? GL_RGBA : (channels == 3) ? GL_RGB : GL_LUMINANCE;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);

    // Use glGenerateMipmap instead of GL_GENERATE_MIPMAP
    if (caps_.has_generate_mipmap_func) {
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        // Fallback: enable auto mipmap
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        // Re-upload to trigger mipmap generation
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    TextureHandle th;
    if (!free_tex_slots_.empty()) {
        th = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[th] = gt;
    } else {
        th = static_cast<TextureHandle>(textures_.size());
        textures_.push_back(gt);
    }
    return th;
}

void GL3Renderer::drawMesh(MeshHandle h) {
    if (!isValidMesh(h)) return;
    const GLMesh& gm = meshes_[h];

    if (gm.vao) {
        // Fast path: VAO has all state
        glBindVertexArray(gm.vao);
        glDrawElements(GL_TRIANGLES, gm.index_count, gm.index_type, 0);
        glBindVertexArray(0);
    } else {
        // Fallback to GL2 path
        GL2Renderer::drawMesh(h);
    }
}

void GL3Renderer::drawMeshInstanced(MeshHandle h, int instance_count) {
    if (!isValidMesh(h)) return;
    const GLMesh& gm = meshes_[h];

    if (gm.vao && has_instancing_) {
        glBindVertexArray(gm.vao);
        glDrawElementsInstanced(GL_TRIANGLES, gm.index_count, gm.index_type,
                                0, instance_count);
        glBindVertexArray(0);
    } else {
        // Fallback: draw in a loop (no real instancing)
        for (int i = 0; i < instance_count; i++)
            drawMesh(h);
    }
}

void GL3Renderer::unbindState() {
    GL2Renderer::unbindState();
    glBindVertexArray(0);
}

const char* GL3Renderer::getRendererName() const { return "GL3"; }

// --- MRT render targets ---

RenderTargetHandle GL3Renderer::createMRTRenderTarget(int w, int h, int num_attachments) {
    if (!caps_.has_fbo || !has_mrt_) return INVALID_RENDER_TARGET;
    if (num_attachments < 1 || num_attachments > 4) return INVALID_RENDER_TARGET;

    GLFBO rt;
    rt.w = w;
    rt.h = h;

    // Create FBO
    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);

    // Create color textures for each attachment
    // Store first color texture in the GLFBO struct
    // Additional textures stored as raw GL objects (cleaned up via FBO deletion)
    GLenum attachments[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                               GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };

    GLuint color_textures[4] = {0};
    for (int i = 0; i < num_attachments; i++) {
        glGenTextures(1, &color_textures[i]);
        glBindTexture(GL_TEXTURE_2D, color_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[i], GL_TEXTURE_2D, color_textures[i], 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // Set draw buffers
    glDrawBuffers(num_attachments, attachments);

    // Depth renderbuffer
    glGenRenderbuffers(1, &rt.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rt.depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &rt.fbo);
        for (int i = 0; i < num_attachments; i++)
            glDeleteTextures(1, &color_textures[i]);
        glDeleteRenderbuffers(1, &rt.depth_rb);
        Log::err("MRT FBO not complete: 0x%X", status);
        return INVALID_RENDER_TARGET;
    }

    rt.color_tex = color_textures[0]; // Primary color texture
    rt.valid = true;

    RenderTargetHandle handle;
    if (!free_rt_slots_.empty()) {
        handle = free_rt_slots_.back();
        free_rt_slots_.pop_back();
        render_targets_[handle] = rt;
    } else {
        handle = static_cast<RenderTargetHandle>(render_targets_.size());
        render_targets_.push_back(rt);
    }
    return handle;
}

// --- Texture arrays ---

TextureHandle GL3Renderer::createTextureArray(int w, int h, int layers, int channels, const unsigned char* pixels) {
    if (!has_texture_array_) return INVALID_TEXTURE;

    GLTex gt;
    gt.valid = true;
    glGenTextures(1, &gt.id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, gt.id);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLenum fmt = (channels == 4) ? GL_RGBA : (channels == 3) ? GL_RGB : GL_RED;
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, fmt, w, h, layers, 0, fmt, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    TextureHandle th;
    if (!free_tex_slots_.empty()) {
        th = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[th] = gt;
    } else {
        th = static_cast<TextureHandle>(textures_.size());
        textures_.push_back(gt);
    }
    return th;
}

void GL3Renderer::bindTextureArray(TextureHandle h) {
    if (!isValidTexture(h)) return;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textures_[h].id);
}

// --- Geometry shader ---

ShaderHandle GL3Renderer::createShaderVGF(const char* vs_src, const char* gs_src, const char* fs_src) {
    if (!has_geometry_shader_) return INVALID_SHADER;

    GLuint vs = compileShader(GL_VERTEX_SHADER, vs_src);
    if (!vs) return INVALID_SHADER;

    GLuint gs = compileShader(GL_GEOMETRY_SHADER, gs_src);
    if (!gs) { glDeleteShader(vs); return INVALID_SHADER; }

    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    if (!fs) { glDeleteShader(vs); glDeleteShader(gs); return INVALID_SHADER; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, gs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a_pos");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(gs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        Log::err("VGF shader link error: %s", log);
        glDeleteProgram(prog);
        return INVALID_SHADER;
    }

    ShaderHandle h;
    if (!free_custom_slots_.empty()) {
        h = free_custom_slots_.back();
        free_custom_slots_.pop_back();
        custom_shaders_[h] = prog;
    } else {
        h = ShaderHandle(static_cast<unsigned int>(custom_shaders_.size()));
        custom_shaders_.push_back(prog);
    }
    return h;
}

// --- UBO ---

BufferHandle GL3Renderer::createUBO(int size_bytes) {
    if (!has_ubo_) {
        Log::err("createUBO: UBO not supported");
        return INVALID_BUFFER;
    }

    GLBuffer buf;
    glGenBuffers(1, &buf.id);
    glBindBuffer(GL_UNIFORM_BUFFER, buf.id);
    glBufferData(GL_UNIFORM_BUFFER, size_bytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    buf.valid = true;

    BufferHandle h;
    if (!free_ubo_slots_.empty()) {
        h = free_ubo_slots_.back();
        free_ubo_slots_.pop_back();
        ubos_[h] = buf;
    } else {
        h = BufferHandle(static_cast<unsigned int>(ubos_.size()));
        ubos_.push_back(buf);
    }
    return h;
}

void GL3Renderer::updateUBO(BufferHandle h, const void* data, int size) {
    if (h == INVALID_BUFFER || h >= ubos_.size() || !ubos_[h].valid) return;
    glBindBuffer(GL_UNIFORM_BUFFER, ubos_[h].id);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GL3Renderer::bindUBO(BufferHandle h, int binding) {
    if (h == INVALID_BUFFER || h >= ubos_.size() || !ubos_[h].valid) return;
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubos_[h].id);
}

void GL3Renderer::destroyUBO(BufferHandle h) {
    if (h == INVALID_BUFFER || h >= ubos_.size() || !ubos_[h].valid) return;
    glDeleteBuffers(1, &ubos_[h].id);
    ubos_[h].valid = false;
    free_ubo_slots_.push_back(h);
}

// --- Rasterizer discard ---

void GL3Renderer::setRasterizerDiscard(bool enable) {
    if (enable)
        glEnable(GL_RASTERIZER_DISCARD);
    else
        glDisable(GL_RASTERIZER_DISCARD);
}

// --- Transform feedback buffers ---

BufferHandle GL3Renderer::createTransformFeedbackBuffer(int size_bytes) {
    if (!has_transform_feedback_) {
        Log::err("createTransformFeedbackBuffer: transform feedback not supported");
        return INVALID_BUFFER;
    }

    GLBuffer buf;
    glGenBuffers(1, &buf.id);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf.id);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, size_bytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);
    buf.valid = true;

    BufferHandle h;
    if (!free_tf_slots_.empty()) {
        h = free_tf_slots_.back();
        free_tf_slots_.pop_back();
        tf_buffers_[h] = buf;
    } else {
        h = BufferHandle(static_cast<unsigned int>(tf_buffers_.size()));
        tf_buffers_.push_back(buf);
    }
    return h;
}

void GL3Renderer::destroyTransformFeedbackBuffer(BufferHandle h) {
    if (h == INVALID_BUFFER || h >= tf_buffers_.size() || !tf_buffers_[h].valid) return;
    glDeleteBuffers(1, &tf_buffers_[h].id);
    tf_buffers_[h].valid = false;
    free_tf_slots_.push_back(h);
}

ShaderHandle GL3Renderer::createTransformFeedbackShader(const char* vs_src, const char* fs_src,
                                                         const char* const* varyings, int varying_count) {
    if (!has_transform_feedback_) return INVALID_SHADER;

    GLuint vs = compileShader(GL_VERTEX_SHADER, vs_src);
    if (!vs) return INVALID_SHADER;

    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    if (!fs) { glDeleteShader(vs); return INVALID_SHADER; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);

    glBindAttribLocation(prog, 0, "a_pos");
    glBindAttribLocation(prog, 1, "a_normal");
    glBindAttribLocation(prog, 2, "a_uv");

    // Set transform feedback varyings BEFORE linking
    glTransformFeedbackVaryings(prog, varying_count, varyings, GL_INTERLEAVED_ATTRIBS);

    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        Log::err("TF shader link error: %s", log);
        glDeleteProgram(prog);
        return INVALID_SHADER;
    }

    ShaderHandle h;
    if (!free_custom_slots_.empty()) {
        h = free_custom_slots_.back();
        free_custom_slots_.pop_back();
        custom_shaders_[h] = prog;
    } else {
        h = ShaderHandle(static_cast<unsigned int>(custom_shaders_.size()));
        custom_shaders_.push_back(prog);
    }
    return h;
}

void GL3Renderer::beginTransformFeedback(BufferHandle tf_buf) {
    if (!has_transform_feedback_) return;
    if (tf_buf == INVALID_BUFFER || tf_buf >= tf_buffers_.size() || !tf_buffers_[tf_buf].valid) return;

    assert(!tf_active_ && "Nested beginTransformFeedback");
    tf_active_ = true;
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tf_buffers_[tf_buf].id);
    glBeginTransformFeedback(GL_TRIANGLES);
}

void GL3Renderer::endTransformFeedback() {
    if (!has_transform_feedback_) return;
    assert(tf_active_ && "endTransformFeedback without begin");
    tf_active_ = false;
    glEndTransformFeedback();
}

// --- Depth-only render target (shadow maps) ---

RenderTargetHandle GL3Renderer::createDepthRenderTarget(int w, int h) {
    if (!caps_.has_fbo) return INVALID_RENDER_TARGET;

    GLFBO rt;
    rt.w = w;
    rt.h = h;

    // Create depth texture
    GLuint depth_tex;
    glGenTextures(1, &depth_tex);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, w, h, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create FBO with depth texture only (no color attachment)
    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);

    // No color attachment — tell GL
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &rt.fbo);
        glDeleteTextures(1, &depth_tex);
        Log::err("Depth-only FBO not complete: 0x%X", status);
        return INVALID_RENDER_TARGET;
    }

    // Store depth texture as color_tex (repurposed), no renderbuffer
    rt.color_tex = depth_tex;
    rt.depth_rb = 0;
    rt.valid = true;

    RenderTargetHandle handle;
    if (!free_rt_slots_.empty()) {
        handle = free_rt_slots_.back();
        free_rt_slots_.pop_back();
        render_targets_[handle] = rt;
    } else {
        handle = static_cast<RenderTargetHandle>(render_targets_.size());
        render_targets_.push_back(rt);
    }
    return handle;
}

TextureHandle GL3Renderer::getDepthTexture(RenderTargetHandle rt) {
    if (!isValidRenderTarget(rt)) return INVALID_TEXTURE;
    // For depth-only FBOs, we stored the depth texture in color_tex
    GLuint tex_id = render_targets_[rt].color_tex;
    if (!tex_id) return INVALID_TEXTURE;

    // Wrap the raw GL texture in a TextureHandle
    GLTex gt;
    gt.id = tex_id;
    gt.valid = true;

    TextureHandle th;
    if (!free_tex_slots_.empty()) {
        th = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[th] = gt;
    } else {
        th = static_cast<TextureHandle>(textures_.size());
        textures_.push_back(gt);
    }
    return th;
}
