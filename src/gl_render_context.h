#pragma once
#include "render_context.h"
#include "renderer_backend.h"

// GL/GLES render context — creates desktop GL or GLES context,
// loads GL functions, initializes ImGui OpenGL3 backend.
class GLRenderContext : public RenderContext {
public:
    GLRenderContext();
    ~GLRenderContext();

    bool init(const AppConfig& cfg) override;
    void shutdown() override;

    void imguiNewFrame() override;
    void imguiRender() override;

    void swapBuffers() override;
    void setVSync(bool enable) override;

protected:
    void onEvent(SDL_Event* e) override;

private:
    SDL_GLContext gl_context_;
    bool          imgui_initialized_;

    bool createGLESContext();
    bool createDesktopGLContext(RendererBackend backend);
    bool initImGui();
};
