#include "renderer/backend/gl4_renderer.h"
#ifndef CB_GLES_NATIVE
#include "renderer/backend/gl/gl_profile.h"
#include "renderer/backend/gl/gl_extensions.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

// Validate that SSBOUsage enum values match GL constants
static_assert(static_cast<unsigned>(ComputeFeatures::SSBOUsage::GpuReadWrite) == GL_DYNAMIC_DRAW,
              "SSBOUsage::GpuReadWrite must match GL_DYNAMIC_DRAW");
static_assert(static_cast<unsigned>(ComputeFeatures::SSBOUsage::CpuReadBack) == GL_DYNAMIC_READ,
              "SSBOUsage::CpuReadBack must match GL_DYNAMIC_READ");

GL4Renderer::GL4Renderer() {}
// ~GL4Renderer: default. shutdown() must be called explicitly before destruction.

bool GL4Renderer::init(int w, int h) {
    if (!GL3Renderer::init(w, h)) return false;
    LOG_DBG("GL4Renderer::init: GL3 base init done, detecting GL4 features");

    // Baseline from GL spec
    GLProfile baseline = GLProfile::coreProfile(caps_.gl_major, caps_.gl_minor);
    has_compute_ = baseline.compute;
    has_indirect_draw_ = baseline.indirect_draw;
    has_tessellation_ = baseline.tessellation;
    has_texture_gather_ = baseline.texture_gather;
    has_image_load_store_ = baseline.image_load_store;
    has_buffer_storage_ = baseline.buffer_storage;
    has_bindless_texture_ = false;  // Never from baseline

#ifdef CB_NEED_GL_LOAD
    // Verify function pointers
    if (has_compute_ && !SDL_GL_GetProcAddress("glDispatchCompute")) {
        LOG_WRN("GL%d.%d claims compute but glDispatchCompute missing", caps_.gl_major, caps_.gl_minor);
        has_compute_ = false;
    }
    if (has_indirect_draw_ && !cb_glMultiDrawElementsIndirect) {
        has_indirect_draw_ = false;
    }
    if (has_tessellation_ && !cb_glPatchParameteri) {
        has_tessellation_ = false;
    }
    if (has_image_load_store_ && !cb_glBindImageTexture) {
        has_image_load_store_ = false;
    }
    if (has_buffer_storage_ && (!cb_glBufferStorage || !cb_glMapBufferRange)) {
        has_buffer_storage_ = false;
    }

    // Extensions below core version
    if (!has_compute_)
        has_compute_ = (SDL_GL_GetProcAddress("glDispatchCompute") != 0);
    if (!has_indirect_draw_)
        has_indirect_draw_ = (cb_glMultiDrawElementsIndirect != 0);
    if (!has_tessellation_)
        has_tessellation_ = (cb_glPatchParameteri != 0);
    if (!has_image_load_store_)
        has_image_load_store_ = (cb_glBindImageTexture != 0);
    if (!has_buffer_storage_)
        has_buffer_storage_ = (cb_glBufferStorage != 0) && (cb_glMapBufferRange != 0);

    // Bindless texture (pure extension)
    has_bindless_texture_ = (cb_glGetTextureHandleARB != 0)
                         && (cb_glMakeTextureHandleResidentARB != 0);
#else
    // Extension fallback
    if (!has_compute_ && GLExtensions::has("GL_ARB_compute_shader"))
        has_compute_ = true;
    if (!has_indirect_draw_ && GLExtensions::has("GL_ARB_multi_draw_indirect"))
        has_indirect_draw_ = true;
    if (!has_tessellation_ && GLExtensions::has("GL_ARB_tessellation_shader"))
        has_tessellation_ = true;
    if (!has_texture_gather_ && GLExtensions::has("GL_ARB_texture_gather"))
        has_texture_gather_ = true;
    if (!has_image_load_store_ && GLExtensions::has("GL_ARB_shader_image_load_store"))
        has_image_load_store_ = true;
    if (!has_buffer_storage_ && GLExtensions::has("GL_ARB_buffer_storage"))
        has_buffer_storage_ = true;
    if (GLExtensions::has("GL_ARB_bindless_texture"))
        has_bindless_texture_ = (cb_glGetTextureHandleARB != nullptr)
                             && (cb_glMakeTextureHandleResidentARB != nullptr);
#endif

    // Load GL4 function pointers
    LOG_DBG("GL4Renderer::init: loading GL4 function pointers");
    loadGL4Functions();

    // Reserve slot 0 as invalid for SSBO handles
    GLBuffer invalid_buf;
    ssbos_.push_back(invalid_buf);

    // Reserve slot 0 for indirect buffers
    GLBuffer invalid_ind;
    indirect_buffers_.push_back(invalid_ind);

    // Reserve slot 0 for persistent buffers
    PersistentBuffer invalid_pb;
    persistent_buffers_.push_back(invalid_pb);

    LOG_DBG("GL4Renderer: compute=%s, indirect_draw=%s, tessellation=%s, "
             "texture_gather=%s, image_load_store=%s, buffer_storage=%s, bindless_texture=%s",
            has_compute_ ? "yes" : "no",
            has_indirect_draw_ ? "yes" : "no",
            has_tessellation_ ? "yes" : "no",
            has_texture_gather_ ? "yes" : "no",
            has_image_load_store_ ? "yes" : "no",
            has_buffer_storage_ ? "yes" : "no",
            has_bindless_texture_ ? "yes" : "no");
    return true;
}

