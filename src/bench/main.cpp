// gpu_benchmark — GPU benchmark test suite, separate from gpu_demo
#include "core/app_config.h"
#include "renderer/render_context.h"
#include "renderer/renderer_factory.h"
#include "bench/bench.h"
#include "bench/bench_runner.h"
#include "bench/stress_runner.h"
#include "bench/preset.h"
#include "bench/preset_io.h"
#include "bench/results.h"
#include "tests/test_registry.h"
#include "bench/bench_ui.h"
#include "platform/hwinfo.h"
#include "platform/gpu_select.h"
#include "platform/logger.h"
#include "platform/timer.h"
#include "platform/compat.h"
#include "geometry/mesh_gen.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <vector>

// --- CLI helpers ---

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

static const NameVal kPresetVals[] = {
    {"light",   static_cast<int>(PresetIndex::Light)},
    {"medium",  static_cast<int>(PresetIndex::Medium)},
    {"heavy",   static_cast<int>(PresetIndex::Heavy)},
    {"ultra",   static_cast<int>(PresetIndex::Ultra)},
    {"extreme", static_cast<int>(PresetIndex::Extreme)}
};

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

static const NameVal kTimingVals[] = {
    {"sync", static_cast<int>(TimingMode::Sync)},
    {"gpu",  static_cast<int>(TimingMode::GPU)}
};

#define NVALS(a) (static_cast<int>(sizeof(a) / sizeof(a[0])))

// --- CLI parsing ---

static void printHelp() {
    fprintf(stderr,
        "gpu_benchmark [options]\n"
        "  --preset <light|medium|heavy|ultra|extreme>  Preset (default: medium)\n"
        "  --renderer <gl2|gl3|gl4|gles|auto>           Renderer (default: auto)\n"
        "  --config <path>                              Load config INI\n"
        "  --headless                                   No GUI, run tests, print results\n"
        "  --test <name,...>                             Run specific tests\n"
        "  --output <text|csv|json>                     Output format (default: text)\n"
        "  --output-file <path>                         Write results to file\n"
        "  --width <n>                                  Window width (default: 800)\n"
        "  --height <n>                                 Window height (default: 600)\n"
        "  --timing <sync|gpu>                          Timing mode (default: sync)\n"
        "  --render-res <WxH>                           Render resolution (default: native)\n"
        "  --stress <seconds>                           Stress test mode (headless)\n"
        "  --gpu <index>                                Select GPU by index\n"
        "  --debug                                      Enable debug logging\n"
        "  --list-gpus                                  List GPUs and exit\n"
        "  --help, -h                                   Show this help\n"
    );
}

