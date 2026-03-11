#pragma once
#include "renderer.h"
#include "bench.h"
#include "tests.h"
#include "preset.h"
#include "hwinfo.h"
#include "timer.h"
#include <SDL.h>
#include <vector>
#include <string>

enum OutputFormat {
    OUTPUT_TEXT = 0,
    OUTPUT_CSV,
    OUTPUT_JSON
};

struct AppConfig {
    int width;
    int height;
    int preset_index;
    int force_gl;      // 0=auto, 2=GL2, 3=GL3
    bool headless;
    OutputFormat output_format;
    std::string output_file;
    std::string config_path;
    std::string test_filter;  // comma-separated test names or "all"
};

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

    AppConfig     config_;
    SDL_Window*   window_;
    SDL_GLContext  gl_context_;
    Renderer*     renderer_;
    HWInfo        hw_info_;

    bool running_;
    bool benchmarking_;
    bool initialized_;
    int  window_w_, window_h_;

    // Timing
    Timer frame_timer_;
    double last_frame_time_;

    // Preset state
    BenchPreset   current_preset_;
    int           selected_preset_index_; // -1 = custom
    int           suggested_preset_;
    std::string   validation_error_;

    // Test selection checkboxes
    bool test_enabled_[10];
    static const char* test_names_[10];

    // Benchmark state
    std::vector<BenchResult> results_;
    std::string bench_status_;
    int bench_progress_;

    // Deferred actions (set by UI, executed in run loop)
    enum PendingAction { ACTION_NONE, ACTION_RUN_SELECTED, ACTION_RUN_ALL };
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
};