// See GL3Renderer::queryFeature comment — cast to interface type, not renderer type.
void* GL4Renderer::queryFeature(int id) {
    if (id == FeatureTag<GL4Features>::id)
        return static_cast<GL4Features*>(this);
    if (id == FeatureTag<ComputeFeatures>::id)
        return static_cast<ComputeFeatures*>(this);
    return GL3Renderer::queryFeature(id);
}

void GL4Renderer::shutdown() {
    for (size_t i = 1; i < ssbos_.size(); i++) {
        if (ssbos_[i].valid) {
            glDeleteBuffers(1, &ssbos_[i].id);
            ssbos_[i].valid = false;
        }
    }
    ssbos_.clear();
    free_ssbo_slots_.clear();

    for (size_t i = 1; i < indirect_buffers_.size(); i++) {
        if (indirect_buffers_[i].valid) {
            glDeleteBuffers(1, &indirect_buffers_[i].id);
            indirect_buffers_[i].valid = false;
        }
    }
    indirect_buffers_.clear();
    free_indirect_slots_.clear();

    for (size_t i = 1; i < persistent_buffers_.size(); i++) {
        if (persistent_buffers_[i].valid) {
            if (persistent_buffers_[i].fence) {
                glDeleteSync(persistent_buffers_[i].fence);
            }
            glDeleteBuffers(1, &persistent_buffers_[i].id);
            persistent_buffers_[i].valid = false;
        }
    }
    persistent_buffers_.clear();
    free_persistent_slots_.clear();

    GL3Renderer::shutdown();
}

const char* GL4Renderer::getRendererName() const { return "GL4"; }

// --- Compute shaders ---

ShaderHandle GL4Renderer::createComputeShader(const char* source) {
    if (!has_compute_) return INVALID_SHADER;

    GLuint cs = compileShader(GL_COMPUTE_SHADER, source);
    if (!cs) return INVALID_SHADER;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    glDeleteShader(cs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        LOG_ERR("Compute shader link error: %s", log);
        glDeleteProgram(prog);
        return INVALID_SHADER;
    }

    // Store in custom_shaders_ (shared with VS+FS programs)
    ShaderHandle h;
    if (!free_custom_slots_.empty()) {
        h = free_custom_slots_.back();
        free_custom_slots_.pop_back();
        custom_shaders_[h.id] = prog;
    } else {
        h = ShaderHandle(static_cast<unsigned int>(custom_shaders_.size()));
        custom_shaders_.push_back(prog);
    }
    LOG_DBG("GL4: createComputeShader -> handle %u", (unsigned)h);
    return h;
}

