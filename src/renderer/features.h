#pragma once
#include "geometry/mesh.h"
#include "renderer/renderer.h"
#include <cstdint>
#include <type_traits>

// Forward declarations
struct GL3Features;
struct GL4Features;
struct ComputeFeatures;

// FeatureTag specializations — compile-time IDs for queryFeature dispatch
template<> struct FeatureTag<GL3Features>     { static constexpr int id = 0; };
template<> struct FeatureTag<GL4Features>     { static constexpr int id = 1; };
template<> struct FeatureTag<ComputeFeatures> { static constexpr int id = 2; };

struct GL3Features {
    virtual ~GL3Features() = default;
    GL3Features(const GL3Features&) = delete;
    GL3Features& operator=(const GL3Features&) = delete;
    GL3Features(GL3Features&&) = delete;
    GL3Features& operator=(GL3Features&&) = delete;
protected:
    GL3Features() = default;
public:
    // Capability queries (set by GL3Renderer::init)
    bool hasVAO() const             { return has_vao_; }
    bool hasInstancing() const      { return has_instancing_; }
    bool hasMRT() const             { return has_mrt_; }
    bool hasTextureArray() const    { return has_texture_array_; }
    bool hasGeometryShader() const  { return has_geometry_shader_; }
    bool hasUBO() const             { return has_ubo_; }
    bool hasTransformFeedback() const { return has_transform_feedback_; }

    // Instancing
    virtual void drawMeshInstanced(MeshHandle h, int instance_count) = 0;
    // MRT
    virtual RenderTargetHandle createMRTRenderTarget(int w, int h, int num_attachments) = 0;
    // Texture arrays
    virtual TextureHandle createTextureArray(int w, int h, int layers, int channels, const unsigned char* pixels) = 0;
    virtual void bindTextureArray(TextureHandle h) = 0;
    // Geometry shaders
    virtual ShaderHandle createShaderVGF(const char* vs, const char* gs, const char* fs) = 0;
    // UBO
    virtual BufferHandle createUBO(int size_bytes) = 0;
    virtual void updateUBO(BufferHandle h, const void* data, int size) = 0;
    virtual void bindUBO(BufferHandle h, int binding) = 0;
    virtual void destroyUBO(BufferHandle h) = 0;
    // Transform feedback
    virtual void setRasterizerDiscard(bool enable) = 0;
    virtual BufferHandle createTransformFeedbackBuffer(int size_bytes) = 0;
    virtual void destroyTransformFeedbackBuffer(BufferHandle h) = 0;
    virtual ShaderHandle createTransformFeedbackShader(const char* vs, const char* fs,
                                                        const char* const* varyings, int varying_count) = 0;
    virtual void beginTransformFeedback(BufferHandle tf_buf) = 0;
    virtual void endTransformFeedback() = 0;

protected:
    bool has_vao_ = false;
    bool has_instancing_ = false;
    bool has_mrt_ = false;
    bool has_texture_array_ = false;
    bool has_geometry_shader_ = false;
    bool has_ubo_ = false;
    bool has_transform_feedback_ = false;
};

struct ComputeFeatures {
    virtual ~ComputeFeatures() = default;
    ComputeFeatures(const ComputeFeatures&) = delete;
    ComputeFeatures& operator=(const ComputeFeatures&) = delete;
    ComputeFeatures(ComputeFeatures&&) = delete;
    ComputeFeatures& operator=(ComputeFeatures&&) = delete;
protected:
    ComputeFeatures() = default;
public:
    bool hasCompute() const { return has_compute_; }

    // SSBO usage hints (maps to GL_DYNAMIC_DRAW / GL_DYNAMIC_READ)
    enum class SSBOUsage : unsigned int {
        GpuReadWrite = 0x88E8,  // GL_DYNAMIC_DRAW: GPU writes, GPU reads (default)
        CpuReadBack  = 0x88E9,  // GL_DYNAMIC_READ: GPU writes, CPU reads back
    };

    virtual ShaderHandle createComputeShader(const char* source) = 0;
    virtual void dispatchCompute(int groups_x, int groups_y, int groups_z) = 0;
    virtual void computeMemoryBarrier() = 0;
    virtual BufferHandle createSSBO(int size_bytes, SSBOUsage usage = SSBOUsage::GpuReadWrite) = 0;
    virtual void destroySSBO(BufferHandle h) = 0;
    virtual void bindSSBO(BufferHandle h, int binding) = 0;
    virtual void updateSSBO(BufferHandle h, const void* data, int size_bytes) = 0;
    virtual void readSSBO(BufferHandle h, void* data, int offset, int size_bytes) = 0;

protected:
    bool has_compute_ = false;
};

