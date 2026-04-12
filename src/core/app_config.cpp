#include "core/app_config.h"
#include <SDL.h>

void AppConfig::applyPlatformDefaults() {
#ifdef __ANDROID__
    backend = RendererBackend::GLES;
    headless = false;
    fullscreen = true;
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
        width = dm.w;
        height = dm.h;
    }
#endif
    (void)this; // suppress unused-parameter on non-Android
}
