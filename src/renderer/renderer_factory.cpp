#include "renderer/renderer_factory.h"
#include "renderer/gl2_renderer.h"
#include "renderer/gl3_renderer.h"
#include "renderer/gl4_renderer.h"
#include "renderer/gles_renderer.h"
#include "renderer/gl_loader.h"
#include "platform/logger.h"
#include <cstdio>

// Try GL4 -> GL3 -> GL2 cascade, starting from requested level.
static std::unique_ptr<Renderer> createWithFallback(RendererBackend requested) {
    bool want_gl4 = (requested == RendererBackend::GL4);
    bool want_gl3 = (requested == RendererBackend::GL3 || want_gl4);

    if (want_gl4 && GLLoader::hasGL4() && GLLoader::hasGL3()) {
        Log::dbg("RendererFactory: selected GL4 (GL %d.%d, GL4+GL3 available)",
                 GLLoader::glMajor(), GLLoader::glMinor());
        return std::unique_ptr<Renderer>(new GL4Renderer());
    }
    if (want_gl4) {
        Log::warn("GL4 requested but not available (GL %d.%d)",
                GLLoader::glMajor(), GLLoader::glMinor());
    }

    if (want_gl3 && GLLoader::hasGL3()) {
        if (want_gl4)
            Log::warn("Falling back to GL3 renderer");
        Log::dbg("RendererFactory: selected GL3 (GL %d.%d)",
                 GLLoader::glMajor(), GLLoader::glMinor());
        return std::unique_ptr<Renderer>(new GL3Renderer());
    }
    if (want_gl3) {
        Log::warn("GL3 requested but not available (GL %d.%d)",
                GLLoader::glMajor(), GLLoader::glMinor());
        Log::warn("Falling back to GL2 renderer");
    }

    Log::dbg("RendererFactory: selected GL2 fallback (GL %d.%d)",
             GLLoader::glMajor(), GLLoader::glMinor());
    return std::unique_ptr<Renderer>(new GL2Renderer());
}

std::unique_ptr<Renderer> createRenderer(RendererBackend backend) {
    if (backend == RendererBackend::GLES ||
        (backend == RendererBackend::Auto && GLLoader::isGLES())) {
        Log::dbg("RendererFactory: selected GLES (GLES detected or requested)");
        return std::unique_ptr<Renderer>(new GLESRenderer());
    }

    if (backend == RendererBackend::GL2) {
        Log::dbg("RendererFactory: selected GL2 (explicitly requested)");
        return std::unique_ptr<Renderer>(new GL2Renderer());
    }

    if (backend == RendererBackend::GL3 || backend == RendererBackend::GL4) {
        return createWithFallback(backend);
    }

    // Auto-detect
    auto r = createWithFallback(RendererBackend::GL4);
    Log::info("Auto-detected GL %d.%d, using %s renderer",
            GLLoader::glMajor(), GLLoader::glMinor(), r->getRendererName());
    return r;
}
