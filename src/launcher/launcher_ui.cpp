#include "launcher/launcher_ui.h"
#include <imgui.h>
#include <cstring>

// CLI value tables (must match order of combo indices)
static const char* kPresetNames[]  = { "light", "medium", "heavy", "ultra", "extreme" };
static const char* kBackendNames[] = { "auto", "gl2", "gl3", "gl4", "gles" };
static const char* kOutputNames[]  = { "text", "csv", "json" };
static const char* kTimingNames[]  = { "sync", "gpu" };

static const int kPresetCount  = 5;
static const int kBackendCount = 5;
static const int kOutputCount  = 3;
static const int kTimingCount  = 2;

// Display labels (capitalized for UI)
static const char* kPresetLabels[]  = { "Light", "Medium", "Heavy", "Ultra", "Extreme" };
static const char* kBackendLabels[] = { "Auto", "GL2", "GL3", "GL4", "GLES" };
static const char* kOutputLabels[]  = { "Text", "CSV", "JSON" };
static const char* kTimingLabels[]  = { "Sync (CPU)", "GPU Timer Queries" };
static const char* kTierLabels[]    = { "Auto", "Tier 1 (Basic)", "Tier 2 (Enhanced)", "Tier 3 (Quality)", "Tier 4 (Ultra)" };

// Common resolutions for combo
struct Resolution { int w, h; const char* label; };
static const Resolution kResolutions[] = {
    { 640,  360,  "640x360" },
    { 800,  600,  "800x600" },
    { 1024, 768,  "1024x768" },
    { 1280, 720,  "1280x720 (HD)" },
    { 1920, 1080, "1920x1080 (FHD)" },
    { 2560, 1440, "2560x1440 (QHD)" },
    { 3840, 2160, "3840x2160 (4K)" },
};
static const int kResolutionCount = sizeof(kResolutions) / sizeof(kResolutions[0]);

static int findResolutionIndex(int w, int h) {
    for (int i = 0; i < kResolutionCount; ++i)
        if (kResolutions[i].w == w && kResolutions[i].h == h) return i;
    return -1;
}

LauncherState::LauncherState()
    : mode(0)
    , gpu_index(-1)
    , backend(0)
    , width(800), height(600)
    , render_width(0), render_height(0)
    , use_render_res(false)
    , headless(false)
    , debug(false)
    , verbose(false)
    , preset(1)
    , output_format(0)
    , timing_mode(0)
    , stress_seconds(0)
    , tier(0)
    , duration(15)
{
    memset(output_file, 0, sizeof(output_file));
    strncpy(test_filter, "all", sizeof(test_filter) - 1);
}

// --- Free functions: argument building ---

