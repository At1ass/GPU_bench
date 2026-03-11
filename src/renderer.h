#pragma once
#include "mesh.h"
#include "math_types.h"
#include <vector>

struct RenderCaps {
    int max_texture_size;
    bool supports_32bit_indices;
    int estimated_vram_mb;  // 0 = unknown
    int gl_major, gl_minor;
    bool has_vao;
    bool has_instancing;
    bool has_generate_mipmap_func;

    RenderCaps() : max_texture_size(256), supports_32bit_indices(true), estimated_vram_mb(0),
                   gl_major(2), gl_minor(0), has_vao(false), has_instancing(false),
                   has_generate_mipmap_func(false) {}
};

typedef unsigned int ShaderHandle;
static const ShaderHandle INVALID_SHADER = 0;

// Abstract renderer interface.
class Renderer {
public:
    virtual ~Renderer() {}

    virtual bool init(int w, int h) = 0;
    virtual void shutdown() = 0;

    virtual void setViewport(int x, int y, int w, int h) = 0;
    virtual void clear(float r, float g, float b, float a) = 0;

    // Resources
    virtual MeshHandle    createMesh(const MeshData& data) = 0;
    virtual void          destroyMesh(MeshHandle h) = 0;
    virtual TextureHandle createTexture(int w, int h, int channels, const unsigned char* pixels) = 0;
    virtual void          destroyTexture(TextureHandle h) = 0;

    // Shader selection (built-in shaders)
    enum ShaderType { SHADER_3D, SHADER_2D_COLOR, SHADER_2D_TEXTURED };
    virtual void useShader(ShaderType type) = 0;

    // Custom shaders (for ShaderALU, StateChange tests)
    virtual ShaderHandle createCustomShader(const char* vs, const char* fs) = 0;
    virtual void         useCustomShader(ShaderHandle h) = 0;
    virtual void         destroyCustomShader(ShaderHandle h) = 0;
    virtual int          getCustomUniformLoc(ShaderHandle h, const char* name) = 0;
    virtual void         setUniform1i(int loc, int v) = 0;
    virtual void         setUniform1f(int loc, float v) = 0;

    // Transforms (for SHADER_3D)
    virtual void setProjection(const Mat4& m) = 0;
    virtual void setView(const Mat4& m) = 0;
    virtual void setModel(const Mat4& m) = 0;

    // Material
    virtual void setColor(float r, float g, float b, float a) = 0;
    virtual void setLightDir(float x, float y, float z) = 0;
    virtual void bindTexture(TextureHandle tex) = 0;
    virtual void setUseTexture(bool use) = 0;

    // Draw
    virtual void drawMesh(MeshHandle h) = 0;

    // Texture re-upload (for TexUpload test)
    virtual void uploadTextureData(TextureHandle h, int w, int h_,
                                   int channels, const unsigned char* pixels) = 0;

    // Color write mask (for Vertex throughput test)
    virtual void setColorMask(bool r, bool g, bool b, bool a) = 0;

    // State
    virtual void setBlending(bool enable) = 0;
    virtual void setDepthTest(bool enable) = 0;

    // Hardware capabilities
    virtual const RenderCaps& getCaps() const = 0;

    // Info
    virtual const char* getGPUVendor() const = 0;
    virtual const char* getGPURenderer() const = 0;
    virtual const char* getGLVersion() const = 0;
    virtual const char* getRendererName() const = 0;
};
