#pragma once
#include "renderer/renderer.h"
#include "renderer/gl_funcs.h"
#include "renderer/gpu_timer.h"
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

    void uploadTextureData(TextureHandle h, int w, int h_,
                           int channels, const unsigned char* pixels) override;
    void setColorMask(bool r, bool g, bool b, bool a) override;

    void setBlending(bool enable) override;
    void setDepthTest(bool enable) override;
    void setCullFace(bool enable) override;
    void setDepthMask(bool write) override;

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
    struct GLMesh {
        GLuint vbo = 0;
        GLuint ibo = 0;
        int index_count = 0;
        GLenum index_type = 0; // GL_UNSIGNED_INT or GL_UNSIGNED_SHORT
        bool valid = false;
        GLuint vao = 0; // 0 if not using VAO (GL2 path)
        GLMesh() = default;
    };
    struct GLTex {
        GLuint id = 0;
        bool valid = false;
        bool rt_owned = false; // true = GL object owned by render target, don't glDelete
        GLTex() = default;
    };
    struct ShaderProg {
        GLuint program;
        // Uniform locations
        GLint u_proj, u_view, u_model;
        GLint u_color, u_light_dir, u_tex, u_use_tex;
        // Attribute locations (cached)
        GLint a_pos, a_normal, a_uv;
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
    ShaderProg* current_shader_;

    RenderCaps caps_;
    std::string gpu_vendor_, gpu_renderer_, gl_version_;
    bool initialized_;
    bool core_profile_;
    MeshHandle last_drawn_mesh_;  // for skipping redundant vertex attrib setup
    int viewport_x_, viewport_y_, viewport_w_, viewport_h_;

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
        GLFBO() = default;
    };
    std::vector<GLFBO> render_targets_;
    std::vector<RenderTargetHandle> free_rt_slots_;
    MeshHandle blit_quad_;
    bool blit_quad_ready_;
    bool has_blit_framebuffer_;

    // GPU timer
    GPUTimer gpu_timer_;

    GLuint compileShader(GLenum type, const char* src);
    GLuint linkProgram(GLuint vs, GLuint fs);
    bool   buildShader(ShaderProg& prog, const char* vs_src, const char* fs_src);
    void   detectCaps();

    // Handle validation helpers (debug-log on invalid access)
    bool isValidMesh(MeshHandle h) const {
        bool ok = h != 0 && h < meshes_.size() && meshes_[h].valid;
        if (!ok && h != 0) LOG_DBG("Invalid MeshHandle %u", static_cast<unsigned>(h));
        return ok;
    }
    bool isValidTexture(TextureHandle h) const {
        bool ok = h != 0 && h < textures_.size() && textures_[h].valid;
        if (!ok && h != 0) LOG_DBG("Invalid TextureHandle %u", static_cast<unsigned>(h));
        return ok;
    }
    bool isValidShader(ShaderHandle h) const {
        bool ok = h != 0 && h < custom_shaders_.size() && custom_shaders_[h] != 0;
        if (!ok && h != 0) LOG_DBG("Invalid ShaderHandle %u", static_cast<unsigned>(h));
        return ok;
    }
    bool isValidRenderTarget(RenderTargetHandle h) const {
        bool ok = h != INVALID_RENDER_TARGET && h < render_targets_.size() && render_targets_[h].valid;
        if (!ok && h != INVALID_RENDER_TARGET) LOG_DBG("Invalid RenderTargetHandle %u", static_cast<unsigned>(h));
        return ok;
    }
};
