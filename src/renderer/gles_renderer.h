#pragma once
#include "renderer/gl2_renderer.h"

// OpenGL ES renderer. Inherits GL2Renderer with GLES-specific shader
// syntax (precision qualifiers), format handling, and capability limits.
// Supports GLES 2.0+ contexts. On GLES 3.0+ enables VAO and FBO paths.
class GLESRenderer : public GL2Renderer {
public:
    GLESRenderer();
    ~GLESRenderer();

    bool init(int w, int h) override;
    TextureHandle createTexture(int w, int h, int channels, const unsigned char* pixels) override;
    void drawMesh(MeshHandle h) override;
    void unbindState() override;
    const char* getRendererName() const override;

private:
    bool gles3_; // true if GLES 3.0+
};
