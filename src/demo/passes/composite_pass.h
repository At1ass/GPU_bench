#pragma once
#include "engine/fullscreen_pass.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

// T2/T3 final composite: scene + bloom + SSAO + vignette + color grading.
class CompositePass : public FullscreenPass, public DemoPassMeta {
public:
    const char* name() const override { return "composite"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);

    // FullscreenPass interface
    void onExecute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneColor,  ResourceDecl::READ },
            { ResourceId::BloomResult, ResourceDecl::READ },
            { ResourceId::AOResult,    ResourceDecl::READ }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    int minTier() const override { return 2; }
    bool isEnabled() const override;
    int executionOrder() const override { return 200; }
    PassRole passRole() const override { return PassRole::FinalComposite; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
};
