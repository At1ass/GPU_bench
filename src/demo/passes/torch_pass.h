#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/demo_scene.h"

class TorchPass : public RenderPassBase {
public:
    const char* name() const override { return "torch"; }
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
    DemoTier minTier() const override { return DemoTier::Quality; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.point_light_count >= 3;
    }
    int executionOrder() const override { return 48; } // after opaque (20), before particles (50)
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    UniformBlock ub_;
};