static int parseArgs(int argc, char* argv[], AppConfig& cfg) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--preset") == 0) {
            if (++i >= argc) { fprintf(stderr, "--preset requires argument\n"); return 1; }
            if (matchEnumArg(argv[i], kPresetVals, NVALS(kPresetVals), &cfg.preset_index, "preset")) return 1;
        } else if (strcmp(a, "--renderer") == 0) {
            if (++i >= argc) { fprintf(stderr, "--renderer requires argument\n"); return 1; }
            if (matchEnumArg(argv[i], kRendererVals, NVALS(kRendererVals), &cfg.backend, "renderer")) return 1;
        } else if (strcmp(a, "--config") == 0) {
            if (++i >= argc) { fprintf(stderr, "--config requires argument\n"); return 1; }
            cfg.config_path = argv[i];
        } else if (strcmp(a, "--headless") == 0) {
            cfg.headless = true;
        } else if (strcmp(a, "--test") == 0) {
            if (++i >= argc) { fprintf(stderr, "--test requires argument\n"); return 1; }
            cfg.test_filter = argv[i];
        } else if (strcmp(a, "--output") == 0) {
            if (++i >= argc) { fprintf(stderr, "--output requires argument\n"); return 1; }
            if (matchEnumArg(argv[i], kOutputVals, NVALS(kOutputVals), &cfg.output_format, "output")) return 1;
        } else if (strcmp(a, "--output-file") == 0 || strcmp(a, "-o") == 0) {
            if (++i >= argc) { fprintf(stderr, "%s requires argument\n", a); return 1; }
            cfg.output_file = argv[i];
        } else if (strcmp(a, "--width") == 0) {
            if (++i >= argc) { fprintf(stderr, "--width requires argument\n"); return 1; }
            cfg.width = parseIntArg(argv[i], 800);
        } else if (strcmp(a, "--height") == 0) {
            if (++i >= argc) { fprintf(stderr, "--height requires argument\n"); return 1; }
            cfg.height = parseIntArg(argv[i], 600);
        } else if (strcmp(a, "--timing") == 0) {
            if (++i >= argc) { fprintf(stderr, "--timing requires argument\n"); return 1; }
            if (matchEnumArg(argv[i], kTimingVals, NVALS(kTimingVals), &cfg.timing_mode, "timing")) return 1;
        } else if (strcmp(a, "--render-res") == 0) {
            if (++i >= argc) { fprintf(stderr, "--render-res requires argument\n"); return 1; }
            int rw = 0, rh = 0;
            if (sscanf(argv[i], "%dx%d", &rw, &rh) == 2 && rw >= 64 && rh >= 64) {
                cfg.render_width = rw; cfg.render_height = rh;
            } else {
                fprintf(stderr, "Invalid resolution: %s\n", argv[i]); return 1;
            }
        } else if (strcmp(a, "--stress") == 0) {
            if (++i >= argc) { fprintf(stderr, "--stress requires argument\n"); return 1; }
            cfg.stress_duration_sec = parseIntArg(argv[i], 60);
            cfg.headless = true;
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

#undef NVALS

// --- Resolution table ---

static const ResolutionOption RESOLUTIONS[] = {
    {  640,  480, "640x480"   },
    {  800,  600, "800x600"   },
    { 1024,  768, "1024x768"  },
    { 1280,  720, "1280x720"  },
    { 1600,  900, "1600x900"  },
    { 1920, 1080, "1920x1080" },
    { 2560, 1440, "2560x1440" },
    { 3440, 1440, "3440x1440" },
    { 3840, 2160, "3840x2160" },
};
static const int NUM_RESOLUTIONS = static_cast<int>(sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]));
static constexpr int RES_NATIVE = -1;

// --- Preview scene constants ---
static constexpr float PREVIEW_FOV       = 60.0f;
static constexpr float PREVIEW_NEAR_CLIP = 0.1f;
static constexpr float PREVIEW_FAR_CLIP  = 200.0f;
static constexpr float PREVIEW_CAM_DIST  = 22.0f;
static constexpr float PREVIEW_CAM_Y     = 14.0f;
static constexpr int   PREVIEW_TEX_SIZE  = 256;
static const float PREVIEW_ROT_SPEED = 0.3f;

// --- BenchApp ---

struct BenchApp;

struct BenchCB : public BenchCallbacks {
    BenchApp* app;
    bool onFrame(RenderTargetHandle rt) override;
    bool onPoll() override;
};

struct BenchApp {
    AppConfig config;
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<Renderer> renderer;
    HWInfo hw_info;
    BenchRunner bench_runner;
    BenchUI bench_ui;
    BenchCB callbacks;
    bool running;
    int window_w, window_h;
    int render_w, render_h;
    int selected_resolution;
    BenchPreset current_preset;
    int selected_preset_index;
    int suggested_preset;
    GPUTier gpu_tier;
    uint32_t cached_caps;
    std::string validation_error;
    bool test_enabled[NUM_TESTS];
    Timer frame_timer;

    // Preview scene
    MeshHandle preview_terrain, preview_sphere;
    TextureHandle preview_tex;
    float preview_angle;
    bool preview_ready;

    // Pending UI action
    enum class PendingAction { None, RunSelected, RunAll };
    PendingAction pending_action;

