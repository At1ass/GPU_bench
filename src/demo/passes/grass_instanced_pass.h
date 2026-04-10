#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/demo_scene.h"

class GrassInstancedPass : public RenderPassBase {
public:
    const char* name() const override { return "grass_instanced"; }
    void init(const TierResourceView& res);
    void execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ShadowMap, ResourceDecl::READ },
            { ResourceId::HDRColor,  ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    DemoTier minTier() const override { return DemoTier::Enhanced; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.instanced_grass_count > 0;
    }
    int executionOrder() const override { return 30; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    UniformBlock ub_;
};
