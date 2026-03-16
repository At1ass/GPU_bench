#include "app.h"
#include "geometry/mesh_gen.h"
#include "renderer/renderer_factory.h"
#include "bench/preset_io.h"
#include "bench/results.h"
#include "bench/stress_runner.h"
#include "demo/demo_export.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include <cstdio>
#include <cmath>
#include <cstring>

static const float PREVIEW_ROT_SPEED = 0.3f;

// Preview scene parameters
static constexpr float PREVIEW_FOV       = 60.0f;
static constexpr float PREVIEW_NEAR_CLIP = 0.1f;
static constexpr float PREVIEW_FAR_CLIP  = 200.0f;
static constexpr float PREVIEW_CAM_DIST  = 22.0f;
static constexpr float PREVIEW_CAM_Y     = 14.0f;
static constexpr int   PREVIEW_TEX_SIZE  = 256;


const ResolutionOption App::RESOLUTIONS[] = {
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

const int App::NUM_RESOLUTIONS = sizeof(App::RESOLUTIONS) / sizeof(App::RESOLUTIONS[0]);

App::App()
    : running_(false),
      initialized_(false), window_w_(AppConfig().width), window_h_(AppConfig().height),
      render_w_(AppConfig().width), render_h_(AppConfig().height), selected_resolution_(ResNative),
      last_frame_time_(0.0),
      selected_preset_index_(static_cast<int>(PresetIndex::Medium)), suggested_preset_(static_cast<int>(PresetIndex::Medium)),
      pending_action_(PendingAction::None),
      preview_terrain_(INVALID_MESH), preview_sphere_(INVALID_MESH),
      preview_tex_(INVALID_TEXTURE), preview_angle_(0.0f), preview_ready_(false),
      gpu_tier_(GPUTier::Mid)
{
    for (int i = 0; i < NUM_TESTS; i++) test_enabled_[i] = true;
    current_preset_ = getPreset(static_cast<int>(PresetIndex::Medium));
}

// ~App: default. shutdown() is called explicitly from main().


void App::updateRenderResolution() {
    if (selected_resolution_ == ResNative) {
        render_w_ = window_w_;
        render_h_ = window_h_;
    } else if (selected_resolution_ >= 0 && selected_resolution_ < NUM_RESOLUTIONS) {
        render_w_ = RESOLUTIONS[selected_resolution_].w;
        render_h_ = RESOLUTIONS[selected_resolution_].h;
    }
}

bool App::init(const AppConfig& cfg) {
    if (initialized_) return true;
    config_ = cfg;
    Log::init("gpu_benchmark.log");
    Log::setLevel(cfg.debug ? Log::Level::Debug : Log::Level::Info);

    window_w_ = cfg.width;
    window_h_ = cfg.height;

    // Load config file if specified
    if (!cfg.config_path.empty()) {
        if (loadConfig(cfg.config_path.c_str(), current_preset_)) {
            selected_preset_index_ = -1; // Custom
        } else {
            Log::warn("Could not load config '%s'", cfg.config_path.c_str());
        }
    } else {
        applyPreset(cfg.preset_index);
    }

    Log::dbg("App::init: creating render context");
    ctx_ = createRenderContext(cfg);
    if (!ctx_ || !ctx_->init(cfg)) {
        Log::err("Failed to initialize render context");
        return false;
    }

    // Query actual drawable size (handles tiling WMs and HiDPI scaling)
    SDL_GL_GetDrawableSize(ctx_->window(), &window_w_, &window_h_);
    Log::dbg("App::init: drawable size %dx%d", window_w_, window_h_);

    renderer_ = createRenderer(cfg.backend);
    if (!renderer_ || !renderer_->init(window_w_, window_h_)) {
        Log::err("Renderer init failed");
        return false;
    }
    Log::dbg("App::init: renderer initialized");

    hw_info_ = HWInfo::detect();
    Log::dbg("App::init: CPU=%s, OS=%s %s",
             hw_info_.cpu_name.c_str(), hw_info_.os_name.c_str(), hw_info_.os_version.c_str());

    // Fallback to sync timing if GPU timer queries unavailable
    if (config_.timing_mode == TimingMode::GPU && !renderer_->hasTimerQueries()) {
        config_.timing_mode = TimingMode::Sync;
        Log::warn("GPU timer queries not available, falling back to sync timing");
    }

    // Quick probe for GPU tier detection and preset recommendation
    // Pass nullptr for callbacks — UI is not ready during init
    gpu_tier_ = bench_runner_.runQuickProbe(renderer_.get(), ctx_.get(),
                                            render_w_, render_h_, nullptr);
    suggested_preset_ = tierToPresetIndex(gpu_tier_);
    validateCurrentPreset();

    // Set up render resolution
    if (cfg.render_width > 0 && cfg.render_height > 0) {
        // Find matching resolution or set directly
        selected_resolution_ = ResNative;
        for (int i = 0; i < NUM_RESOLUTIONS; i++) {
            if (RESOLUTIONS[i].w == cfg.render_width && RESOLUTIONS[i].h == cfg.render_height) {
                selected_resolution_ = i;
                break;
            }
        }
        render_w_ = cfg.render_width;
        render_h_ = cfg.render_height;
    } else {
        selected_resolution_ = ResNative;
        render_w_ = window_w_;
        render_h_ = window_h_;
    }

    // Parse test filter for headless mode
    if (cfg.test_filter != "all") {
        for (int i = 0; i < NUM_TESTS; i++) test_enabled_[i] = false;
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
                    strcasecmp(name.c_str(), g_tests[i].cli_name) == 0) {
                    test_enabled_[i] = true;
                }
            }
            pos = comma + 1;
        }
    }

    if (!cfg.headless) {
        setupPreviewScene();
    }

    frame_timer_.reset();
    running_ = true;
    initialized_ = true;
    return true;
}

