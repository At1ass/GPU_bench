#include "render_context.h"
#include "app.h"
#include "logger.h"
#include <cstdio>

RenderContext::RenderContext() : window_(0), headless_(false) {}
RenderContext::~RenderContext() {}

bool RenderContext::initSDL(const AppConfig& cfg) {
    headless_ = cfg.headless;

#ifndef _WIN32
    if (headless_ && !getenv("SDL_VIDEODRIVER")) {
        SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
    }
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        Log::err("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    Uint32 win_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
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
        Log::err("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void RenderContext::shutdownSDL() {
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = 0;
    }
    SDL_Quit();
}

bool RenderContext::pollEvent(SDL_Event* e) {
    if (!SDL_PollEvent(e)) return false;
    onEvent(e);
    return true;
}