    BenchApp()
        : running(true), window_w(800), window_h(600)
        , render_w(800), render_h(600), selected_resolution(RES_NATIVE)
        , selected_preset_index(static_cast<int>(PresetIndex::Medium))
        , suggested_preset(static_cast<int>(PresetIndex::Medium))
        , gpu_tier(GPUTier::Mid), cached_caps(0)
        , preview_terrain(INVALID_MESH), preview_sphere(INVALID_MESH)
        , preview_tex(INVALID_TEXTURE), preview_angle(0.0f), preview_ready(false)
        , pending_action(PendingAction::None) {
        for (int i = 0; i < NUM_TESTS; i++) test_enabled[i] = true;
        current_preset = getPreset(static_cast<int>(PresetIndex::Medium));
        callbacks.app = this;
    }

    bool init(const AppConfig& cfg) {
        config = cfg;
        Log::init("gpu_benchmark.log");
        Log::setLevel(cfg.debug ? Log::Level::Debug : Log::Level::Info);
        window_w = cfg.width;
        window_h = cfg.height;

        // Load config file if specified
        if (!cfg.config_path.empty()) {
            if (loadConfig(cfg.config_path.c_str(), current_preset))
                selected_preset_index = -1;
            else
                LOG_WRN("Could not load config '%s'", cfg.config_path.c_str());
        } else {
            applyPreset(cfg.preset_index);
        }

        ctx = createRenderContext(cfg);
        if (!ctx || !ctx->init(cfg)) { LOG_ERR("Render context init failed"); return false; }
        SDL_GL_GetDrawableSize(ctx->window(), &window_w, &window_h);

        renderer = createRenderer(cfg.backend);
        if (!renderer || !renderer->init(window_w, window_h)) { LOG_ERR("Renderer init failed"); return false; }

        hw_info = HWInfo::detect();
        cached_caps = getAvailableCaps(*renderer);

        if (config.timing_mode == TimingMode::GPU && !renderer->hasTimerQueries()) {
            config.timing_mode = TimingMode::Sync;
            LOG_WRN("GPU timer queries unavailable, falling back to sync");
        }

        gpu_tier = bench_runner.runQuickProbe(renderer.get(), ctx.get(), render_w, render_h, nullptr);
        suggested_preset = tierToPresetIndex(gpu_tier);
        validateCurrentPreset();

        // Render resolution
        if (cfg.render_width > 0 && cfg.render_height > 0) {
            selected_resolution = RES_NATIVE;
            for (int i = 0; i < NUM_RESOLUTIONS; i++) {
                if (RESOLUTIONS[i].w == cfg.render_width && RESOLUTIONS[i].h == cfg.render_height) {
                    selected_resolution = i; break;
                }
            }
            render_w = cfg.render_width;
            render_h = cfg.render_height;
        } else {
            render_w = window_w;
            render_h = window_h;
        }

        // Test filter
        if (cfg.test_filter != "all") {
            for (int i = 0; i < NUM_TESTS; i++) test_enabled[i] = false;
            std::string filter = cfg.test_filter;
            size_t pos = 0;
            while (pos < filter.size()) {
                size_t comma = filter.find(',', pos);
                if (comma == std::string::npos) comma = filter.size();
                std::string name = filter.substr(pos, comma - pos);
                while (!name.empty() && name[0] == ' ') name.erase(0, 1);
                while (!name.empty() && name.back() == ' ') name.pop_back();
                for (int i = 0; i < NUM_TESTS; i++) {
                    if (strcasecmp(name.c_str(), g_tests[i].display_name) == 0 ||
                        strcasecmp(name.c_str(), g_tests[i].cli_name) == 0)
                        test_enabled[i] = true;
                }
                pos = comma + 1;
            }
        }

        if (!cfg.headless) setupPreviewScene();
        frame_timer.reset();
        return true;
    }