void GL4Renderer::dispatchCompute(int groups_x, int groups_y, int groups_z) {
#ifdef CB_NEED_GL_LOAD
    if (cb_glDispatchCompute)
        cb_glDispatchCompute(static_cast<GLuint>(groups_x), static_cast<GLuint>(groups_y), static_cast<GLuint>(groups_z));
#else
    glDispatchCompute(static_cast<GLuint>(groups_x), static_cast<GLuint>(groups_y), static_cast<GLuint>(groups_z));
#endif
    renderer_stats_.compute_dispatches++;
}

void GL4Renderer::computeMemoryBarrier() {
    // GL4.3: ensure SSBO writes AND image writes are visible to subsequent operations.
    // SHADER_STORAGE for SSBO, TEXTURE_FETCH for compute→fragment texture() reads.
#ifdef CB_NEED_GL_LOAD
    if (cb_glMemoryBarrier)
        cb_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
#else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
#endif
    renderer_stats_.barriers_issued++;
}

// --- SSBO ---

BufferHandle GL4Renderer::createSSBO(int size_bytes, SSBOUsage usage) {
    GLBuffer buf;
    glGenBuffers(1, &buf.id);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf.id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size_bytes, nullptr,
                 static_cast<GLenum>(usage));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    buf.valid = true;

    BufferHandle h;
    if (!free_ssbo_slots_.empty()) {
        h = free_ssbo_slots_.back();
        free_ssbo_slots_.pop_back();
        ssbos_[h.id] = buf;
    } else {
        h = BufferHandle(static_cast<unsigned int>(ssbos_.size()));
        ssbos_.push_back(buf);
    }
    LOG_DBG("GL4: createSSBO %d bytes -> handle %u", size_bytes, (unsigned)h);
    return h;
}

void GL4Renderer::destroySSBO(BufferHandle h) {
    if (h == INVALID_BUFFER || h.id >= ssbos_.size() || !ssbos_[h.id].valid) return;
    glDeleteBuffers(1, &ssbos_[h.id].id);
    ssbos_[h.id].valid = false;
    free_ssbo_slots_.push_back(h);
}

void GL4Renderer::destroyBuffer(BufferHandle h) {
    destroySSBO(h);
}

void GL4Renderer::bindSSBO(BufferHandle h, int binding) {
    if (h == INVALID_BUFFER || h.id >= ssbos_.size() || !ssbos_[h.id].valid) return;
#ifdef CB_NEED_GL_LOAD
    if (cb_glBindBufferBase)
        cb_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(binding), ssbos_[h.id].id);
#else
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(binding), ssbos_[h.id].id);
#endif
}

void GL4Renderer::updateSSBO(BufferHandle h, const void* data, int size_bytes) {
    if (h == INVALID_BUFFER || h.id >= ssbos_.size() || !ssbos_[h.id].valid) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos_[h.id].id);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size_bytes, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GL4Renderer::readSSBO(BufferHandle h, void* data, int offset, int size_bytes) {
    if (h == INVALID_BUFFER || h.id >= ssbos_.size() || !ssbos_[h.id].valid || !data) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos_[h.id].id);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size_bytes, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// --- Float render targets (HDR) ---

