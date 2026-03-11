#include "app.h"
#include "mesh_gen.h"
#include "renderer_factory.h"
#include "config.h"
#include "results.h"
#include <imgui.h>
#include "compat.h"
#include "logger.h"
#include <cstdio>
#include <cmath>
#include <cstring>

static const float PREVIEW_ROT_SPEED = 0.3f;


const char* App::test_names_[NUM_TESTS] = {
    "Fillrate", "Geometry", "Texturing", "Scene", "DrawCall",
    "Overdraw", "TexUpload", "StateChange", "Vertex", "ShaderALU",
    "ShaderFMA", "DrawCallRaw"
};

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
    : running_(false), benchmarking_(false),
      initialized_(false), window_w_(800), window_h_(600),
      render_w_(800), render_h_(600), selected_resolution_(RES_NATIVE),
      last_frame_time_(0.0),
      selected_preset_index_(PRESET_MEDIUM), suggested_preset_(PRESET_MEDIUM),
      bench_progress_(0),
      pending_action_(ACTION_NONE),
      preview_terrain_(INVALID_MESH), preview_sphere_(INVALID_MESH),
      preview_tex_(INVALID_TEXTURE), preview_angle_(0.0f), preview_ready_(false),
      bench_rt_(INVALID_RENDER_TARGET),
      gpu_tier_(GPU_TIER_MID)
{
    for (int i = 0; i < NUM_TESTS; i++) test_enabled_[i] = true;
    current_preset_ = getPreset(PRESET_MEDIUM);
}

App::~App() { shutdown(); }


