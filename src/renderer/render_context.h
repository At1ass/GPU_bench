#pragma once
#include <SDL.h>
#include <memory>

struct AppConfig;

// Abstract render context interface.
// Owns the window, graphics API context, ImGui backend, and event pump.
// Concrete implementations: GLRenderContext (GL/GLES), future VulkanRenderContext.
class RenderContext {
public:
    virtual ~RenderContext();

    // Create window + API context + load functions + init ImGui.
    virtual bool init(const AppConfig& cfg) = 0;

    // Shutdown ImGui, destroy context + window, quit SDL.
    virtual void shutdown() = 0;

    // Access
    SDL_Window* window() const { return window_; }
    bool        isHeadless() const { return headless_; }

    // Events — pumps SDL events, feeds ImGui, returns true while events remain.
    // Fills *e with the current event. Caller handles SDL_QUIT etc.
    bool pollEvent(SDL_Event* e);

    // ImGui frame
    virtual void imguiNewFrame() = 0;
    virtual void imguiRender() = 0;

    // Swap / VSync
    virtual void swapBuffers() = 0;
    virtual void setVSync(bool enable) = 0;

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;
    RenderContext(RenderContext&&) = delete;
    RenderContext& operator=(RenderContext&&) = delete;

protected:
    RenderContext();

    // Shared SDL init (SDL_Init + window creation). Called by subclasses.
    bool initSDL(const AppConfig& cfg);
    void shutdownSDL();

    // ImGui event forwarding — called by pollEvent(), implemented by subclass.
    virtual void onEvent(SDL_Event* e) = 0;

    SDL_Window* window_;
    bool        headless_;
};

// Factory: creates the appropriate RenderContext for the given config.
// Currently always returns GLRenderContext; future: VulkanRenderContext.
std::unique_ptr<RenderContext> createRenderContext(const AppConfig& cfg);
