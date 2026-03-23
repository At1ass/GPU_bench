#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"

class TessellatedModelPass : public DemoRenderPass {
public:
    const char* name() const override { return "tessellated_model"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    // TessellatedModelPass calls FurPass at the end.
    // Set via this setter so DemoScene can wire it up.
    void setFurPass(DemoRenderPass* fur) { fur_pass_ = fur; }

private:
    UniformBlock ub_;
    DemoRenderPass* fur_pass_ = nullptr;
};
