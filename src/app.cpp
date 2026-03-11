#include "app.h"
#include "mesh_gen.h"
#include "gl_funcs.h"
#include "renderer_factory.h"
#include "config.h"
#include "results.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include "compat.h"
#include "logger.h"
#include <cstdio>
#include <cmath>
#include <cstring>

static const float PREVIEW_ROT_SPEED = 0.3f;

// Unbind our renderer state before ImGui rendering.
// ImGui GL3 backend saves and restores all GL state, so a full reset is not needed.
static void resetGLStateForImGui() {
    glUseProgram(0);
#ifdef CB_NEED_GL_LOAD
    if (imgl3wProcs.gl.BindVertexArray)
        glBindVertexArray(0);
#else
    glBindVertexArray(0);
#endif
}

const char* App::test_names_[10] = {
    "Fillrate", "Geometry", "Texturing", "Scene", "DrawCall",
    "Overdraw", "TexUpload", "StateChange", "Vertex", "ShaderALU"
};

App::App()
    : window_(0), gl_context_(0), renderer_(0),
      running_(false), benchmarking_(false),
      initialized_(false), window_w_(800), window_h_(600),
      last_frame_time_(0.0),
      selected_preset_index_(PRESET_MEDIUM), suggested_preset_(PRESET_MEDIUM),
      bench_progress_(0),
      pending_action_(ACTION_NONE),
      preview_terrain_(INVALID_MESH), preview_sphere_(INVALID_MESH),
      preview_tex_(INVALID_TEXTURE), preview_angle_(0.0f), preview_ready_(false)
{
    for (int i = 0; i < 10; i++) test_enabled_[i] = true;
    current_preset_ = getPreset(PRESET_MEDIUM);
}

App::~App() { shutdown(); }

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
            fprintf(stderr, "Warning: could not load config '%s'\n", cfg.config_path.c_str());
        }
    } else {
        applyPreset(cfg.preset_index);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint32 win_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (cfg.headless) {
        win_flags |= SDL_WINDOW_HIDDEN;
    }

    window_ = SDL_CreateWindow(
        "GPU_benchmark v0.2.0",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_w_, window_h_,
        win_flags
    );
    if (!window_) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // Try GL 3.2 compatibility context first (needed for GL3 renderer + ImGui OpenGL3 backend).
    // Fall back to GL 2.1 if that fails (old hardware / drivers).
    gl_context_ = 0;
    if (cfg.force_gl != 2) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        gl_context_ = SDL_GL_CreateContext(window_);
        if (gl_context_) {
            Log::info("Created GL 3.2 compatibility context");
        }
    }
    if (!gl_context_) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
        gl_context_ = SDL_GL_CreateContext(window_);
        if (gl_context_) {
            Log::info("Created GL 2.1 context");
        }
    }
    if (!gl_context_) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    if (!loadGLFunctions()) {
        fprintf(stderr, "Failed to load OpenGL 2.0 functions\n");
        return false;
    }

    // Init ImGui with OpenGL3 backend (modern GL, shader-based).
    // GL3 backend uses its own shaders, VBOs, VAOs and properly saves/restores
    // all GL state. Its built-in loader (imgl3w) uses wglGetProcAddress with
    // GetProcAddress(opengl32.dll) fallback for GL 1.x — correct for Wine.
    if (!cfg.headless) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
        // Pick GLSL version matching the GL context we got
        const char* glsl_version = "#version 120";  // GL 2.1 default
        const char* gl_ver_str = (const char*)glGetString(GL_VERSION);
        if (gl_ver_str) {
            int gl_major = 0;
            sscanf(gl_ver_str, "%d", &gl_major);
            if (gl_major >= 3)
                glsl_version = "#version 150";
        }
        ImGui_ImplOpenGL3_Init(glsl_version);
    }

    renderer_ = createRenderer(cfg.force_gl);
    if (!renderer_ || !renderer_->init(window_w_, window_h_)) {
        fprintf(stderr, "Renderer init failed\n");
        return false;
    }
    hw_info_ = HWInfo::detect();
    suggested_preset_ = suggestPresetIndex(renderer_->getCaps());
    validateCurrentPreset();

    // Parse test filter for headless mode
    if (cfg.test_filter != "all") {
        for (int i = 0; i < 10; i++) test_enabled_[i] = false;
        // Parse comma-separated list
        std::string filter = cfg.test_filter;
        size_t pos = 0;
        while (pos < filter.size()) {
            size_t comma = filter.find(',', pos);
            if (comma == std::string::npos) comma = filter.size();
            std::string name = filter.substr(pos, comma - pos);
            // Trim
            while (!name.empty() && name[0] == ' ') name.erase(0, 1);
            while (!name.empty() && name.back() == ' ') name.pop_back();
            for (int i = 0; i < 10; i++) {
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

    if (!config_.headless) {
        cleanupPreviewScene();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    if (renderer_) {
        renderer_->shutdown();
        delete renderer_;
        renderer_ = 0;
    }
    if (gl_context_) { SDL_GL_DeleteContext(gl_context_); gl_context_ = 0; }
    if (window_) { SDL_DestroyWindow(window_); window_ = 0; }
    SDL_Quit();
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
    while (SDL_PollEvent(&e)) {
        if (!config_.headless) {
            ImGui_ImplSDL2_ProcessEvent(&e);
        }
        if (e.type == SDL_QUIT) running_ = false;
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            window_w_ = e.window.data1;
            window_h_ = e.window.data2;
        }
    }
}

void App::renderPreviewScene(float dt) {
    if (!preview_ready_) return;

    int panel_h = (int)(window_h_ * 0.55f);
    int view_h = window_h_ - panel_h;
    if (view_h <= 0) return;

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, panel_h, window_w_, view_h);
    renderer_->setViewport(0, panel_h, window_w_, view_h);
    renderer_->clear(0.08f, 0.08f, 0.10f, 1.0f);

    renderer_->setDepthTest(true);
    renderer_->setBlending(false);
    renderer_->useShader(Renderer::SHADER_3D);

    float aspect = (float)window_w_ / (float)view_h;
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
        float a = (float)i / 5.0f * 6.28318f + preview_angle_ * 0.5f;
        renderer_->setColor(colors[i][0], colors[i][1], colors[i][2], 1.0f);
        renderer_->setModel(Mat4::translate(cosf(a) * 8.0f, 4.0f + sinf(a * 2.0f) * 1.5f, sinf(a) * 8.0f)
                           * Mat4::scale(1.2f, 1.2f, 1.2f));
        renderer_->drawMesh(preview_sphere_);
    }

    glDisable(GL_SCISSOR_TEST);
    preview_angle_ += PREVIEW_ROT_SPEED * dt;
}