void App::shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    Log::shutdown();

    if (!config_.headless) {
        cleanupPreviewScene();
    }

    if (renderer_) {
        renderer_->shutdown();
        renderer_.reset();
    }
    if (ctx_) {
        ctx_->shutdown();
        ctx_.reset();
    }
}

void App::applyPreset(int index) {
    if (index >= 0 && index < static_cast<int>(PresetIndex::Count)) {
        current_preset_ = getPreset(index);
        selected_preset_index_ = index;
    }
}

void App::validateCurrentPreset() {
    if (renderer_) {
        PresetValidation v = validatePreset(current_preset_, renderer_->getCaps());
        if (!v.ok) {
            validation_error_ = v.reason;
        } else {
            validation_error_.clear();
        }
    }
}

void App::setupPreviewScene() {
    preview_terrain_ = renderer_->createMesh(MeshGen::terrain(30.0f, 64));
    preview_sphere_  = renderer_->createMesh(MeshGen::sphere(24, 16));

    int sz = PREVIEW_TEX_SIZE;
    std::vector<unsigned char> pixels(sz * sz * 3);
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++) {
            bool white = ((x / 16 + y / 16) % 2) != 0;
            unsigned char c = white ? 200 : 80;
            int i = (y * sz + x) * 3;
            pixels[i] = c; pixels[i+1] = c; pixels[i+2] = c;
        }
    preview_tex_ = renderer_->createTexture(sz, sz, 3, pixels.data());
    preview_ready_ = true;
}

void App::cleanupPreviewScene() {
    if (!preview_ready_) return;
    renderer_->destroyMesh(preview_terrain_);
    renderer_->destroyMesh(preview_sphere_);
    renderer_->destroyTexture(preview_tex_);
    preview_ready_ = false;
}

void App::processEvents() {
    SDL_Event e;
    while (ctx_->pollEvent(&e)) {
        if (e.type == SDL_QUIT) running_ = false;
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            // Use drawable size for correct viewport on HiDPI and tiling WMs
            SDL_GL_GetDrawableSize(ctx_->window(), &window_w_, &window_h_);
            if (selected_resolution_ == ResNative) {
                render_w_ = window_w_;
                render_h_ = window_h_;
            }
        }
    }
}