RenderTargetHandle GL4Renderer::createFloatRenderTarget(int w, int h) {
    if (!caps_.has_fbo) return INVALID_RENDER_TARGET;

    GLFBO rt;
    rt.w = w;
    rt.h = h;

    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);

    // RGBA16F color texture
    glGenTextures(1, &rt.color_tex);
    glBindTexture(GL_TEXTURE_2D, rt.color_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA16F), w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.color_tex, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &rt.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rt.depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &rt.fbo);
        glDeleteTextures(1, &rt.color_tex);
        glDeleteRenderbuffers(1, &rt.depth_rb);
        LOG_ERR("Float FBO not complete: 0x%X", status);
        return INVALID_RENDER_TARGET;
    }

    rt.valid = true;

    RenderTargetHandle handle;
    if (!free_rt_slots_.empty()) {
        handle = free_rt_slots_.back();
        free_rt_slots_.pop_back();
        render_targets_[handle.id] = std::move(rt);
    } else {
        handle = RenderTargetHandle(static_cast<unsigned int>(render_targets_.size()));
        render_targets_.push_back(std::move(rt));
    }
    LOG_DBG("GL4: createFloatRenderTarget %dx%d -> handle %u", w, h, (unsigned)handle);
    return handle;
}

RenderTargetHandle GL4Renderer::createFloatRenderTargetWithDepth(int w, int h) {
    if (!caps_.has_fbo) return INVALID_RENDER_TARGET;

    GLFBO rt;
    rt.w = w;
    rt.h = h;

    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);

    // RGBA16F color texture
    glGenTextures(1, &rt.color_tex);
    glBindTexture(GL_TEXTURE_2D, rt.color_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA16F), w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.color_tex, 0);

    // Sampleable depth texture
    glGenTextures(1, &rt.depth_tex);
    glBindTexture(GL_TEXTURE_2D, rt.depth_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_DEPTH_COMPONENT24), w, h, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt.depth_tex, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    rt.depth_rb = 0;

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &rt.fbo);
        glDeleteTextures(1, &rt.color_tex);
        glDeleteTextures(1, &rt.depth_tex);
        LOG_ERR("Float FBO with depth not complete: 0x%X", status);
        return INVALID_RENDER_TARGET;
    }

    rt.valid = true;

    RenderTargetHandle handle;
    if (!free_rt_slots_.empty()) {
        handle = free_rt_slots_.back();
        free_rt_slots_.pop_back();
        render_targets_[handle.id] = std::move(rt);
    } else {
        handle = RenderTargetHandle(static_cast<unsigned int>(render_targets_.size()));
        render_targets_.push_back(std::move(rt));
    }
    return handle;
}

// --- Indirect draw ---

BufferHandle GL4Renderer::createIndirectBuffer(int size_bytes, const void* data) {
    if (!has_indirect_draw_) return INVALID_BUFFER;

    GLBuffer buf;
    glGenBuffers(1, &buf.id);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buf.id);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, size_bytes, data, GL_STATIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    buf.valid = true;

    BufferHandle h;
    if (!free_indirect_slots_.empty()) {
        h = free_indirect_slots_.back();
        free_indirect_slots_.pop_back();
        indirect_buffers_[h.id] = buf;
    } else {
        h = BufferHandle(static_cast<unsigned int>(indirect_buffers_.size()));
        indirect_buffers_.push_back(buf);
    }
    LOG_DBG("GL4: createIndirectBuffer %d bytes -> handle %u", size_bytes, (unsigned)h);
    return h;
}

void GL4Renderer::destroyIndirectBuffer(BufferHandle h) {
    if (h == INVALID_BUFFER || h.id >= indirect_buffers_.size() || !indirect_buffers_[h.id].valid) return;
    glDeleteBuffers(1, &indirect_buffers_[h.id].id);
    indirect_buffers_[h.id].valid = false;
    free_indirect_slots_.push_back(h);
}

void GL4Renderer::multiDrawMeshIndirect(MeshHandle mesh, BufferHandle indirect,
                                         int draw_count, int stride) {
    if (!isValidMesh(mesh)) return;
    if (indirect == INVALID_BUFFER || indirect.id >= indirect_buffers_.size()
        || !indirect_buffers_[indirect.id].valid) return;

    const GLMesh& gm = meshes_[mesh.id];
    if (gm.vao) {
        glBindVertexArray(gm.vao);
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffers_[indirect.id].id);

#ifdef CB_NEED_GL_LOAD
    if (cb_glMultiDrawElementsIndirect)
        cb_glMultiDrawElementsIndirect(GL_TRIANGLES, gm.index_type, nullptr, draw_count, stride);
#else
    glMultiDrawElementsIndirect(GL_TRIANGLES, gm.index_type, nullptr, draw_count, stride);
#endif

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    if (gm.vao) {
        glBindVertexArray(0);
    }
}

