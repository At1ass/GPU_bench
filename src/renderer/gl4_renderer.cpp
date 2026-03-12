#include "renderer/gl4_renderer.h"
#include "renderer/gl_profile.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>

GL4Renderer::GL4Renderer() {}
GL4Renderer::~GL4Renderer() { shutdown(); }

bool GL4Renderer::init(int w, int h) {
    if (!GL3Renderer::init(w, h)) return false;

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
        Log::warn("GL%d.%d claims compute but glDispatchCompute missing", caps_.gl_major, caps_.gl_minor);
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
    const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (exts) {
        if (!has_compute_ && strstr(exts, "GL_ARB_compute_shader"))
            has_compute_ = true;
        if (!has_indirect_draw_ && strstr(exts, "GL_ARB_multi_draw_indirect"))
            has_indirect_draw_ = true;
        if (!has_tessellation_ && strstr(exts, "GL_ARB_tessellation_shader"))
            has_tessellation_ = true;
        if (!has_texture_gather_ && strstr(exts, "GL_ARB_texture_gather"))
            has_texture_gather_ = true;
        if (!has_image_load_store_ && strstr(exts, "GL_ARB_shader_image_load_store"))
            has_image_load_store_ = true;
        if (!has_buffer_storage_ && strstr(exts, "GL_ARB_buffer_storage"))
            has_buffer_storage_ = true;
        if (strstr(exts, "GL_ARB_bindless_texture"))
            has_bindless_texture_ = (cb_glGetTextureHandleARB != 0)
                                 && (cb_glMakeTextureHandleResidentARB != 0);
    }
#endif

    // Load GL4 function pointers
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

    Log::info("GL4Renderer: compute=%s, indirect_draw=%s, tessellation=%s, "
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
        Log::err("Compute shader link error: %s", log);
        glDeleteProgram(prog);
        return INVALID_SHADER;
    }

    // Store in custom_shaders_ (shared with VS+FS programs)
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

void GL4Renderer::dispatchCompute(int groups_x, int groups_y, int groups_z) {
#ifdef CB_NEED_GL_LOAD
    if (cb_glDispatchCompute)
        cb_glDispatchCompute(groups_x, groups_y, groups_z);
#else
    glDispatchCompute(groups_x, groups_y, groups_z);
#endif
}

void GL4Renderer::computeMemoryBarrier() {
#ifdef CB_NEED_GL_LOAD
    if (cb_glMemoryBarrier)
        cb_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
#else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
#endif
}

// --- SSBO ---

BufferHandle GL4Renderer::createSSBO(int size_bytes) {
    GLBuffer buf;
    glGenBuffers(1, &buf.id);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf.id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size_bytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    buf.valid = true;

    BufferHandle h;
    if (!free_ssbo_slots_.empty()) {
        h = free_ssbo_slots_.back();
        free_ssbo_slots_.pop_back();
        ssbos_[h] = buf;
    } else {
        h = BufferHandle(static_cast<unsigned int>(ssbos_.size()));
        ssbos_.push_back(buf);
    }
    return h;
}

void GL4Renderer::destroySSBO(BufferHandle h) {
    if (h == INVALID_BUFFER || h >= ssbos_.size() || !ssbos_[h].valid) return;
    glDeleteBuffers(1, &ssbos_[h].id);
    ssbos_[h].valid = false;
    free_ssbo_slots_.push_back(h);
}

void GL4Renderer::bindSSBO(BufferHandle h, int binding) {
    if (h == INVALID_BUFFER || h >= ssbos_.size() || !ssbos_[h].valid) return;
#ifdef CB_NEED_GL_LOAD
    if (cb_glBindBufferBase)
        cb_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ssbos_[h].id);
#else
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ssbos_[h].id);
#endif
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
        indirect_buffers_[h] = buf;
    } else {
        h = BufferHandle(static_cast<unsigned int>(indirect_buffers_.size()));
        indirect_buffers_.push_back(buf);
    }
    return h;
}

