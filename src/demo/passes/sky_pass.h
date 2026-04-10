#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"

class SkyPass : public RenderPassBase {
public:
    const char* name() const override { return "sky"; }
    void init(const TierResourceView& res);
    void execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRColor, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    DemoTier minTier() const override { return DemoTier::Basic; }
    bool isEnabled(const DemoTierConfig&, const DemoDebugOverrides&) const override {
        return true;
    }
    int executionOrder() const override { return 10; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    UniformBlock ub_;
};