// --- Tessellation ---

void GL4Renderer::setPatchVertices(int count) {
#ifdef CB_NEED_GL_LOAD
    if (cb_glPatchParameteri)
        cb_glPatchParameteri(GL_PATCH_VERTICES, count);
#else
    glPatchParameteri(GL_PATCH_VERTICES, count);
#endif
}

ShaderHandle GL4Renderer::createTessShader(const char* vs, const char* tcs,
                                            const char* tes, const char* fs) {
    if (!has_tessellation_) return INVALID_SHADER;

    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    if (!v) return INVALID_SHADER;

    GLuint tc = compileShader(GL_TESS_CONTROL_SHADER, tcs);
    if (!tc) { glDeleteShader(v); return INVALID_SHADER; }

    GLuint te = compileShader(GL_TESS_EVALUATION_SHADER, tes);
    if (!te) { glDeleteShader(v); glDeleteShader(tc); return INVALID_SHADER; }

    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!f) { glDeleteShader(v); glDeleteShader(tc); glDeleteShader(te); return INVALID_SHADER; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, tc);
    glAttachShader(prog, te);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(tc);
    glDeleteShader(te);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        LOG_ERR("Tessellation shader link error: %s", log);
        glDeleteProgram(prog);
        return INVALID_SHADER;
    }

    ShaderHandle h;
    if (!free_custom_slots_.empty()) {
        h = free_custom_slots_.back();
        free_custom_slots_.pop_back();
        custom_shaders_[h.id] = prog;
    } else {
        h = ShaderHandle(static_cast<unsigned int>(custom_shaders_.size()));
        custom_shaders_.push_back(prog);
    }
    LOG_DBG("GL4: createTessShader -> handle %u", (unsigned)h);
    return h;
}

void GL4Renderer::drawMeshAsPatches(MeshHandle h) {
    if (!isValidMesh(h)) return;
    const GLMesh& gm = meshes_[h.id];
    if (gm.vao) {
        glBindVertexArray(gm.vao);
        glDrawElements(GL_PATCHES, gm.index_count, gm.index_type, nullptr);
        glBindVertexArray(0);
        renderer_stats_.draw_calls++;
    }
}

TextureHandle GL4Renderer::createFloatTexture(int w, int h) {
    GLTex tex;
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    // Zero-initialize to prevent flicker from undefined texture data on first frame
    std::vector<float> zeros(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA32F), w, h, 0, GL_RGBA, GL_FLOAT, zeros.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    tex.valid = true;

    TextureHandle handle;
    if (!free_tex_slots_.empty()) {
        handle = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[handle.id] = std::move(tex);
    } else {
        handle = TextureHandle(static_cast<unsigned int>(textures_.size()));
        textures_.push_back(std::move(tex));
    }
    LOG_DBG("GL4: createFloatTexture %dx%d -> handle %u", w, h, (unsigned)handle);
    return handle;
}

// --- Image load/store ---

