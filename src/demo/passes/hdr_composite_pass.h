#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"

// T4 HDR composite: ACES tone mapping + bloom + AO + volumetric fog + SSR + DoF.
// Reads from many textures and applies final post-processing.
class HDRCompositePass : public DemoRenderPass {
public:
    HDRCompositePass() : prev_exposure_(1.0f) {}

    const char* name() const override { return "hdr_composite"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

private:
    UniformBlock ub_tone_map_;
    float prev_exposure_;
};
