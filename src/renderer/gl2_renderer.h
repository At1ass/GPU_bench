#pragma once
#include "renderer/renderer.h"
#include "renderer/gl_funcs.h"
#include "renderer/gpu_timer.h"
#include "engine/state_cache.h"
#include <vector>
#include <string>
#include "platform/logger.h"

class GL2Renderer : public Renderer {
public:
    GL2Renderer();

    bool init(int w, int h) override;
    void shutdown() override;

    void setViewport(int x, int y, int w, int h) override;
    void clear(float r, float g, float b, float a) override;

    MeshHandle    createMesh(const MeshData& data) override;
    void          destroyMesh(MeshHandle h) override;
    TextureHandle createTexture(int w, int h, int channels, const unsigned char* pixels) override;
    void          destroyTexture(TextureHandle h) override;

    void useShader(ShaderType type) override;

    ShaderHandle createCustomShader(const char* vs, const char* fs) override;
    void         useCustomShader(ShaderHandle h) override;
    void         destroyCustomShader(ShaderHandle h) override;
    int          getCustomUniformLoc(ShaderHandle h, const char* name) override;
    void         setUniform1i(int loc, int v) override;
    void         setUniform1f(int loc, float v) override;
    void         setUniform2f(int loc, float x, float y) override;
    void         setUniform3f(int loc, float x, float y, float z) override;
    void         setUniform4f(int loc, float r, float g, float b, float a) override;
    void         setUniformMat4(int loc, const Mat4& m) override;

    void setProjection(const Mat4& m) override;
    void setView(const Mat4& m) override;
    void setModel(const Mat4& m) override;

    void setColor(float r, float g, float b, float a) override;
    void setLightDir(float x, float y, float z) override;
    void bindTexture(TextureHandle tex) override;
    void bindTextureUnit(int unit, TextureHandle tex) override;
    void setUseTexture(bool use) override;

    void drawMesh(MeshHandle h) override;

    void uploadTextureData(TextureHandle h, int width, int height,
                           int channels, const unsigned char* pixels) override;
    void setColorMask(bool r, bool g, bool b, bool a) override;

    void setBlending(bool enable) override;
    void setBlendingAdditive(bool enable) override;
    void setDepthTest(bool enable) override;
    void setCullFace(bool enable) override;
    void setDepthMask(bool write) override;
    void setPolygonOffset(bool enable, float factor, float units) override;

    void resetState() override;
    void unbindState() override;
    void setScissor(bool enable, int x = 0, int y = 0, int w = 0, int h = 0) override;
    void finish() override;
    void readPixels(int x, int y, int w, int h, unsigned char* rgba_out) override;
    void copyFramebufferToTexture(TextureHandle tex, int w, int h) override;

    bool              supportsRenderTargets() const override;
    RenderTargetHandle createRenderTarget(int w, int h) override;
    void              destroyRenderTarget(RenderTargetHandle rt) override;
    void              bindRenderTarget(RenderTargetHandle rt) override;
    void              blitToScreen(RenderTargetHandle rt,
                                   int dst_x, int dst_y, int dst_w, int dst_h) override;
    void              bindRenderTargetTexture(RenderTargetHandle rt, int unit) override;

    bool   hasTimerQueries() const override;
    void   timerBegin() override;
    void   timerEnd() override;
    double timerElapsedMs() override;

    const RenderCaps& getCaps() const override;
    bool isCoreProfile() const override;
    const char* getGPUVendor() const override;
    const char* getGPURenderer() const override;
    const char* getGLVersion() const override;
    const char* getRendererName() const override;

protected:
    // GL resource structs: move-only (GL objects cannot be shallow-copied)
    struct GLMesh {
        GLuint vbo = 0;
        GLuint ibo = 0;
        int index_count = 0;
        GLenum index_type = 0; // GL_UNSIGNED_INT or GL_UNSIGNED_SHORT
        bool valid = false;
        GLuint vao = 0; // 0 if not using VAO (GL2 path)
        GLMesh() = default;
        GLMesh(const GLMesh&) = delete;
        GLMesh& operator=(const GLMesh&) = delete;
        GLMesh(GLMesh&& o) noexcept
            : vbo(o.vbo), ibo(o.ibo), index_count(o.index_count),
              index_type(o.index_type), valid(o.valid), vao(o.vao)
            { o.vbo = 0; o.ibo = 0; o.valid = false; o.vao = 0; }
        GLMesh& operator=(GLMesh&& o) noexcept {
            if (this != &o) {
                vbo = o.vbo; ibo = o.ibo; index_count = o.index_count;
                index_type = o.index_type; valid = o.valid; vao = o.vao;
                o.vbo = 0; o.ibo = 0; o.valid = false; o.vao = 0;
            }
            return *this;
        }
    };
    struct GLTex {
        GLuint id = 0;
        bool valid = false;
        bool rt_owned = false; // true = GL object owned by render target, don't glDelete
        GLTex() = default;
        GLTex(const GLTex&) = delete;
        GLTex& operator=(const GLTex&) = delete;
        GLTex(GLTex&& o) noexcept : id(o.id), valid(o.valid), rt_owned(o.rt_owned)
            { o.id = 0; o.valid = false; }
        GLTex& operator=(GLTex&& o) noexcept {
            if (this != &o) { id = o.id; valid = o.valid; rt_owned = o.rt_owned; o.id = 0; o.valid = false; }
            return *this;
        }
    };
    struct ShaderProg {
        GLuint program = 0;
        // Uniform locations
        GLint u_proj = -1, u_view = -1, u_model = -1;
        GLint u_color = -1, u_light_dir = -1, u_tex = -1, u_use_tex = -1;
        // Attribute locations (cached)
        GLint a_pos = -1, a_normal = -1, a_uv = -1;
    };

