// gpu_demo — visual demo benchmark (3DMark-style), separate from gpu_benchmark
#include "core/app_config.h"
#include "renderer/render_context.h"
#include "renderer/renderer_factory.h"
#include "demo/demo_runner.h"
#include "demo/demo_ui.h"
#include "demo/demo_export.h"
#include "core/poll_callback.h"
#include "platform/hwinfo.h"
#include "platform/gpu_select.h"
#include "platform/logger.h"
#include "platform/timer.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>

// --- CLI helpers (shared pattern from main.cpp) ---

static int parseIntArg(const char* s, int fallback = 0) {
    char* end = nullptr;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        fprintf(stderr, "Warning: invalid integer '%s', using %d\n", s, fallback);
        return fallback;
    }
    return static_cast<int>(v);
}

struct NameVal { const char* name; int value; };

template<typename T>
static int matchEnumArg(const char* arg, const NameVal* vals, int count,
                        T* out, const char* label) {
    for (int i = 0; i < count; i++) {
        if (strcmp(arg, vals[i].name) == 0) {
            *out = static_cast<T>(vals[i].value);
            return 0;
        }
    }
    fprintf(stderr, "Unknown %s: %s\n", label, arg);
    return 1;
}

static const NameVal kRendererVals[] = {
    {"gl2",  static_cast<int>(RendererBackend::GL2)},
    {"gl3",  static_cast<int>(RendererBackend::GL3)},
    {"gl4",  static_cast<int>(RendererBackend::GL4)},
    {"gles", static_cast<int>(RendererBackend::GLES)},
    {"auto", static_cast<int>(RendererBackend::Auto)}
};

static const NameVal kOutputVals[] = {
    {"text", static_cast<int>(OutputFormat::Text)},
    {"csv",  static_cast<int>(OutputFormat::CSV)},
    {"json", static_cast<int>(OutputFormat::JSON)}
};

// --- CLI parsing ---

static void printHelp() {
    fprintf(stderr,
        "gpu_demo [options]\n"
        "  --tier <1-4>                        Run specific tier only\n"
        "  --duration <seconds>                Duration per tier (default: 15)\n"
        "  --headless                          No GUI, run demo, print results\n"
        "  --renderer <gl2|gl3|gl4|gles|auto>  Renderer (default: auto)\n"
        "  --output <text|csv|json>            Output format (default: text)\n"
        "  --output-file <path>                Write results to file\n"
        "  --width <n>                         Window width (default: 800)\n"
        "  --height <n>                        Window height (default: 600)\n"
        "  --render-res <WxH>                  Render resolution (default: native)\n"
        "  --gpu <index>                       Select GPU by index\n"
        "  --debug                             Enable debug logging\n"
        "  --list-gpus                         List available GPUs and exit\n"
        "  --help, -h                          Show this help\n"
    );
}

static int parseArgs(int argc, char* argv[], AppConfig& cfg) {
    cfg.demo_mode = true;  // always demo mode
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--tier") == 0) {
            if (++i >= argc) { fprintf(stderr, "--tier requires argument\n"); return 1; }
            cfg.demo_tier = parseIntArg(argv[i], 0);
            if (cfg.demo_tier < 1 || cfg.demo_tier > 4) {
                fprintf(stderr, "Invalid tier: %d (must be 1-4)\n", cfg.demo_tier); return 1;
            }
        } else if (strcmp(a, "--duration") == 0) {
            if (++i >= argc) { fprintf(stderr, "--duration requires argument\n"); return 1; }
            cfg.demo_duration = parseIntArg(argv[i], 15);
            if (cfg.demo_duration < 1) cfg.demo_duration = 1;
        } else if (strcmp(a, "--headless") == 0) {
            cfg.headless = true;
        } else if (strcmp(a, "--renderer") == 0) {
            if (++i >= argc) { fprintf(stderr, "--renderer requires argument\n"); return 1; }
            if (matchEnumArg(argv[i], kRendererVals, 5, &cfg.backend, "renderer")) return 1;
        } else if (strcmp(a, "--output") == 0) {
            if (++i >= argc) { fprintf(stderr, "--output requires argument\n"); return 1; }
            if (matchEnumArg(argv[i], kOutputVals, 3, &cfg.output_format, "output")) return 1;
        } else if (strcmp(a, "--output-file") == 0 || strcmp(a, "-o") == 0) {
            if (++i >= argc) { fprintf(stderr, "%s requires argument\n", a); return 1; }
            cfg.output_file = argv[i];
        } else if (strcmp(a, "--width") == 0) {
            if (++i >= argc) { fprintf(stderr, "--width requires argument\n"); return 1; }
            cfg.width = parseIntArg(argv[i], 800);
        } else if (strcmp(a, "--height") == 0) {
            if (++i >= argc) { fprintf(stderr, "--height requires argument\n"); return 1; }
            cfg.height = parseIntArg(argv[i], 600);
        } else if (strcmp(a, "--render-res") == 0) {
            if (++i >= argc) { fprintf(stderr, "--render-res requires argument\n"); return 1; }
            int rw = 0, rh = 0;
            if (sscanf(argv[i], "%dx%d", &rw, &rh) == 2 && rw >= 64 && rh >= 64) {
                cfg.render_width = rw; cfg.render_height = rh;
            } else {
                fprintf(stderr, "Invalid resolution: %s\n", argv[i]); return 1;
            }
        } else if (strcmp(a, "--gpu") == 0) {
            if (++i >= argc) { fprintf(stderr, "--gpu requires argument\n"); return 1; }
            cfg.gpu_index = parseIntArg(argv[i], -1);
        } else if (strcmp(a, "--debug") == 0) {
            cfg.debug = true;
        } else if (strcmp(a, "--list-gpus") == 0) {
            auto gpus = enumerateGPUs();
            printGPUList(gpus);
            return -1;
        } else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            printHelp();
            return -1;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", a);
            printHelp();
            return 1;
        }
    }
    if (cfg.width < 64) cfg.width = 64;
    if (cfg.height < 64) cfg.height = 64;
    return 0;
}

