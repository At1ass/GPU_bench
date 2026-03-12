#pragma once
#include "renderer/gl3_renderer.h"

class GL4Renderer : public GL3Renderer, public GL4Features, public ComputeFeatures {
public:
    GL4Renderer();
    ~GL4Renderer();

    bool init(int w, int h) override;
    void shutdown() override;
    const char* getRendererName() const override;

    // Feature accessors
    GL4Features* gl4() override { return this; }
    ComputeFeatures* compute() override { return this; }
    const GL4Features* gl4() const override { return this; }
    const ComputeFeatures* compute() const override { return this; }

    // ComputeFeatures implementation
    ShaderHandle createComputeShader(const char* source) override;
    void         dispatchCompute(int groups_x, int groups_y, int groups_z) override;
    void         computeMemoryBarrier() override;

    BufferHandle createSSBO(int size_bytes) override;
    void         destroySSBO(BufferHandle h) override;
    void         bindSSBO(BufferHandle h, int binding) override;

    // GL4Features implementation
    BufferHandle createIndirectBuffer(int size_bytes, const void* data) override;
    void         destroyIndirectBuffer(BufferHandle h) override;
    void         multiDrawMeshIndirect(MeshHandle mesh, BufferHandle indirect, int draw_count, int stride) override;

    // Tessellation
    void         setPatchVertices(int count) override;
    ShaderHandle createTessShader(const char* vs, const char* tcs,
                                  const char* tes, const char* fs) override;

    // Image load/store
    void         bindImageTexture(TextureHandle h, int unit,
                                   bool read, bool write) override;
    void         imageMemoryBarrier() override;

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
        void*  mapped = nullptr;
        GLsync fence = nullptr;
        bool   valid = false;
    };
    std::vector<PersistentBuffer> persistent_buffers_;
    std::vector<BufferHandle> free_persistent_slots_;
};