void GL4Renderer::bindImageTexture(TextureHandle h, int unit,
                                    bool read, bool write) {
    if (h == INVALID_TEXTURE || h.id >= textures_.size() || !textures_[h.id].valid) {
        // Unbind: bind texture 0 to release the image unit
#ifdef CB_NEED_GL_LOAD
        if (cb_glBindImageTexture)
            cb_glBindImageTexture(static_cast<GLuint>(unit), 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
#else
        glBindImageTexture(static_cast<GLuint>(unit), 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
#endif
        return;
    }
    GLenum access = GL_READ_WRITE;
    if (read && !write) access = GL_READ_ONLY;
    else if (!read && write) access = GL_WRITE_ONLY;

#ifdef CB_NEED_GL_LOAD
    if (cb_glBindImageTexture)
        cb_glBindImageTexture(static_cast<GLuint>(unit), textures_[h.id].id, 0, GL_FALSE, 0, access, GL_RGBA32F);
#else
    glBindImageTexture(static_cast<GLuint>(unit), textures_[h.id].id, 0, GL_FALSE, 0, access, GL_RGBA32F);
#endif
}

void GL4Renderer::imageMemoryBarrier() {
    // GL4.3 spec: imageStore writes need different barriers depending on consumer:
    //   GL_SHADER_IMAGE_ACCESS_BARRIER_BIT - for subsequent imageLoad()
    //   GL_TEXTURE_FETCH_BARRIER_BIT       - for subsequent texture() sampler reads
    // Use both since compute outputs are consumed by both compute (imageLoad) and
    // fragment shaders (texture sampling in tone_map composite).
#ifdef CB_NEED_GL_LOAD
    if (cb_glMemoryBarrier)
        cb_glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
#else
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
#endif
    renderer_stats_.barriers_issued++;
}

// --- Texture copy (glCopyImageSubData) ---

void GL4Renderer::copyImageSubData(TextureHandle src, TextureHandle dst, int w, int h) {
    if (!isValidTexture(src) || !isValidTexture(dst)) return;
    GLuint src_id = textures_[src.id].id;
    GLuint dst_id = textures_[dst.id].id;
#ifdef CB_NEED_GL_LOAD
    if (cb_glCopyImageSubData)
        cb_glCopyImageSubData(src_id, GL_TEXTURE_2D, 0, 0, 0, 0,
                              dst_id, GL_TEXTURE_2D, 0, 0, 0, 0,
                              w, h, 1);
#else
    glCopyImageSubData(src_id, GL_TEXTURE_2D, 0, 0, 0, 0,
                       dst_id, GL_TEXTURE_2D, 0, 0, 0, 0,
                       w, h, 1);
#endif
}

// --- RGBA16F texture (matching HDR render target color format) ---

TextureHandle GL4Renderer::createFloat16Texture(int w, int h) {
    GLTex tex;
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA16F), w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    tex.valid = true;

    TextureHandle handle;
    if (!free_tex_slots_.empty()) {
        handle = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[handle.id] = std::move(tex);
    } else {
        handle = TextureHandle(static_cast<unsigned int>(textures_.size()));
        textures_.push_back(std::move(tex));
    }
    LOG_DBG("GL4: createFloat16Texture %dx%d -> handle %u", w, h, (unsigned)handle);
    return handle;
}

// --- Depth texture (GL_DEPTH_COMPONENT24, matching HDR RT depth format) ---

TextureHandle GL4Renderer::createDepthTexture(int w, int h) {
    GLTex tex;
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_DEPTH_COMPONENT24), w, h, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    tex.valid = true;

    TextureHandle handle;
    if (!free_tex_slots_.empty()) {
        handle = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[handle.id] = std::move(tex);
    } else {
        handle = TextureHandle(static_cast<unsigned int>(textures_.size()));
        textures_.push_back(std::move(tex));
    }
    LOG_DBG("GL4: createDepthTexture %dx%d -> handle %u", w, h, (unsigned)handle);
    return handle;
}

// --- Persistent mapping ---

BufferHandle GL4Renderer::createPersistentBuffer(int size_bytes) {
    if (!has_buffer_storage_) return INVALID_BUFFER;

    PersistentBuffer pb;
    glGenBuffers(1, &pb.id);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pb.id);

    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
#ifdef CB_NEED_GL_LOAD
    if (cb_glBufferStorage)
        cb_glBufferStorage(GL_SHADER_STORAGE_BUFFER, size_bytes, nullptr, flags);
