#include "renderer/context/render_context.h"
#include "core/app_config.h"
#include "platform/logger.h"
#include <cstdio>

RenderContext::RenderContext() : window_(nullptr), headless_(false) {}
RenderContext::~RenderContext() {}

void RenderContext::getDrawableSize(int* w, int* h) const {
    if (window_) {
        SDL_GL_GetDrawableSize(window_, w, h);
    } else {
        if (w) *w = 0;
        if (h) *h = 0;
    }
}

bool RenderContext::initSDL(const AppConfig& cfg) {
    headless_ = cfg.headless;

#ifndef _WIN32
    if (headless_ && !cfg.fullscreen && !getenv("SDL_VIDEODRIVER")) {
        SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
    }
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        LOG_ERR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    Uint32 win_flags = SDL_WINDOW_OPENGL;
    if (cfg.fullscreen)
        win_flags |= SDL_WINDOW_FULLSCREEN;
    else
        win_flags |= SDL_WINDOW_RESIZABLE;
    if (headless_) {
        win_flags |= SDL_WINDOW_HIDDEN;
    }

    window_ = SDL_CreateWindow(
        "GPU_benchmark v0.4.0",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg.width, cfg.height,
        win_flags
    );
    if (!window_) {
        LOG_ERR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void RenderContext::shutdownSDL() {
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool RenderContext::pollEvent(SDL_Event* e) {
    if (!SDL_PollEvent(e)) return false;
    onEvent(e);
    return true;
}
