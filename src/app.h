#pragma once
#include "renderer.h"
#include "render_context.h"
#include "bench.h"
#include "tests.h"
#include "preset.h"
#include "hwinfo.h"
#include "timer.h"
#include <SDL.h>
#include <memory>
#include <vector>
#include <string>

enum class OutputFormat {
    Text = 0,
    CSV,
    JSON
};

enum class TimingMode {
    Sync = 0,  // CPU timer + glFinish
    GPU        // GPU timer queries
};

#include "renderer_backend.h"

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

static constexpr int NUM_TESTS = 12;

struct ResolutionOption {
    int w, h;
    const char* label;
};

// -1 = native, 0..N = index into RESOLUTIONS[]
static constexpr int RES_NATIVE = -1;

class App {
public:
    App();
    ~App();

    bool init(const AppConfig& cfg);
    void run();
    void shutdown();

private:
    void processEvents();
    void renderUI();
    void renderPreviewScene(float dt);
    void runTest(BenchTest* test);
    void runAllTests();
    void runSelectedTests();
    void runHeadless();
    void exportResults();
    bool isTestSelected(const char* name) const;

    AppConfig       config_;
    std::unique_ptr<RenderContext>  ctx_;
    std::unique_ptr<Renderer>       renderer_;
    HWInfo        hw_info_;

    bool running_;
    bool benchmarking_;
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
    static const char* test_names_[NUM_TESTS];

    // Benchmark state
    std::vector<BenchResult> results_;
    CompositeScore composite_score_;
    BottleneckInfo bottleneck_info_;
    std::string bench_status_;
    int bench_progress_;

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
    void computeAnalysis();
    void updateRenderResolution();
    void runQuickProbe();

    // Render target for off-screen rendering at non-native resolution
    RenderTargetHandle bench_rt_;

    GPUTier gpu_tier_;

    static const ResolutionOption RESOLUTIONS[];
    static const int NUM_RESOLUTIONS;
};
