#pragma once
#include "demo/demo_pipeline.h"
#include "demo/pass_factory.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

// SSR framebuffer copy command (used as PipelineCommandFn)
void ssrCopyCommand(Renderer* r, FrameData& fd,
                    const TierResourceView& res,
                    const DemoTierConfig& cfg,
                    const SceneData& scene);

// Builds render pipeline for a given tier configuration.
// Knows the rules: which passes, in what order, with what RT bindings.
// DemoScene and DemoPipeline don't know these rules.
void buildPipeline(DemoPipeline& pipeline,
                   const DemoPassSet& passes,
                   const DemoTierConfig& config,
                   const DemoDebugOverrides& debug,
                   const TierResourceView& res,
                   int viewport_w, int viewport_h);
