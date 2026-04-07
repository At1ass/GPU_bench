#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"
#include "demo/demo_scene.h"

// SSAO pass: compute ambient occlusion from scene depth + blur.
// Combines renderSSAOPass() and renderSSAOBlur() into a single execute().
class SSAOPass : public DemoRenderPass {
public:
    const char* name() const override { return "ssao"; }
    void init(const TierResourceView& res);
    void execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneDepth, ResourceDecl::READ },
            { ResourceId::AOResult,   ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    DemoTier minTier() const override { return DemoTier::Enhanced; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_ssao && !cfg.enable_gtao;
    }
    int executionOrder() const override { return 100; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    void renderSSAO(Renderer* r, FrameData& fd, const TierResourceView& res,
                    const DemoTierConfig& cfg);
    void renderSSAOBlur(Renderer* r, FrameData& fd, const TierResourceView& res);

    UniformBlock ub_ssao_;
    UniformBlock ub_ssao_blur_;
};
