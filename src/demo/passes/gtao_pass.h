#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"

class GTAOPass : public DemoRenderPass {
public:
    const char* name() const override { return "gtao"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

private:
    UniformBlock ub_gtao_;
    UniformBlock ub_blur_;
};
