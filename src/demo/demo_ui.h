#pragma once
#include "demo/demo_runner.h"
#include "engine/frame_stats.h"
#include "renderer/renderer.h"
#include <vector>

class RenderContext;
struct TechniqueInfo;

// Minimal overlay drawn on top of the demo scene during benchmark.
class DemoUI {
public:
    DemoUI() = default;

    // Draw the in-progress overlay (FPS, tier, progress, technique info)
    void drawOverlay(RenderContext* ctx,
                     const char* gpu_name,
                     DemoTier current_tier,
                     int tier_index, int total_tiers,
                     float tier_progress,
                     double fps, double frame_ms,
                     const std::vector<double>& frame_history,
                     const TechniqueInfo* tech_info,
                     const FrameStats* stats = nullptr,
                     const RendererStats* rstats = nullptr);

    // Draw the final results screen
    void drawResults(RenderContext* ctx,
                     const DemoResults& results);
};
