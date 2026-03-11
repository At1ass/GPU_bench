#pragma once
#include "renderer/renderer.h"
#include "renderer/render_context.h"
#include "bench/bench.h"
#include "tests/tests.h"
#include "bench/preset.h"
#include "tests/test_registry.h"
#include "bench/bench_runner.h"
#include "ui/bench_ui.h"
#include "bench/stress_runner.h"
#include "platform/hwinfo.h"
#include "platform/timer.h"
#include <SDL.h>
#include <memory>
#include <string>

enum class OutputFormat {
    Text = 0,
    CSV,
    JSON
};

// TimingMode is defined in bench_runner.h

#include "renderer/renderer_backend.h"

struct AppConfig {
    int width;
    int height;
    int render_width;   // 0 = use window size (native)
    int render_height;  // 0 = use window size (native)
    int preset_index;
    RendererBackend backend;
    bool headless;
    OutputFormat output_format;
    TimingMode timing_mode;
    std::string output_file;
    std::string config_path;
    std::string test_filter;  // comma-separated test names or "all"
    int stress_duration_sec;  // 0 = disabled
};

// NUM_TESTS is defined in test_registry.h

// -1 = native, 0..N = index into RESOLUTIONS[]
static constexpr int RES_NATIVE = -1;

class App : public BenchCallbacks {
public:
    App();
    ~App();

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

    static const ResolutionOption RESOLUTIONS[];
    static const int NUM_RESOLUTIONS;
};