void App::renderPreviewScene(float dt) {
    if (!preview_ready_) return;

    int panel_h = static_cast<int>(static_cast<float>(window_h_) * 0.55f);
    int view_h = window_h_ - panel_h;
    if (view_h <= 0) return;

    renderer_->setScissor(true, 0, panel_h, window_w_, view_h);
    renderer_->setViewport(0, panel_h, window_w_, view_h);
    renderer_->clear(0.08f, 0.08f, 0.10f, 1.0f);

    renderer_->setDepthTest(true);
    renderer_->setBlending(false);
    renderer_->useShader(Renderer::ShaderType::Scene3D);

    float aspect = static_cast<float>(window_w_) / static_cast<float>(view_h);
    renderer_->setProjection(Mat4::perspective(PREVIEW_FOV, aspect, PREVIEW_NEAR_CLIP, PREVIEW_FAR_CLIP));

    float cx = cosf(preview_angle_) * PREVIEW_CAM_DIST;
    float cz = sinf(preview_angle_) * PREVIEW_CAM_DIST;
    renderer_->setView(Mat4::lookAt(Vec3(cx, PREVIEW_CAM_Y, cz), Vec3(0, 2, 0), Vec3(0, 1, 0)));
    renderer_->setLightDir(0.5f, 0.8f, 0.3f);

    renderer_->setModel(Mat4());
    renderer_->setUseTexture(true);
    renderer_->setColor(1.0f, 1.0f, 1.0f, 1.0f);
    renderer_->bindTexture(preview_tex_);
    renderer_->drawMesh(preview_terrain_);

    renderer_->setUseTexture(false);
    float colors[][3] = {
        {0.4f, 0.7f, 1.0f}, {1.0f, 0.5f, 0.3f}, {0.3f, 1.0f, 0.5f},
        {1.0f, 0.8f, 0.2f}, {0.8f, 0.3f, 1.0f},
    };
    for (int i = 0; i < 5; i++) {
        float a = static_cast<float>(i) / 5.0f * 6.28318f + preview_angle_ * 0.5f;
        renderer_->setColor(colors[i][0], colors[i][1], colors[i][2], 1.0f);
        renderer_->setModel(Mat4::translate(cosf(a) * 8.0f, 4.0f + sinf(a * 2.0f) * 1.5f, sinf(a) * 8.0f)
                           * Mat4::scale(1.2f, 1.2f, 1.2f));
        renderer_->drawMesh(preview_sphere_);
    }

    renderer_->setScissor(false);
    preview_angle_ += PREVIEW_ROT_SPEED * dt;
}

bool App::isTestSelected(const char* name) const {
    for (int i = 0; i < NUM_TESTS; i++) {
        if (strcmp(name, g_tests[i].display_name) == 0)
            return test_enabled_[i];
    }
    return false;
}

// --- BenchCallbacks implementation ---

bool App::onFrame(RenderTargetHandle rt) {
    processEvents();

    if (!config_.headless) {
        // Present benchmark output to screen
        if (rt != INVALID_RENDER_TARGET) {
            renderer_->bindRenderTarget(INVALID_RENDER_TARGET);
            renderer_->setViewport(0, 0, window_w_, window_h_);
            renderer_->clear(0.12f, 0.12f, 0.15f, 1.0f);
            int panel_h = static_cast<int>(static_cast<float>(window_h_) * 0.55f);
            int view_h = window_h_ - panel_h;
            if (view_h > 0)
                renderer_->blitToScreen(rt, 0, panel_h, window_w_, view_h);
        }
        renderer_->setViewport(0, 0, window_w_, window_h_);
        renderer_->setDepthTest(false);
        renderer_->setBlending(false);
        renderer_->unbindState();
        renderUI();
    }

    ctx_->swapBuffers();
    return running_;
}

bool App::onPoll() {
    processEvents();
    return running_;
}