// --- DemoApp ---

struct DemoApp {
    AppConfig config;
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<Renderer> renderer;
    HWInfo hw_info;
    DemoUI demo_ui;
    DemoResults results;
    bool running;
    int window_w, window_h;
    int render_w, render_h;

    DemoApp() : running(true), window_w(800), window_h(600), render_w(800), render_h(600) {}

    bool init(const AppConfig& cfg) {
        config = cfg;
        Log::init("gpu_demo.log");
        Log::setLevel(cfg.debug ? Log::Level::Debug : Log::Level::Info);

        window_w = cfg.width;
        window_h = cfg.height;

        ctx = createRenderContext(cfg);
        if (!ctx || !ctx->init(cfg)) {
            LOG_ERR("Failed to create render context");
            return false;
        }

        SDL_GL_GetDrawableSize(ctx->window(), &window_w, &window_h);
        LOG_DBG("DemoApp: drawable size %dx%d", window_w, window_h);

        renderer = createRenderer(cfg.backend);
        if (!renderer || !renderer->init(window_w, window_h)) {
            LOG_ERR("Renderer init failed");
            return false;
        }

        hw_info = HWInfo::detect();
        LOG_DBG("DemoApp: CPU=%s, GPU=%s", hw_info.cpu_name.c_str(), renderer->getGPURenderer());

        // Render resolution
        if (cfg.render_width > 0 && cfg.render_height > 0) {
            render_w = cfg.render_width;
            render_h = cfg.render_height;
        } else {
            render_w = window_w;
            render_h = window_h;
        }

        return true;
    }

    void processEvents() {
        SDL_Event e;
        while (ctx->pollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED)
                SDL_GL_GetDrawableSize(ctx->window(), &window_w, &window_h);
        }
    }

    void run() {
        DemoConfig dcfg;
        dcfg.tier_override = config.demo_tier;
        dcfg.duration_per_tier = config.demo_duration;

        // PollCallback for headless event pumping
        struct PollCB : public PollCallback {
            DemoApp* app;
            bool onPoll() override { app->processEvents(); return app->running; }
        };
        PollCB poll_cb;
        poll_cb.app = this;

        // DemoCallbacks for interactive overlay
        struct DemoCB : public DemoCallbacks {
            DemoApp* app;
            bool onDemoFrame(DemoTier tier, int tier_idx, int total,
                             float progress, double fps, double frame_ms,
                             const std::vector<double>& history,
                             const TechniqueInfo* tech_info) override {
                if (app->config.headless) { app->processEvents(); return app->running; }
                app->renderer->setViewport(0, 0, app->window_w, app->window_h);
                app->renderer->setDepthTest(false);
                app->renderer->unbindState();
                app->demo_ui.drawOverlay(app->ctx.get(), app->renderer->getGPURenderer(),
                                          tier, tier_idx, total, progress, fps, frame_ms,
                                          history, tech_info);
                app->processEvents();
                return app->running;
            }
        };
        DemoCB demo_cb;
        demo_cb.app = this;

        DemoRunner runner;
        results = runner.run(renderer.get(), ctx.get(), dcfg,
                             render_w, render_h, window_w, window_h,
                             &poll_cb,
                             config.headless ? nullptr : &demo_cb);

        // Show results screen (interactive mode)
        if (!config.headless && running) {
            Timer results_timer;
            while (running && results_timer.elapsed_sec() < 30.0) {
                processEvents();
                renderer->setViewport(0, 0, window_w, window_h);
                renderer->clear(0.1f, 0.1f, 0.12f, 1.0f);
                renderer->setDepthTest(false);
                renderer->unbindState();
                demo_ui.drawResults(ctx.get(), results);
                ctx->swapBuffers();
            }
        }

        exportResults();
    }

    void exportResults() {
        const char* path = config.output_file.empty() ? nullptr : config.output_file.c_str();
        if (!writeDemoResults(config.output_format, path, results, hw_info,
                              renderer->getGPURenderer(), renderer->getGLVersion(),
                              renderer->getRendererName())) {
            LOG_ERR("Could not open output file: %s", config.output_file.c_str());
        }
    }

    void shutdown() {
        if (renderer) { renderer->shutdown(); renderer.reset(); }
        if (ctx) { ctx->shutdown(); ctx.reset(); }
        Log::shutdown();
    }
};

// --- Entry point ---

int main(int argc, char* argv[]) {
    AppConfig cfg;
    int rc = parseArgs(argc, argv, cfg);
    if (rc != 0) return (rc < 0) ? 0 : rc;

    if (cfg.gpu_index >= 0)
        selectGPUAndReexec(cfg.gpu_index, argc, argv);

    DemoApp app;
    if (!app.init(cfg)) {
        fprintf(stderr, "Failed to initialize gpu_demo\n");
        return 1;
    }

    LOG_DBG("CLI: mode=demo, %dx%d, tier=%d, duration=%d",
            cfg.width, cfg.height, cfg.demo_tier, cfg.demo_duration);

    app.run();
    app.shutdown();
    return 0;
}
