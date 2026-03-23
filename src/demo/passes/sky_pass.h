#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"

class SkyPass : public DemoRenderPass {
public:
    const char* name() const override { return "sky"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

private:
    UniformBlock ub_;
};
