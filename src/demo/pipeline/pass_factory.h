#pragma once
#include "engine/render_pass.h"
#include <memory>
#include <vector>

struct TierResourceView;
struct DemoTierConfig;
struct DemoDebugOverrides;

// Create all render passes. Passes are owned by the vector.
// Sub-pass wiring (tess->fur, sceneToFBO->sub-passes) done internally.
void createPasses(std::vector<std::unique_ptr<RenderPassBase>>& out,
                  const TierResourceView& res,
                  const DemoTierConfig& cfg,
                  const DemoDebugOverrides& dbg);
