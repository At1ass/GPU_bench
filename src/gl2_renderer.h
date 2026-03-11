#pragma once
#include "renderer.h"
#include "gl_funcs.h"
#include <vector>
#include <string>

class GL2Renderer : public Renderer {
public:
    GL2Renderer();
    virtual ~GL2Renderer();

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

    void setProjection(const Mat4& m) override;
    void setView(const Mat4& m) override;
    void setModel(const Mat4& m) override;

    void setColor(float r, float g, float b, float a) override;
    void setLightDir(float x, float y, float z) override;
    void bindTexture(TextureHandle tex) override;
    void setUseTexture(bool use) override;

    void drawMesh(MeshHandle h) override;

    void uploadTextureData(TextureHandle h, int w, int h_,
                           int channels, const unsigned char* pixels) override;
    void setColorMask(bool r, bool g, bool b, bool a) override;

    void setBlending(bool enable) override;
    void setDepthTest(bool enable) override;

    const RenderCaps& getCaps() const override;
    const char* getGPUVendor() const override;
    const char* getGPURenderer() const override;
    const char* getGLVersion() const override;
    const char* getRendererName() const override;

protected:
    struct GLMesh {
        GLuint vbo, ibo;
        int index_count;
        GLenum index_type; // GL_UNSIGNED_INT or GL_UNSIGNED_SHORT
        bool valid;
        GLuint vao; // 0 if not using VAO (GL2 path)
        GLMesh() : vbo(0), ibo(0), index_count(0), index_type(0), valid(false), vao(0) {}
    };
    struct GLTex {
        GLuint id;
        bool valid;
        GLTex() : id(0), valid(false) {}
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

    GLuint compileShader(GLenum type, const char* src);
    GLuint linkProgram(GLuint vs, GLuint fs);
    bool   buildShader(ShaderProg& prog, const char* vs_src, const char* fs_src);
    void   detectCaps();
};