// --- Benchmark execution ---

void App::runSelectedTests() {
    BenchConfig cfg;
    cfg.warmup_frames = current_preset_.warmup_frames;
    cfg.measure_frames = current_preset_.measure_frames;
    cfg.min_duration_sec = current_preset_.min_duration_sec;
    cfg.timing_mode = config_.timing_mode;
    cfg.headless = config_.headless;
    cfg.render_w = render_w_;
    cfg.render_h = render_h_;
    cfg.window_w = window_w_;
    cfg.window_h = window_h_;

    uint32_t caps = getAvailableCaps(*renderer_);
    bench_runner_.runSelected(renderer_.get(), ctx_.get(), current_preset_, cfg,
                              test_enabled_, caps, this);
}

void App::renderUI() {
    // Build read-only view
    UIView view;
    view.hw_info = &hw_info_;
    view.gpu_renderer = renderer_->getGPURenderer();
    view.gl_version = renderer_->getGLVersion();
    view.renderer_name = renderer_->getRendererName();
    view.caps = &renderer_->getCaps();
    view.available_caps = getAvailableCaps(*renderer_);
    view.supports_render_targets = renderer_->supportsRenderTargets();
    view.gpu_tier = gpu_tier_;
    view.window_w = window_w_;
    view.window_h = window_h_;
    view.render_w = render_w_;
    view.render_h = render_h_;
    view.suggested_preset = suggested_preset_;
    view.bench_active = bench_runner_.isActive();
    view.bench_status = &bench_runner_.status();
    view.bench_progress = bench_runner_.progress();
    view.results = &bench_runner_.results();
    view.composite = &bench_runner_.compositeScore();
    view.bottleneck = &bench_runner_.bottleneckInfo();
    view.resolutions = RESOLUTIONS;
    view.num_resolutions = NUM_RESOLUTIONS;
    view.validation_error = &validation_error_;

    // Build mutable state
    UIState state;
    state.current_preset = &current_preset_;
    state.selected_preset_index = &selected_preset_index_;
    state.test_enabled = test_enabled_;
    state.selected_resolution = &selected_resolution_;
    state.validation_error = &validation_error_;

    // Render UI and get action
    UIAction action = bench_ui_.render(ctx_.get(), view, state);

    // Dispatch action
    switch (action.type) {
        case UIAction::ApplyPreset:
            applyPreset(action.preset_index);
            validateCurrentPreset();
            break;
        case UIAction::ResolutionChanged:
            updateRenderResolution();
            break;
        case UIAction::CustomParamsChanged:
            selected_preset_index_ = -1;
            current_preset_.name = "Custom";
            validateCurrentPreset();
            break;
        case UIAction::RunSelected:
            pending_action_ = PendingAction::RunSelected;
            break;
        case UIAction::RunAll:
            pending_action_ = PendingAction::RunAll;
            break;
        case UIAction::Clear:
            bench_runner_.clearResults();
            break;
        case UIAction::Export:
            exportResults();
            break;
        default:
            break;
    }
}

void App::exportDemoResults(const DemoResults& results) {
    const char* path = config_.output_file.empty() ? nullptr : config_.output_file.c_str();
    if (!writeDemoResults(config_.output_format, path, results, hw_info_,
                          renderer_->getGPURenderer(), renderer_->getGLVersion(),
                          renderer_->getRendererName())) {
        Log::err("Could not open output file: %s", config_.output_file.c_str());
    }
}

void App::exportResults() {
    const auto& results = bench_runner_.results();
    if (results.empty()) return;

    ExportConfig ecfg;
    ecfg.width = render_w_;
    ecfg.height = render_h_;
    ecfg.warmup_frames = current_preset_.warmup_frames;
    ecfg.measure_frames = current_preset_.measure_frames;
    ecfg.vsync = false;
    ecfg.gpu_tier = gpuTierName(gpu_tier_);

    const char* path = config_.output_file.empty() ? nullptr : config_.output_file.c_str();
    if (!writeBenchResults(config_.output_format, path, results, hw_info_,
                           renderer_->getCaps(), getAvailableCaps(*renderer_),
                           renderer_->getGPURenderer(), renderer_->getGLVersion(),
                           renderer_->getRendererName(), current_preset_.name, ecfg,
                           &bench_runner_.compositeScore(), &bench_runner_.bottleneckInfo())) {
        Log::err("Could not open output file: %s", config_.output_file.c_str());
    }
}