bool App::isTestSelected(const char* name) const {
    for (int i = 0; i < 10; i++) {
        if (strcmp(name, test_names_[i]) == 0)
            return test_enabled_[i];
    }
    return false;
}

void App::renderUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    float panel_h = window_h_ * 0.55f;
    ImGui::SetNextWindowPos(ImVec2(0, window_h_ - panel_h));
    ImGui::SetNextWindowSize(ImVec2((float)window_w_, panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("##panel", NULL, flags);

    // ---- Hardware info ----
    ImGui::Text("CPU: %s", hw_info_.cpu_name.c_str());
    ImGui::SameLine((float)window_w_ * 0.5f);
    ImGui::Text("GPU: %s", renderer_->getGPURenderer());
    ImGui::Text("OS: %s %s", hw_info_.os_name.c_str(), hw_info_.os_version.c_str());
    ImGui::SameLine((float)window_w_ * 0.5f);
    ImGui::Text("GL: %s  Renderer: %s", renderer_->getGLVersion(), renderer_->getRendererName());
    const RenderCaps& caps = renderer_->getCaps();
    if (caps.estimated_vram_mb > 0) {
        ImGui::SameLine();
        ImGui::Text("  VRAM: %d MB", caps.estimated_vram_mb);
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

        // Show custom preset indicator
        if (selected_preset_index_ < 0) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[Custom]");
        }

        // Validation error
        if (!validation_error_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", validation_error_.c_str());
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

            if (changed) {
                selected_preset_index_ = -1; // Now "Custom"
                current_preset_.name = "Custom";
                validateCurrentPreset();
            }

            ImGui::TreePop();
        }

        // Clamp values
        if (current_preset_.warmup_frames < 30) current_preset_.warmup_frames = 30;
        if (current_preset_.measure_frames < 60) current_preset_.measure_frames = 60;

        ImGui::Separator();

        // ---- Test selection ----
        ImGui::Text("Tests:");
        for (int i = 0; i < 10; i++) {
            if (i > 0 && i % 5 != 0) ImGui::SameLine();
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
        }
        ImGui::SameLine();
        if (!results_.empty() && ImGui::Button("Export", ImVec2(60, 0))) {
            exportResults();
        }
    } else {
        // Benchmarking in progress
        ImGui::Text("Status: %s", bench_status_.c_str());
        ImGui::ProgressBar(bench_progress_ / 100.0f, ImVec2(-1, 0));
    }

    ImGui::Separator();

    // ---- Results table ----
    if (!results_.empty() && ImGui::BeginTable("results", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp
            | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Test",       ImGuiTableColumnFlags_None, 1.0f);
        ImGui::TableSetupColumn("Score",      ImGuiTableColumnFlags_None, 1.2f);
        ImGui::TableSetupColumn("Unit",       ImGuiTableColumnFlags_None, 0.7f);
        ImGui::TableSetupColumn("Avg (ms)",   ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("Min (ms)",   ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("Max (ms)",   ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableSetupColumn("Median (ms)",ImGuiTableColumnFlags_None, 0.8f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < results_.size(); i++) {
            const BenchResult& r = results_[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", r.name.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", r.score);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", r.unit.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", r.avg_ms);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f", r.min_ms);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%.3f", r.max_ms);
            ImGui::TableSetColumnIndex(6); ImGui::Text("%.3f", r.median_ms);
        }
        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void App::runTest(BenchTest* test) {
    benchmarking_ = true;
    bench_status_ = std::string("Running: ") + test->name();
    bench_progress_ = 0;

    SDL_GL_SetSwapInterval(0);
    test->setup(renderer_, window_w_, window_h_);

    // Warmup
    bench_status_ = std::string("Warmup: ") + test->name();
    for (int i = 0; i < current_preset_.warmup_frames && running_; i++) {
        processEvents();
        renderer_->setViewport(0, 0, window_w_, window_h_);
        renderer_->clear(0.1f, 0.1f, 0.12f, 1.0f);
        test->render(renderer_);
        glFinish();

        bench_progress_ = (int)(100.0 * i / current_preset_.warmup_frames * 0.1);
        if (!config_.headless) {
            renderer_->setViewport(0, 0, window_w_, window_h_);
            renderer_->setDepthTest(false);
            renderer_->setBlending(false);
            resetGLStateForImGui();
            renderUI();
        }
        SDL_GL_SwapWindow(window_);
    }

    // Measurement
    bench_status_ = std::string("Measuring: ") + test->name();
    std::vector<double> times;
    times.reserve(current_preset_.measure_frames * 2);
    Timer total_timer;
    Timer frame_t;

    int frame = 0;
    while (running_) {
        processEvents();
        renderer_->setViewport(0, 0, window_w_, window_h_);
        renderer_->clear(0.1f, 0.1f, 0.12f, 1.0f);

        // Pipeline barrier: ensure GPU is fully idle before timing.
        // Without this, glFinish() after render() would also wait for
        // the clear() above and any ImGui residual from previous frame.
        glFinish();

        frame_t.reset();
        test->render(renderer_);
        glFinish();
        double ms = frame_t.elapsed_ms();
        times.push_back(ms);

        double elapsed = total_timer.elapsed_sec();
        bench_progress_ = 10 + (int)(90.0 * std::min(
            (double)frame / current_preset_.measure_frames,
            elapsed / current_preset_.min_duration_sec
        ));

        if (!config_.headless) {
            renderer_->setViewport(0, 0, window_w_, window_h_);
            renderer_->setDepthTest(false);
            renderer_->setBlending(false);
            resetGLStateForImGui();
            renderUI();
        }

        SDL_GL_SwapWindow(window_);
        frame++;

        if (frame >= current_preset_.measure_frames && elapsed >= current_preset_.min_duration_sec)
            break;
    }

    double score = test->computeScore(times, window_w_, window_h_);
    results_.push_back(computeStats(test->name(), test->scoreUnit(), times, score));

    test->cleanup(renderer_);
    benchmarking_ = false;
    bench_status_ = "Done";
    SDL_GL_SetSwapInterval(1);
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
}

void App::runAllTests() {
    for (int i = 0; i < 10; i++) test_enabled_[i] = true;
    runSelectedTests();
}

void App::exportResults() {
    FILE* out = stdout;
    bool close_file = false;

    if (!config_.output_file.empty()) {
        out = fopen(config_.output_file.c_str(), "w");
        if (!out) {
            fprintf(stderr, "Could not open output file: %s\n", config_.output_file.c_str());
            return;
        }
        close_file = true;
    }

    const char* gpu = renderer_->getGPURenderer();
    const char* gl_ver = renderer_->getGLVersion();
    const char* rname = renderer_->getRendererName();
    const char* pname = current_preset_.name;

    switch (config_.output_format) {
        case OUTPUT_CSV:
            exportCSV(out, results_, hw_info_, renderer_->getCaps(), gpu, gl_ver, rname, pname);
            break;
        case OUTPUT_JSON:
            exportJSON(out, results_, hw_info_, renderer_->getCaps(), gpu, gl_ver, rname, pname);
            break;
        default:
            exportText(out, results_, hw_info_, renderer_->getCaps(), gpu, gl_ver, rname, pname);
            break;
    }

    if (close_file) fclose(out);
}

void App::runHeadless() {
    if (!validation_error_.empty()) {
        fprintf(stderr, "Preset validation failed: %s\n", validation_error_.c_str());
        return;
    }

    runSelectedTests();
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
            glViewport(0, 0, window_w_, window_h_);
            glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderPreviewScene((float)dt);
            resetGLStateForImGui();
        }

        glViewport(0, 0, window_w_, window_h_);
        renderUI();

        SDL_GL_SwapWindow(window_);

        // Handle deferred actions after ImGui frame is fully rendered
        if (pending_action_ != ACTION_NONE) {
            PendingAction action = pending_action_;
            pending_action_ = ACTION_NONE;
            if (action == ACTION_RUN_ALL) {
                for (int i = 0; i < 10; i++) test_enabled_[i] = true;
            }
            runSelectedTests();
        }
    }
}
