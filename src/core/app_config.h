#pragma once
#include "renderer/renderer_backend.h"
#include <string>

// Output format for benchmark/demo results export
enum class OutputFormat : int { Text = 0, CSV, JSON };

// Timing measurement mode for benchmarks
enum class TimingMode {
    Sync = 0,  // CPU timer + glFinish
    GPU        // GPU timer queries
};

// Shared application configuration.
// Contains only parameters needed by the core library (renderer, window, platform).
// Mode-specific parameters (preset, demo tier, etc.) belong in each binary's main.
struct AppConfig {
    int width  = 800;
    int height = 600;
    int render_width  = 0;      // 0 = use window size (native)
    int render_height = 0;
    RendererBackend backend = RendererBackend::Auto;
    bool headless = false;
    OutputFormat output_format = OutputFormat::Text;
    TimingMode timing_mode = TimingMode::Sync;
    std::string output_file;
    int gpu_index = -1;         // -1 = default, >=0 = select GPU
    bool debug = false;         // --debug: enable debug logging (Debug level)
    bool verbose = false;       // --debug=verbose: enable trace logging (Trace level, GL debug groups)

    // Bench-specific (kept here for backward compat during transition)
    int preset_index = 1;       // PresetIndex::Medium
    std::string config_path;
    std::string test_filter = "all";
    int stress_duration_sec = 0;

    // Demo-specific (kept here for backward compat during transition)
    bool demo_mode = false;
    int demo_tier = 0;          // 0 = auto
    int demo_duration = 15;

    AppConfig() = default;
    AppConfig(const AppConfig&) = default;
    AppConfig& operator=(const AppConfig&) = default;
    AppConfig(AppConfig&&) noexcept = default;
    AppConfig& operator=(AppConfig&&) noexcept = default;
};
