#include "renderer/gl3_renderer.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>
#include <vector>

GL3Renderer::GL3Renderer() {}
GL3Renderer::~GL3Renderer() { shutdown(); }

bool GL3Renderer::init(int w, int h) {
    if (!GL2Renderer::init(w, h)) return false;

    // Re-enable GL3 capabilities that GL2Renderer conservatively disabled.
    // GL3Renderer actually implements VAO and instancing.
#ifdef CB_NEED_GL_LOAD
    caps_.has_vao = (imgl3wProcs.gl.GenVertexArrays != 0)
                 && (imgl3wProcs.gl.BindVertexArray != 0)
                 && (imgl3wProcs.gl.DeleteVertexArrays != 0);
    caps_.has_instancing = (cb_glDrawElementsInstanced != 0)
                        && (cb_glVertexAttribDivisor != 0);
#else
    caps_.has_vao = (caps_.gl_major >= 3);
    caps_.has_instancing = (caps_.gl_major >= 3);
    if (!caps_.has_vao && caps_.gl_major == 2) {
        const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (exts && strstr(exts, "GL_ARB_vertex_array_object"))
            caps_.has_vao = true;
        if (exts && strstr(exts, "GL_ARB_draw_instanced"))
            caps_.has_instancing = true;
    }
#endif

    if (!caps_.has_vao) {
        Log::warn("GL3Renderer: VAO not available, falling back to GL2 behavior");
    }
    Log::info("GL3Renderer: vao=%s, instancing=%s",
            caps_.has_vao ? "yes" : "no",
            caps_.has_instancing ? "yes" : "no");
    return true;
}

void GL3Renderer::shutdown() {
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
    if (h == INVALID_MESH || !caps_.has_vao) return h;

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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
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

    if (gm.vao && caps_.has_instancing) {
        glBindVertexArray(gm.vao);
        glDrawElementsInstanced(GL_TRIANGLES, gm.index_count, gm.index_type,
                                0, instance_count);
        glBindVertexArray(0);
    } else {
        // Fallback to GL2 loop
        GL2Renderer::drawMeshInstanced(h, instance_count);
    }
}

void GL3Renderer::unbindState() {
    GL2Renderer::unbindState();
    glBindVertexArray(0);
}

const char* GL3Renderer::getRendererName() const { return "GL3"; }
