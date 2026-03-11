#pragma once
#include "bench/bench.h"
#include "platform/hwinfo.h"
#include "bench/preset.h"
#include "tests/test_registry.h"
#include <string>
#include <vector>

class RenderContext;
struct RenderCaps;

struct ResolutionOption {
    int w, h;
    const char* label;
};

// Read-only snapshot of application state for UI rendering
struct UIView {
    // Hardware info
    const HWInfo*      hw_info;
    const char*        gpu_renderer;   // renderer->getGPURenderer()
    const char*        gl_version;     // renderer->getGLVersion()
    const char*        renderer_name;  // renderer->getRendererName()
    const RenderCaps*  caps;
    bool               supports_render_targets;
    GPUTier            gpu_tier;

    // Dimensions
    int window_w, window_h;
    int render_w, render_h;

    // Preset hint from GPU probe
    int suggested_preset;

    // BenchRunner state
    bool                              bench_active;
    const std::string*                bench_status;
    int                               bench_progress;
    const std::vector<BenchResult>*   results;
    const CompositeScore*             composite;
    const BottleneckInfo*             bottleneck;

    // Resolution options table (owned by App)
    const ResolutionOption*           resolutions;
    int                               num_resolutions;

    // Validation (read-only for controls enable/disable)
    const std::string*                validation_error;
};

// Mutable state that ImGui widgets edit in-place
struct UIState {
    BenchPreset*  current_preset;
    int*          selected_preset_index;  // -1 = custom
    bool*         test_enabled;           // array [NUM_TESTS]
    int*          selected_resolution;    // -1 = native
    std::string*  validation_error;
};

// Action requested by the UI, dispatched by App
struct UIAction {
    enum Type {
        None,
        RunSelected,
        RunAll,
        Clear,
        Export,
        ApplyPreset,         // preset_index is valid
        ResolutionChanged,
        CustomParamsChanged
    };

    Type type;
    int  preset_index;       // valid when type == ApplyPreset

    UIAction() : type(None), preset_index(-1) {}
};

// Renders the benchmark UI panel. Stateless — all data flows through UIView/UIState.
class BenchUI {
public:
    BenchUI() = default;

    // Draw the full UI panel. Returns the action the user triggered (if any).
    UIAction render(RenderContext* ctx, const UIView& view, UIState& state);

private:
    void drawHardwareInfo(const UIView& view);
    void drawPresetSelector(const UIView& view, UIState& state, UIAction& out);
    void drawResolutionSelector(const UIView& view, UIState& state, UIAction& out);
    void drawCustomParams(UIState& state, UIAction& out);
    void drawTestSelector(const UIView& view, UIState& state);
    void drawControls(const UIView& view, UIAction& out);
    void drawCompositeScore(const UIView& view);
    void drawBottleneck(const UIView& view);
    void drawResultsTable(const UIView& view);
};