void App::updateRenderResolution() {
    if (selected_resolution_ == RES_NATIVE) {
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

    ctx_.reset(createRenderContext(cfg));
    if (!ctx_ || !ctx_->init(cfg)) {
        Log::err("Failed to initialize render context");
        return false;
    }

    renderer_.reset(createRenderer(cfg.backend));
    if (!renderer_ || !renderer_->init(window_w_, window_h_)) {
        Log::err("Renderer init failed");
        return false;
    }

    hw_info_ = HWInfo::detect();

    // Fallback to sync timing if GPU timer queries unavailable
    if (config_.timing_mode == TIMING_GPU && !renderer_->hasTimerQueries()) {
        config_.timing_mode = TIMING_SYNC;
        Log::warn("GPU timer queries not available, falling back to sync timing");
    }

    // Quick probe for GPU tier detection and preset recommendation
    runQuickProbe();
    validateCurrentPreset();

    // Set up render resolution
    if (cfg.render_width > 0 && cfg.render_height > 0) {
        // Find matching resolution or set directly
        selected_resolution_ = RES_NATIVE;
        for (int i = 0; i < NUM_RESOLUTIONS; i++) {
            if (RESOLUTIONS[i].w == cfg.render_width && RESOLUTIONS[i].h == cfg.render_height) {
                selected_resolution_ = i;
                break;
            }
        }
        render_w_ = cfg.render_width;
        render_h_ = cfg.render_height;
    } else {
        selected_resolution_ = RES_NATIVE;
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
                if (strcasecmp(name.c_str(), test_names_[i]) == 0) {
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

    if (bench_rt_ != INVALID_RENDER_TARGET && renderer_) {
        renderer_->destroyRenderTarget(bench_rt_);
        bench_rt_ = INVALID_RENDER_TARGET;
    }

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
    if (index >= 0 && index < PRESET_COUNT) {
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

    int sz = 256;
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
            window_w_ = e.window.data1;
            window_h_ = e.window.data2;
            if (selected_resolution_ == RES_NATIVE) {
                render_w_ = window_w_;
                render_h_ = window_h_;
            }
        }
    }
}

void App::renderPreviewScene(float dt) {
    if (!preview_ready_) return;

    int panel_h = static_cast<int>(window_h_ * 0.55f);
    int view_h = window_h_ - panel_h;
    if (view_h <= 0) return;

    renderer_->setScissor(true, 0, panel_h, window_w_, view_h);
    renderer_->setViewport(0, panel_h, window_w_, view_h);
    renderer_->clear(0.08f, 0.08f, 0.10f, 1.0f);

    renderer_->setDepthTest(true);
    renderer_->setBlending(false);
    renderer_->useShader(Renderer::SHADER_3D);

    float aspect = static_cast<float>(window_w_) / static_cast<float>(view_h);
    renderer_->setProjection(Mat4::perspective(60.0f, aspect, 0.1f, 200.0f));

    float cx = cosf(preview_angle_) * 22.0f;
    float cz = sinf(preview_angle_) * 22.0f;
    renderer_->setView(Mat4::lookAt(Vec3(cx, 14.0f, cz), Vec3(0, 2, 0), Vec3(0, 1, 0)));
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
        if (strcmp(name, test_names_[i]) == 0)
            return test_enabled_[i];
    }
    return false;
}

void App::runQuickProbe() {
    // Quick ~1 second probe to estimate GPU performance tier.
    // Runs short fillrate + geometry tests at small viewport.
    const int PROBE_FRAMES = 40;
    const int PROBE_W = 256, PROBE_H = 256;

    ctx_->setVSync(false);
    renderer_->resetState();

    double fill_score = 0;
    double geom_score = 0;

    // Probe fillrate: 50 layers, 40 frames
    {
        FillrateParams fp; fp.layers = 50;
        FillrateTest test(fp);
        test.setup(renderer_.get(), PROBE_W, PROBE_H);
        renderer_->setViewport(0, 0, PROBE_W, PROBE_H);

        // Warmup 5 frames
        for (int i = 0; i < 5; i++) {
            renderer_->clear(0.0f, 0.0f, 0.0f, 1.0f);
            test.render(renderer_.get());
            renderer_->finish();
        }

        // Measure
        std::vector<double> times;
        times.reserve(PROBE_FRAMES);
        Timer ft;
        for (int i = 0; i < PROBE_FRAMES; i++) {
            renderer_->clear(0.0f, 0.0f, 0.0f, 1.0f);
            renderer_->finish();
            ft.reset();
            test.render(renderer_.get());
            renderer_->finish();
            times.push_back(ft.elapsed_ms());
        }
        fill_score = test.computeScore(times, PROBE_W, PROBE_H);
        test.cleanup(renderer_.get());
    }

    // Probe geometry: grid 10, 40 frames
    {
        GeometryParams gp; gp.grid_size = 10;
        GeometryTest test(gp);
        renderer_->resetState();
        test.setup(renderer_.get(), PROBE_W, PROBE_H);
        renderer_->setViewport(0, 0, PROBE_W, PROBE_H);

        for (int i = 0; i < 5; i++) {
            renderer_->clear(0.0f, 0.0f, 0.0f, 1.0f);
            test.render(renderer_.get());
            renderer_->finish();
        }

        std::vector<double> times;
        times.reserve(PROBE_FRAMES);
        Timer ft;
        for (int i = 0; i < PROBE_FRAMES; i++) {
            renderer_->clear(0.0f, 0.0f, 0.0f, 1.0f);
            renderer_->finish();
            ft.reset();
            test.render(renderer_.get());
            renderer_->finish();
            times.push_back(ft.elapsed_ms());
        }
        geom_score = test.computeScore(times, PROBE_W, PROBE_H);
        test.cleanup(renderer_.get());
    }

    renderer_->resetState();
    ctx_->setVSync(true);

    gpu_tier_ = classifyGPUTier(renderer_->getCaps(), fill_score, geom_score);
    suggested_preset_ = tierToPresetIndex(gpu_tier_);

    Log::info("Quick probe: fill=%.1f Mpix/s, geom=%.1f Ktris/s -> tier=%s, preset=%d",
            fill_score, geom_score, gpuTierName(gpu_tier_), suggested_preset_);
}

void App::computeAnalysis() {
    composite_score_ = computeCompositeScores(results_);
    bottleneck_info_ = detectBottleneck(results_, composite_score_);
}

void App::renderUI() {
    ctx_->imguiNewFrame();

    float panel_h = window_h_ * 0.55f;
    ImGui::SetNextWindowPos(ImVec2(0, window_h_ - panel_h));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(window_w_), panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("##panel", NULL, flags);

    // ---- Hardware info + resolution ----
    ImGui::Text("CPU: %s", hw_info_.cpu_name.c_str());
    ImGui::SameLine(static_cast<float>(window_w_) * 0.5f);
    ImGui::Text("GPU: %s", renderer_->getGPURenderer());
    ImGui::Text("OS: %s %s", hw_info_.os_name.c_str(), hw_info_.os_version.c_str());
    ImGui::SameLine(static_cast<float>(window_w_) * 0.5f);
    ImGui::Text("GL: %s  Renderer: %s", renderer_->getGLVersion(), renderer_->getRendererName());
    const RenderCaps& caps = renderer_->getCaps();
    if (caps.estimated_vram_mb > 0) {
        ImGui::SameLine();
        ImGui::Text("  VRAM: %d MB", caps.estimated_vram_mb);
    }
    // Capabilities line
    ImGui::Text("MaxTex: %d  Attribs: %d", caps.max_texture_size, caps.max_vertex_attribs);
    ImGui::SameLine();
    auto capLabel = [](const char* name, bool supported) {
        if (supported)
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", name);
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", name);
    };
    capLabel("VAO", caps.has_vao);
    ImGui::SameLine(); capLabel("Instancing", caps.has_instancing);
    ImGui::SameLine(); capLabel("FBO", caps.has_fbo);
    ImGui::SameLine(); capLabel("TimerQ", caps.has_timer_queries);
    ImGui::SameLine();
    ImVec4 tier_colors[] = {
        ImVec4(0.7f, 0.5f, 0.3f, 1.0f),  // legacy
        ImVec4(0.8f, 0.8f, 0.3f, 1.0f),  // low
        ImVec4(0.3f, 0.8f, 0.3f, 1.0f),  // mid
        ImVec4(0.3f, 0.6f, 1.0f, 1.0f),  // high
    };
    int ti = static_cast<int>(gpu_tier_);
    if (ti < 0 || ti > 3) ti = 2;
    ImGui::TextColored(tier_colors[ti], "Tier: %s", gpuTierName(gpu_tier_));
    // Resolution display
    ImGui::Text("Window: %dx%d", window_w_, window_h_);
    ImGui::SameLine();
    if (render_w_ == window_w_ && render_h_ == window_h_) {
        ImGui::Text("  Render: %dx%d (native)", render_w_, render_h_);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
            "  Render: %dx%d", render_w_, render_h_);
    }

    ImGui::Separator();

    // ---- Preset selection ----
    if (!benchmarking_) {
        ImGui::Text("Preset:");
        ImGui::SameLine();

        const char* preset_labels[PRESET_COUNT] = {"Light", "Medium", "Heavy", "Ultra"};
        for (int i = 0; i < PRESET_COUNT; i++) {
            if (i > 0) ImGui::SameLine();
            char label[64];
            if (i == suggested_preset_)
                snprintf(label, sizeof(label), "%s*", preset_labels[i]);
            else
                snprintf(label, sizeof(label), "%s", preset_labels[i]);

            bool is_selected = (selected_preset_index_ == i);
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
            }
            if (ImGui::Button(label, ImVec2(80, 0))) {
                applyPreset(i);
                validateCurrentPreset();
            }
            if (is_selected) {
                ImGui::PopStyleColor();
            }
        }
        if (suggested_preset_ >= 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(* = recommended)");
        }

        if (selected_preset_index_ < 0) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[Custom]");
        }

        if (!validation_error_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", validation_error_.c_str());
        }

        // ---- Render resolution selector ----
        ImGui::Text("Render:");
        ImGui::SameLine();

        // Native button
        {
            bool is_native = (selected_resolution_ == RES_NATIVE);
            if (is_native) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
            char native_label[64];
            snprintf(native_label, sizeof(native_label), "Native (%dx%d)", window_w_, window_h_);
            if (ImGui::Button(native_label)) {
                selected_resolution_ = RES_NATIVE;
                updateRenderResolution();
            }
            if (is_native) ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);

        // Build combo items
        char combo_label[64];
        if (selected_resolution_ >= 0 && selected_resolution_ < NUM_RESOLUTIONS)
            snprintf(combo_label, sizeof(combo_label), "%s", RESOLUTIONS[selected_resolution_].label);
        else
            snprintf(combo_label, sizeof(combo_label), "Select...");

        if (ImGui::BeginCombo("##res", (selected_resolution_ == RES_NATIVE) ? "---" : combo_label)) {
            for (int i = 0; i < NUM_RESOLUTIONS; i++) {
                bool is_sel = (selected_resolution_ == i);
                // Mark if FBO needed
                bool needs_fbo = (RESOLUTIONS[i].w > window_w_ || RESOLUTIONS[i].h > window_h_);
                char item_label[64];
                if (needs_fbo && !renderer_->supportsRenderTargets())
                    snprintf(item_label, sizeof(item_label), "%s (no FBO)", RESOLUTIONS[i].label);
                else if (needs_fbo)
                    snprintf(item_label, sizeof(item_label), "%s (FBO)", RESOLUTIONS[i].label);
                else
                    snprintf(item_label, sizeof(item_label), "%s", RESOLUTIONS[i].label);

                // Disable if needs FBO and no support
                bool disabled = needs_fbo && !renderer_->supportsRenderTargets();
                if (disabled) ImGui::BeginDisabled();

                if (ImGui::Selectable(item_label, is_sel)) {
                    selected_resolution_ = i;
                    updateRenderResolution();
                }
                if (is_sel) ImGui::SetItemDefaultFocus();

                if (disabled) ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }

        // Collapsible custom parameters
        if (ImGui::TreeNode("Custom Parameters")) {
            bool changed = false;
            ImGui::Text("Timing:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("Warmup##w", &current_preset_.warmup_frames, 10, 50);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("Measure##m", &current_preset_.measure_frames, 50, 200);

            ImGui::Separator();
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("Fillrate layers", &current_preset_.fillrate.layers, 50, 200);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("Geometry grid", &current_preset_.geometry.grid_size, 5, 10);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("Texturing tex size", &current_preset_.texturing.tex_size, 256, 512);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("Texturing layers", &current_preset_.texturing.layers, 50, 100);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("DrawCall draws", &current_preset_.drawcall.draws_per_frame, 100, 500);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("Overdraw layers", &current_preset_.overdraw.layers, 50, 100);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("TexUpload size", &current_preset_.texupload.tex_size, 128, 256);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("StateChange switches", &current_preset_.statechange.switches, 50, 100);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("Vertex count", &current_preset_.vertex.vertex_count, 10000, 100000);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("ShaderALU iters", &current_preset_.shader_alu.iterations, 10, 50);
            ImGui::SetNextItemWidth(80);
            changed |= ImGui::InputInt("ShaderFMA iters", &current_preset_.shader_fma.iterations, 10, 50);

            if (changed) {
                selected_preset_index_ = -1;
                current_preset_.name = "Custom";
                validateCurrentPreset();
            }

            ImGui::TreePop();
        }

        if (current_preset_.warmup_frames < 30) current_preset_.warmup_frames = 30;
        if (current_preset_.measure_frames < 60) current_preset_.measure_frames = 60;

        ImGui::Separator();

        // ---- Test selection ----
        ImGui::Text("Tests:");
        for (int i = 0; i < NUM_TESTS; i++) {
            if (i > 0 && i % 6 != 0) ImGui::SameLine();
            ImGui::Checkbox(test_names_[i], &test_enabled_[i]);
        }

        // ---- Benchmark controls ----
        bool can_run = validation_error_.empty();

        if (!can_run) ImGui::BeginDisabled();
        if (ImGui::Button("Run Selected", ImVec2(110, 0))) {
            pending_action_ = ACTION_RUN_SELECTED;
        }
        ImGui::SameLine();
        if (ImGui::Button("Run All", ImVec2(80, 0))) {
            pending_action_ = ACTION_RUN_ALL;
        }
        if (!can_run) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Clear", ImVec2(60, 0))) {
            results_.clear();
            composite_score_ = CompositeScore();
            bottleneck_info_ = BottleneckInfo();
        }
        ImGui::SameLine();
        if (!results_.empty() && ImGui::Button("Export", ImVec2(60, 0))) {
            exportResults();
        }
    } else {
        ImGui::Text("Status: %s", bench_status_.c_str());
        ImGui::ProgressBar(bench_progress_ / 100.0f, ImVec2(-1, 0));
    }

    ImGui::Separator();

    // ---- Composite score ----
    if (!results_.empty()) {
        int cats_present = (composite_score_.fill > 0 ? 1 : 0)
                         + (composite_score_.geometry > 0 ? 1 : 0)
                         + (composite_score_.compute > 0 ? 1 : 0)
                         + (composite_score_.overhead > 0 ? 1 : 0);
        if (composite_score_.overall > 0) {
            ImVec4 color = (cats_present < 4)
                ? ImVec4(0.8f, 0.8f, 0.4f, 1.0f)
                : ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
            ImGui::TextColored(color,
                "Composite: %.1f  |  Fill: %.1f  Geometry: %.1f  Compute: %.1f  Overhead: %.1f",
                composite_score_.overall, composite_score_.fill,
                composite_score_.geometry, composite_score_.compute, composite_score_.overhead);
            if (cats_present < 4) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), " (partial — run all for full score)");
            }
        }
    }

    // ---- Bottleneck analysis ----
    if (!results_.empty() && !bottleneck_info_.detail.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Bottleneck: %s", bottleneck_info_.detail.c_str());
    }

    // ---- Results table ----
    if (!results_.empty() && ImGui::BeginTable("results", 10,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp
            | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Test",       ImGuiTableColumnFlags_None, 1.2f);
        ImGui::TableSetupColumn("Score",      ImGuiTableColumnFlags_None, 1.0f);
        ImGui::TableSetupColumn("Unit",       ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("Avg (ms)",   ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("Min (ms)",   ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("Max (ms)",   ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("P1 (ms)",    ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("Median (ms)",ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("P99 (ms)",   ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("CV%",        ImGuiTableColumnFlags_None, 0.6f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < results_.size(); i++) {
            const BenchResult& r = results_[i];
            ImGui::TableNextRow();

            if (!r.valid) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImVec4(0.4f, 0.1f, 0.1f, 0.5f)));
            }

            ImGui::TableSetColumnIndex(0);
            if (!r.sanity_ok) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s [!]", r.name.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Sanity check failed: render output was black");
            } else {
                ImGui::Text("%s", r.name.c_str());
            }
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", r.score);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", r.unit.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", r.avg_ms);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f", r.min_ms);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%.3f", r.max_ms);
            ImGui::TableSetColumnIndex(6); ImGui::Text("%.3f", r.p1_ms);
            ImGui::TableSetColumnIndex(7); ImGui::Text("%.3f", r.median_ms);
            ImGui::TableSetColumnIndex(8); ImGui::Text("%.3f", r.p99_ms);

            ImGui::TableSetColumnIndex(9);
            double cv_pct = r.cv * 100.0;
            ImVec4 cv_color;
            if (cv_pct < 5.0)       cv_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            else if (cv_pct < 15.0) cv_color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
            else                    cv_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(cv_color, "%.1f%%", cv_pct);
        }
        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::Render();
    ctx_->imguiRender();
}

void App::runTest(BenchTest* test) {
    benchmarking_ = true;
    bench_status_ = std::string("Running: ") + test->name();
    bench_progress_ = 0;

    ctx_->setVSync(false);

    // Determine if we need render target (render resolution != window)
    bool use_fbo = (render_w_ != window_w_ || render_h_ != window_h_) && renderer_->supportsRenderTargets();
    if (use_fbo) {
        // Recreate render target if needed
        if (bench_rt_ != INVALID_RENDER_TARGET) {
            renderer_->destroyRenderTarget(bench_rt_);
            bench_rt_ = INVALID_RENDER_TARGET;
        }
        bench_rt_ = renderer_->createRenderTarget(render_w_, render_h_);
        if (bench_rt_ == INVALID_RENDER_TARGET) {
            use_fbo = false;
            Log::warn("Render target creation failed, falling back to viewport-only");
        }
    }

    // Bind render target for rendering if needed
    if (use_fbo) renderer_->bindRenderTarget(bench_rt_);

    renderer_->resetState();
    test->setup(renderer_.get(), render_w_, render_h_);

    bool use_gpu_timer = (config_.timing_mode == TIMING_GPU) && renderer_->hasTimerQueries();

    // Warmup
    bool sanity_ok = true;
    bench_status_ = std::string("Warmup: ") + test->name();
    for (int i = 0; i < current_preset_.warmup_frames && running_; i++) {
        processEvents();
        if (use_fbo) renderer_->bindRenderTarget(bench_rt_);
        renderer_->setViewport(0, 0, render_w_, render_h_);
        renderer_->clear(0.1f, 0.1f, 0.12f, 1.0f);
        test->render(renderer_.get());
        renderer_->finish();

        // Sanity check after first warmup frame: verify non-black output
        if (i == 0) {
            unsigned char px[4 * 4 * 4] = {0}; // 4x4 RGBA
            int cx = render_w_ / 2 - 2, cy = render_h_ / 2 - 2;
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            renderer_->readPixels(cx, cy, 4, 4, px);
            int nonzero = 0;
            for (int j = 0; j < 4 * 4 * 4; j++) nonzero += (px[j] > 0) ? 1 : 0;
            if (nonzero == 0) {
                sanity_ok = false;
                Log::warn("Sanity check FAILED for '%s' — render output is black",
                        test->name());
            }
        }

        bench_progress_ = static_cast<int>(100.0 * i / current_preset_.warmup_frames * 0.1);
        if (!config_.headless) {
            if (use_fbo) {
                renderer_->bindRenderTarget(0);
                renderer_->setViewport(0, 0, window_w_, window_h_);
                renderer_->clear(0.12f, 0.12f, 0.15f, 1.0f);
                int panel_h = static_cast<int>(window_h_ * 0.55f);
                int view_h = window_h_ - panel_h;
                if (view_h > 0)
                    renderer_->blitToScreen(bench_rt_, 0, panel_h, window_w_, view_h);
            }
            renderer_->setViewport(0, 0, window_w_, window_h_);
            renderer_->setDepthTest(false);
            renderer_->setBlending(false);
            renderer_->unbindState();
            renderUI();
        }
        ctx_->swapBuffers();
    }

    // Measurement
    bench_status_ = std::string("Measuring: ") + test->name();
    std::vector<double> times;
    std::vector<double> gpu_times;
    times.reserve(current_preset_.measure_frames * 2);
    if (use_gpu_timer) gpu_times.reserve(current_preset_.measure_frames * 2);
    Timer total_timer;
    Timer frame_t;

    int frame = 0;
    while (running_) {
        processEvents();
        if (use_fbo) renderer_->bindRenderTarget(bench_rt_);
        renderer_->setViewport(0, 0, render_w_, render_h_);
        renderer_->clear(0.1f, 0.1f, 0.12f, 1.0f);

        renderer_->finish();

        if (use_gpu_timer) renderer_->timerBegin();
        frame_t.reset();
        test->render(renderer_.get());
        if (use_gpu_timer) renderer_->timerEnd();
        renderer_->finish();
        double ms = frame_t.elapsed_ms();
        times.push_back(ms);
        if (use_gpu_timer) gpu_times.push_back(renderer_->timerElapsedMs());

        double elapsed = total_timer.elapsed_sec();
        bench_progress_ = 10 + static_cast<int>(90.0 * std::min(
            static_cast<double>(frame) / current_preset_.measure_frames,
            elapsed / current_preset_.min_duration_sec
        ));

        if (!config_.headless) {
            if (use_fbo) {
                renderer_->bindRenderTarget(0);
                renderer_->setViewport(0, 0, window_w_, window_h_);
                renderer_->clear(0.12f, 0.12f, 0.15f, 1.0f);
                int panel_h = static_cast<int>(window_h_ * 0.55f);
                int view_h = window_h_ - panel_h;
                if (view_h > 0)
                    renderer_->blitToScreen(bench_rt_, 0, panel_h, window_w_, view_h);
            }
            renderer_->setViewport(0, 0, window_w_, window_h_);
            renderer_->setDepthTest(false);
            renderer_->setBlending(false);
            renderer_->unbindState();
            renderUI();
        }

        ctx_->swapBuffers();
        frame++;

        if (frame >= current_preset_.measure_frames && elapsed >= current_preset_.min_duration_sec)
            break;
    }

    if (use_fbo) renderer_->bindRenderTarget(0);

    double score = test->computeScore(times, render_w_, render_h_);
    BenchResult result = computeStats(test->name(), test->scoreUnit(), times, score);

    if (!gpu_times.empty()) {
        double gpu_sum = 0;
        for (size_t i = 0; i < gpu_times.size(); i++) gpu_sum += gpu_times[i];
        result.gpu_ms = gpu_sum / gpu_times.size();
    }

    result.sanity_ok = sanity_ok;
    if (!sanity_ok) result.valid = false;

    results_.push_back(result);

    test->cleanup(renderer_.get());
    benchmarking_ = false;
    bench_status_ = "Done";
    ctx_->setVSync(true);
}

void App::runSelectedTests() {
    results_.clear();
    const BenchPreset& p = current_preset_;

    if (test_enabled_[0] && running_) { FillrateTest t(p.fillrate); runTest(&t); }
    if (test_enabled_[1] && running_) { GeometryTest t(p.geometry); runTest(&t); }
    if (test_enabled_[2] && running_) { TexturingTest t(p.texturing); runTest(&t); }
    if (test_enabled_[3] && running_) { SceneTest t(p.scene); runTest(&t); }
    if (test_enabled_[4] && running_) { DrawCallTest t(p.drawcall); runTest(&t); }
    if (test_enabled_[5] && running_) { OverdrawTest t(p.overdraw); runTest(&t); }
    if (test_enabled_[6] && running_) { TexUploadTest t(p.texupload); runTest(&t); }
    if (test_enabled_[7] && running_) { StateChangeTest t(p.statechange); runTest(&t); }
    if (test_enabled_[8] && running_) { VertexTest t(p.vertex); runTest(&t); }
    if (test_enabled_[9] && running_) { ShaderALUTest t(p.shader_alu); runTest(&t); }
    if (test_enabled_[10] && running_) { ShaderFMATest t(p.shader_fma); runTest(&t); }
    if (test_enabled_[11] && running_) { DrawCallRawTest t(p.drawcall); runTest(&t); }

    // Clean up render target after all tests
    if (bench_rt_ != INVALID_RENDER_TARGET) {
        renderer_->destroyRenderTarget(bench_rt_);
        bench_rt_ = INVALID_RENDER_TARGET;
    }

    computeAnalysis();
}

void App::runAllTests() {
    for (int i = 0; i < NUM_TESTS; i++) test_enabled_[i] = true;
    runSelectedTests();
}

void App::exportResults() {
    if (results_.empty()) return;

    FILE* out = stdout;
    bool close_file = false;

    if (!config_.output_file.empty()) {
        out = fopen(config_.output_file.c_str(), "w");
        if (!out) {
            Log::err("Could not open output file: %s", config_.output_file.c_str());
            return;
        }
        close_file = true;
    }

    const char* gpu = renderer_->getGPURenderer();
    const char* gl_ver = renderer_->getGLVersion();
    const char* rname = renderer_->getRendererName();
    const char* pname = current_preset_.name;

    ExportConfig ecfg;
    ecfg.width = render_w_;
    ecfg.height = render_h_;
    ecfg.warmup_frames = current_preset_.warmup_frames;
    ecfg.measure_frames = current_preset_.measure_frames;
    ecfg.vsync = false;
    ecfg.gpu_tier = gpuTierName(gpu_tier_);

    switch (config_.output_format) {
        case OUTPUT_CSV:
            exportCSV(out, results_, hw_info_, renderer_->getCaps(), gpu, gl_ver, rname, pname, ecfg);
            break;
        case OUTPUT_JSON:
            exportJSON(out, results_, hw_info_, renderer_->getCaps(), gpu, gl_ver, rname, pname, ecfg,
                       &composite_score_, &bottleneck_info_);
            break;
        default:
            exportText(out, results_, hw_info_, renderer_->getCaps(), gpu, gl_ver, rname, pname, ecfg,
                       &composite_score_, &bottleneck_info_);
            break;
    }

    if (close_file) fclose(out);
}

void App::runHeadless() {
    if (!validation_error_.empty()) {
        Log::err("Preset validation failed: %s", validation_error_.c_str());
        return;
    }

    if (config_.stress_duration_sec > 0) {
        // Stress mode: combined shader that stresses ALL GPU units simultaneously
        // - FMA loops (shader cores / CUDA cores)
        // - Transcendentals (SFU units)
        // - Texture sampling (TMU units)
        // - Blending (ROP units)
        // - Multiple passes (memory bandwidth)
        // Self-calibrating: adjusts pass count to saturate any GPU.

        static const char* STRESS_VS =
            "#version 120\n"
            "attribute vec2 a_pos;\n"
            "attribute vec2 a_uv;\n"
            "varying vec2 v_uv;\n"
            "void main() {\n"
            "    v_uv = a_uv;\n"
            "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
            "}\n";

        // Combined stress fragment shader:
        // - 4 texture samples per pixel (TMU stress)
        // - FMA loop with data dependency (ALU stress)
        // - sin/cos per iteration (SFU stress)
        // - Output with alpha < 1.0 + blending (ROP stress)
        static const char* STRESS_FS =
            "#version 120\n"
            "varying vec2 v_uv;\n"
            "uniform sampler2D u_tex;\n"
            "uniform int u_iterations;\n"
            "uniform float u_time;\n"
            "void main() {\n"
            "    vec4 t0 = texture2D(u_tex, v_uv);\n"
            "    vec4 t1 = texture2D(u_tex, v_uv * 2.0 + vec2(u_time * 0.01));\n"
            "    vec4 t2 = texture2D(u_tex, v_uv * 4.0 - vec2(u_time * 0.02));\n"
            "    vec4 t3 = texture2D(u_tex, v_uv.yx * 3.0 + vec2(u_time * 0.015));\n"
            "    vec4 acc = t0 * 0.25 + t1 * 0.25 + t2 * 0.25 + t3 * 0.25;\n"
            "    vec4 fma_acc = acc;\n"
            "    for (int i = 0; i < u_iterations; i++) {\n"
            "        fma_acc = fma_acc * acc + fma_acc;\n"
            "        fma_acc = fma_acc * vec4(0.5) + vec4(0.25);\n"
            "        acc.x = sin(fma_acc.x * 3.14159);\n"
            "        acc.y = cos(fma_acc.y * 3.14159);\n"
            "        acc.z = sqrt(abs(fma_acc.z));\n"
            "        acc.w = fma_acc.w;\n"
            "        fma_acc = fma_acc * acc + fma_acc;\n"
            "        fma_acc = fma_acc * vec4(0.5) + vec4(0.25);\n"
            "    }\n"
            "    gl_FragColor = vec4(fma_acc.rgb * 0.5 + 0.25, 0.8);\n"
            "}\n";

        ctx_->setVSync(false);
        renderer_->resetState();

        // Create stress resources
        MeshHandle stress_quad = renderer_->createMesh(MeshGen::quad());
        ShaderHandle stress_shader = renderer_->createCustomShader(STRESS_VS, STRESS_FS);

        // Create noise texture for TMU stress
        int tex_size = 1024;
        std::vector<unsigned char> noise = genColorNoise(tex_size, 42);
        TextureHandle stress_tex = renderer_->createTexture(tex_size, tex_size, 3, noise.data());

        int u_tex_loc = -1, u_iter_loc = -1, u_time_loc = -1;
        if (stress_shader != INVALID_SHADER) {
            u_tex_loc = renderer_->getCustomUniformLoc(stress_shader, "u_tex");
            u_iter_loc = renderer_->getCustomUniformLoc(stress_shader, "u_iterations");
            u_time_loc = renderer_->getCustomUniformLoc(stress_shader, "u_time");
        }

        int iterations = current_preset_.shader_alu.iterations;

        // Helper lambda-like: render one stress pass
        #define STRESS_RENDER_PASS(time_val) do { \
            if (stress_shader != INVALID_SHADER) { \
                renderer_->useCustomShader(stress_shader); \
                renderer_->setUniform1i(u_iter_loc, iterations); \
                renderer_->setUniform1f(u_time_loc, time_val); \
                renderer_->setUniform1i(u_tex_loc, 0); \
            } \
            renderer_->bindTexture(stress_tex); \
            renderer_->drawMesh(stress_quad); \
        } while(0)

        // --- Calibration: find pass count to hit ~40ms per frame ---
        const double TARGET_FRAME_MS = 40.0;
        int passes = 1;
        float stress_time = 0.0f;

        Log::info("Stress test: calibrating...");

        renderer_->setViewport(0, 0, render_w_, render_h_);
        renderer_->setDepthTest(false);
        renderer_->setBlending(true);

        for (int attempt = 0; attempt < 8 && running_; attempt++) {
            processEvents();
            // Warmup pass
            renderer_->clear(0.1f, 0.1f, 0.12f, 1.0f);
            for (int i = 0; i < passes; i++) STRESS_RENDER_PASS(stress_time);
            renderer_->finish();

            // Measure
            Timer cal_t;
            renderer_->clear(0.1f, 0.1f, 0.12f, 1.0f);
            for (int i = 0; i < passes; i++) STRESS_RENDER_PASS(stress_time);
            renderer_->finish();
            double ms = cal_t.elapsed_ms();

            Log::info("  %d passes = %.1f ms", passes, ms);

            if (ms >= TARGET_FRAME_MS * 0.8) break;

            int new_passes = (ms > 0.1)
                ? static_cast<int>(passes * TARGET_FRAME_MS / ms + 0.5) : passes * 4;
            if (new_passes <= passes) new_passes = passes + 1;
            if (new_passes > 10000) new_passes = 10000;
            passes = new_passes;
        }
        ctx_->swapBuffers();

        Log::info("Stress test: combined shader (%s, %d iters, %d passes/frame), %d seconds",
                current_preset_.name, iterations, passes, config_.stress_duration_sec);

        // --- Main stress loop ---
        Timer stress_timer;
        Timer interval_timer;
        const double REPORT_INTERVAL = 10.0;
        int total_frames = 0;
        double interval_time_sum = 0;
        int interval_frames = 0;
        double baseline_fps = 0;
        int interval_num = 0;

        while (running_ && stress_timer.elapsed_sec() < config_.stress_duration_sec) {
            processEvents();

            renderer_->setViewport(0, 0, render_w_, render_h_);
            renderer_->setDepthTest(false);
            renderer_->setBlending(true);
            renderer_->clear(0.1f, 0.1f, 0.12f, 1.0f);

            renderer_->finish();
            Timer frame_t;
            for (int i = 0; i < passes; i++) {
                STRESS_RENDER_PASS(stress_time);
                stress_time += 0.001f;
            }
            renderer_->finish();
            double ms = frame_t.elapsed_ms();

            interval_time_sum += ms;
            interval_frames++;
            total_frames++;

            ctx_->swapBuffers();

            // Periodic report
            if (interval_timer.elapsed_sec() >= REPORT_INTERVAL) {
                interval_num++;
                double avg_ms = interval_time_sum / interval_frames;
                double fps = (avg_ms > 0) ? 1000.0 / avg_ms : 0;

                if (interval_num == 1) {
                    baseline_fps = fps;
                    Log::info("[%3.0fs] Baseline: %.1f FPS (avg %.1f ms)",
                            stress_timer.elapsed_sec(), fps, avg_ms);
                } else {
                    double degradation = (baseline_fps > 0)
                        ? (baseline_fps - fps) / baseline_fps * 100.0 : 0;
                    if (degradation > 5.0) {
                        Log::warn("[%3.0fs] %.1f FPS (avg %.1f ms) — THROTTLING: %.1f%% degradation",
                                stress_timer.elapsed_sec(), fps, avg_ms, degradation);
                    } else if (degradation > 2.0) {
                        Log::info("[%3.0fs] %.1f FPS (avg %.1f ms) — minor degradation: %.1f%%",
                                stress_timer.elapsed_sec(), fps, avg_ms, degradation);
                    } else {
                        Log::info("[%3.0fs] %.1f FPS (avg %.1f ms)",
                                stress_timer.elapsed_sec(), fps, avg_ms);
                    }
                }

                interval_time_sum = 0;
                interval_frames = 0;
                interval_timer.reset();
            }
        }

        #undef STRESS_RENDER_PASS

        renderer_->destroyMesh(stress_quad);
        renderer_->destroyTexture(stress_tex);
        if (stress_shader != INVALID_SHADER)
            renderer_->destroyCustomShader(stress_shader);
        ctx_->setVSync(true);

        double total_sec = stress_timer.elapsed_sec();
        Log::info("Stress test completed: %d frames in %.0fs (avg %.1f FPS)",
                total_frames, total_sec,
                (total_sec > 0) ? total_frames / total_sec : 0);
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

    frame_timer_.reset();
    while (running_) {
        double dt = frame_timer_.elapsed_sec();
        frame_timer_.reset();
        last_frame_time_ = dt;

        processEvents();

        if (!benchmarking_) {
            renderer_->setViewport(0, 0, window_w_, window_h_);
            renderer_->clear(0.12f, 0.12f, 0.15f, 1.0f);
            renderPreviewScene(static_cast<float>(dt));
            renderer_->unbindState();
        }

        renderer_->setViewport(0, 0, window_w_, window_h_);
        renderUI();

        ctx_->swapBuffers();

        if (pending_action_ != ACTION_NONE) {
            PendingAction action = pending_action_;
            pending_action_ = ACTION_NONE;
            if (action == ACTION_RUN_ALL) {
                for (int i = 0; i < NUM_TESTS; i++) test_enabled_[i] = true;
            }
            runSelectedTests();
        }
    }
}
