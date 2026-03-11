#include "renderer/gl_render_context.h"
#include "app.h"
#include "renderer/gl_loader.h"
#include "platform/logger.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <cstring>

GLRenderContext::GLRenderContext()
    : gl_context_(nullptr), imgui_initialized_(false) {}

GLRenderContext::~GLRenderContext() { shutdown(); }

bool GLRenderContext::createGLESContext() {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    gl_context_ = SDL_GL_CreateContext(window_);
    if (gl_context_) {
        Log::info("Created GLES 3.0 context");
        return true;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    gl_context_ = SDL_GL_CreateContext(window_);
    if (gl_context_) {
        Log::info("Created GLES 2.0 context");
        return true;
    }
    return false;
}

bool GLRenderContext::createDesktopGLContext(RendererBackend backend) {
    if (backend != RendererBackend::GL2 && backend != RendererBackend::GL3) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        gl_context_ = SDL_GL_CreateContext(window_);
        if (gl_context_) {
            Log::info("Created GL 4.3 compatibility context");
            return true;
        }
    }

    if (backend != RendererBackend::GL2) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        gl_context_ = SDL_GL_CreateContext(window_);
        if (gl_context_) {
            Log::info("Created GL 3.2 compatibility context");
            return true;
        }
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
    gl_context_ = SDL_GL_CreateContext(window_);
    if (gl_context_) {
        Log::info("Created GL 2.1 context");
        return true;
    }
    return false;
}

bool GLRenderContext::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);

    const char* glsl_version = "#version 120";
    if (GLLoader::isGLES()) {
        glsl_version = (GLLoader::glMajor() >= 3) ? "#version 300 es" : "#version 100";
    } else if (GLLoader::glMajor() >= 3) {
        glsl_version = "#version 150";
    }
    ImGui_ImplOpenGL3_Init(glsl_version);
    imgui_initialized_ = true;
    return true;
}

bool GLRenderContext::init(const AppConfig& cfg) {
    if (!initSDL(cfg)) return false;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    bool ctx_ok = (cfg.backend == RendererBackend::GLES)
        ? createGLESContext()
        : createDesktopGLContext(cfg.backend);

    if (!ctx_ok) {
        Log::err("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    if (!GLLoader::init()) {
        Log::err("Failed to load GL functions");
        return false;
    }

    if (!headless_) {
        if (!initImGui()) {
            Log::err("Failed to initialize ImGui");
            return false;
        }
    }
    return true;
}

void GLRenderContext::shutdown() {
    if (imgui_initialized_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        imgui_initialized_ = false;
    }
    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    shutdownSDL();
}

void GLRenderContext::onEvent(SDL_Event* e) {
    if (imgui_initialized_) {
        ImGui_ImplSDL2_ProcessEvent(e);
    }
}

void GLRenderContext::imguiNewFrame() {
    if (!imgui_initialized_) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void GLRenderContext::imguiRender() {
    if (!imgui_initialized_) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GLRenderContext::swapBuffers() {
    if (window_) SDL_GL_SwapWindow(window_);
}

void GLRenderContext::setVSync(bool enable) {
    SDL_GL_SetSwapInterval(enable ? 1 : 0);
}

// Factory
std::unique_ptr<RenderContext> createRenderContext(const AppConfig& cfg) {
    // Currently only GL/GLES. Future: check cfg for Vulkan.
    (void)cfg;
    return std::unique_ptr<RenderContext>(new GLRenderContext());
}
