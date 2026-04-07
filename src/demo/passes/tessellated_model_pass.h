#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"
#include "demo/demo_scene.h"

class TessellatedModelPass : public DemoRenderPass {
public:
    const char* name() const override { return "tessellated_model"; }
    void init(const TierResourceView& res);
    void execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    // TessellatedModelPass calls FurPass at the end.
    // Set via this setter so DemoScene can wire it up.
    void setFurPass(DemoRenderPass* fur) { fur_pass_ = fur; }

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ShadowMap, ResourceDecl::READ },
            { ResourceId::HDRColor,  ResourceDecl::WRITE },
            { ResourceId::HDRDepth,  ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_tessellation;
    }
    int executionOrder() const override { return 40; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    UniformBlock ub_;
    DemoRenderPass* fur_pass_ = nullptr;
};
