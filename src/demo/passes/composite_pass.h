#pragma once
#include "engine/fullscreen_pass.h"
#include "demo/scene/demo_scene.h"

// T2/T3 final composite: scene + bloom + SSAO + vignette + color grading.
class CompositePass : public FullscreenPass {
public:
    const char* name() const override { return "composite"; }
    void init(const TierResourceView& res);

    // FullscreenPass interface
    void setup(const TierResourceView& res) override;
    void inputs(PassContext& ctx, const TierResourceView& res,
                const FrameData& fd) override;
    void uniforms(UniformBlock& ub, const FrameData& fd,
                  const DemoTierConfig& cfg) override;

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
    PassRole passRole() const override { return PassRole::FinalComposite; }

private:
    const TierResourceView* res_ = nullptr;
};