struct GL4Features {
    virtual ~GL4Features() = default;
    GL4Features(const GL4Features&) = delete;
    GL4Features& operator=(const GL4Features&) = delete;
    GL4Features(GL4Features&&) = delete;
    GL4Features& operator=(GL4Features&&) = delete;
protected:
    GL4Features() = default;
public:
    bool hasIndirectDraw() const    { return has_indirect_draw_; }
    bool hasTessellation() const    { return has_tessellation_; }
    bool hasTextureGather() const   { return has_texture_gather_; }
    bool hasImageLoadStore() const  { return has_image_load_store_; }
    bool hasBufferStorage() const   { return has_buffer_storage_; }
    bool hasBindlessTexture() const { return has_bindless_texture_; }

    // Indirect draw
    virtual BufferHandle createIndirectBuffer(int size_bytes, const void* data) = 0;
    virtual void destroyIndirectBuffer(BufferHandle h) = 0;
    virtual void multiDrawMeshIndirect(MeshHandle mesh, BufferHandle indirect, int draw_count, int stride) = 0;

    // Tessellation
    virtual void setPatchVertices(int count) = 0;
    virtual ShaderHandle createTessShader(const char* vs, const char* tcs,
                                          const char* tes, const char* fs) = 0;
    virtual void drawMeshAsPatches(MeshHandle h) = 0;

    // Image load/store
    virtual void bindImageTexture(TextureHandle h, int unit,
                                   bool read, bool write) = 0;
    virtual void imageMemoryBarrier() = 0;

    // Texture copy (glCopyImageSubData — format must match between src and dst)
    virtual void copyImageSubData(TextureHandle src, TextureHandle dst, int w, int h) = 0;

    // RGBA16F texture (matching HDR RT color format)
    virtual TextureHandle createFloat16Texture(int w, int h) = 0;

    // Depth texture (GL_DEPTH_COMPONENT24, matching HDR RT depth format)
    virtual TextureHandle createDepthTexture(int w, int h) = 0;

    // Persistent mapping (buffer storage)
    virtual BufferHandle createPersistentBuffer(int size_bytes) = 0;
    virtual void* mapPersistentBuffer(BufferHandle h) = 0;
    virtual void persistentBufferFence(BufferHandle h) = 0;
    virtual void destroyPersistentBuffer(BufferHandle h) = 0;

    // Bindless texture
    virtual uint64_t getBindlessHandle(TextureHandle h) = 0;
    virtual void makeTextureResident(uint64_t handle) = 0;
    virtual void makeTextureNonResident(uint64_t handle) = 0;
    virtual void setUniformHandle(int loc, uint64_t handle) = 0;

protected:
    bool has_indirect_draw_ = false;
    bool has_tessellation_ = false;
    bool has_texture_gather_ = false;
    bool has_image_load_store_ = false;
    bool has_buffer_storage_ = false;
    bool has_bindless_texture_ = false;
};

static_assert(std::is_abstract<GL3Features>::value, "Must be pure interface");
static_assert(std::is_abstract<GL4Features>::value, "Must be pure interface");
static_assert(std::is_abstract<ComputeFeatures>::value, "Must be pure interface");

// Compile-time: what does renderer R provide?
template<typename R>
struct renderer_traits {
    static const bool provides_gl3    = std::is_base_of<GL3Features, R>::value;
    static const bool provides_gl4    = std::is_base_of<GL4Features, R>::value;
    static const bool provides_compute = std::is_base_of<ComputeFeatures, R>::value;
};

// Forward declarations for test base classes (defined in bench/bench.h)
class GL3BenchTest;
class ComputeBenchTest;
class GL4BenchTest;

// Compile-time: what does test T require?
template<typename T>
struct test_traits {
    static const bool requires_gl3    = std::is_base_of<GL3BenchTest, T>::value;
    static const bool requires_compute = std::is_base_of<ComputeBenchTest, T>::value;
    static const bool requires_gl4    = std::is_base_of<GL4BenchTest, T>::value;
};
