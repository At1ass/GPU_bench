#pragma once
#include "demo/demo_scene.h"
#include <vector>
#include <string>

class Renderer;
class RenderContext;
class PollCallback;

struct DemoConfig {
    int tier_override;       // 0 = auto (run all supported), 1-4 = specific tier
    int duration_per_tier;   // seconds per tier (default: 15)

    DemoConfig() : tier_override(0), duration_per_tier(15) {}
};

struct DemoTierResult {
    DemoTier tier;
    double avg_fps;
    double min_fps;
    double p1_fps;
    double p99_fps;
    double avg_ms;
    double target_fps;
    double normalized_score; // fps / target_fps
    int frames;
    bool valid;

    DemoTierResult() : tier(DemoTier::Basic), avg_fps(0), min_fps(0),
        p1_fps(0), p99_fps(0), avg_ms(0), target_fps(60),
        normalized_score(0), frames(0), valid(false) {}
};

struct DemoResults {
    std::vector<DemoTierResult> tiers;
    double demo_score;  // Geometric mean × 10000
    std::string gpu_name;
    int render_w, render_h; // Render resolution used

    DemoResults() : demo_score(0), render_w(0), render_h(0) {}
    DemoResults(const DemoResults&) = default;
    DemoResults& operator=(const DemoResults&) = default;
    DemoResults(DemoResults&&) noexcept = default;
    DemoResults& operator=(DemoResults&&) noexcept = default;
};

// Callback interface for demo overlay
class DemoCallbacks {
public:
    virtual ~DemoCallbacks() {}
    DemoCallbacks(const DemoCallbacks&) = delete;
    DemoCallbacks& operator=(const DemoCallbacks&) = delete;
    DemoCallbacks(DemoCallbacks&&) = delete;
    DemoCallbacks& operator=(DemoCallbacks&&) = delete;
protected:
    DemoCallbacks() = default;
public:
    // Called each frame with current state. Returns false to abort.
    virtual bool onDemoFrame(DemoTier current_tier, int tier_index, int total_tiers,
                             float tier_progress, double fps, double frame_ms,
                             const std::vector<double>& frame_history) = 0;
};

// Runs the demo benchmark: iterates through supported tiers, measures FPS.
class DemoRunner {
public:
    DemoRunner();

    // Run the demo. Returns results.
    DemoResults run(Renderer* r, RenderContext* ctx,
                    const DemoConfig& cfg,
                    int render_w, int render_h,
                    PollCallback* poll_cb,
                    DemoCallbacks* demo_cb);

private:
    DemoTierResult runTier(Renderer* r, RenderContext* ctx,
                           DemoTier tier, int duration_sec,
                           int render_w, int render_h,
                           PollCallback* poll_cb,
                           DemoCallbacks* demo_cb,
                           int tier_index, int total_tiers);

    static double targetFPS(DemoTier tier);
};
