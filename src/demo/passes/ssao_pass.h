#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"

// SSAO pass: compute ambient occlusion from scene depth + blur.
// Combines renderSSAOPass() and renderSSAOBlur() into a single execute().
class SSAOPass : public DemoRenderPass {
public:
    const char* name() const override { return "ssao"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

private:
    void renderSSAO(Renderer* r, FrameData& fd, const TierResourceView& res,
                    const DemoTierConfig& cfg);
    void renderSSAOBlur(Renderer* r, FrameData& fd, const TierResourceView& res);

    UniformBlock ub_ssao_;
    UniformBlock ub_ssao_blur_;
};