    std::vector<GLMesh> meshes_;
    std::vector<GLTex>  textures_;
    std::vector<MeshHandle>    free_mesh_slots_;
    std::vector<TextureHandle> free_tex_slots_;

    // Custom shaders storage
    std::vector<GLuint> custom_shaders_;
    std::vector<ShaderHandle> free_custom_slots_;

    ShaderProg shader_3d_;
    ShaderProg shader_2d_color_;
    ShaderProg shader_2d_tex_;
    ShaderProg* current_shader_ = nullptr;  // non-owning: points to shader_3d_/2d_color_/2d_tex_

    RenderCaps caps_;
    std::string gpu_vendor_, gpu_renderer_, gl_version_;
    bool initialized_ = false;
    bool core_profile_ = false;
    MeshHandle last_drawn_mesh_;  // for skipping redundant vertex attrib setup
    int viewport_x_ = 0, viewport_y_ = 0, viewport_w_ = 0, viewport_h_ = 0;

    // Render targets (FBO)
    struct GLFBO {
        GLuint fbo = 0;
        GLuint color_tex = 0;
        GLuint extra_color_tex[3] = {0, 0, 0}; // MRT attachments 1-3
        int num_extra_color = 0;
        GLuint depth_rb = 0;
        GLuint depth_tex = 0;   // sampleable depth texture (0 = uses renderbuffer)
        int w = 0;
        int h = 0;
        bool valid = false;
        TextureHandle cached_depth_th;  // cached getRTDepthTexture() result
        TextureHandle cached_color_th;  // cached getRTColorTexture() result
        GLFBO() = default;
        GLFBO(const GLFBO&) = delete;
        GLFBO& operator=(const GLFBO&) = delete;
        GLFBO(GLFBO&& o) noexcept
            : fbo(o.fbo), color_tex(o.color_tex), num_extra_color(o.num_extra_color),
              depth_rb(o.depth_rb), depth_tex(o.depth_tex), w(o.w), h(o.h), valid(o.valid),
              cached_depth_th(o.cached_depth_th), cached_color_th(o.cached_color_th) {
            for (int i = 0; i < 3; i++) { extra_color_tex[i] = o.extra_color_tex[i]; o.extra_color_tex[i] = 0; }
            o.fbo = 0; o.color_tex = 0; o.depth_rb = 0; o.depth_tex = 0;
            o.num_extra_color = 0; o.valid = false;
        }
        GLFBO& operator=(GLFBO&& o) noexcept {
            if (this != &o) {
                fbo = o.fbo; color_tex = o.color_tex; num_extra_color = o.num_extra_color;
                depth_rb = o.depth_rb; depth_tex = o.depth_tex; w = o.w; h = o.h; valid = o.valid;
                cached_depth_th = o.cached_depth_th; cached_color_th = o.cached_color_th;
                for (int i = 0; i < 3; i++) { extra_color_tex[i] = o.extra_color_tex[i]; o.extra_color_tex[i] = 0; }
                o.fbo = 0; o.color_tex = 0; o.depth_rb = 0; o.depth_tex = 0;
                o.num_extra_color = 0; o.valid = false;
            }
            return *this;
        }
    };
    std::vector<GLFBO> render_targets_;
    std::vector<RenderTargetHandle> free_rt_slots_;
    MeshHandle blit_quad_;
    bool blit_quad_ready_ = false;
    bool has_blit_framebuffer_ = false;

    // GPU timer
    GPUTimer gpu_timer_;

    // GL state shadow cache (eliminates redundant glEnable/glDisable calls)
    StateCache state_cache_;

    GLuint compileShader(GLenum type, const char* src);
    GLuint linkProgram(GLuint vs, GLuint fs);
    bool   buildShader(ShaderProg& prog, const char* vs_src, const char* fs_src);
    void   detectCaps();

    // Handle validation helpers (debug-log on invalid access)
    bool isValidMesh(MeshHandle h) const {
        bool ok = h.id != 0 && h.id < meshes_.size() && meshes_[h.id].valid;
        if (!ok && h.id != 0) LOG_DBG("Invalid MeshHandle %u", h.id);
        return ok;
    }
    bool isValidTexture(TextureHandle h) const {
        bool ok = h.id != 0 && h.id < textures_.size() && textures_[h.id].valid;
        if (!ok && h.id != 0) LOG_DBG("Invalid TextureHandle %u", h.id);
        return ok;
    }
    bool isValidShader(ShaderHandle h) const {
        bool ok = h.id != 0 && h.id < custom_shaders_.size() && custom_shaders_[h.id] != 0;
        if (!ok && h.id != 0) LOG_DBG("Invalid ShaderHandle %u", h.id);
        return ok;
    }
    bool isValidRenderTarget(RenderTargetHandle h) const {
        bool ok = h != INVALID_RENDER_TARGET && h.id < render_targets_.size() && render_targets_[h.id].valid;
        if (!ok && h != INVALID_RENDER_TARGET) LOG_DBG("Invalid RenderTargetHandle %u", h.id);
        return ok;
    }
};
