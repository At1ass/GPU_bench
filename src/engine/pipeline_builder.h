#pragma once
#include "engine/render_pipeline.h"
#include "engine/pipeline_policy.h"
#include <vector>

class RenderPassBase;

// Build a render pipeline from pre-filtered passes using:
// 1. Topological sort by ResourceDecl dependencies
// 2. Policy-driven RT routing and barrier insertion
//
// Passes are assumed already filtered by the application (tier gating,
// enable/disable checks). The engine builder only handles ordering and
// RT/barrier configuration via the policy interface.
void buildPipeline(RenderPipeline& out,
                   const std::vector<RenderPassBase*>& passes,
                   const PipelinePolicy& policy,
                   int viewport_w, int viewport_h);
