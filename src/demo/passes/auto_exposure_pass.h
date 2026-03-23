#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"

class AutoExposurePass : public DemoRenderPass {
public:
    const char* name() const override { return "auto_exposure"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

private:
    UniformBlock ub_histogram_;
    UniformBlock ub_exposure_;
};