std::vector<std::string> buildLaunchArgs(const LauncherState& s) {
    std::vector<std::string> args;

    // Common args (only add non-default values)
    if (s.backend != 0) {
        args.push_back("--renderer");
        args.push_back(kBackendNames[s.backend]);
    }
    if (s.gpu_index >= 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", s.gpu_index);
        args.push_back("--gpu");
        args.push_back(buf);
    }
    if (s.width != 800 || s.height != 600) {
        char buf[16];
        args.push_back("--width");
        snprintf(buf, sizeof(buf), "%d", s.width);
        args.push_back(buf);
        args.push_back("--height");
        snprintf(buf, sizeof(buf), "%d", s.height);
        args.push_back(buf);
    }
    if (s.use_render_res && s.render_width > 0 && s.render_height > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%dx%d", s.render_width, s.render_height);
        args.push_back("--render-res");
        args.push_back(buf);
    }
    if (s.headless) args.push_back("--headless");
    if (s.debug) {
        args.push_back(s.verbose ? "--debug=verbose" : "--debug");
    }

    // Mode-specific args
    if (s.mode == 0) {
        if (s.preset != 1) {
            args.push_back("--preset");
            args.push_back(kPresetNames[s.preset]);
        }
        if (strcmp(s.test_filter, "all") != 0 && s.test_filter[0] != '\0') {
            args.push_back("--test");
            args.push_back(s.test_filter);
        }
        if (s.timing_mode != 0) {
            args.push_back("--timing");
            args.push_back(kTimingNames[s.timing_mode]);
        }
        if (s.output_format != 0) {
            args.push_back("--output");
            args.push_back(kOutputNames[s.output_format]);
        }
        if (s.output_file[0] != '\0') {
            args.push_back("--output-file");
            args.push_back(s.output_file);
        }
        if (s.stress_seconds > 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", s.stress_seconds);
            args.push_back("--stress");
            args.push_back(buf);
        }
    } else {
        if (s.tier > 0) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", s.tier);
            args.push_back("--tier");
            args.push_back(buf);
        }
        if (s.duration != 15) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", s.duration);
            args.push_back("--duration");
            args.push_back(buf);
        }
        if (s.output_format != 0) {
            args.push_back("--output");
            args.push_back(kOutputNames[s.output_format]);
        }
        if (s.output_file[0] != '\0') {
            args.push_back("--output-file");
            args.push_back(s.output_file);
        }
    }

    return args;
}

std::string buildCommandPreview(const LauncherState& s) {
    std::string exe = (s.mode == 0) ? "gpu_benchmark" : "gpu_demo";
    std::vector<std::string> args = buildLaunchArgs(s);
    std::string cmd = exe;
    for (size_t i = 0; i < args.size(); ++i) {
        cmd += " ";
        if (args[i].find(' ') != std::string::npos)
            cmd += "\"" + args[i] + "\"";
        else
            cmd += args[i];
    }
    return cmd;
}

// --- LauncherUI: private draw methods ---

void LauncherUI::drawHardwareInfo(const LauncherView& view) {
    if (!ImGui::CollapsingHeader("Hardware Info", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Text("CPU: %s", view.hw_info->cpu_name.c_str());
    ImGui::Text("OS:  %s %s", view.hw_info->os_name.c_str(), view.hw_info->os_version.c_str());

    if (view.gpus->empty()) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "No GPUs detected");
    } else {
        ImGui::Text("GPUs:");
        for (size_t i = 0; i < view.gpus->size(); ++i) {
            const GPUDevice& g = (*view.gpus)[i];
            ImGui::BulletText("[%d] %s (%s)%s",
                g.index, g.name.c_str(), g.vendor.c_str(),
                g.is_integrated ? " [iGPU]" : "");
        }
    }
    ImGui::Separator();
}

