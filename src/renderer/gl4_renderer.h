#pragma once
#include "renderer/gl3_renderer.h"

class GL4Renderer : public GL3Renderer, public GL4Features, public ComputeFeatures {
public:
    GL4Renderer();

    bool init(int w, int h) override;
    void shutdown() override;
    const char* getRendererName() const override;

    // ComputeFeatures implementation
    ShaderHandle createComputeShader(const char* source) override;
    void         dispatchCompute(int groups_x, int groups_y, int groups_z) override;
    void         computeMemoryBarrier() override;

    BufferHandle createSSBO(int size_bytes) override;
    void         destroySSBO(BufferHandle h) override;
    void         bindSSBO(BufferHandle h, int binding) override;
    void         updateSSBO(BufferHandle h, const void* data, int size_bytes) override;
    void         readSSBO(BufferHandle h, void* data, int offset, int size_bytes) override;

    // Float render targets (HDR)
    RenderTargetHandle createFloatRenderTarget(int w, int h) override;
    RenderTargetHandle createFloatRenderTargetWithDepth(int w, int h) override;

    // GL4Features implementation
    BufferHandle createIndirectBuffer(int size_bytes, const void* data) override;
    void         destroyIndirectBuffer(BufferHandle h) override;
    void         multiDrawMeshIndirect(MeshHandle mesh, BufferHandle indirect, int draw_count, int stride) override;

    // Tessellation
    void         setPatchVertices(int count) override;
    ShaderHandle createTessShader(const char* vs, const char* tcs,
                                  const char* tes, const char* fs) override;
    void         drawMeshAsPatches(MeshHandle h) override;

    // Float texture (RGBA32F for compute image load/store)
    TextureHandle createFloatTexture(int w, int h) override;

    // Image load/store
    void         bindImageTexture(TextureHandle h, int unit,
                                   bool read, bool write) override;
    void         imageMemoryBarrier() override;

    // Texture copy (glCopyImageSubData)
    void         copyImageSubData(TextureHandle src, TextureHandle dst, int w, int h) override;

    // RGBA16F texture (matching HDR RT color format)
    TextureHandle createFloat16Texture(int w, int h) override;

    // Depth texture (GL_DEPTH_COMPONENT24)
    TextureHandle createDepthTexture(int w, int h) override;

    // Persistent mapping
    BufferHandle createPersistentBuffer(int size_bytes) override;
    void*        mapPersistentBuffer(BufferHandle h) override;
    void         persistentBufferFence(BufferHandle h) override;
    void         destroyPersistentBuffer(BufferHandle h) override;

    // Bindless texture
    uint64_t     getBindlessHandle(TextureHandle h) override;
    void         makeTextureResident(uint64_t handle) override;
    void         makeTextureNonResident(uint64_t handle) override;
    void         setUniformHandle(int loc, uint64_t handle) override;

protected:
    void* queryFeature(int id) override;

private:
    // SSBO storage
    struct GLBuffer {
        GLuint id = 0;
        bool valid = false;
    };
    std::vector<GLBuffer> ssbos_;
    std::vector<BufferHandle> free_ssbo_slots_;

    // Indirect draw buffers
    std::vector<GLBuffer> indirect_buffers_;
    std::vector<BufferHandle> free_indirect_slots_;

    // Persistent buffers
    struct PersistentBuffer {
        GLuint id = 0;
        int    size = 0;
        void*  mapped = nullptr;
        GLsync fence = nullptr;
        bool   valid = false;
    };
    std::vector<PersistentBuffer> persistent_buffers_;
    std::vector<BufferHandle> free_persistent_slots_;
};