#else
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, size_bytes, nullptr, flags);
#endif

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    pb.valid = true;
    pb.size = size_bytes;
    pb.fence = nullptr;
    pb.mapped = nullptr;

    BufferHandle h;
    if (!free_persistent_slots_.empty()) {
        h = free_persistent_slots_.back();
        free_persistent_slots_.pop_back();
        persistent_buffers_[h.id] = pb;
    } else {
        h = BufferHandle(static_cast<unsigned int>(persistent_buffers_.size()));
        persistent_buffers_.push_back(pb);
    }
    LOG_DBG("GL4: createPersistentBuffer %d bytes -> handle %u", size_bytes, (unsigned)h);
    return h;
}

void* GL4Renderer::mapPersistentBuffer(BufferHandle h) {
    if (h == INVALID_BUFFER || h.id >= persistent_buffers_.size()
        || !persistent_buffers_[h.id].valid) return nullptr;

    PersistentBuffer& pb = persistent_buffers_[h.id];
    if (pb.mapped) return pb.mapped;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pb.id);
    GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
#ifdef CB_NEED_GL_LOAD
    if (cb_glMapBufferRange)
        pb.mapped = cb_glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, pb.size, access);
#else
    pb.mapped = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, pb.size, access);
#endif
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return pb.mapped;
}

void GL4Renderer::persistentBufferFence(BufferHandle h) {
    if (h == INVALID_BUFFER || h.id >= persistent_buffers_.size()
        || !persistent_buffers_[h.id].valid) return;

    PersistentBuffer& pb = persistent_buffers_[h.id];

    // Wait for previous fence if any
    if (pb.fence) {
#ifdef CB_NEED_GL_LOAD
        if (cb_glClientWaitSync)
            cb_glClientWaitSync(pb.fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
        if (cb_glDeleteSync)
            cb_glDeleteSync(pb.fence);
#else
        glClientWaitSync(pb.fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
        glDeleteSync(pb.fence);
#endif
    }

    // Insert new fence
#ifdef CB_NEED_GL_LOAD
    if (cb_glFenceSync)
        pb.fence = cb_glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#else
    pb.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
}

void GL4Renderer::destroyPersistentBuffer(BufferHandle h) {
    if (h == INVALID_BUFFER || h.id >= persistent_buffers_.size()
        || !persistent_buffers_[h.id].valid) return;

    PersistentBuffer& pb = persistent_buffers_[h.id];
    if (pb.fence) {
#ifdef CB_NEED_GL_LOAD
        if (cb_glDeleteSync) cb_glDeleteSync(pb.fence);
#else
        glDeleteSync(pb.fence);
#endif
    }
    glDeleteBuffers(1, &pb.id);
    pb.valid = false;
    pb.mapped = nullptr;
    pb.fence = nullptr;
    free_persistent_slots_.push_back(h);
}

// --- Bindless texture ---

uint64_t GL4Renderer::getBindlessHandle(TextureHandle h) {
    if (!has_bindless_texture_) return 0;
    if (h == INVALID_TEXTURE || h.id >= textures_.size() || !textures_[h.id].valid) return 0;
    if (cb_glGetTextureHandleARB)
        return cb_glGetTextureHandleARB(textures_[h.id].id);
    return 0;
}

void GL4Renderer::makeTextureResident(uint64_t handle) {
    if (!handle) return;
    if (cb_glMakeTextureHandleResidentARB)
        cb_glMakeTextureHandleResidentARB(handle);
}

void GL4Renderer::makeTextureNonResident(uint64_t handle) {
    if (!handle) return;
    if (cb_glMakeTextureHandleNonResidentARB)
        cb_glMakeTextureHandleNonResidentARB(handle);
}

void GL4Renderer::setUniformHandle(int loc, uint64_t handle) {
    if (cb_glUniformHandleui64ARB)
        cb_glUniformHandleui64ARB(loc, handle);
}
#endif // CB_GLES_NATIVE
