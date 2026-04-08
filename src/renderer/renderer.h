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

// Per-frame GPU call counters. Counted at the renderer level where GL calls
// actually happen — follows bgfx/Godot/Ogre3D pattern.
// Reset once per frame via resetRendererStats().
struct RendererStats {
    int draw_calls = 0;
    int state_applied = 0;
    int state_skipped = 0;
    int texture_binds = 0;
    int texture_skipped = 0;
    int rt_switches = 0;
    int shader_switches = 0;
    int compute_dispatches = 0;
    int barriers_issued = 0;
    void reset() { *this = {}; }
};

// Feature tag: specializations in features.h assign compile-time IDs.
// Usage: renderer.features<GL3Features>() returns GL3Features* or nullptr.
template<typename T> struct FeatureTag;

// Abstract renderer interface.
class Renderer {
public:
    virtual ~Renderer() {}
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

protected:
    Renderer() = default;

public:
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
    virtual void         setUniform2f(int loc, float x, float y) = 0;
    virtual void         setUniform3f(int loc, float x, float y, float z) = 0;
    virtual void         setUniform4f(int loc, float r, float g, float b, float a) = 0;
    virtual void         setUniformMat4(int loc, const Mat4& m) = 0;

    // Transforms (for ShaderType::Scene3D)
    virtual void setProjection(const Mat4& m) = 0;
    virtual void setView(const Mat4& m) = 0;
    virtual void setModel(const Mat4& m) = 0;

    // Bind texture to specific texture unit (for multi-texture / shadow maps)
    virtual void bindTextureUnit(int unit, TextureHandle tex) = 0;

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
    virtual void setBlendingAdditive(bool enable) = 0;
    virtual void setDepthTest(bool enable) = 0;
    virtual void setCullFace(bool enable) = 0;
    virtual void setDepthMask(bool write) = 0;
    virtual void setPolygonOffset(bool enable, float factor, float units) = 0;

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

    // Depth-only render target (for shadow maps). Default: unsupported.
    virtual RenderTargetHandle createDepthRenderTarget(int w, int h) { (void)w; (void)h; return INVALID_RENDER_TARGET; }
    virtual TextureHandle      getDepthTexture(RenderTargetHandle rt) { (void)rt; return INVALID_TEXTURE; }

    // Render target with sampleable depth texture (for SSAO etc). Default: falls back to regular RT.
    virtual RenderTargetHandle createRenderTargetWithDepth(int w, int h) { return createRenderTarget(w, h); }
    virtual TextureHandle      getRTDepthTexture(RenderTargetHandle rt) { (void)rt; return INVALID_TEXTURE; }
    virtual TextureHandle      getRTColorTexture(RenderTargetHandle rt) { (void)rt; return INVALID_TEXTURE; }

    // Bind a render target's color texture to a texture unit (for post-process chains).
    virtual void bindRenderTargetTexture(RenderTargetHandle rt, int unit) { (void)rt; (void)unit; }

    // Copy current FBO color attachment to a bound texture. Default: no-op.
    virtual void copyFramebufferToTexture(TextureHandle tex, int w, int h) { (void)tex; (void)w; (void)h; }

    // Float texture (RGBA16F) for compute shader output. Default: unsupported.
    virtual TextureHandle createFloatTexture(int w, int h) { (void)w; (void)h; return INVALID_TEXTURE; }

    // Float render target (RGBA16F FBO for HDR). Default: falls back to regular RT.
    virtual RenderTargetHandle createFloatRenderTarget(int w, int h) { return createRenderTarget(w, h); }
    virtual RenderTargetHandle createFloatRenderTargetWithDepth(int w, int h) { return createRenderTargetWithDepth(w, h); }

    // GPU timer queries
    virtual bool   hasTimerQueries() const = 0;
    virtual void   timerBegin() = 0;
    virtual void   timerEnd() = 0;
    virtual double timerElapsedMs() = 0;

    // Feature query (LLVM-style): override in subclasses to expose feature interfaces.
    // Returns typed pointer via static_cast from void* — safe round-trip.
    template<typename T>
    T* features() { return static_cast<T*>(queryFeature(FeatureTag<T>::id)); }

    template<typename T>
    const T* features() const {
        return static_cast<const T*>(
            const_cast<Renderer*>(this)->queryFeature(FeatureTag<T>::id));
    }

    // Per-frame renderer stats (counted where GL calls happen)
    const RendererStats& rendererStats() const { return renderer_stats_; }
    void resetRendererStats() { renderer_stats_.reset(); }

    // Hardware capabilities
    virtual const RenderCaps& getCaps() const = 0;

    // Core profile: GLSL 1.20/1.40 unavailable, tests must use 1.50+
    virtual bool isCoreProfile() const { return false; }

    // Info
    virtual const char* getGPUVendor() const = 0;
    virtual const char* getGPURenderer() const = 0;
    virtual const char* getGLVersion() const = 0;
    virtual const char* getRendererName() const = 0;

protected:
    virtual void* queryFeature(int) { return nullptr; }
    RendererStats renderer_stats_;
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
