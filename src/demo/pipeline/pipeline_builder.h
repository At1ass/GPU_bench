#pragma once
#include "demo/pipeline/demo_pipeline.h"
#include <memory>
#include <vector>

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;
class RenderPassBase;

// Build render pipeline from pass vector using resource dependency
// topological sort. Passes declare reads/writes via resourceDecls(),
// builder determines execution order automatically.
void buildPipeline(DemoPipeline& pipeline,
                   const std::vector<std::unique_ptr<RenderPassBase>>& passes,
                   const DemoTierConfig& config,
                   const DemoDebugOverrides& debug,
                   const TierResourceView& res,
                   int viewport_w, int viewport_h);
