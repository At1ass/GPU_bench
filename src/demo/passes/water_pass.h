#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"

class WaterPass : public DemoRenderPass {
public:
    const char* name() const override { return "water"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ShadowMap, ResourceDecl::READ },
            { ResourceId::SSRResult, ResourceDecl::READ },
            { ResourceId::HDRDepth,  ResourceDecl::READ },
            { ResourceId::HDRColor,  ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 4; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig&, const DemoDebugOverrides&) const override {
        return true;
    }
    int executionOrder() const override { return 60; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    UniformBlock ub_;
};
