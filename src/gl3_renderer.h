#pragma once
#include "gl2_renderer.h"

class GL3Renderer : public GL2Renderer {
public:
    GL3Renderer();
    ~GL3Renderer();

    bool init(int w, int h) override;
    void shutdown() override;

    MeshHandle    createMesh(const MeshData& data) override;
    void          destroyMesh(MeshHandle h) override;
    TextureHandle createTexture(int w, int h, int channels, const unsigned char* pixels) override;
    void          drawMesh(MeshHandle h) override;

    const char* getRendererName() const override;
};
