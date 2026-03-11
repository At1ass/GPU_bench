#pragma once
#include "gl3_renderer.h"

class GL4Renderer : public GL3Renderer {
public:
    GL4Renderer();
    ~GL4Renderer();

    bool init(int w, int h) override;
    const char* getRendererName() const override;
};
