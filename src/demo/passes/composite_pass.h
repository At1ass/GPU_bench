#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"
#include "demo/demo_scene.h"

// T2/T3 final composite: scene + bloom + SSAO + vignette + color grading.
class CompositePass : public DemoRenderPass {
public:
    const char* name() const override { return "composite"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneColor,  ResourceDecl::READ },
            { ResourceId::BloomResult, ResourceDecl::READ },
            { ResourceId::AOResult,    ResourceDecl::READ }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    DemoTier minTier() const override { return DemoTier::Enhanced; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_bloom && !cfg.enable_hdr;
    }
    int executionOrder() const override { return 200; }
    QueueType queueType() const override { return QueueType::Graphics; }
    PassRole passRole() const override { return PassRole::FinalComposite; }

private:
    UniformBlock ub_bloom_composite_;
};
