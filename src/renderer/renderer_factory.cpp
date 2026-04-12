#include "renderer/renderer_factory.h"
#include "renderer/backend/gl2_renderer.h"
#ifndef CB_GLES_NATIVE
#include "renderer/backend/gl3_renderer.h"
#include "renderer/backend/gl4_renderer.h"
#endif
#include "renderer/backend/gles_renderer.h"
#include "renderer/backend/gl/gl_loader.h"
#include "platform/logger.h"
#include <cstdio>

#ifndef CB_GLES_NATIVE
// Try GL4 -> GL3 -> GL2 cascade, starting from requested level.
static std::unique_ptr<Renderer> createWithFallback(RendererBackend requested) {
    bool want_gl4 = (requested == RendererBackend::GL4);
    bool want_gl3 = (requested == RendererBackend::GL3 || want_gl4);

    if (want_gl4 && GLLoader::hasGL4() && GLLoader::hasGL3()) {
        LOG_DBG("RendererFactory: selected GL4 (GL %d.%d, GL4+GL3 available)",
                 GLLoader::glMajor(), GLLoader::glMinor());
        return std::unique_ptr<Renderer>(new GL4Renderer());
    }
    if (want_gl4) {
        LOG_WRN("GL4 requested but not available (GL %d.%d)",
                GLLoader::glMajor(), GLLoader::glMinor());
    }

    if (want_gl3 && GLLoader::hasGL3()) {
        if (want_gl4)
            LOG_WRN("Falling back to GL3 renderer");
        LOG_DBG("RendererFactory: selected GL3 (GL %d.%d)",
                 GLLoader::glMajor(), GLLoader::glMinor());
        return std::unique_ptr<Renderer>(new GL3Renderer());
    }
    if (want_gl3) {
        LOG_WRN("GL3 requested but not available (GL %d.%d)",
                GLLoader::glMajor(), GLLoader::glMinor());
        LOG_WRN("Falling back to GL2 renderer");
    }

    LOG_DBG("RendererFactory: selected GL2 fallback (GL %d.%d)",
             GLLoader::glMajor(), GLLoader::glMinor());
    return std::unique_ptr<Renderer>(new GL2Renderer());
}
#endif // CB_GLES_NATIVE

std::unique_ptr<Renderer> createRenderer(RendererBackend backend) {
    if (backend == RendererBackend::GLES ||
        (backend == RendererBackend::Auto && GLLoader::isGLES())) {
        LOG_DBG("RendererFactory: selected GLES (GLES detected or requested)");
        return std::unique_ptr<Renderer>(new GLESRenderer());
    }

#ifdef CB_GLES_NATIVE
    // On Android GLES-only: always use GLESRenderer
    LOG_DBG("RendererFactory: GLES-only platform, using GLES renderer");
    return std::unique_ptr<Renderer>(new GLESRenderer());
#else
    if (backend == RendererBackend::GL2) {
        LOG_DBG("RendererFactory: selected GL2 (explicitly requested)");
        return std::unique_ptr<Renderer>(new GL2Renderer());
    }

    if (backend == RendererBackend::GL3 || backend == RendererBackend::GL4) {
        return createWithFallback(backend);
    }

    // Auto-detect
    auto r = createWithFallback(RendererBackend::GL4);
    LOG_INF("Auto-detected GL %d.%d, using %s renderer",
            GLLoader::glMajor(), GLLoader::glMinor(), r->getRendererName());
    return r;
#endif
}