    void shutdown() {
        if (!config.headless) cleanupPreviewScene();
        if (renderer) { renderer->shutdown(); renderer.reset(); }
        if (ctx) { ctx->shutdown(); ctx.reset(); }
        Log::shutdown();
    }

    void processEvents() {
        SDL_Event e;
        while (ctx->pollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_GL_GetDrawableSize(ctx->window(), &window_w, &window_h);
                if (selected_resolution == RES_NATIVE) { render_w = window_w; render_h = window_h; }
            }
        }
    }

    void applyPreset(int index) {
        if (index >= 0 && index < static_cast<int>(PresetIndex::Count)) {
            current_preset = getPreset(index);
            selected_preset_index = index;
        }
    }

    void validateCurrentPreset() {
        if (renderer) {
            PresetValidation v = validatePreset(current_preset, renderer->getCaps());
            validation_error = v.ok ? "" : v.reason;
        }
    }

    void updateRenderResolution() {
        if (selected_resolution == RES_NATIVE) { render_w = window_w; render_h = window_h; }
        else if (selected_resolution >= 0 && selected_resolution < NUM_RESOLUTIONS) {
            render_w = RESOLUTIONS[selected_resolution].w;
            render_h = RESOLUTIONS[selected_resolution].h;
        }
    }

    void setupPreviewScene() {
        preview_terrain = renderer->createMesh(MeshGen::terrain(30.0f, 64));
        preview_sphere  = renderer->createMesh(MeshGen::sphere(24, 16));
        int sz = PREVIEW_TEX_SIZE;
        std::vector<unsigned char> pixels(static_cast<size_t>(sz) * static_cast<size_t>(sz) * 3);
        for (int y = 0; y < sz; y++)
            for (int x = 0; x < sz; x++) {
                bool white = ((x / 16 + y / 16) % 2) != 0;
                unsigned char c = static_cast<unsigned char>(white ? 200 : 80);
                size_t idx = static_cast<size_t>((y * sz + x) * 3);
                pixels[idx] = c; pixels[idx+1] = c; pixels[idx+2] = c;
            }
        preview_tex = renderer->createTexture(sz, sz, 3, pixels.data());
        preview_ready = true;
    }

    void cleanupPreviewScene() {
        if (!preview_ready) return;
        renderer->destroyMesh(preview_terrain);
        renderer->destroyMesh(preview_sphere);
        renderer->destroyTexture(preview_tex);
        preview_ready = false;
    }

    void renderPreviewScene(float dt) {
        if (!preview_ready) return;
        int panel_h = static_cast<int>(static_cast<float>(window_h) * 0.55f);
        int view_h = window_h - panel_h;
        if (view_h <= 0) return;

        renderer->setScissor(true, 0, panel_h, window_w, view_h);
        renderer->setViewport(0, panel_h, window_w, view_h);
        renderer->clear(0.08f, 0.08f, 0.10f, 1.0f);
        renderer->setDepthTest(true);
        renderer->setBlending(false);
        renderer->useShader(Renderer::ShaderType::Scene3D);

        float aspect = static_cast<float>(window_w) / static_cast<float>(view_h);
        renderer->setProjection(Mat4::perspective(PREVIEW_FOV, aspect, PREVIEW_NEAR_CLIP, PREVIEW_FAR_CLIP));
        float cx = cosf(preview_angle) * PREVIEW_CAM_DIST;
        float cz = sinf(preview_angle) * PREVIEW_CAM_DIST;
        renderer->setView(Mat4::lookAt(Vec3(cx, PREVIEW_CAM_Y, cz), Vec3(0, 2, 0), Vec3(0, 1, 0)));
        renderer->setLightDir(0.5f, 0.8f, 0.3f);

        renderer->setModel(Mat4());
        renderer->setUseTexture(true);
        renderer->setColor(1.0f, 1.0f, 1.0f, 1.0f);
        renderer->bindTexture(preview_tex);
        renderer->drawMesh(preview_terrain);

        renderer->setUseTexture(false);
        float colors[][3] = {
            {0.4f, 0.7f, 1.0f}, {1.0f, 0.5f, 0.3f}, {0.3f, 1.0f, 0.5f},
            {1.0f, 0.8f, 0.2f}, {0.8f, 0.3f, 1.0f},
        };
        for (int i = 0; i < 5; i++) {
            float a = static_cast<float>(i) / 5.0f * 6.28318f + preview_angle * 0.5f;
            renderer->setColor(colors[i][0], colors[i][1], colors[i][2], 1.0f);
            renderer->setModel(Mat4::translate(cosf(a) * 8.0f, 4.0f + sinf(a * 2.0f) * 1.5f, sinf(a) * 8.0f)
                               * Mat4::scale(1.2f, 1.2f, 1.2f));
            renderer->drawMesh(preview_sphere);
        }
        renderer->setScissor(false);
        preview_angle += PREVIEW_ROT_SPEED * dt;
    }

    void renderUI() {
        UIView view;
        view.hw_info = &hw_info;
        view.gpu_renderer = renderer->getGPURenderer();
        view.gl_version = renderer->getGLVersion();
        view.renderer_name = renderer->getRendererName();
        view.caps = &renderer->getCaps();
        view.available_caps = cached_caps;
        view.supports_render_targets = renderer->supportsRenderTargets();
        view.gpu_tier = gpu_tier;
        view.window_w = window_w; view.window_h = window_h;
        view.render_w = render_w; view.render_h = render_h;
        view.suggested_preset = suggested_preset;
        view.bench_active = bench_runner.isActive();
        view.bench_status = &bench_runner.status();
        view.bench_progress = bench_runner.progress();
        view.results = &bench_runner.results();
        view.composite = &bench_runner.compositeScore();
        view.bottleneck = &bench_runner.bottleneckInfo();
        view.resolutions = RESOLUTIONS;
        view.num_resolutions = NUM_RESOLUTIONS;
        view.validation_error = &validation_error;

        UIState state;
        state.current_preset = &current_preset;
        state.selected_preset_index = &selected_preset_index;
        state.test_enabled = test_enabled;
        state.selected_resolution = &selected_resolution;
        state.validation_error = &validation_error;

        UIAction action = bench_ui.render(ctx.get(), view, state);
        switch (action.type) {
            case UIAction::ApplyPreset: applyPreset(action.preset_index); validateCurrentPreset(); break;
            case UIAction::ResolutionChanged: updateRenderResolution(); break;
            case UIAction::CustomParamsChanged: selected_preset_index = -1; current_preset.name = "Custom"; validateCurrentPreset(); break;
            case UIAction::RunSelected: pending_action = PendingAction::RunSelected; break;
            case UIAction::RunAll: pending_action = PendingAction::RunAll; break;
            case UIAction::Clear: bench_runner.clearResults(); break;
            case UIAction::Export: exportResults(); break;
            default: break;
        }
    }

    void runSelectedTests() {
        BenchConfig bcfg;
        bcfg.warmup_frames = current_preset.warmup_frames;
        bcfg.measure_frames = current_preset.measure_frames;
        bcfg.min_duration_sec = current_preset.min_duration_sec;
        bcfg.timing_mode = config.timing_mode;
        bcfg.headless = config.headless;
        bcfg.render_w = render_w; bcfg.render_h = render_h;
        bcfg.window_w = window_w; bcfg.window_h = window_h;
        bench_runner.runSelected(renderer.get(), ctx.get(), current_preset, bcfg,
                                  test_enabled, cached_caps, &callbacks);
    }

    void exportResults() {
        const auto& results = bench_runner.results();
        if (results.empty()) return;
        ExportConfig ecfg;
        ecfg.width = render_w; ecfg.height = render_h;
        ecfg.warmup_frames = current_preset.warmup_frames;
        ecfg.measure_frames = current_preset.measure_frames;
        ecfg.vsync = false;
        ecfg.gpu_tier = gpuTierName(gpu_tier);
        const char* path = config.output_file.empty() ? nullptr : config.output_file.c_str();
        if (!writeBenchResults(config.output_format, path, results, hw_info,
                               renderer->getCaps(), cached_caps,
                               renderer->getGPURenderer(), renderer->getGLVersion(),
                               renderer->getRendererName(), current_preset.name, ecfg,
                               &bench_runner.compositeScore(), &bench_runner.bottleneckInfo())) {
            LOG_ERR("Could not open output file: %s", config.output_file.c_str());
        }
    }

    void runInteractive() {
        frame_timer.reset();
        while (running) {
            double dt = frame_timer.elapsed_sec();
            frame_timer.reset();

            processEvents();
            if (!bench_runner.isActive()) {
                renderer->clear(0.12f, 0.12f, 0.15f, 1.0f);
                renderPreviewScene(static_cast<float>(dt));
                renderer->unbindState();
            }
            renderer->setViewport(0, 0, window_w, window_h);
            renderUI();
            ctx->swapBuffers();

            if (pending_action != PendingAction::None) {
                PendingAction action = pending_action;
                pending_action = PendingAction::None;
                if (!validation_error.empty()) {
                    LOG_WRN("Cannot run: preset validation failed: %s", validation_error.c_str());
                } else {
                    if (action == PendingAction::RunAll)
                        for (int i = 0; i < NUM_TESTS; i++) test_enabled[i] = true;
                    runSelectedTests();
                }
            }
        }
    }

    void runHeadless() {
        if (!validation_error.empty()) {
            LOG_ERR("Preset validation failed: %s", validation_error.c_str());
            return;
        }
        if (config.stress_duration_sec > 0) {
            StressRunner stress;
            stress.run(renderer.get(), ctx.get(),
                       current_preset.shader_alu.iterations,
                       render_w, render_h,
                       config.stress_duration_sec, &callbacks);
        } else {
            runSelectedTests();
        }
        exportResults();
    }
};