void LauncherUI::drawModeSelector(LauncherState& state) {
    ImGui::Text("Mode:");
    ImGui::SameLine();
    ImGui::RadioButton("Benchmark", &state.mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Demo", &state.mode, 1);
    ImGui::Separator();
}

void LauncherUI::drawCommonSettings(const LauncherView& view, LauncherState& state) {
    if (!ImGui::CollapsingHeader("Common Settings", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // GPU selector
    {
        std::string current_label = "Default";
        if (state.gpu_index >= 0 && state.gpu_index < static_cast<int>(view.gpus->size()))
            current_label = (*view.gpus)[static_cast<size_t>(state.gpu_index)].name;

        if (ImGui::BeginCombo("GPU", current_label.c_str())) {
            if (ImGui::Selectable("Default", state.gpu_index < 0))
                state.gpu_index = -1;
            for (size_t i = 0; i < view.gpus->size(); ++i) {
                char label[256];
                snprintf(label, sizeof(label), "[%d] %s",
                    (*view.gpus)[i].index, (*view.gpus)[i].name.c_str());
                if (ImGui::Selectable(label, state.gpu_index == static_cast<int>(i)))
                    state.gpu_index = static_cast<int>(i);
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Combo("Renderer", &state.backend, kBackendLabels, kBackendCount);

    // Window resolution
    {
        int res_idx = findResolutionIndex(state.width, state.height);
        int combo_idx = (res_idx < 0) ? kResolutionCount : res_idx;

        const char* labels[kResolutionCount + 1];
        for (int i = 0; i < kResolutionCount; ++i) labels[i] = kResolutions[i].label;
        labels[kResolutionCount] = "Custom";

        if (ImGui::Combo("Window Resolution", &combo_idx, labels, kResolutionCount + 1)) {
            if (combo_idx < kResolutionCount) {
                state.width  = kResolutions[combo_idx].w;
                state.height = kResolutions[combo_idx].h;
            }
        }
        if (combo_idx >= kResolutionCount) {
            ImGui::InputInt("Width", &state.width);
            ImGui::InputInt("Height", &state.height);
        }
    }

    ImGui::Checkbox("Custom Render Resolution", &state.use_render_res);
    if (state.use_render_res) {
        ImGui::InputInt("Render Width", &state.render_width);
        ImGui::InputInt("Render Height", &state.render_height);
    }

    ImGui::Checkbox("Headless", &state.headless);
    ImGui::SameLine();
    ImGui::Checkbox("Debug", &state.debug);
    if (state.debug) {
        ImGui::SameLine();
        ImGui::Checkbox("Verbose", &state.verbose);
    }
}

void LauncherUI::drawBenchmarkSettings(LauncherState& state) {
    if (!ImGui::CollapsingHeader("Benchmark Settings", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Combo("Preset", &state.preset, kPresetLabels, kPresetCount);
    ImGui::InputText("Test Filter", state.test_filter, sizeof(state.test_filter));
    ImGui::SetItemTooltip("Comma-separated test names, or \"all\"");
    ImGui::Combo("Timing Mode", &state.timing_mode, kTimingLabels, kTimingCount);
    ImGui::Combo("Output Format", &state.output_format, kOutputLabels, kOutputCount);
    ImGui::InputText("Output File", state.output_file, sizeof(state.output_file));
    ImGui::InputInt("Stress Mode (sec)", &state.stress_seconds);
    ImGui::SetItemTooltip("0 = disabled. Nonzero = run stress test for N seconds (headless)");
    if (state.stress_seconds < 0) state.stress_seconds = 0;
}

void LauncherUI::drawDemoSettings(LauncherState& state) {
    if (!ImGui::CollapsingHeader("Demo Settings", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Combo("Tier", &state.tier, kTierLabels, 5);
    ImGui::InputInt("Duration (sec/tier)", &state.duration);
    if (state.duration < 1) state.duration = 1;
    ImGui::Combo("Output Format", &state.output_format, kOutputLabels, kOutputCount);
    ImGui::InputText("Output File", state.output_file, sizeof(state.output_file));
}

void LauncherUI::drawCommandPreview(const LauncherState& state) {
    std::string preview = buildCommandPreview(state);
    ImGui::Text("Command:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "%s", preview.c_str());
}

// --- LauncherUI: public render ---

LaunchAction LauncherUI::render(const LauncherView& view, LauncherState& state) {
    LaunchAction action;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("GPU Benchmark Launcher", NULL,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    drawHardwareInfo(view);
    drawModeSelector(state);
    drawCommonSettings(view, state);
    ImGui::Separator();

    if (state.mode == 0)
        drawBenchmarkSettings(state);
    else
        drawDemoSettings(state);

    ImGui::Separator();
    drawCommandPreview(state);
    ImGui::Spacing();

    const char* launch_label = (state.mode == 0) ? "Launch Benchmark" : "Launch Demo";
    float button_width = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button(launch_label, ImVec2(button_width, 40))) {
        action.type = (state.mode == 0) ? LaunchAction::LaunchBenchmark : LaunchAction::LaunchDemo;
        action.args = buildLaunchArgs(state);
    }

    ImGui::End();
    return action;
}
