#pragma once
#include "engine/pipeline_policy.h"
#include "engine/render_pipeline.h"
#include "engine/resource_id.h"
#include "demo/tier/tier_resource_view.h"
#include <memory>
#include <vector>

class RenderPassBase;
struct DemoTierConfig;
struct DemoDebugOverrides;

// Demo-level pipeline policy: routes passes to render targets
// based on the detected rendering path (HDR / SceneFBO / DefaultFB).
//
// Implements the three-path RT routing logic from the demo pipeline:
//   - T4 HDR path: shadow RT -> HDR RT -> unbind -> post-proc -> dest
//   - T2/T3 scene FBO path: shadow RT -> scene_to_fbo -> post-proc -> dest
//   - T1 default FB path: shadow RT -> default FB with clear -> passes
//
// Stateful: routePass() tracks binding state across calls (mutable).
// Must be constructed fresh for each pipeline rebuild.
class DemoPipelinePolicy : public PipelinePolicy {
public:
    // Detected rendering path
    enum class Path { DefaultFB, SceneFBO, HDR };

    DemoPipelinePolicy(Path path,
                       const TierResourceView& res,
                       int viewport_w, int viewport_h);

    PassRTBinding routePass(const RenderPassBase& pass,
                            int sorted_index, int total_passes) const override;
    PassSyncHint syncHint(const RenderPassBase& pass) const override;
    PassRTBinding pipelinePrologue() const override;

private:
    Path path_;
    const TierResourceView& res_;
    int viewport_w_, viewport_h_;
    float fog_r_, fog_g_, fog_b_;
    int shadow_size_;

    // Mutable state tracking for HDR path
    mutable bool hdr_rt_bound_;
    mutable bool hdr_rt_unbound_;

    // Mutable state tracking for DefaultFB path
    mutable bool fb_cleared_;

    // Internal routing helpers
    PassRTBinding routeHDR(const RenderPassBase& pass) const;
    PassRTBinding routeSceneFBO(const RenderPassBase& pass) const;
    PassRTBinding routeDefaultFB(const RenderPassBase& pass) const;

    bool writesResource(const RenderPassBase& pass, ResourceId rid) const;
    bool readsResource(const RenderPassBase& pass, ResourceId rid) const;
};

// Demo pipeline entry point: filters passes by tier/enabled via DemoPassMeta,
// detects rendering path, creates policy, calls engine buildPipeline.
void demoBuildPipeline(RenderPipeline& pipeline,
                       const std::vector<std::unique_ptr<RenderPassBase>>& passes,
                       const DemoTierConfig& config,
                       const DemoDebugOverrides& debug,
                       const TierResourceView& res,
                       int viewport_w, int viewport_h);
