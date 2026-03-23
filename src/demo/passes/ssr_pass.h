#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"

class SSRPass : public DemoRenderPass {
public:
    const char* name() const override { return "ssr"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRColor,  ResourceDecl::READ },
            { ResourceId::HDRDepth,  ResourceDecl::READ },
            { ResourceId::SSRResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig&, const DemoDebugOverrides&) const override {
        return false;
    }
    int executionOrder() const override { return 55; }
    QueueType queueType() const override { return QueueType::Compute; }

private:
    UniformBlock ub_;
};
