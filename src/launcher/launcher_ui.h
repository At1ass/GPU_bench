#pragma once
#include "platform/gpu_select.h"
#include "platform/hwinfo.h"
#include <string>
#include <vector>

// Read-only snapshot of detected hardware for UI display
struct LauncherView {
    const HWInfo* hw_info;
    const std::vector<GPUDevice>* gpus;
};

// Mutable widget state edited by ImGui in-place
struct LauncherState {
    // Mode: 0 = benchmark, 1 = demo
    int mode;

    // Common parameters
    int gpu_index;          // -1 = default
    int backend;            // 0=auto, 1=gl2, 2=gl3, 3=gl4, 4=gles
    int width, height;
    int render_width, render_height;
    bool use_render_res;
    bool headless;
    bool debug;
    bool verbose;

    // Benchmark-specific
    int preset;             // 0-4 (Light..Extreme)
    int output_format;      // 0=text, 1=csv, 2=json
    int timing_mode;        // 0=sync, 1=gpu
    char output_file[256];
    char test_filter[512];
    int stress_seconds;

    // Demo-specific
    int tier;               // 0=auto, 1-4
    int duration;

    LauncherState();
};

// Action requested by the UI, dispatched by main
struct LaunchAction {
    enum Type { None, LaunchBenchmark, LaunchDemo };
    Type type;
    std::vector<std::string> args;

    LaunchAction() : type(None) {}
};

// Renders the launcher UI panel. Stateless — all data flows through View/State.
class LauncherUI {
public:
    LauncherUI() = default;

    // Draw the full launcher panel. Returns the action the user triggered (if any).
    LaunchAction render(const LauncherView& view, LauncherState& state);

private:
    void drawHardwareInfo(const LauncherView& view);
    void drawModeSelector(LauncherState& state);
    void drawCommonSettings(const LauncherView& view, LauncherState& state);
    void drawBenchmarkSettings(LauncherState& state);
    void drawDemoSettings(LauncherState& state);
    void drawCommandPreview(const LauncherState& state);
};

// Build CLI argument vector from current state
std::vector<std::string> buildLaunchArgs(const LauncherState& state);

// Build command preview string from current state
std::string buildCommandPreview(const LauncherState& state);
