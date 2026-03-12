#pragma once
#include "geometry/mesh.h"
#include "geometry/math_types.h"
#include <vector>
#include <type_traits>

struct RenderCaps {
    int max_texture_size = 256;
    int max_vertex_attribs = 8;
    bool supports_32bit_indices = true;
    int estimated_vram_mb = 0;  // 0 = unknown
    int gl_major = 2;
    int gl_minor = 0;
    bool has_generate_mipmap_func = false;
    bool has_timer_queries = false;
    bool has_fbo = false;

    RenderCaps() = default;
};

struct ShaderTag {};
struct RenderTargetTag {};
struct BufferTag {};

using ShaderHandle       = Handle<ShaderTag>;
using RenderTargetHandle = Handle<RenderTargetTag>;
using BufferHandle       = Handle<BufferTag>;

static const ShaderHandle       INVALID_SHADER;
static const RenderTargetHandle INVALID_RENDER_TARGET;
static const BufferHandle       INVALID_BUFFER;

// Forward declarations for feature interfaces
struct GL3Features;
struct GL4Features;
struct ComputeFeatures;

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
    enum class ShaderType { Scene3D, Color2D, Textured2D };
    virtual void useShader(ShaderType type) = 0;

    // Custom shaders (for ShaderALU, StateChange tests)
    virtual ShaderHandle createCustomShader(const char* vs, const char* fs) = 0;
    virtual void         useCustomShader(ShaderHandle h) = 0;
    virtual void         destroyCustomShader(ShaderHandle h) = 0;
    virtual int          getCustomUniformLoc(ShaderHandle h, const char* name) = 0;
    virtual void         setUniform1i(int loc, int v) = 0;
    virtual void         setUniform1f(int loc, float v) = 0;
    virtual void         setUniform4f(int loc, float r, float g, float b, float a) = 0;
    virtual void         setUniformMat4(int loc, const Mat4& m) = 0;

    // Transforms (for ShaderType::Scene3D)
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

    // GL state reset (call between tests for deterministic state)
    virtual void resetState() = 0;

    // Unbind shader/VAO/texture state (for ImGui interop)
    virtual void unbindState() = 0;

    // Scissor test
    virtual void setScissor(bool enable, int x = 0, int y = 0, int w = 0, int h = 0) = 0;

    // GPU sync (flush all pending commands and wait for completion)
    virtual void finish() = 0;

    // Pixel readback (reads RGBA from current render target)
    virtual void readPixels(int x, int y, int w, int h, unsigned char* rgba_out) = 0;

    // Render targets (FBO abstraction)
    virtual bool              supportsRenderTargets() const = 0;
    virtual RenderTargetHandle createRenderTarget(int w, int h) = 0;
    virtual void              destroyRenderTarget(RenderTargetHandle rt) = 0;
    virtual void              bindRenderTarget(RenderTargetHandle rt) = 0;  // 0 = default framebuffer
    virtual void              blitToScreen(RenderTargetHandle rt,
                                           int dst_x, int dst_y, int dst_w, int dst_h) = 0;

    // GPU timer queries
    virtual bool   hasTimerQueries() const = 0;
    virtual void   timerBegin() = 0;
    virtual void   timerEnd() = 0;
    virtual double timerElapsedMs() = 0;

    // Feature groups (nullptr if not supported by this renderer)
    virtual GL3Features*     gl3()     { return nullptr; }
    virtual GL4Features*     gl4()     { return nullptr; }
    virtual ComputeFeatures* compute() { return nullptr; }
    virtual const GL3Features*     gl3()     const { return nullptr; }
    virtual const GL4Features*     gl4()     const { return nullptr; }
    virtual const ComputeFeatures* compute() const { return nullptr; }

    // Hardware capabilities
    virtual const RenderCaps& getCaps() const = 0;

    // Core profile: GLSL 1.20/1.40 unavailable, tests must use 1.50+
    virtual bool isCoreProfile() const { return false; }

    // Info
    virtual const char* getGPUVendor() const = 0;
    virtual const char* getGPURenderer() const = 0;
    virtual const char* getGLVersion() const = 0;
    virtual const char* getRendererName() const = 0;
};

// Handle type safety
static_assert(!std::is_same<MeshHandle, TextureHandle>::value, "");
static_assert(!std::is_same<MeshHandle, ShaderHandle>::value, "");
static_assert(!std::is_same<MeshHandle, BufferHandle>::value, "");
static_assert(!std::is_same<TextureHandle, ShaderHandle>::value, "");
static_assert(!std::is_same<TextureHandle, BufferHandle>::value, "");
static_assert(!std::is_same<ShaderHandle, BufferHandle>::value, "");
static_assert(!std::is_same<BufferHandle, RenderTargetHandle>::value, "");

static_assert(std::is_trivially_copyable<MeshHandle>::value, "");
static_assert(std::is_trivially_copyable<BufferHandle>::value, "");
static_assert(std::is_trivially_copyable<ShaderHandle>::value, "");
static_assert(std::is_trivially_copyable<TextureHandle>::value, "");
static_assert(std::is_trivially_copyable<RenderTargetHandle>::value, "");
