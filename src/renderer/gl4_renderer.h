#pragma once
#include "renderer/gl3_renderer.h"

class GL4Renderer : public GL3Renderer {
public:
    GL4Renderer();
    ~GL4Renderer();

    bool init(int w, int h) override;
    void shutdown() override;
    const char* getRendererName() const override;

    // Compute shader support (GL 4.3+)
    ShaderHandle createComputeShader(const char* source) override;
    void         dispatchCompute(int groups_x, int groups_y, int groups_z) override;
    void         computeMemoryBarrier() override;

    BufferHandle createSSBO(int size_bytes) override;
    void         destroySSBO(BufferHandle h) override;
    void         bindSSBO(BufferHandle h, int binding) override;

private:
    // SSBO storage
    struct GLBuffer {
        GLuint id = 0;
        bool valid = false;
    };
    std::vector<GLBuffer> ssbos_;
    std::vector<BufferHandle> free_ssbo_slots_;
};
