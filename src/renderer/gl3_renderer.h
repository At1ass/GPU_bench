#pragma once
#include "renderer/gl2_renderer.h"
#include "renderer/features.h"

class GL3Renderer : public GL2Renderer, public GL3Features {
public:
    GL3Renderer();

    bool init(int w, int h) override;
    void shutdown() override;

    MeshHandle    createMesh(const MeshData& data) override;
    void          destroyMesh(MeshHandle h) override;
    TextureHandle createTexture(int w, int h, int channels, const unsigned char* pixels) override;
    void          drawMesh(MeshHandle h) override;

    void unbindState() override;
    const char* getRendererName() const override;

    // Depth-only render target (shadow maps)
    RenderTargetHandle createDepthRenderTarget(int w, int h) override;
    TextureHandle      getDepthTexture(RenderTargetHandle rt) override;

    // Render target with sampleable depth texture (for SSAO)
    RenderTargetHandle createRenderTargetWithDepth(int w, int h) override;
    TextureHandle      getRTDepthTexture(RenderTargetHandle rt) override;
    TextureHandle      getRTColorTexture(RenderTargetHandle rt) override;

    // GL3Features implementation
    void drawMeshInstanced(MeshHandle h, int instance_count) override;
    RenderTargetHandle createMRTRenderTarget(int w, int h, int num_attachments) override;
    TextureHandle createTextureArray(int w, int h, int layers, int channels, const unsigned char* pixels) override;
    void          bindTextureArray(TextureHandle h) override;
    ShaderHandle createShaderVGF(const char* vs, const char* gs, const char* fs) override;
    BufferHandle createUBO(int size_bytes) override;
    void         updateUBO(BufferHandle h, const void* data, int size) override;
    void         bindUBO(BufferHandle h, int binding) override;
    void         destroyUBO(BufferHandle h) override;
    void         setRasterizerDiscard(bool enable) override;
    BufferHandle createTransformFeedbackBuffer(int size_bytes) override;
    void         destroyTransformFeedbackBuffer(BufferHandle h) override;
    ShaderHandle createTransformFeedbackShader(const char* vs, const char* fs,
                                               const char* const* varyings, int varying_count) override;
    void         beginTransformFeedback(BufferHandle tf_buf) override;
    void         endTransformFeedback() override;

protected:
    void* queryFeature(int id) const override;

private:
    struct GLBuffer {
        GLuint id = 0;
        bool valid = false;
    };
    std::vector<GLBuffer> ubos_;
    std::vector<BufferHandle> free_ubo_slots_;
    std::vector<GLBuffer> tf_buffers_;
    std::vector<BufferHandle> free_tf_slots_;
    bool tf_active_ = false;
};
