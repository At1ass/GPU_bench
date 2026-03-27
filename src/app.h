#pragma once
#include "renderer/renderer.h"
#include "renderer/render_context.h"
#include "bench/bench.h"
#include "bench/preset.h"
#include "tests/test_registry.h"
#include "bench/bench_runner.h"
#include "bench/results.h"
#include "ui/bench_ui.h"
#include "demo/demo_runner.h"
#include "demo/demo_ui.h"
#include "platform/hwinfo.h"
#include "platform/timer.h"
#include <SDL.h>
#include <memory>
#include <string>

// OutputFormat is defined in bench/results.h
// TimingMode is defined in bench_runner.h

#include "renderer/renderer_backend.h"

struct AppConfig {
    int width  = 800;
    int height = 600;
    int render_width  = 0;      // 0 = use window size (native)
    int render_height = 0;      // 0 = use window size (native)
    int preset_index  = static_cast<int>(PresetIndex::Medium);
    RendererBackend backend = RendererBackend::Auto;
    bool headless = false;
    OutputFormat output_format = OutputFormat::Text;
    TimingMode timing_mode = TimingMode::Sync;
    std::string output_file;
    std::string config_path;
    std::string test_filter = "all";    // comma-separated test names or "all"
    int stress_duration_sec = 0;        // 0 = disabled
    bool demo_mode    = false;          // --demo
    int demo_tier     = 0;              // 0 = auto, 1-4 = specific tier
    int demo_duration = 15;             // seconds per tier
    int gpu_index     = -1;              // -1 = default, >=0 = select GPU
    bool debug        = false;           // --debug: enable debug logging

    AppConfig() = default;
    AppConfig(const AppConfig&) = default;
    AppConfig& operator=(const AppConfig&) = default;
    AppConfig(AppConfig&&) noexcept = default;
    AppConfig& operator=(AppConfig&&) noexcept = default;
};

// NUM_TESTS is defined in test_registry.h

// -1 = native, 0..N = index into RESOLUTIONS[]
static constexpr int ResNative = -1;

class App : public BenchCallbacks {
public:
    App();
    ~App() { shutdown(); }

    bool init(const AppConfig& cfg);
    void run();
    void shutdown();

    // BenchCallbacks implementation
    bool onFrame(RenderTargetHandle rt) override;
    bool onPoll() override;

private:
    void processEvents();
    void renderUI();
    void renderPreviewScene(float dt);
    void runSelectedTests();
    void runHeadless();
    void exportResults();
    void runDemo();
    void exportDemoResults(const DemoResults& results);
    bool isTestSelected(const char* name) const;

    AppConfig       config_;
    std::unique_ptr<RenderContext>  ctx_;
    std::unique_ptr<Renderer>       renderer_;
    HWInfo        hw_info_;

    bool running_;
    bool initialized_;
    int  window_w_, window_h_;
    int  render_w_, render_h_;     // actual test rendering resolution
    int  selected_resolution_;     // index into resolution list, -1 = native

    // Timing
    Timer frame_timer_;
    double last_frame_time_;

    // Preset state
    BenchPreset   current_preset_;
    int           selected_preset_index_; // -1 = custom
    int           suggested_preset_;
    std::string   validation_error_;

    // Test selection checkboxes
    bool test_enabled_[NUM_TESTS];

    // Benchmark engine (owns results, composite score, bottleneck info)
    BenchRunner bench_runner_;

    // UI layer
    BenchUI bench_ui_;

    // Deferred actions (set by UI, executed in run loop)
    enum class PendingAction { None, RunSelected, RunAll };
    PendingAction pending_action_;

    // Preview scene (shown when idle)
    MeshHandle    preview_terrain_;
    MeshHandle    preview_sphere_;
    TextureHandle preview_tex_;
    float         preview_angle_;
    bool          preview_ready_;

    void setupPreviewScene();
    void cleanupPreviewScene();
    void validateCurrentPreset();
    void applyPreset(int index);
    void updateRenderResolution();

    GPUTier gpu_tier_;
    uint32_t cached_caps_ = 0;
    DemoUI demo_ui_;
    DemoResults demo_results_;

    static const ResolutionOption RESOLUTIONS[];
    static const int NUM_RESOLUTIONS;
};
