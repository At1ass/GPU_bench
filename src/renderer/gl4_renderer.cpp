#include "renderer/gl4_renderer.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>

GL4Renderer::GL4Renderer() {}
GL4Renderer::~GL4Renderer() { shutdown(); }

bool GL4Renderer::init(int w, int h) {
    if (!GL3Renderer::init(w, h)) return false;

    // Detect GL4-specific capabilities
    caps_.has_compute = false;

    // Compute shaders require GL 4.3+
    if (caps_.gl_major > 4 || (caps_.gl_major == 4 && caps_.gl_minor >= 3)) {
        caps_.has_compute = true;
    }

    // Extension fallback for GL 4.0-4.2
    if (!caps_.has_compute) {
#ifdef CB_NEED_GL_LOAD
        caps_.has_compute = (SDL_GL_GetProcAddress("glDispatchCompute") != 0);
#else
        const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (exts && strstr(exts, "GL_ARB_compute_shader"))
            caps_.has_compute = true;
#endif
    }

    // Load GL4 function pointers
    loadGL4Functions();

    // Reserve slot 0 as invalid for SSBO handles
    GLBuffer invalid_buf;
    ssbos_.push_back(invalid_buf);

    Log::info("GL4Renderer: compute=%s",
            caps_.has_compute ? "yes" : "no");
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
    GL3Renderer::shutdown();
}

const char* GL4Renderer::getRendererName() const { return "GL4"; }

// --- Compute shaders ---

ShaderHandle GL4Renderer::createComputeShader(const char* source) {
    if (!caps_.has_compute) return INVALID_SHADER;

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
