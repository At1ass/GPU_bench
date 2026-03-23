#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"
#include "demo/demo_scene.h"

// Fragment bloom pass: extract bright pixels + horizontal blur + vertical blur (ping-pong).
class BloomPass : public DemoRenderPass {
public:
    const char* name() const override { return "bloom"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneColor,  ResourceDecl::READ },
            { ResourceId::BloomResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    DemoTier minTier() const override { return DemoTier::Enhanced; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_bloom && !cfg.enable_compute_bloom;
    }
    int executionOrder() const override { return 100; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    UniformBlock ub_bloom_extract_;
    UniformBlock ub_bloom_blur_;
};