void App::runDemo() {
    DemoConfig dcfg;
    dcfg.tier_override = config_.demo_tier;
    dcfg.duration_per_tier = config_.demo_duration;

    // Demo callbacks for UI overlay (only in interactive mode)
    struct DemoCB : public DemoCallbacks {
        App* app;
        DemoUI* ui;
        const char* gpu_name;
        bool headless;

        bool onDemoFrame(DemoTier tier, int tier_idx, int total,
                         float progress, double fps, double frame_ms,
                         const std::vector<double>& history) override {
            if (headless) return app->onPoll();

            // Render UI overlay on top of scene (runner handles swapBuffers)
            app->renderer_->setViewport(0, 0, app->window_w_, app->window_h_);
            app->renderer_->setDepthTest(false);
            app->renderer_->unbindState();
            ui->drawOverlay(app->ctx_.get(), gpu_name, tier, tier_idx, total,
                           progress, fps, frame_ms, history);
            app->processEvents();
            return app->running_;
        }
    };

    DemoCB demo_cb;
    demo_cb.app = this;
    demo_cb.ui = &demo_ui_;
    demo_cb.gpu_name = renderer_->getGPURenderer();
    demo_cb.headless = config_.headless;

    DemoRunner runner;
    demo_results_ = runner.run(renderer_.get(), ctx_.get(), dcfg,
                                render_w_, render_h_, this,
                                config_.headless ? nullptr : &demo_cb);

    // Show results screen (interactive mode)
    if (!config_.headless && running_) {
        Timer results_timer;
        while (running_ && results_timer.elapsed_sec() < 30.0) {
            processEvents();
            renderer_->setViewport(0, 0, window_w_, window_h_);
            renderer_->clear(0.1f, 0.1f, 0.12f, 1.0f);
            renderer_->setDepthTest(false);
            renderer_->unbindState();
            demo_ui_.drawResults(ctx_.get(), demo_results_);
            ctx_->swapBuffers();
        }
    }

    exportDemoResults(demo_results_);
}

void App::runHeadless() {
    if (config_.demo_mode) {
        runDemo();
        return;
    }

    if (!validation_error_.empty()) {
        Log::err("Preset validation failed: %s", validation_error_.c_str());
        return;
    }

    if (config_.stress_duration_sec > 0) {
        StressRunner stress;
        stress.run(renderer_.get(), ctx_.get(),
                   current_preset_.shader_alu.iterations,
                   render_w_, render_h_,
                   config_.stress_duration_sec, this);
    } else {
        runSelectedTests();
    }

    exportResults();
}

void App::run() {
    if (config_.headless) {
        runHeadless();
        return;
    }

    if (config_.demo_mode) {
        runDemo();
        return;
    }

    frame_timer_.reset();
    while (running_) {
        double dt = frame_timer_.elapsed_sec();
        frame_timer_.reset();
        last_frame_time_ = dt;

        processEvents();

        if (!bench_runner_.isActive()) {
            renderer_->clear(0.12f, 0.12f, 0.15f, 1.0f);
            renderPreviewScene(static_cast<float>(dt));
            renderer_->unbindState();
        }

        renderer_->setViewport(0, 0, window_w_, window_h_);
        renderUI();

        ctx_->swapBuffers();

        if (pending_action_ != PendingAction::None) {
            PendingAction action = pending_action_;
            pending_action_ = PendingAction::None;
            if (action == PendingAction::RunAll) {
                for (int i = 0; i < NUM_TESTS; i++) test_enabled_[i] = true;
            }
            runSelectedTests();
        }
    }
}