void GL4Renderer::destroyIndirectBuffer(BufferHandle h) {
    if (h == INVALID_BUFFER || h >= indirect_buffers_.size() || !indirect_buffers_[h].valid) return;
    glDeleteBuffers(1, &indirect_buffers_[h].id);
    indirect_buffers_[h].valid = false;
    free_indirect_slots_.push_back(h);
}

void GL4Renderer::multiDrawMeshIndirect(MeshHandle mesh, BufferHandle indirect,
                                         int draw_count, int stride) {
    if (!isValidMesh(mesh)) return;
    if (indirect == INVALID_BUFFER || indirect >= indirect_buffers_.size()
        || !indirect_buffers_[indirect].valid) return;

    const GLMesh& gm = meshes_[mesh];
    if (gm.vao) {
        glBindVertexArray(gm.vao);
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffers_[indirect].id);

#ifdef CB_NEED_GL_LOAD
    if (cb_glMultiDrawElementsIndirect)
        cb_glMultiDrawElementsIndirect(GL_TRIANGLES, gm.index_type, 0, draw_count, stride);
#else
    glMultiDrawElementsIndirect(GL_TRIANGLES, gm.index_type, 0, draw_count, stride);
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
        Log::err("Tessellation shader link error: %s", log);
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

// --- Image load/store ---

void GL4Renderer::bindImageTexture(TextureHandle h, int unit,
                                    bool read, bool write) {
    if (h == INVALID_TEXTURE || h >= textures_.size() || !textures_[h].valid) return;
    GLenum access = GL_READ_WRITE;
    if (read && !write) access = GL_READ_ONLY;
    else if (!read && write) access = GL_WRITE_ONLY;

#ifdef CB_NEED_GL_LOAD
    if (cb_glBindImageTexture)
        cb_glBindImageTexture(unit, textures_[h].id, 0, GL_FALSE, 0, access, GL_RGBA32F);
#else
    glBindImageTexture(unit, textures_[h].id, 0, GL_FALSE, 0, access, GL_RGBA32F);
#endif
}

void GL4Renderer::imageMemoryBarrier() {
#ifdef CB_NEED_GL_LOAD
    if (cb_glMemoryBarrier)
        cb_glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
#else
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
#endif
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
    pb.fence = nullptr;
    pb.mapped = nullptr;

    BufferHandle h;
    if (!free_persistent_slots_.empty()) {
        h = free_persistent_slots_.back();
        free_persistent_slots_.pop_back();
        persistent_buffers_[h] = pb;
    } else {
        h = BufferHandle(static_cast<unsigned int>(persistent_buffers_.size()));
        persistent_buffers_.push_back(pb);
    }
    return h;
}

void* GL4Renderer::mapPersistentBuffer(BufferHandle h) {
    if (h == INVALID_BUFFER || h >= persistent_buffers_.size()
        || !persistent_buffers_[h].valid) return nullptr;

    PersistentBuffer& pb = persistent_buffers_[h];
    if (pb.mapped) return pb.mapped;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pb.id);
    GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
#ifdef CB_NEED_GL_LOAD
    if (cb_glMapBufferRange)
        pb.mapped = cb_glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 0, access);
#else
    pb.mapped = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 0, access);
#endif
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return pb.mapped;
}

void GL4Renderer::persistentBufferFence(BufferHandle h) {
    if (h == INVALID_BUFFER || h >= persistent_buffers_.size()
        || !persistent_buffers_[h].valid) return;

    PersistentBuffer& pb = persistent_buffers_[h];

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
    if (h == INVALID_BUFFER || h >= persistent_buffers_.size()
        || !persistent_buffers_[h].valid) return;

    PersistentBuffer& pb = persistent_buffers_[h];
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
    if (h == INVALID_TEXTURE || h >= textures_.size() || !textures_[h].valid) return 0;
    if (cb_glGetTextureHandleARB)
        return cb_glGetTextureHandleARB(textures_[h].id);
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