// --- BenchCallbacks implementation ---

bool BenchCB::onFrame(RenderTargetHandle rt) {
    app->processEvents();
    if (!app->config.headless) {
        if (rt != INVALID_RENDER_TARGET) {
            app->renderer->bindRenderTarget(INVALID_RENDER_TARGET);
            app->renderer->setViewport(0, 0, app->window_w, app->window_h);
            app->renderer->clear(0.12f, 0.12f, 0.15f, 1.0f);
            int panel_h = static_cast<int>(static_cast<float>(app->window_h) * 0.55f);
            int view_h = app->window_h - panel_h;
            if (view_h > 0)
                app->renderer->blitToScreen(rt, 0, panel_h, app->window_w, view_h);
        }
        app->renderer->setViewport(0, 0, app->window_w, app->window_h);
        app->renderer->setDepthTest(false);
        app->renderer->setBlending(false);
        app->renderer->unbindState();
        app->renderUI();
    }
    app->ctx->swapBuffers();
    return app->running;
}

bool BenchCB::onPoll() {
    app->processEvents();
    return app->running;
}

// --- Entry point ---

int main(int argc, char* argv[]) {
    AppConfig cfg;
    int rc = parseArgs(argc, argv, cfg);
    if (rc != 0) return (rc < 0) ? 0 : rc;

    if (cfg.gpu_index >= 0)
        selectGPUAndReexec(cfg.gpu_index, argc, argv);

    BenchApp app;
    if (!app.init(cfg)) {
        fprintf(stderr, "Failed to initialize gpu_benchmark\n");
        return 1;
    }

    LOG_DBG("CLI: mode=%s, %dx%d, preset=%s",
            cfg.headless ? (cfg.stress_duration_sec > 0 ? "stress" : "benchmark") : "interactive",
            cfg.width, cfg.height, app.current_preset.name);

    if (cfg.headless)
        app.runHeadless();
    else
        app.runInteractive();

    app.shutdown();
    return 0;
}
