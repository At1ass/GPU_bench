#pragma once
#include "bench/bench.h"
#include "tests/test_registry.h"
#include "platform/timer.h"
#include <vector>
#include <string>

class RenderContext;
struct BenchPreset;

// Timing mode for benchmark measurements
enum class TimingMode {
    Sync = 0,  // CPU timer + glFinish
    GPU        // GPU timer queries
};

// Callback interface for frame presentation during benchmarking.
// Implemented by the application to handle events, UI, and buffer swaps.
class BenchCallbacks {
public:
    virtual ~BenchCallbacks() {}

    // Called each frame during warmup/measurement.
    // rt = active render target (INVALID_RENDER_TARGET if rendering to default FB).
    // Returns false to abort the benchmark.
    virtual bool onFrame(RenderTargetHandle rt) = 0;

    // Lightweight poll (for probe): process events, check if still running.
    // Returns false to abort.
    virtual bool onPoll() { return true; }
};

// Configuration for a benchmark run
struct BenchConfig {
    int warmup_frames = 60;
    int measure_frames = 120;
    double min_duration_sec = 3.0;
    TimingMode timing_mode = TimingMode::Sync;
    bool headless = false;
    int render_w = 800;
    int render_h = 600;
    int window_w = 800;
    int window_h = 600;
};

// Encapsulates benchmark test execution, result collection, and analysis.
class BenchRunner {
public:
    BenchRunner();

    // Execute selected tests based on test_enabled flags.
    // available_caps: bitmask from getAvailableCaps() — tests requiring
    // unsupported caps are skipped automatically.
    void runSelected(Renderer* r, RenderContext* ctx,
                     const BenchPreset& preset, const BenchConfig& cfg,
                     const bool* test_enabled, uint32_t available_caps,
                     BenchCallbacks* cb);

    // Execute all tests
    void runAll(Renderer* r, RenderContext* ctx,
                const BenchPreset& preset, const BenchConfig& cfg,
                uint32_t available_caps, BenchCallbacks* cb);

    // Quick GPU probe for tier classification
    GPUTier runQuickProbe(Renderer* r, RenderContext* ctx,
                          int render_w, int render_h,
                          BenchCallbacks* cb);

    // Clear accumulated results
    void clearResults();

    // Result accessors
    const std::vector<BenchResult>& results() const { return results_; }
    const CompositeScore& compositeScore() const { return composite_; }
    const BottleneckInfo& bottleneckInfo() const { return bottleneck_; }

    // Status (readable during execution via callbacks)
    const std::string& status() const { return status_; }
    int progress() const { return progress_; }
    bool isActive() const { return active_; }

private:
    void runTest(BenchTest* test, Renderer* r, RenderContext* ctx,
                 const BenchConfig& cfg, BenchCallbacks* cb);
    void computeAnalysis();

    std::vector<BenchResult> results_;
    CompositeScore composite_;
    BottleneckInfo bottleneck_;
    std::string status_;
    int progress_ = 0;
    bool active_ = false;
    RenderTargetHandle bench_rt_;
};
